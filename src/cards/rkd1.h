#pragma once
// RKD-1 - controller of the cartridge disk CM 5403 (Zbrojovka Brno), boards RKD-1A + RKD-1B (two slots),
// see docs/desky/RKD-1.md.
//
// - 40H-43H 8253: counter 0 divides 2 MHz by 2 (1 us), counter 1 (mode 5) the delay from the end of the
//   sector pulse (write 3, read 20 us), counter 2 (mode 0) counts the bytes, OUT2 = DATA_CNT_OK.
// - 44H-47H 8255: PA track number T1-T128, PB status (input), PC control: PC0 drive the track number (and
//   clear T00), PC1-PC2 unit (74145 -> SEL0-SEL3), PC3 read, PC4 write, PC5 HLAVA (surface), PC6 DISK, PC7
//   PB4-PB7 = status (1) or ATN0-ATN3 (0).
// - PB: D0 BUSY, D1 write protected (WPD), D2 DATA_CNT_OK, D3 APT (track accepted), with PC7 = 1 D4 /SRDY,
//   D5 /STO, D6 /UNR, D7 /ERR (all active low as on the interface), with PC7 = 0 D4-D7 /ATN0-/ATN3.
// - 48H write: STB (500 ns) - the drive takes the track (PC0 = 1) or a restore (T00).
// - 49H write: RUN - read (PC3) or write (PC4) the sector whose number is in 4AH, data through the RAM.
// - 4AH write: D0-D4 sector number inverted (74LS174, compared with S1-S16), D7 = 1 clears the RAM pointer.
// - 4BH write: D0 -> T00 flip-flop (D7CA, cleared while PC0 = 1), D1 = 1 a WPP pulse (write protect the
//   selected disk).
// - 4CH: the 1 KB buffer RAM (2x 2114), the pointer goes on with every access. 4DH-4FH decoded, unused.
// Drive (model): 2400 rpm, hard sectored (12 or 24 sector pulses of 40 us per turn, a property of the
// platter), 204 tracks, two surfaces; one cartridge (removable) and one fixed platter (DISK); bit 400 ns
// (3.2 us per byte) with the data separator in the drive. A platter keeps the byte stream written into
// every sector slot (zeros, 80H, header, data, sum), so both formats and the errors of the driver work.
#include <string>
#include <vector>
#include "core/card.h"
#include "chips/i8253.h"
#include "chips/i8255.h"

// One platter: 2 surfaces x 204 tracks x 12 or 24 sector slots, each slot the bytes written into it.
struct RkdPlatter {
    static const int TRACKS = 204;
    static const int SURFACES = 2;
    std::string image;          // as configured
    std::string path;           // the file used (work copy)
    bool loaded = false;
    bool dirty = false;
    bool write_protect = false; // WPD: the switch of the drive or a WPP pulse
    int sectors = 24;           // sector pulses per turn: 12 (512 B, SMEP) or 24 (256 B, Tesla CP/M)
    std::vector<uint8_t> data;  // records of 2 + cap() bytes: the length (LE), the bytes

    int cap(void) const { return sectors == 12 ? 636 : 312; }   // bytes that fit between two pulses
    size_t record(int surface, int track, int sector) const
    {
        return ((size_t)(surface * TRACKS + track) * sectors + sector) * (size_t)(2 + cap());
    }
    bool create(const std::string& file, int nsectors, std::string& err);   // unformatted
    bool load(const std::string& file, bool work_copy, std::string& err);
    bool save(std::string& err);
    void unload(void);
};

class Rkd1 : public Card, public I8255Peripheral {
public:
    Rkd1() : Card("RKD-1") {}
    ~Rkd1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    void flush() override;
    void describe(nlohmann::json& out) override;
    int decode_io(uint8_t port, const char** what) override;
    std::string live_status() override;

    // I8255Peripheral
    uint8_t pins_b() override;
    void outputs_changed(int port, uint8_t value) override;

    static const int UNITS = 4;
    static const int CARTRIDGE = 0;   // DISK inactive (PC6 = 0): the removable cartridge
    static const int FIXED = 1;

    struct Unit {
        bool fitted = false;
        RkdPlatter disk[2];           // CARTRIDGE, FIXED
        int track = 0;                // where the heads are
        uint64_t seek_end = 0;        // bus time: the heads settled (SRDY, ATN)
        bool err = false;             // ERR of the drive, cleared by a restore
        bool apt = false;             // APT: the last track number was accepted
    };
    Unit units[UNITS];

    // Media window, MCP: insert a cartridge image into a unit (creates an unformatted one when the file
    // does not exist), take it out
    bool insert(int unit, const std::string& image, bool wp, std::string& err);
    void eject(int unit);
    int new_sectors = 24;             // format of a new cartridge (sector pulses per turn)

    // A sector as the driver RKD.MAC (Tesla Elstroj 1987) writes and reads it: 26 x 00H, 80H, the header
    // (bit 15 = 512 B, 14 = disk, 13 = surface, 12-5 track, 4-0 sector; high byte first), the data, the
    // 16-bit sum of the data (low byte first). For tools and tests (making a cartridge outside the emulated
    // CPU); `len` 256 or 512. read_sector returns false for a missing sector or a wrong header or sum.
    bool write_sector(int unit, int disk, int surface, int track, int sector, const uint8_t* data, int len);
    bool read_sector(int unit, int disk, int surface, int track, int sector, uint8_t* data, int len);

private:
    uint64_t us(double t) const;      // microseconds as bus time
    double now_us(void) const;
    int unit_no(void) const { return (ppi.out_c >> 1) & 3; }
    Unit& unit(void) { return units[unit_no()]; }
    RkdPlatter& platter(void) { return unit().disk[(ppi.out_c & 0x40) ? FIXED : CARTRIDGE]; }
    bool ready(const Unit& u) const;
    bool seeking(const Unit& u) const;
    void strobe(void);
    void start(void);
    void finish(void);
    void update(void);

    I8255 ppi;
    I8253 pit;
    uint8_t ram[1024];
    uint16_t ptr = 0;                 // 10 bit RAM address counter
    uint8_t sector_reg = 0;           // 74LS174 as written (inverted sector number)
    bool t00 = false;                 // D7CA
    bool cnt_ok = false;              // OUT2 of the 8253: the counted bytes went through
    // operation in progress (RUN)
    bool busy = false;
    bool op_write = false;
    uint64_t op_end = 0;
    bool op_ok = false;               // the sector was found (and had data when reading)
    int op_unit = 0, op_disk = 0, op_surface = 0, op_track = 0, op_sector = 0;
    uint32_t op_count = 0;
    size_t op_sync = 0;               // reading: index of the byte after 80H in the stored stream

    bool work_copy = true;
    double spinup_s = 0;              // drive start after a cartridge goes in
    uint64_t ready_at[UNITS] = { 0, 0, 0, 0 };
};
