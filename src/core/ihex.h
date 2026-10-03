#pragma once
// Intel HEX parser and ROM image loader.
#include <stdint.h>
#include <string>
#include <vector>

typedef void (*ihex_byte_fn)(void* user, uint32_t addr, uint8_t value);

// Parses Intel HEX text (record types 00, 01, 02, 04; 03/05 are ignored).
// Calls `fn` for every data byte. Returns false and fills `err` on a malformed record.
bool ihex_parse(const std::string& text, ihex_byte_fn fn, void* user, std::string& err);

// Loads a ROM/PROM image into `image` (resized to `size`, filled with `fill`).
// Intel HEX files (.hex/.txt or content starting with ':') are placed at their
// record addresses minus `hex_base`; binary files are copied from offset 0.
bool rom_load(const std::string& path, std::vector<uint8_t>& image, size_t size,
              uint8_t fill, uint32_t hex_base, std::string& err);
