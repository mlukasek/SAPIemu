#pragma once
// RTC-1 - real time clock, timers and watchdog of SAPI-1 (Tesla Liberec, 6XV 122 84), see docs/desky/RTC-1.md.
//
// - 16 I/O ports at A7..A4 = jumper (A7) + SA1..SA8 (A6..A4): +0..3 8255 DD2, +4..7 user 8253 DD5
//   (clocks and gates only on XC2), +8..11 system 8253 DD6, +12..15 write = load of the real time
//   counter. Control words and +12..15 read FFh (8286 drives the undriven internal bus).
// - Real time counter: 5 x 4029, binary up, +1 every 2 s (U114D, battery backed). Bits 0-7 = PA,
//   8-15 = PB, 16-18 = PC0-2, 19 = PC3 (jumper 9-11). Load: OUT +12 with PC6 = 1 jams the 8255
//   pin levels (output latches; input bits keep their value).
// - DD6: CLK2 = Phi2 of the bus, OUT2 = CLK0 = CLK1, gates high. OUT0 AND PC7 = /RTL (watchdog).
//   OUT1: jumpers 5-6, 9-11 (async, factory) /INT while OUT1 is high; 5-7, 6-8, 10-11 (realtime)
//   OUT1 = /STB A of the 8255 in mode 1, /INT = INTR A (PC3), bit 19 cleared by a load.
// - /INT goes only to XC2/14: a cable to INT0 or INT1 of the computer (parameter).
// - XC2 in APS-5 (parameter xc2): OUT2 of DD6 (XC2/29) clocks CLK0 and CLK1 of DD5, its OUT0 and OUT1
//   go to P2-IN5 and P2-IN4 of JPR-1 (HRK-1 reads them through user_out()).
#include <string>
#include "chips/i8253.h"
#include "chips/i8255.h"
#include "core/card.h"

class Rtc1 : public Card, public I8255Peripheral {
public:
    Rtc1() : Card("RTC-1") { ppi.dev = this; }

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int irq_lines() override;
    bool rtl_line() override;
    void flush() override;
    void describe(nlohmann::json& out) override;
    void state(StateIO& io) override;
    int decode_io(uint8_t port, const char** what) override;

    // I8255Peripheral: the counter outputs through 100k on the 8255 pins
    uint8_t pins_a() override { return (uint8_t)value(); }
    uint8_t pins_b() override { return (uint8_t)(value() >> 8); }
    uint8_t pins_c() override;

    bool user_out(int n);                // OUTn of the user 8253 DD5 now (XC2)

private:
    uint32_t value(void);                // the 20-bit real time counter now
    void load(uint32_t v);               // set the counter (the 2 s phase goes on)
    void clock_system(uint64_t now);     // DD6 from Phi2
    bool pc_out(int bit) const;          // level of an 8255 port C pin driven as an output (else pull-down)

    I8255 ppi;
    I8253 user, sys;
    uint8_t base = 0xA0;
    bool realtime = false;               // jumpers 5-7, 6-8, 10-11
    int int_line = 0;                    // IRQ_INT0 / IRQ_INT1 / 0: the cable from XC2/14
    bool pc_time = true;                 // the clock follows the PC (else the emulated time)
    bool battery = true;
    std::string file;
    int64_t offset = 0;                  // counter = offset + 2 s periods (of the PC or emulated time)
    uint64_t last = 0;                   // bus time of the last clock_system()
    double phase = 0;                    // Phi2 fraction
    bool out1_prev = true;
    bool xc2_aps5 = false;               // the cable of APS-5 on XC2
};
