#pragma once
// Keyboards with a parallel code output on connector X2 of the CPU card
// (see docs/desky/klavesnice.md):
//   P1-IN (port 02h)  = character code, inverted (active low lines)
//   P0-IN0 (port 01h) = strobe, active low
//   P0-OUT0           = acknowledge from the computer, P0-OUT1 = beeper (active low)
// Every SAPI driver reads them the same way: wait for P0-IN0 = 0, IN 02h + CPL,
// OUT 01h,03h, wait for P0-IN0 = 1, OUT 01h,02h.
//
// Two strobe behaviours:
//  - latched: the strobe stays low until the computer acknowledges (EKL-1,
//    Consul 259.11, Consul 262.3 with the 7474 modification)
//  - pulse:   a 1 ms strobe pulse, no acknowledge (original Consul 262.3)
#include <deque>
#include "core/devices.h"

class ParallelKeyboard : public KeyboardDevice {
public:
    void reset() override;
    void advance(uint64_t now) override;
    uint8_t p0_in() override;
    uint8_t p1_in() override;
    void p0_out(uint8_t value) override;
    size_t pending() const override { return queue.size() + (strobe_low ? 1 : 0); }
    void clear() override { queue.clear(); }

    void push_code(uint8_t code) { queue.push_back(code); }

    // configuration
    bool latched = true;            // strobe held until acknowledged
    uint64_t pulse_cycles = 2000;   // strobe pulse length when not latched (1 ms)
    uint64_t gap_cycles = 2000;     // minimum time between characters
    bool strict = false;            // true: never wait for ACK=0 before the next key
                                    // (like the hardware; keys may be lost)
    bool seven_bit = false;         // P1-IN7 is the /CLR key instead of data bit 7
    uint64_t hold_cycles = 200000;  // how long a "held" key (CLEAR) stays pressed

protected:
    void press_clear(void);         // Consul 259.11 CLEAR key

    std::deque<uint8_t> queue;
    uint8_t data = 0;               // code currently on the data lines
    bool strobe_low = false;
    uint64_t strobe_until = 0;
    bool ack = false;
    bool beep = false;
    bool clr = false;
    uint64_t clr_until = 0;
    uint64_t now = 0;
    uint64_t next_allowed = 0;
};

// Shared helpers
uint8_t koi8cs_keyboard_code(uint32_t cp, bool& ok);   // KOI-8 cs with swapped case for C0-FF
int wordstar_code(int key, int mods);                  // EKL-1 cursor keys, also for the terminal
