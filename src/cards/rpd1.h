#pragma once
// RPD-1 - floppy disk controller of SAPI-1, boards RPD-1A + RPD-1B (two slots),
// see docs/desky/RPD-1.md.
//
// No FDC chip: the software sees the FM byte stream of the disk through a data
// register and a small state machine; CRC and sector handling are software.
// - RPD-1A 20H-23H: 8253 timer, all data lines reversed (D0<->D7...).
//     counter 0: byte clock (32 us), time-out of an operation (TOUT)
//     counter 1: 0.5 us clock, started by SYC (address mark found): write gate
//       delay after the ID, data mark search window when reading (TOUT)
//     counter 2: counts the CPU transfers through 24H; after N+1 of them OVFL
//       ends the transfer (control bits D3-D0 cleared)
// - 24H: data register (read: byte from the disk, write: byte to the disk).
// - 25H read: D7-D5 = control D7-D5 (jump table for PCHL), D4 OVFL, D3 TOUT,
//   D2 /DR (0 = byte ready / byte wanted), D1-D0 = 0.
// - 25H write: D7-D5 table pointer, D3 RAM (find ID mark FE), D2 RDM (find data
//   mark FB/F8), D1 WE (write), D0 RE (read).
// - RPD-1B 3CH write: D5 INTE, D3 T43, D2 DIR (1 = to higher tracks), D1 FR
//   (fault reset), D0 SEL (drive 0/1). 3DH write: head load (unloads after 8
//   revolutions without another 3DH). 3EH write: one step. 3FH: test interrupt.
// - 3CH read: D7 HLR, D6 STR, D5 T00, D4 INDX, D3 WP, D2 RDY, D1 WF, D0 INR.
// Disks: 8" IBM 3740, single sided FM, 77 tracks x 26 sectors x 128 B, images
// in logical order (.img, 256256 bytes); the track with its gaps, marks and
// CRCs is generated from the image.
#include <vector>
#include "core/card.h"

class Rpd1 : public Card {
public:
    Rpd1() : Card("RPD-1") {}
    ~Rpd1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    void flush() override;
    void describe(nlohmann::json& out) override;
    int decode_io(uint8_t port, const char** what) override;

    static const int TRACKS = 77;
    static const int SECTORS = 26;
    static const int SECTOR_SIZE = 128;
    static const int IMAGE_SIZE = TRACKS * SECTORS * SECTOR_SIZE;
    static const int TRACK_LEN = 5208;   // bytes per revolution (166.7 ms at 32 us)
    static const int DRIVES = 2;

    struct Drive {
        std::string image;            // as configured
        std::string path;             // file in use (working copy)
        std::vector<uint8_t> data;    // IMAGE_SIZE when a disk is inserted
        bool write_protect = false;
        bool dirty = false;
        int track = 0;                // head position
    };
    Drive drives[DRIVES];

    bool insert(int drive, const std::string& image, bool wp, std::string& err);
    void eject(int drive);
    bool save(int drive, std::string& err);

private:
    enum Mark { M_NONE, M_ID, M_DATA };
    struct Counter {
        int mode = 0;
        int rl = 3;                   // 1 LSB, 2 MSB, 3 LSB then MSB
        uint16_t value = 0;
        bool msb_next = false;        // LSB then MSB (the driver uses only that)
        uint8_t lsb = 0;
    };

    uint64_t byte_cycles(void) const;
    uint64_t byte_now(void) const;
    Drive& cur(void) { return drives[sel]; }
    void build_track(void);
    void control(uint8_t v);
    void counter_loaded(int n);
    void write_update(void);
    void write_commit(void);
    bool tout(void);
    void transfer(void);

    // drive interface (RPD-1B)
    int sel = 0;
    uint8_t drive_ctrl = 0;
    bool head_loaded = false;
    uint64_t head_ready_at = 0, head_unload_at = 0;
    uint64_t step_ready_at = 0;
    bool wf = false;
    int step_ms = 8;                  // Consul 7213: 8 ms, 7212: 12 ms
    int settle_ms = 35;

    // generated track of the selected drive
    uint8_t raw[TRACK_LEN];
    uint8_t mark[TRACK_LEN];
    uint8_t sector_at[TRACK_LEN];     // sector index (0-25) of an ID mark position
    int raw_drive = -1, raw_track = -1;
    bool raw_valid = false;

    // controller (RPD-1A)
    uint8_t ctrl = 0;
    Counter cnt[3];
    uint16_t n0 = 0, n1 = 0, n2 = 0;
    bool cnt0_armed = false;
    uint64_t cnt0_expire = 0;
    bool cnt1_armed = false;
    uint64_t cnt1_fire = 0;
    unsigned transfers = 0;
    bool ovfl = false;
    bool tout_dam = false;
    // reading: the stream of bytes from the found mark on
    bool streaming = false;
    uint64_t stream_b = 0;            // absolute byte number of the mark
    uint64_t consumed_b = 0;          // last byte read by the CPU
    bool search_dam = false;          // RDM: waiting for the data mark
    int sync_sector = -1;             // sector of the last ID mark (SYC)
    // writing
    bool writing = false;
    bool holding_full = false;
    uint8_t holding = 0;
    uint64_t shifted_b = 0;           // last byte boundary handled
    bool shift_started = false;
    std::vector<uint8_t> written;
    int write_sector = -1, write_drive = 0, write_track = 0;

    bool work_copy = true;
};
