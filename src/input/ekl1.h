#pragma once
// EKL-1 - PS/2 keyboard converter (AT89C2051 firmware KEYB_REPAIR.a51).
// Emulated at the port level only (see docs/desky/klavesnice.md):
//   P1-IN (port 02h) = inverted character code
//   P0-IN0 (port 01h bit 0) = STROBE, active low
//   P0-OUT0 = ACK from the computer, P0-OUT1 = beeper (active low)
// Firmware OUTPUT routine: put data, STROBE=0, wait until ACK=1, STROBE=1.
#include <deque>
#include "core/devices.h"

class Ekl1 : public KeyboardDevice {
public:
    const char* type_name() const override { return "EKL-1"; }
    void reset() override;
    void advance(uint64_t now) override;
    uint8_t p0_in() override;
    uint8_t p1_in() override;
    void p0_out(uint8_t value) override;
    void push_code(uint8_t code) override { queue.push_back(code); }
    size_t pending() const override { return queue.size() + (strobe_low ? 1 : 0); }
    void clear() override { queue.clear(); }

    // configuration
    uint64_t gap_cycles = 2000;   // minimum time between characters (PS/2 + firmware)
    bool strict = false;          // true: like the real firmware (may lose characters
                                  // if the program leaves ACK high); false: wait for ACK=0
private:
    std::deque<uint8_t> queue;
    uint8_t data = 0;             // code currently presented
    bool strobe_low = false;
    bool ack = false;
    bool beep = false;
    uint64_t now = 0;
    uint64_t next_allowed = 0;
};
