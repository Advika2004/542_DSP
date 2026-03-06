#include "audio_player.h"
#include <iostream>
#include <sndfile.h>

// ─────────────────────────────────────────────
// constructor / destructor
// ─────────────────────────────────────────────

AudioPlayer::AudioPlayer() : stream(nullptr) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio init failed: " << Pa_GetErrorText(err) << "\n";
    }
}

AudioPlayer::~AudioPlayer() {
    stop();
    Pa_Terminate();
}

// ─────────────────────────────────────────────
// loadFile
// opens a WAV with libsndfile and reads all samples into state.samples
// libsndfile handles WAV, FLAC, AIFF - no format parsing needed
// ─────────────────────────────────────────────
bool AudioPlayer::loadFile(const std::string& path) {
    SF_INFO info;
    SNDFILE* file = sf_open(path.c_str(), SFM_READ, &info);
    if (!file) {
        std::cerr << "ERROR: could not open " << path << "\n";
        std::cerr << sf_strerror(file) << "\n";
        return false;
    }

    state.sampleRate  = info.samplerate;
    state.numChannels = info.channels;
    state.playhead.store(0); // reset playhead for new file

    sf_count_t totalSamples = info.frames * info.channels;
    state.samples.resize(totalSamples);
    sf_read_float(file, state.samples.data(), totalSamples);
    sf_close(file);

    std::cout << "loaded: " << path << "\n";
    std::cout << "  sample rate : " << state.sampleRate  << " Hz\n";
    std::cout << "  channels    : " << state.numChannels  << "\n";
    std::cout << "  duration    : "
              << (float)info.frames / info.samplerate << " sec\n";
    return true;
}

// ─────────────────────────────────────────────
// listDevices
// prints all output devices with their index
// on the Pi, look for "USB Audio Device" or similar
// pass that index to play() to route audio to it
// ─────────────────────────────────────────────
void AudioPlayer::listDevices() {
    int count = Pa_GetDeviceCount();
    std::cout << "\navailable output devices:\n";
    for (int i = 0; i < count; i++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info->maxOutputChannels > 0) {
            std::cout << "  [" << i << "] " << info->name
                      << "  (" << info->maxOutputChannels << " ch)\n";
        }
    }
    std::cout << "  system default: ["
              << Pa_GetDefaultOutputDevice() << "]\n\n";
}

// ─────────────────────────────────────────────
// play
// opens and starts the PortAudio stream
// deviceIndex = -1 means use system default output
// on the Pi, plug in USB adapter, run listDevices(), pass that index
// ─────────────────────────────────────────────
bool AudioPlayer::play(int deviceIndex) {
    if (state.samples.empty()) {
        std::cerr << "ERROR: no file loaded - call loadFile() first\n";
        return false;
    }

    PaStreamParameters outputParams;
    outputParams.device = (deviceIndex == -1)
                            ? Pa_GetDefaultOutputDevice()
                            : deviceIndex;

    if (outputParams.device == paNoDevice) {
        std::cerr << "ERROR: no output device found\n";
        return false;
    }
    state.outChannels = 2;
    outputParams.channelCount              = state.outChannels;
    outputParams.sampleFormat              = paFloat32;
    outputParams.suggestedLatency          =
        Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
    outputParams.hostApiSpecificStreamInfo = nullptr;

    std::cout << "using device: ["
              << outputParams.device << "] "
              << Pa_GetDeviceInfo(outputParams.device)->name << "\n";

    PaError err = Pa_OpenStream(&stream,
                                nullptr,        // no input
                                &outputParams,
                                state.sampleRate,
                                256,            // frames per buffer - low latency
                                paClipOff,
                                audioCallback,
                                &state);        // passed as userData to callback

    if (err != paNoError) {
        std::cerr << "Pa_OpenStream failed: " << Pa_GetErrorText(err) << "\n";
        return false;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Pa_StartStream failed: " << Pa_GetErrorText(err) << "\n";
        return false;
    }

    std::cout << "playing...\n";
    return true;
}

// ─────────────────────────────────────────────
// stop
// stops and closes the stream cleanly
// safe to call even if stream is already stopped
// ─────────────────────────────────────────────
void AudioPlayer::stop() {
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        stream = nullptr;
        std::cout << "stopped.\n";
    }
}

// ─────────────────────────────────────────────
// isPlaying
// returns true if the stream is open and active
// ─────────────────────────────────────────────
bool AudioPlayer::isPlaying() {
    return stream && Pa_IsStreamActive(stream) == 1;
}

// ─────────────────────────────────────────────
// effect controls
// safe to call while audio is playing
// atomic stores are picked up by the callback within ~5ms
// ─────────────────────────────────────────────
void AudioPlayer::setVolume(float volume) {
    state.volume.store(volume);
}

void AudioPlayer::setLowPass(bool enabled, float cutoffHz) {
    state.lowPassEnabled.store(enabled);
    state.lowPassCutoff.store(cutoffHz);
}

void AudioPlayer::setHighPass(bool enabled, float cutoffHz) {
    state.highPassEnabled.store(enabled);
    state.highPassCutoff.store(cutoffHz);
}

// ─────────────────────────────────────────────
// audioCallback
// called by PortAudio every ~5ms on its own thread
// fills framesPerBuffer frames into the output buffer
// NO malloc, no printf, no file I/O in here - just fast math
// ─────────────────────────────────────────────
int AudioPlayer::audioCallback(const void* inputBuffer,
                                void* outputBuffer,
                                unsigned long framesPerBuffer,
                                const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void* userData)
{
    AudioState* state = (AudioState*)userData;
    float* out = (float*)outputBuffer;

    // snapshot atomics once - cheaper than loading on every sample
    size_t playhead  = state->playhead.load();
    float  volume    = state->volume.load();
    bool   lpEnabled = state->lowPassEnabled.load();
    bool   hpEnabled = state->highPassEnabled.load();

    // recompute filter coefficients if enabled
    // any cutoff change is reflected within this callback
    if (lpEnabled) {
        Coeffs c = lowPassCoeffs(state->lowPassCutoff.load(), state->sampleRate);
        for (int ch = 0; ch < state->numChannels; ch++)
            state->lowPassFilter[ch].setCoeffs(c);
    }
    if (hpEnabled) {
        Coeffs c = highPassCoeffs(state->highPassCutoff.load(), state->sampleRate);
        for (int ch = 0; ch < state->numChannels; ch++)
            state->highPassFilter[ch].setCoeffs(c);
    }

    // fill output buffer frame by frame
    // one frame = numChannels samples (e.g. stereo = 2 floats)
    for (unsigned long frame = 0; frame < framesPerBuffer; ++frame) {
    	float inL = 0.0f, inR = 0.0f;

    	if (state->numChannels == 1) {
            size_t idx = playhead + frame; // mono file
            float s = (idx < state->samples.size()) ? state->samples[idx] : 0.0f;
            inL = inR = s; // upmix
    	} else {
            size_t idxL = playhead + frame * state->numChannels + 0;
            size_t idxR = playhead + frame * state->numChannels + 1;
            inL = (idxL < state->samples.size()) ? state->samples[idxL] : 0.0f;
            inR = (idxR < state->samples.size()) ? state->samples[idxR] : 0.0f;
        }

        float outL = inL;
    	float outR = inR;

    	if (lpEnabled) {
            outL = state->lowPassFilter[0].process(outL);
            outR = state->lowPassFilter[1].process(outR);
     	}
    	if (hpEnabled) {
            outL = state->highPassFilter[0].process(outL);
            outR = state->highPassFilter[1].process(outR);
	}

   	outL *= volume;
        outR *= volume;

   	// optional but good: clamp to avoid clipping
   	outL = std::max(-1.0f, std::min(1.0f, outL));
   	outR = std::max(-1.0f, std::min(1.0f, outR));

    	*out++ = outL;
    	*out++ = outR;
    }
    // advance playhead by the samples we just consumed
    size_t newPlayhead = playhead + framesPerBuffer * state->numChannels;
    state->playhead.store(newPlayhead);

    return (newPlayhead >= state->samples.size()) ? paComplete : paContinue;
}
