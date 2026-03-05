#include "main.h"


//! just tests the dsp in c++, doesn't use portaudio to read anything or output anything
//reads a WAV file into a float vector
//returns true on success, fills samples, sampleRate, numChannels
bool readWav(const std::string& path, std::vector<float>& samples, int& sampleRate, int& numChannels)
{
    //struct where wav metadata is stored
    SF_INFO info;
    //opens the wav file to read it and puts all metadata into info
    SNDFILE* file = sf_open(path.c_str(), SFM_READ, &info);
    if (!file) {
        std::cerr << "ERROR: could not open " << path << "\n";
        std::cerr << sf_strerror(file) << "\n";
        return false;
    }

    //get out hte values from info
    sampleRate  = info.samplerate;
    numChannels = info.channels;

    //total number of samples = frames * channels
    sf_count_t totalSamples = info.frames * info.channels;

    //like malloc allocates that many spaces in the float vector
    samples.resize(totalSamples);

    //reads the float vector into memory
    //samples.data() is a pointer to the float array in memory
    sf_count_t read = sf_read_float(file, samples.data(), totalSamples);
    sf_close(file);

    //prints just to verify info about wav file
    std::cout << "read " << path << "\n";
    std::cout << "  sample rate : " << sampleRate  << " Hz\n";
    std::cout << "  channels    : " << numChannels  << "\n";
    std::cout << "  frames      : " << info.frames  << "\n";
    std::cout << "  samples read: " << read         << "\n";

    return true;
}

//writes a float vector back out to a WAV file
bool writeWav(const std::string& path, const std::vector<float>& samples, int sampleRate, int numChannels)
{
    //creates metadata for new 
    SF_INFO info;
    info.samplerate = sampleRate;
    info.channels   = numChannels;
    info.format     = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

    //creates metadata for new output wav file
    SNDFILE* file = sf_open(path.c_str(), SFM_WRITE, &info);
    if (!file) {
        std::cerr << "ERROR: could not open " << path << " for writing\n";
        std::cerr << sf_strerror(file) << "\n";
        return false;
    }

    //writes to output file and closes it
    sf_write_float(file, samples.data(), samples.size());
    sf_close(file);

    std::cout << "wrote " << path << "\n";
    return true;
}

// applies a single BiquadFilter to an interleaved stereo (or mono) buffer
// interleaved means samples are stored as [L, R, L, R, L, R ...]
// we need one filter instance per channel so their state doesn't mix
void applyFilter(std::vector<float>& samples, int numChannels, const Coeffs& coeffs)
{
    // one filter per channel so state variables stay independent
    std::vector<BiquadFilter> filters(numChannels);
    for (auto& f : filters) {
        f.setCoeffs(coeffs);
    }

    // walk through every sample
    // index % numChannels tells us which channel this sample belongs to
    for (size_t i = 0; i < samples.size(); i++) {
        int channel = i % numChannels;
        samples[i] = filters[channel].process(samples[i]);
    }
}

void applyReverb(std::vector<float>& samples, int numChannels, int sampleRate)
{
    std::vector<SchroederReverb> reverbs;
    reverbs.reserve(numChannels);
    for (int ch = 0; ch < numChannels; ++ch) {
        reverbs.emplace_back(sampleRate);
    }

    const float wet = 1.0f;
    const float dry = 0.0f;

    for (size_t i = 0; i < samples.size(); ++i) {
        int ch = (int)(i % numChannels);
        float x = samples[i];
        float y = reverbs[ch].process(x);
        samples[i] = dry * x + wet * y;
    }
}

// applies volume scaling to every sample
void applyVolume(std::vector<float>& samples, float volume) {
    for (auto& s : samples) {
        s = s * volume;
    }
}

int main(int argc, char* argv[]) {
    std::string inputPath = (argc > 1) ? argv[1] : "input.wav";

    AudioPlayer player;

    // print available output devices
    // on the Pi, look for your USB adapter index here
    // then pass it to player.play(index) below
    player.listDevices();

    // load WAV file
    if (!player.loadFile(inputPath)) {
        return 1;
    }

    // ─────────────────────────────────────────────
    // set initial effect parameters
    // flip enabled flags to true to test each effect
    // later these get called by your partner's gesture code
    // ─────────────────────────────────────────────
    player.setVolume(0.8f);
    player.setLowPass(false, 500.0f);    // true = cut treble, keep bass
    player.setHighPass(false, 2000.0f);  // true = cut bass, keep treble

    // start playback - pass USB device index here if not using default
    // e.g. player.play(2) if your USB adapter shows as [2]
    if (!player.play()) {
        return 1;
    }

    // ─────────────────────────────────────────────
    // main thread waits here while audio plays
    // this is where gesture commands plug in later:
    //
    //   player.setVolume(0.5f);
    //   player.setLowPass(true, 300.0f);
    //   player.setHighPass(true, 4000.0f);
    //
    // those calls are safe to make while audio is playing
    // ─────────────────────────────────────────────
    std::cout << "press enter to stop\n";
    std::cin.get();

    player.stop();
    return 0;
}

// int main(int argc, char* argv[]) {

//     std::string inputPath  = (argc > 1) ? argv[1] : "input.wav";
//     std::string outputPath = (argc > 2) ? argv[2] : "output.wav";

//     //init the float array and the counters
//     std::vector<float> samples;
//     int sampleRate   = 0;
//     int numChannels  = 0;

//     //check if we can read the metadata out
//     if (!readWav(inputPath, samples, sampleRate, numChannels)) {
//         return 1;
//     }

//     //! testing filters
//     //TEST 1: low pass at 500 Hz - should sound muffled, bass only
//     //calculate the coefficients
//     //Coeffs c = lowPassCoeffs(500.0f, sampleRate);

//     //TEST 2: high pass at 2000 Hz - should sound tinny, highs only
//     //Coeffs c = highPassCoeffs(2000.0f, sampleRate);


//     //apply the coefficients
//     //applyFilter(samples, numChannels, c);
//     //TEST 3: reverb
//     applyReverb(samples, numChannels, sampleRate);  

//     // TEST 5: volume - try 0.2f (quiet) or 1.0f (unchanged)
//     // applyVolume(samples, 0.5f);

//     if (!writeWav(outputPath, samples, sampleRate, numChannels)) {
//         return 1;
//     }

//     std::cout << "done. open " << outputPath << " in any audio player to listen.\n";
//     return 0;
// }