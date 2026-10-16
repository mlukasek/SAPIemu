#pragma once
// SD card of DPI-1L over a folder of the PC: what the DPI firmware does with FatFs on its microSD for the Repository
// API (SAPI_DPI-firmware common/repo_fs.c, config.c, ini.c, koi8.c), docs/desky/DPI-1L.md.
//
// The folder is the root of the card. Paths inside are FatFs paths ("/A/B", real names in UTF-8); the Z80 sends and
// gets names in KOI-8cs2. FatFs behaviour that programs can see is followed: names compared without regard to case,
// short names 8.3 (generated, an approximation), results FR_* in status byte 3, one open file with a page of 4 KB
// that reaches the disk only at a page change, SYNC, CLOSE, TRUNC, RESET, READDIR or OPEN, the size in the directory
// entry written at SYNC and CLOSE, /DPI/SECRET.INI hidden, the card size of a real card (GETFREE, card full).
#include <stdint.h>
#include <string>
#include <vector>

class StateIO;

// FatFs results (status byte 3)
enum DpiFr {
    DFR_OK = 0, DFR_DISK_ERR = 1, DFR_INT_ERR = 2, DFR_NOT_READY = 3, DFR_NO_FILE = 4, DFR_NO_PATH = 5,
    DFR_INVALID_NAME = 6, DFR_DENIED = 7, DFR_EXIST = 8, DFR_INVALID_OBJECT = 9, DFR_WRITE_PROTECTED = 10,
    DFR_NOT_ENABLED = 12, DFR_LOCKED = 16,
};

// Repository API error codes (status byte 2, as MZPico)
enum {
    REPO_ERR_NOT_IMPLEMENTED = 1, REPO_ERR_BAD_PARAM = 2, REPO_ERR_OVERFLOW = 3, REPO_ERR_NO_FILE = 4,
    REPO_ERR_BUSY = 5, REPO_ERR_FATFS = 6,
};

// KOI-8cs2 <-> UTF-8 for file names (common/koi8.c)
size_t koi8_to_utf8(const uint8_t* in, char* out, size_t size);
size_t utf8_to_koi8(const char* in, uint8_t* out, size_t size);
int koi8_ncasecmp(const uint8_t* a, const uint8_t* b, size_t n);
uint8_t koi8_plain(uint8_t c);
int koi8_nplaincmp(const uint8_t* a, const uint8_t* b, size_t n);

static const uint32_t DPI_PAGE = 4096;

// The open file (repo_file of the firmware)
struct DpiFile {
    bool open = false;
    uint8_t mode = 0;          // FA_READ 01h, FA_WRITE 02h ... as the Z80 gave it (20h ignored)
    uint32_t pos = 0;          // logical position
    uint32_t size = 0;         // logical size, including bytes not written back yet
    bool valid = false;        // page holds the file from page_pos on
    bool dirty = false;        // page has bytes not on the disk yet
    uint32_t page_pos = 0, page_len = 0;
    uint8_t page[DPI_PAGE];
    std::string path;          // FatFs path of the file
    uint32_t entry_size = 0;   // the size in its directory entry (FatFs writes it at SYNC and CLOSE)
};

// One command for the card (repo_req_t / repo_res_t of the firmware)
struct DpiSdResult {
    uint8_t err_code = 0;      // 0 = OK, else REPO_ERR_*
    uint8_t ff_res = 0;        // FatFs result for status byte 3
    uint16_t out_len = 0;      // output on the data port (DOUTRQ)
    uint16_t rec_len = 0;      // directory or configuration record streamed on the data port
    uint8_t data[256];
};

struct DpiEntry {
    std::string name;          // real name (UTF-8)
    std::string sfn;           // short name 8.3 (generated), "" = none
    bool dir = false;
    uint32_t size = 0;
    uint16_t fdate = 0, ftime = 0;
    uint8_t attr = 0;
};

class DpiSd {
public:
    // `host_root`: the folder of the card (UTF-8), created with /DPI and /DPI/DPI.INI if missing
    bool init(const std::string& host_root, uint32_t size_mb, std::string& err);
    void power_on(void);                       // nothing open, CWD = /, the page lost

    // Commands 00h (RESET), 20h-56h, 92h, 99h, B0h; F2h = the next directory record, F3h = the page with file.pos
    void execute(uint8_t cmd, uint8_t dir_cmd, const uint8_t* param, DpiSdResult& res);
    bool dir_active(void) const { return dir_open || cfg_stream; }   // status bit 2
    uint8_t flush_page(void);                  // write the page back if dirty (FR_*)
    void state(StateIO& io);
    const std::string& cwd_path(void) const { return cwd; }

    DpiFile file;
    std::string root;                          // host folder (UTF-8)

private:
    std::string host(const std::string& path) const;        // FatFs path -> host path (UTF-8)
    bool list(const std::string& path, std::vector<DpiEntry>& out);
    int walk(const std::string& path, DpiEntry* e, std::string* real);
    bool look_up(const std::string& dir, const std::string& utf8, const uint8_t* koi, std::string& real);
    bool resolve(const uint8_t* koi_path, std::string& out);
    uint8_t load_page(uint32_t pos);
    uint8_t close_file(void);
    void close_dir(void);
    void dir_next(uint8_t dir_cmd, DpiSdResult& res);
    void cfg_next(DpiSdResult& res);
    uint8_t do_open(uint8_t mode, const std::string& path);
    uint8_t do_unlink(const std::string& path);
    uint8_t do_rename(const std::string& from, const std::string& to);
    uint8_t do_mkdir(const std::string& path);
    uint8_t config_set(const char* section, const char* key, const char* value);
    uint64_t used_clusters(const std::string& path);
    uint32_t cluster_bytes(void) const;
    uint32_t total_clusters(void) const;
    void fatfs(DpiSdResult& res, uint8_t fr);
    void fix_entry_size(DpiEntry& e, const std::string& path);

    std::string cwd = "/";
    uint32_t size_mb = 32;
    bool ambiguous = false;                    // the last resolve() met several names without diacritics
    // the open directory (READDIR, FILELIST): the entries when it was opened, the next one
    bool dir_open = false, dir_is_dpi = false, dir_is_root = false;
    std::string dir_path;
    std::vector<DpiEntry> dir_list;
    uint32_t dir_index = 0;
    // GETCONFIG: records of the section follow
    bool cfg_stream = false;
    std::string cfg_section;
    uint32_t cfg_index = 0;
};
