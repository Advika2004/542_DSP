#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <string>
#include <vector>
#include <atomic>
#include <portaudio.h>
#include "dsp.h"


// ─────────────────────────────────────────────
// AudioState
// holds the loaded audio data and all effect parameters
// lives here so both audio_player.cpp and main.cpp can see it
// all effect parameters are std::atomic because the PortAudio
// callback reads them on its own thread while main() writes them
// ─────────────────────────────────────────────
struct AudioState {
    std::vector<float> samples;     // entire WAV in memory as interleaved floats
    int numChannels = 0;
    int sampleRate  = 0;

    std::atomic<size_t> playhead{0}; // current position in samples[]

    // effect parameters - write from main(), read in callback
    std::atomic<float> volume{1.0f};

    std::atomic<bool>  lowPassEnabled{false};
    std::atomic<float> lowPassCutoff{500.0f};

    std::atomic<bool>  highPassEnabled{false};
    std::atomic<float> highPassCutoff{2000.0f};

    // filter instances - one per channel, state persists across callbacks
    BiquadFilter lowPassFilter[2];
    BiquadFilter highPassFilter[2];

    std::atomic<bool>  reverbEnabled{false};
    std::atomic<float> reverbWet{0.3f};
    std::atomic<float> reverbDry{0.7f};

    SchroederReverb* reverb[2] = {nullptr, nullptr};
};

// ─────────────────────────────────────────────
// AudioPlayer
// owns the PortAudio stream and AudioState
// call loadFile() then play(), then use setVolume() etc. to change effects
// ─────────────────────────────────────────────
class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    // load a WAV file into memory - call before play()
    bool loadFile(const std::string& path);

    // list all output devices to find your USB adapter index
    void listDevices();

    // open and start the PortAudio stream
    // deviceIndex = -1 uses system default
    bool play(int deviceIndex = -1);

    // stop and close the stream
    void stop();

    // returns true if audio is currently playing
    bool isPlaying();

    // ─────────────────────────────────────────────
    // effect controls
    // safe to call from main() while audio is playing
    // changes take effect within one callback (~5ms)
    // ─────────────────────────────────────────────
    void setVolume(float volume);                  // 0.0 to 1.0
    void setLowPass(bool enabled, float cutoffHz); // cut treble
    void setHighPass(bool enabled, float cutoffHz);// cut bass
    void setReverb(bool enabled, float wet = 0.3f, float dry = 0.7f);

    // direct access to state for your partner's gesture code
    // they can call player.state.volume.store(newVal) directly
    AudioState state;

private:
    PaStream* stream;

    // PortAudio callback - static because C callbacks cant be member functions
    // userData pointer is cast back to AudioState* inside
    static int audioCallback(const void* input,
                             void* output,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void* userData);
};
#endif