#ifndef DSP_H
#define DSP_H

#include <cmath>
#include <vector>

//? These are my coefficients
//b's are the input coefficients
//a's are the output coefficients
struct Coeffs {
    float b0;
    float b1;
    float b2;
    float a1; 
    float a2;
};

//?BiquadFilter equation
//y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
//x1 and x2 = previous two input samples
//y1 and y2 = previous two output samples
//class like in java to hold all the variables and the functions together

class BiquadFilter {
public:
    BiquadFilter();

    //loads the coefficients into the next filter
    void setCoeffs(const Coeffs& c);

    //processes one sample at a time and returns the filtered output sample
    float process(float sample);

    //zeros everything out incase you change songs or change the filter 
    void reset();

private:
    Coeffs coeffs;
    float x1;
    float x2;
    float y1;
    float y2;
};


//!Coefficient calculator functions
//all take cutoff Hz and sampleRate from the WAV file header

//?low pass - keeps frequencies below the cutoff frequency
//BASS BOOSTTTT
Coeffs lowPassCoeffs(float cutoffHz, float sampleRate, float Q = 0.707f);

//?high pass: keeps frequencies above cutoff frequency
//treble boost
Coeffs highPassCoeffs(float cutoffHz, float sampleRate, float Q = 0.707f);

//! dont know if i need this, might want to do reverb instead
// peak EQ - boosts or cuts a band of frequencies around center frqeuncy
// gainDb positive = boost, negative = cut
// use this for more surgical EQ control
Coeffs peakEQCoeffs(float centerHz, float sampleRate, float Q, float gainDb);
//! might want to do reverb instead

// Volume utility
// simple inline multiply - no need for a whole function
// just call this in your sample loop:
//   sample = applyVolume(sample, 0.5f); // 50% volume
inline float applyVolume(float sample, float volume) {
    return sample * volume;
}



//circular buffer for reverb
class CircleBuff {
public:
    CircleBuff(int maxDelay);
    void fifo_update(float sample); //updates the fifo when new sample comes in 
    float fifo_get(int delaySamples);   //gets the sample

private:
    std::vector<float> buffer;
    int writeIndex;
    int size;
};

//reverb using Schroeder architecture
//4 parallel comb filters into 2 series all-pass filters

class SchroederReverb {
public:
    SchroederReverb(int sampleRate);
    float process(float sample);

private:
    // 1) delay lengths first
    int T1, T2, T3, T4, T5, T6;

    // 2) gains next
    float g;
    float g_in;
    float g7;

    // 3) buffers last (because they depend on T1..T6)
    CircleBuff w1, w2, w3, w4;
    CircleBuff x5, w5;
    CircleBuff x6, w6;
};

#endif