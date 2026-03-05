#ifndef MAIN_H
#define MAIN_H

#include <iostream>
#include <vector>
#include <string>
#include <sndfile.h>
#include "dsp.h"
#include "audio_player.h"

bool readWav(const std::string& path, std::vector<float>& samples, int& sampleRate, int& numChannels);
bool writeWav(const std::string& path, const std::vector<float>& samples, int sampleRate, int numChannels);
void applyFilter(std::vector<float>& samples, int numChannels, const Coeffs& coeffs);
void applyVolume(std::vector<float>& samples, float volume);



#endif