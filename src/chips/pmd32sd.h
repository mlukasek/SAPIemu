#pragma once
// PMD32-SD floppy drive emulator (Roman Bórik, firmware v3.2) emulated at the
// protocol level on the host side of an 8255 in mode 2 (see docs/desky/PMD32-SD.md).
// A host directory plays the role of the SD card (images.cfg in its root).
// The firmware main loop is reproduced as a resumable state machine running in
// emulated time, so its timeouts behave like on the real device.
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

struct I8255;

class Pmd32Sd {
public:
    static const int DRIVES = 4;

    Pmd32Sd();
    ~Pmd32Sd();

    // sd_root: host directory acting as the SD card
    bool init(const std::string& sd_root, std::string& err);
    void attach(I8255* p) { ppi = p; }
    void power_on(uint64_t now);
    void advance(uint64_t now, uint32_t cpu_hz);

    // Insert an image (path relative to the SD root, '/' separated). Returns a
    // RESULT_* code (0 = OK).
    int mount(int drive, const std::string& rel_path, bool wp);
    void eject(int drive);
    std::string drive_path(int drive) const { return drives[drive].path; }
    bool drive_wp(int drive) const { return drives[drive].wp; }
    bool drive_present(int drive) const { return drives[drive].f != nullptr; }
    int drive_tracks(int drive) const { return drives[drive].tracks ? drives[drive].tracks : 256; }
    int drive_sectors(int drive) const { return drives[drive].sectors; }
    void set_wp(int drive, bool wp) { drives[drive].wp = wp; }
    // image files in the SD root with an extension known to images.cfg
    std::vector<std::string> list_images(void) const;
    void flush(void);

    bool card_locked = false;      // SD card write lock switch
    uint32_t startup_ms = 0;       // the real firmware shows its banner for 3 s after power on
    const std::string& root(void) const { return sd_root; }

    enum {
        RESULT_OK = 0, RESULT_WP = 1, RESULT_FE = 2, RESULT_RE = 3, RESULT_WE = 4, RESULT_BD = 5,
        RESULT_NF = 6, RESULT_TL = 7, RESULT_CE = 8, RESULT_CM = 9, RESULT_CR = 10, RESULT_CC = 11,
        RESULT_UI = 12,
    };

private:
    struct Drive {
        FILE* f = nullptr;
        std::string path;          // relative to the SD root
        bool wp = false;
        uint8_t tracks = 0, sectors = 0, phys = 0;
    };

    // primitive operations the main sequence waits for
    enum OpKind { OP_READ, OP_READ_EX, OP_CRC_CHECK, OP_SEND, OP_DELAY };
    void start_op(OpKind kind, uint8_t value, uint32_t ms);
    bool op_step(void);            // true when the operation finished (op_ok tells the result)
    uint64_t ms_to_cycles(uint32_t ms) const { return (uint64_t)ms * cpu_hz / 1000; }

    void run(void);                // firmware main loop (resumable)
    int check_image_type(const std::string& name, uint8_t& trks, uint8_t& secs, uint8_t& fsec) const;
    std::string default_image(void) const;
    bool prepare_sector(void);
    bool write_sector(void);
    int create_image(const std::string& name);
    void build_listing(void);
    std::string host_path(const std::string& rel) const;

    I8255* ppi = nullptr;
    std::string sd_root;
    Drive drives[DRIVES];
    uint64_t now = 0;
    uint32_t cpu_hz = 2000000;

    // resumable state (no locals in run(): they would not survive a yield)
    int pt = 0;
    OpKind op_kind = OP_READ;
    int op_stage = 0;
    uint8_t op_value = 0;
    uint32_t op_ms = 0;
    uint64_t op_deadline = 0;
    bool op_ok = false;
    uint8_t op_byte = 0;

    uint8_t command = 0, crc = 0, val = 0;
    int op = 0;                    // firmware "op" (transfer kind)
    uint8_t drvnum = 0, sector = 0, track = 0;
    uint16_t length = 0, temp = 0, idx = 0;
    uint8_t buffer[640];
    std::string cur_dir;
    std::vector<std::string> listing;
    size_t listing_pos = 0;
};
