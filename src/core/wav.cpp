#include "core/wav.h"
#include "core/util.h"

#include <string.h>
#include <vector>

static void put16(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t* p, uint32_t v)
{
    put16(p, v & 0xFFFF);
    put16(p + 2, v >> 16);
}

bool wav_write_mono16(const std::string& path, const int16_t* samples, size_t count, uint32_t rate)
{
    uint32_t data = (uint32_t)(count * 2);
    std::vector<uint8_t> f(44 + (size_t)data);
    uint8_t* h = f.data();
    memcpy(h, "RIFF", 4);
    put32(h + 4, 36 + data);
    memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16);
    put16(h + 20, 1);          // PCM
    put16(h + 22, 1);          // mono
    put32(h + 24, rate);
    put32(h + 28, rate * 2);   // bytes per second
    put16(h + 32, 2);          // block align
    put16(h + 34, 16);         // bits per sample
    memcpy(h + 36, "data", 4);
    put32(h + 40, data);
    uint8_t* d = h + 44;
    for (size_t i = 0; i < count; i++) put16(d + 2 * i, (uint16_t)samples[i]);
    return file_write_all(path, f.data(), f.size());
}
