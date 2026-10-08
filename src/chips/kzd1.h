#pragma once
// KZD-1 - cassette data recorder (Kazetopáskový záznamník dat, ZPA Košíře 1986), see docs/desky/KZD-1.md.
// On SAPI-1 it hangs on connector X2 of DPP-1 (8255 channel 1, MIKOS/K: 30h out, 31h in, 33h = 8Bh).
//
// - Inputs (port A, active low marked *): PA0 VPRED* (forward 48 cm/s), PA1 RYCHLE VPRED* (search
//   96 cm/s; with PA0 rewind 1.44 m/s), PA2 VZAD*, PA3 RYCHLE VZAD* (the same backwards), PA4 ZAZNAM*
//   (L = record / erase), PA5 D.P.* (its falling edge records PA7), PA6 sensitivity, PA7 DATA.
// - Outputs (port B): PB7 DATA 1, PB6 DATA 0 (a 40 us pulse for each flux transition on that track),
//   PB5 motor 1 overloaded, PB4 motor 2 overloaded (H; the only end of tape sign), PB3 ZAZN. U.V.*:
//   the write tab of the side switches ZAZNAM* back (L = recording allowed), without it H.
// - Two track NRZ: every bit is one flux transition, on track 0 or track 1; the read side gives the
//   pulses of both tracks at any speed and direction (monostables 74121, non-retriggerable).
// - The tape is a sorted list of transitions per side (position in micrometres, track). Recording
//   erases what the head passes and adds a transition at each falling edge of D.P.*.
// - The motors start and stop in 50 ms (100 ms for the search speed); at the end of the tape the
//   pulling motor stalls: overload after 30 ms (motor 2 forward, motor 1 backward).
// - The cassette file (.kzd): both sides, their write tabs. The hardware does not stop a write
//   without the tab: it only reports it (the software checks).
#include <stdint.h>
#include <string>
#include <vector>
#include "chips/i8255.h"

struct Bus;
class StateIO;
struct TapeProgram;

struct KzdCassette {
    static const int32_t LENGTH_UM = 86000000;   // 86 m a side (C 60)
    static const int32_t LEADER_UM = 400000;     // clear leader at both ends (no magnetic coating)
    std::vector<uint32_t> tr[2];   // per side: position << 1 | track, sorted
    bool tab[2] = { true, true };  // write tab of side A, B present (recording allowed)
    bool load(const std::string& path, std::string& err);
    bool save(const std::string& path, std::string& err) const;
};

// A cassette with a program (Intel HEX or binary file) as MIKOS/K writes it: side A prepared by K_I
// (tape name `volume`, empty = the program name), then one binary file (K_S) that .K_L loads and .J starts.
// `info` gets the name, range and blocks.
bool kzd_from_program(const std::string& path, const TapeProgram& p, const std::string& volume, KzdCassette& c,
                      std::string& info, std::string& err);
// A new file for such a cassette: work/kzd-1/NAME.kzd, NAME-2.kzd… when it exists.
std::string kzd_program_path(const std::string& name);

class Kzd1 : public I8255Peripheral {
public:
    Bus* bus = nullptr;
    I8255* ppi = nullptr;          // the channel it hangs on

    // the cassette
    bool insert(const std::string& path, std::string& err);   // creates an empty one if missing
    void insert(const KzdCassette& c, const std::string& path);   // a made one; written to path by flush()
    void eject(void);              // writes it back when changed
    void flush(void);
    bool loaded(void) const { return has_cassette; }
    const std::string& path(void) const { return file; }
    int side = 0;                  // 0 = A, 1 = B (the side facing the head)
    void flip(void);               // turn the cassette over (the position mirrors)
    void set_tab(int s, bool present);
    bool tab(int s) const { return cas.tab[s & 1]; }
    size_t transitions(void) const { return cas.tr[side].size() + rec_new.size(); }

    // I8255Peripheral
    uint8_t pins_b() override;
    void outputs_changed(int port, uint8_t value) override;
    void advance(uint64_t now);
    void reset(void) {}            // no reset input: it stops when port A goes high (8255 reset)

    // state for the window / MCP
    double position_m(void) const { return pos_um * 1e-6; }   // from the start of the side
    double speed(void) const { return v; }                     // m/s, + forward
    double target(void) const { return v_target; }
    bool recording(void) const { return !(pa & 0x10); }
    bool overload(int motor) const;                            // 1 or 2
    const char* motion(void) const;                            // "stop", "forward"... (English, for tr())
    uint8_t controls(void) const { return pa; }

    void state(StateIO& io);       // the mechanism; the cassette is in its file (saved first)

private:
    void controls_now(void);       // port A as the 8255 drives it now
    void set_controls(uint8_t value);
    void move(double t);           // the tape up to bus time t (seconds)
    void commit(void);             // the recorded part into the tape
    double now_s(void) const;

    KzdCassette cas;
    bool has_cassette = false;
    bool dirty = false;
    std::string file;
    uint8_t pa = 0xFF;             // control lines (port A pins)
    double t_last = 0;             // seconds of bus time moved to
    double pos_um = KzdCassette::LEADER_UM;
    double v = 0, v_target = 0;    // m/s
    double stall_since = -1;       // the tape is at its end while a motor pulls
    int stall_motor = 0;
    double pulse[2] = { -1, -1 };  // start of the read pulse of track 0, 1 (seconds)
    // recording in progress: the swept range and the new transitions (committed at the end)
    bool rec_active = false;
    double rec_lo = 0, rec_hi = 0;
    std::vector<uint32_t> rec_new;
};
