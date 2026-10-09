#pragma once
// RMP-1 - magnetic tape controller of SAPI-1 for the IZOT SM 5300 tape drive (1 to 3 units), see
// docs/desky/RMP-1.md. Not an autonomous controller: the CPU times everything through two 8255 and an 8253.
//
// - C0H-C3H D1B 8255, mode 1: PA out = write data (WD0 = D7 ... WD7 = D0, the byte as it is), PB in = read
//   data (STB = the RDS of the drive while RE), PC4 WDP (parity bit written), PC5 WDSE (1 = WDS, 0 = WARS),
//   both set by bit set/reset words (08/09, 0A/0B).
// - C4H-C7H D2B 8255, mode 0 (8AH): PA commands (7 WE, 6 RE, 5 TE, 4 OFC, 3 RWC, 2 SWS, 1 SRC, 0 SFC), PB status
//   of the selected unit (6 PON, 5 RDY, 4 WPT, 3 BOT, 2 ONL, 1 RWS, 0 EOT), PC upper in (7 OUT2, 6 OUT0, 5 LPP,
//   4 PAE), PC lower out (3 RPON, 2-0 S2-S0).
// - C8H-CBH D2A 8253, all clocks PHI2 (2 MHz): G0 = TE, OUT0 -> PC6 and the write strobe (D7C: on its rising
//   edge with WE and a byte in PA a WDS or WARS goes to the drive, then /ACK); G1 = the data clock (D4C,
//   every RDS while RE); G2 = OUT1, OUT2 -> PC7. The driver RMP.MAC finds the gaps after blocks with them.
// - PON (D7A) is set by /RES and blocks the motion commands until RPON.
//
// Drive (model of the SM 5300; no manual found): 9 tracks 1/2", 800 bpi NRZI, 12.5 ips (100 us per row),
// rows = 8 data bits + the parity track. An all-zero row is no row at all (NRZI), that is how the gaps are
// written. The write head is 0.15" before the read head (read after write as on Pertec drives). The tape
// starts and stops within about 0.2" (33 ms), RMP.MAC counts on it after a backspace. WARS writes
// the LRC row (the write flip-flops back to zero). BOT and EOT are reflective markers, rewind about 50 ips.
// A tape keeps its rows with their positions; the image file is SIMH .tap (records, 0 = tape mark), the
// CRC and LRC rows are made again when a tape is loaded (ČSN 36 9464, the algorithm of RMP.MAC).
#include <string>
#include <vector>
#include "core/card.h"
#include "chips/i8253.h"
#include "chips/i8255.h"

// One reel of tape: the rows written on it, sorted by position.
struct MagTape {
    static const int64_t UNIT = 160000;           // position units per inch: one PHI2 clock at 12.5 ips
    static const int64_t ROW = UNIT / 800;        // 800 bpi: 200 units = 100 us
    static const int64_t EOT_POS = 13450 * UNIT;  // EOT marker from BOT (350 m reel, markers 3 and 4.3 m in)
    static const int64_t BOT_LEN = UNIT * 11 / 10; // the BOT marker: 28 mm
    static const int64_t END_POS = 13600 * UNIT;  // the physical end of the tape
    struct Row {
        int64_t pos;     // from the BOT marker, in UNIT
        uint16_t ch;     // bits 0-7 = RD7..RD0 as the byte, bit 8 = the parity track
    };
    std::string image;          // as configured
    std::string path;           // the file used (work copy)
    bool loaded = false;
    bool dirty = false;
    std::vector<Row> rows;

    bool load(const std::string& file, bool work_copy, std::string& err);   // SIMH .tap, created when missing
    bool save(std::string& err);
    void unload(void);
    void erase_from(int64_t pos);   // writing: everything from here on is gone
    // records as SIMH .tap would store them (data, or an empty vector for a tape mark); `bad` = a parity,
    // CRC or LRC error in the record
    struct Record {
        std::vector<uint8_t> data;
        bool mark;
        bool bad;
        int64_t pos;
    };
    std::vector<Record> records(void) const;
    // the rows of a record written by RMP.MAC: data with odd parity, the CRC 4 rows and the LRC 8 rows
    // after the last data row; a tape mark is 13H and 13H 8 rows later
    static void put_record(std::vector<Row>& rows, int64_t pos, const uint8_t* data, size_t len);
    static void put_mark(std::vector<Row>& rows, int64_t pos);
    static uint16_t crc(const uint8_t* data, size_t len);   // c2..c9 in bits 0-7, c1 in bit 8
};

class Rmp1 : public Card, public I8255Peripheral {
public:
    Rmp1() : Card("RMP-1") {}
    ~Rmp1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    void flush() override;
    void describe(nlohmann::json& out) override;
    int decode_io(uint8_t port, const char** what) override;
    std::string live_status() override;

    // I8255Peripheral (D2B)
    uint8_t pins_b() override;
    uint8_t pins_c() override;
    void outputs_changed(int port, uint8_t value) override;

    static const int UNITS = 3;
    struct Unit {
        bool fitted = false;
        MagTape tape;
        bool ring = true;       // write enable ring in the reel (without it WPT)
        bool online = false;
        bool loaded = false;    // threaded and at the load point (LOAD pressed): RDY
        bool rewinding = false;
        bool unload_after = false;   // UNLOAD pressed away from BOT: rewind, then unload
        int64_t pos = 0;        // the read head, from the BOT marker (MagTape::UNIT)
        int dir = 0;            // the last motion: 1 forward, -1 reverse, 0 stopped (for the reels)
        int32_t vel = 0;        // speed in 1/65536 of full speed (12.5 ips), + forward: ramps up and down
        int32_t acc = 0;        // the fraction of a position unit travelled
        bool writing = false;   // the write current is on (SWS while forward)
        uint16_t wff = 0;       // the write flip-flops: what WARS turns into the LRC row
        size_t next = 0;        // index of the first row after the read head
    };
    Unit units[UNITS];

    // Media window, MCP
    bool insert(int unit, const std::string& image, std::string& err);   // a new file = a blank tape
    void eject(int unit);
    void press_load(int unit);      // LOAD: thread, to BOT, ready
    void press_online(int unit);    // ON-LINE / OFF-LINE
    void press_rewind(int unit);    // REWIND (off-line)
    void press_unload(int unit);    // UNLOAD: rewind and take the reel off
    int selected(void) const;       // the unit S0-S2 select, -1 none
    // the BOT marker (28 mm of reflective foil, ECMA-62) stays under the sensor for the first 1.1" from the
    // load point: RMP.MAC starts the tape before it asks for BOT when it writes
    bool bot(const Unit& u) const { return u.loaded && u.pos < MagTape::BOT_LEN; }
    bool eot(const Unit& u) const { return u.loaded && u.pos >= MagTape::EOT_POS; }
    bool power_on_reset(void) const { return pon; }   // PON: until the driver's RPON

private:
    void sync(uint64_t now);        // the counters and the drives up to bus time `now`
    void step(void);                // one PHI2 clock
    void read_row(Unit& u, uint16_t ch);

    I8255 ppi_data;                 // D1B
    I8255 ppi_ctl;                  // D2B
    I8253 pit;                      // D2A
    bool pon = true;                // D7A
    uint64_t phi2 = 0;              // PHI2 clocks done
    uint64_t synced = 0;            // bus time of `phi2`
    double phi2_per_cycle = 1.0;    // PHI2 clocks per CPU cycle
    double frac = 0;
    bool out0_prev = true;
    int gate1_hold = 0;             // the D4C pulse on G1 (PHI2 clocks left)
    bool pae = false, lpp = false;
    bool re_prev = false;
    uint8_t cmd_prev = 0;           // PA of D2B before (edges of RWC, OFC)
    bool work_copy = true;
    bool ready_at_start = true;     // comfort: reel loaded and on-line at power on (the operator's job)
};
