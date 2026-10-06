#pragma once
// Cassette tape from a program (Intel HEX or binary file): the audio of a DSM-1
// recording that MIKOS V4/V5 (.K_L, .J) or MIKROBASIC 2.4 (LOAD) reads. The format
// is from the ROM listings and the original recordings (docs/software/kazeta.md).
#include <stdint.h>
#include <string>
#include <vector>

enum TapeFormat { TAPE_MIKOS = 0, TAPE_MIKROBASIC = 1 };

struct TapeProgram {
    int format = TAPE_MIKOS;
    std::string name;          // name on the tape, empty = from the file name
    int64_t address = -1;      // load address of a binary file (required there)
    int64_t entry = -1;        // start address, -1 = the lowest address of the program
};

const char* tape_format_name(int format);              // "mikos", "mikrobasic"
bool tape_format_parse(const std::string& s, int& format);

// .hex, .ihx (Intel HEX) and .bin are programs; anything else is audio.
bool tape_is_program(const std::string& path);
bool tape_is_hex(const std::string& path);

// The name on the tape: upper case, characters 20h-5Fh, at most 11 characters.
std::string tape_name_clean(const std::string& name);
std::string tape_default_name(const std::string& path);

// Builds the tape of a program held in `image` (64 KB) at lo..hi. `info` gets the
// range and the size on the tape.
bool tape_build(const uint8_t* image, uint32_t lo, uint32_t hi, const TapeProgram& p,
                std::vector<int16_t>& samples, int& rate, std::string& info, std::string& err);

// The same from a file (path as given, not relative to the data root).
bool tape_from_file(const std::string& path, const TapeProgram& p, std::vector<int16_t>& samples,
                    int& rate, std::string& info, std::string& err);
