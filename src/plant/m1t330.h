#pragma once
// Digital voltmeter Metra Blansko M1T 330 (about 1982) with the IMS-2 interface, on the measuring bench
// of the DPS-1 (docs/desky/DPS-1.md). A DC voltmeter, dual slope converter integrating over half a mains
// period, ranges 300 mV / 3 V / 30 V / 300 V with 30000 counts, autorange, digital filter, autocalibration.
//
// Sources: the commands from P. Schauer (VUT Brno, "GPIB komunikace s pristroji M1T330, M1T380 a BM595"):
// R0-R3 range, R4 autorange, D1/D0 filter, E start, W0 / Wx delay x * 10 ms, C calibration, K0/K1
// autocalibration off/on, Q the error of the value; the reply from the BASIMS manual (sapi.cz):
// "V+1.2564E+1" for 12.564 V; the panel from photos (Anubis' blog, 2013). Guessed and marked in the
// document: CR LF with EOI after the value, the reply of Q, overload, times of the steps.
//
// IMS-2 functions here: AH1 (acceptor), SH1 (source), L and T with the address from the switches on the
// back, RL (REN + addressed = remote, the EXT lamp; GTL back to local), DCL / SDC (device clear), IFC.
#include <stdint.h>
#include <string>
#include "core/ims2.h"

class StateIO;

class M1t330 : public Ims2Node {
public:
    const char* node_name() const override { return "M1T 330"; }
    void lines_changed(uint64_t t) override;
    uint64_t next_event() const override;
    void on_event(uint64_t t) override;

    void power_on(uint64_t t);
    void power_off(void);
    void state(StateIO& io);

    // the front panel (the window)
    void key_auto(uint64_t t);
    void key_filter(uint64_t t);
    void key_rep(uint64_t t);
    void key_single(uint64_t t);
    void key_error(void);
    std::string display(void) const;   // "-10.151", the digits with the decimal point
    bool unit_mv(void) const { return power && !show_error && range == 0; }
    bool unit_v(void) const { return power && !show_error && range > 0; }
    bool unit_pct(void) const { return power && show_error; }

    // settings
    int address = 3;          // the switches on the back (0-30)
    // the input (H against L), set by the bench from what is connected
    double vin = 0;
    uint32_t seed = 12345;    // the noise of the converter (repeatable)

    // state shown on the panel
    bool power = true;
    bool autorange = true;    // AUTO lamp (R4)
    bool filter = false;      // FILTR lamp (D1)
    bool repeat = true;       // START REP lamp: measures again and again (local)
    bool remote = false;      // EXT lamp
    bool show_error = false;  // CHYBA: the display shows the error in %
    int range = 1;            // 0 = 300 mV, 1 = 3 V, 2 = 30 V, 3 = 300 V
    int32_t counts = 0;       // the last result in counts of the range
    bool overload = false;
    bool valid = false;       // a result since power on
    double value = 0;         // the last result in V
    uint64_t measured = 0;    // measurements since power on

private:
    // IMS-2 interface
    bool listener = false, talker = false;
    int ah = 0;                   // acceptor: 0 idle (not taking part), 1 ready, 2 accepted, 3 busy
    uint64_t ah_at = UINT64_MAX;  // its next step
    bool prev_dav = false, prev_ren = false, prev_ifc = false;
    std::string out;              // the reply waiting for the talker
    size_t out_pos = 0;
    int sh = 0;                   // source: 0 idle, 1 data on the lines (T1), 2 DAV, 3 waiting between bytes
    uint64_t sh_at = UINT64_MAX;
    void take_byte(uint8_t b, bool atn, bool eoi, uint64_t t);
    void command(uint8_t b, uint64_t t);
    void data_char(char c, bool eoi, uint64_t t);
    void flush_cmd(uint64_t t);
    void device_clear(void);
    void update_drives(void);

    // command parser
    char cmd = 0;
    int cmd_num = -1;

    // measurement
    int wait_10ms = 0;            // W
    bool autocal = true;          // K
    bool busy = false;            // a measurement or calibration runs
    bool calibrating = false;     // C runs
    bool single_pending = false;  // E or 1x: one measurement asked for
    bool remote_trigger = false;  // the result goes to the bus
    bool reply_error = false;     // Q: the next reply is the error
    uint64_t meas_at = UINT64_MAX;
    int cal_count = 0;
    void start(uint64_t t, bool from_bus);
    void finish(uint64_t t);
    int32_t convert(int r);
    double noise(void);
    std::string format_reply(void) const;
    double error_pct(void) const;
};
