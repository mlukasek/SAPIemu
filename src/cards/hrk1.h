#pragma once
// HRK-1 - the front panel of APS-5 (Tesla Liberec, docs/desky/APS-5.md) on the connectors X2 and X3
// of JPR-1: the membrane keyboard HRK-1A, its electronics HRK-1B with the watchdog, the key switch,
// and the rest of the cables of the unit.
//
// - HRK-1B: MH 1KK1 encodes the 16 pads into Q1-Q4 (held until the next key), W2 starts a 74121
//   pulse of about 220 us on /INT. Codes as the firmware takes them: 0-9 digits, 0Bh R, 0Dh <-,
//   0Eh M, 0Fh ->. The pads R1-R3 (R in the middle) are connected only with the key in position 2.
// - Watchdog: 74123 DD5A retriggered by WD (P2-OUT7) rising edges, about 51 ms (R11 27k, C8 6u8);
//   while running the relay KA1 is energized (K0-K2) and the LED "porucha" dark. When it runs out
//   DD5B gives a 17 ms pulse on /RTL (R12 27k, C9 2u2): the whole unit restarts.
// - Cables (APS-5): X2: P0-IN = logic inputs LI 1a-8a, P1-IN = LI 1b-8b, P0-OUT = LO 11-81 (fault of
//   the channel), P1-OUT on X3 = LO 12-82. X3: P2-IN0-3 = Q1-Q4, P2-IN4 = OUT1 of DD5 on RTC-1 (1 Hz),
//   P2-IN5 = OUT0 of DD5 (50 Hz), P2-IN6 = /INT of HRK-1B, P2-IN7 = power failure from the supply
//   (L = the mains failed: request R7, RST 0; derived from the firmware, see the docs); P2-OUT7 = WD.
// - Power outage (the window, MCP): /PFAIL low for 100 ms, then the unit is without power (held in
//   reset, outputs off) for the given time, then it starts again. The batteries keep CRM-1 and
//   RTC-1, the firmware measures the outage on RTC-1.
#include <deque>
#include "core/devices.h"

class Hrk1 : public KeyboardDevice {
public:
    explicit Hrk1(uint32_t cpu_hz);
    const char* type_name() const override { return "HRK-1"; }
    void reset() override {}   // no reset input: the watchdog and Q1-Q4 keep their state
    void advance(uint64_t now) override;
    uint8_t p0_in() override;
    uint8_t p1_in() override;
    void p0_out(uint8_t value) override;
    uint8_t p2_in() override;
    void p1_out(uint8_t value) override;
    void p2_out(uint8_t value) override;
    bool rtl() override;
    // switched off for good (Machine::power_down): the outage lasts until the machine goes. EmuHost switches the old
    // machine off only after the new one has built as a trial, so a failed switch does not leave it without power.
    double power_fail_warning() const override { return 0.1; }
    void power_fail() override { outage(1e9); }
    void type_char(uint32_t cp) override;
    bool host_key(int key, int mods) override;
    size_t pending() const override { return queue.size(); }
    void clear() override { queue.clear(); }
    std::string status() const override;
    void state(StateIO& io) override;

    // the window
    enum { KEY_R = 0x0B, KEY_BACK = 0x0D, KEY_M = 0x0E, KEY_ENTER = 0x0F };
    void press(uint8_t code);       // a key of the panel (queued)
    int lock = 2;                   // the key switch: 1 = operator, 2 = programming
    bool relay(void);               // KA1 energized (the watchdog runs): LED "porucha" dark
    bool led_fault(void) { return !relay(); }
    uint8_t lo[2] = { 0, 0 };       // P0-OUT, P1-OUT
    uint8_t code = 0;               // Q1-Q4
    // a power outage of `seconds` from now (the emulated time); 0 = power fail warning only
    void outage(double seconds);
    bool powered(void);             // false during the outage (after the warning)
    double outage_left(void);       // seconds of the outage still to go (0 = none)

private:
    uint64_t now(void) const;
    uint32_t hz;
    uint64_t press_cycles, gap_cycles, int_delay, int_len, wd_cycles, rtl_cycles;
    std::deque<uint8_t> queue;
    uint64_t next_free = 0;         // bus time the next key may go down
    uint64_t int_at = 0;            // start of the /INT pulse (0 = none)
    bool wd_armed = false;          // DD5A was triggered at least once since power on
    uint64_t wd_end = 0;            // bus time DD5A runs out
    uint8_t p2_prev = 0;
    uint64_t pfail_at = 0, power_back = 0;   // bus times of the outage (0 = none)
};
