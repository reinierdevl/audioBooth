#pragma once

#include <stdint.h>

// Initializes the ES8311 for BCLK-derived internal MCLK and configures I2S.
bool beginAudioPlayer();

// Starts one local file from the storage source selected at boot.
bool playAudioFile(const char *path);

// Stops decoding/output and mutes the codec.
void stopAudioPlayback();

// Must be called frequently from loop() while playback may be active.
void serviceAudioPlayback();

bool audioIsPlaying();

// Persistent playback volume shown on the website, expressed as 0..100%.
uint8_t audioVolumePercent();
bool setAudioVolumePercent(uint8_t percent);

// Returns true once after a file reaches its natural end.
bool takeAudioEndEvent();
