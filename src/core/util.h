#pragma once
// File helpers (UTF-8 paths on all platforms) and data root resolution.
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

FILE* file_open(const std::string& utf8_path, const char* mode);
bool file_read_all(const std::string& utf8_path, std::vector<uint8_t>& out);
bool file_read_text(const std::string& utf8_path, std::string& out);
bool file_write_all(const std::string& utf8_path, const void* data, size_t size);
// Text file with the line ends of the platform (CRLF on Windows), like a git checkout.
bool file_write_text(const std::string& utf8_path, const std::string& text);
bool file_exists(const std::string& utf8_path);
bool file_remove(const std::string& utf8_path);
bool make_dirs(const std::string& utf8_dir);
std::string path_join(const std::string& a, const std::string& b);
std::string path_dir(const std::string& p);

// The data root is the directory containing roms/ and machines/. It is looked up
// from the executable directory upwards, then from the build-time source dir.
void data_root_init(void);
void data_root_set(const std::string& dir);
const std::string& data_root(void);
// Resolve a path from a config file: absolute paths are kept, relative ones are
// taken relative to the data root.
std::string data_path(const std::string& rel);
// The other way round: a path inside the data root as relative with '/'
// separators (for machine files), other paths unchanged.
std::string data_relative(const std::string& path);
// Machine descriptions (*.sapi; older *.jsonc, *.json) under machines/ including subfolders,
// as data root relative paths, sorted.
std::vector<std::string> list_machine_files(void);

// Path of the file to use for a writable media image. Images under media/ are
// pristine originals: when `work_copy` is set they are copied to work/ on first
// use and the copy is returned. Other paths are returned resolved.
bool media_path(const std::string& rel, bool work_copy, std::string& out, std::string& err);

// Battery backed memories (DMR-1 SRAM...) keep their content in files under work/.
// Tests switch this off so that they never touch the user's files.
void battery_files_enable(bool on);
bool battery_files_enabled(void);

// Parse a number given as JSON-ish text: "0x1F", "1Fh", "$1F", "31".
bool parse_number(const std::string& s, long long& out);

// Expand C-like escapes used for typed text: \r \n \t \e \\ \xNN
std::string unescape_text(const std::string& s);
