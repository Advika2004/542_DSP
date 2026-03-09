#include "dsp.h"

CircleBuff::CircleBuff(int maxDelay)
: buffer(maxDelay, 0.0f), writeIndex(0), size(maxDelay){}

void CircleBuff::fifo_update(float sample) {
    buffer[writeIndex] = sample;
    writeIndex = (writeIndex + 1) % size;  // same modulo wrap as Python
}

float CircleBuff::fifo_get(int delaySamples) {
    int index = (writeIndex - delaySamples + size) % size;
    return buffer[index];
}


//?basically init function
BiquadFilter::BiquadFilter() {
    //start with all coefficients at 0
    coeffs = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    x1 = x2 = 0.0f;
    y1 = y2 = 0.0f;
}

//?function to set the coefficients 
void BiquadFilter::setCoeffs(const Coeffs& c) {
    coeffs = c;
}

//?difference equation that runs once per sample
// y(n) = b0*x(n) + b1*x(n-1) + b2*x(n-2) - a1*y(n-1) - a2*y(n-2)
float BiquadFilter::process(float sample) {
    float output = coeffs.b0 * sample + coeffs.b1 * x1 + coeffs.b2 * x2 - coeffs.a1 * y1 - coeffs.a2 * y2;
    //update for the new sample to move the filter along 
    x2 = x1;
    x1 = sample;
    y2 = y1;
    y1 = output;
    return output;
}

//?function to reset the filter
void BiquadFilter::reset() {
    x1 = x2 = 0.0f;
    y1 = y2 = 0.0f;
}


//!all math comes from the Audio EQ Cookbook by Robert Bristow-Johnson
//literally copied these filter equations from there, not really sure what this is doing
//actually no i kind of know, its applying different coefficients to the biquad filter eqn

Coeffs lowPassCoeffs(float cutoffHz, float sampleRate, float Q) {
    float w0 = 2.0f * M_PI * cutoffHz / sampleRate;
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float alpha = sinw0 / (2.0f * Q);

    float b0 =  (1.0f - cosw0) / 2.0f;
    float b1 =   1.0f - cosw0;
    float b2 =  (1.0f - cosw0) / 2.0f;
    float a0 =   1.0f + alpha; 
    float a1 =  -2.0f * cosw0;
    float a2 =   1.0f - alpha;

    Coeffs c;
    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
    return c;
}

Coeffs highPassCoeffs(float cutoffHz, float sampleRate, float Q) {
    //same as low pass but b coefficients change signs
    float w0 = 2.0f * M_PI * cutoffHz / sampleRate;
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float alpha = sinw0 / (2.0f * Q);

    float b0 = (1.0f + cosw0) / 2.0f;
    float b1 = -(1.0f + cosw0);
    float b2 = (1.0f + cosw0) / 2.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosw0;
    float a2 = 1.0f - alpha;

    Coeffs c;
    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
    return c;
}

SchroederReverb::SchroederReverb(int sampleRate)
: T1((int)std::lround(0.030 * sampleRate))
, T2((int)std::lround(0.035 * sampleRate))
, T3((int)std::lround(0.041 * sampleRate))
, T4((int)std::lround(0.044 * sampleRate))
, T5((int)std::lround(0.005 * sampleRate))
, T6((int)std::lround(0.0017 * sampleRate))
, g(0.92f) //will change this to increase / decrease reverb
, g_in(0.4f)
, g7(0.4f)
, w1(T1 + 1)
, w2(T2 + 1)
, w3(T3 + 1)
, w4(T4 + 1)
, x5(T5 + 1)
, w5(T5 + 1)
, x6(T6 + 1)
, w6(T6 + 1)
{}


float SchroederReverb::process(float sample) {
    //1. multiply with the in-gain at the start
    float x0 = g_in * sample;

    //put it into the 4 comb filters (w[n] = x[n] + g*w[n-T])
    float cw1 = x0 + g * w1.fifo_get(T1);  
    w1.fifo_update(cw1);

    float cw2 = x0 + g * w2.fifo_get(T2);
     w2.fifo_update(cw2);

    float cw3 = x0 + g * w3.fifo_get(T3);  
    w3.fifo_update(cw3);

    float cw4 = x0 + g * w4.fifo_get(T4);  
    w4.fifo_update(cw4);

    //#then you add those comb filters together
    float sum = cw1 + cw2 + cw3 + cw4;

    //now pass this summed signal into the all-pass filters 
    //W[n] = x[n-T5] + g * w[n-T5]
    //y[n] = -g * x[n]  + (1-g^2) * w[n] BOTH.

    float aw5 = x5.fifo_get(T5) + g * w5.fifo_get(T5);
    float y5  = -g * sum + (1.0f - g*g) * aw5;
    x5.fifo_update(sum);
    w5.fifo_update(aw5);

    // all-pass #6
    float aw6 = x6.fifo_get(T6) + g * w6.fifo_get(T6);
    float y6  = -g * y5 + (1.0f - g*g) * aw6;
    x6.fifo_update(y5);
    w6.fifo_update(aw6);

    //final output gain
    return g7 * y6;
}

//! dont know if i want eq yet
// Coeffs peakEQCoeffs(float centerHz, float sampleRate, float Q, float gainDb) {
//     //A is the gain
//     //positive = boost and negative = cut
//     float A     = powf(10.0f, gainDb / 40.0f);
//     float w0    = 2.0f * M_PI * centerHz / sampleRate;
//     float cosw0 = cosf(w0);
//     float sinw0 = sinf(w0);
//     float alpha = sinw0 / (2.0f * Q);

//     float b0 =   1.0f + alpha * A;
//     float b1 =  -2.0f * cosw0;
//     float b2 =   1.0f - alpha * A;
//     float a0 =   1.0f + alpha / A;
//     float a1 =  -2.0f * cosw0;
//     float a2 =   1.0f - alpha / A;

//     Coeffs c;
//     c.b0 = b0 / a0;
//     c.b1 = b1 / a0;
//     c.b2 = b2 / a0;
//     c.a1 = a1 / a0;
//     c.a2 = a2 / a0;
//     return c;
// }
