#pragma once
// File helpers (UTF-8 paths on all platforms) and data root resolution.
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string>
#include <vector>

FILE* file_open(const std::string& utf8_path, const char* mode);
bool file_read_all(const std::string& utf8_path, std::vector<uint8_t>& out);
bool file_read_text(const std::string& utf8_path, std::string& out);
bool file_write_all(const std::string& utf8_path, const void* data, size_t size);
// The images of media and battery memories: written into a temporary file next to it, flushed to the disk and
// renamed over the old one, so a crash or a power cut never leaves a half written image (the old one or the new
// one). When the rename is refused (Windows: another program holds the file open) it is written in place.
bool file_write_safe(const std::string& utf8_path, const void* data, size_t size);
// Text file with the line ends of the platform (CRLF on Windows), like a git checkout.
bool file_write_text(const std::string& utf8_path, const std::string& text);
bool file_exists(const std::string& utf8_path);
bool dir_exists(const std::string& utf8_path);
// host wall clock for the RTCs of the cards; tests fix it (0 = the real clock again)
time_t host_time_now(void);
void host_time_fix(time_t t);
// size and modification time (file clock ticks) of a file
bool file_stat(const std::string& utf8_path, uint64_t& size, int64_t& mtime);
bool file_remove(const std::string& utf8_path);
// rename a file, over an existing `to` (false: e.g. another program holds one of them open on Windows)
bool file_rename(const std::string& utf8_from, const std::string& utf8_to);
bool make_dirs(const std::string& utf8_dir);
std::string path_join(const std::string& a, const std::string& b);
std::string path_dir(const std::string& p);

// The data root is the directory containing roms/ and machines/. It is looked up
// from the executable directory upwards, then from the build-time source dir.
void data_root_init(void);
void data_root_set(const std::string& dir);
const std::string& data_root(void);
// True when the data root has roms/ and machines/ (else the data were not found).
bool data_root_valid(void);
// The folder of the working copies, battery files and printer outputs: work/ in the data root, or
// SAPIemu/work in the user's local application data when the data root is not writable (the program
// unpacked into Program Files, a read only medium).
const std::string& work_root(void);
// Resolve a path from a config file: absolute paths are kept, relative ones are
// taken relative to the data root ("work/..." relative to work_root()).
std::string data_path(const std::string& rel);
// The other way round: a path inside the data root (or the work root, as "work/...") as relative
// with '/' separators (for machine files), other paths unchanged.
std::string data_relative(const std::string& path);
// Machine descriptions (*.sapi; older *.jsonc, *.json) under machines/ including subfolders,
// as data root relative paths, sorted.
std::vector<std::string> list_machine_files(void);
// Files of a folder with the suffix (".tap"; case ignored), as data root relative paths, sorted.
std::vector<std::string> list_files(const std::string& rel_dir, const std::string& ext);

// Path of the file to use for a writable media image. Images under media/ are
// pristine originals: when `work_copy` is set they are copied to work/ on first
// use and the copy is returned. Other paths are returned resolved.
bool media_path(const std::string& rel, bool work_copy, std::string& out, std::string& err);

// Battery backed memories (DMR-1 SRAM...) keep their content in files under work/, printers
// write their output there. Tests switch this off so that they never touch the user's files.
void battery_files_enable(bool on);
bool battery_files_enabled(void);

// Writes to media kept in files (a sector of a disk, a byte of a RAM disk, a block of a tape): media_written()
// counts them, media_writes() reads the count. EmuHost::autosave saves the media a few seconds after the count stops
// growing (a write is cheap: one increment).
void media_written(void);
uint64_t media_writes(void);

// Parse a number given as JSON-ish text: "0x1F", "1Fh", "$1F", "31".
bool parse_number(const std::string& s, long long& out);

// Expand C-like escapes used for typed text: \r \n \t \e \\ \xNN
std::string unescape_text(const std::string& s);
