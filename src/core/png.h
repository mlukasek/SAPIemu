#pragma once
// Minimal PNG encoder (RGB, uncompressed deflate blocks) for screenshots.
#include <stdint.h>
#include <string>
#include <vector>

// Encode an ARGB8888 image into PNG bytes. `scale` repeats pixels (1..8).
std::vector<uint8_t> png_encode_argb(const uint32_t* argb, int w, int h, int scale_x, int scale_y);
bool png_write_argb(const std::string& path, const uint32_t* argb, int w, int h, int scale_x, int scale_y);
