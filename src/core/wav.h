#pragma once
// WAV (RIFF PCM) writer for the sound output of the emulation.
#include <stdint.h>
#include <string>

bool wav_write_mono16(const std::string& path, const int16_t* samples, size_t count, uint32_t rate);
