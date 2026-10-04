#pragma once
// MPH-1V rev 1.0 - sound and joystick card (Libor Lasota), see docs/desky/MPH-1V.md.
//
// From the schematic rev 1.0 of 2025-09-06 (..\MPH-1V\Eagle, sheets 1-4) and the parts list;
// MPH-test 0.99, SPLAY, RAT PLAYER and y1stdemo use the same bits.
// - 8 ports from the base set by S1 (74LS688 on A3-A7):
//   +0..+2 8253 counters, +3 read STATUS / write the 8253 control word,
//   +4 read JOY (K4 OVL) / write IEN, +5 read MOUSE (K2) / write IACK,
//   +6 YM3812 status / address, +7 YM3812 data.
// - 82C54 (IC6) CLK0 = CLK2 = 55930.4 Hz (14.31818 MHz / 256), CLK1 = OUT0 (cascade);
//   YM3812 at 3.579545 MHz (49716 samples/s).
// - STATUS: D7 F2, D6 F1, D5 OUT2, D4 OUT1, D3 OUT0, D2 D1 = 0, D0 YM IRQ.
//   F2 / F1: latches set by the rising edge of OUT2 / OUT1 (74LS74).
// - IEN (74LS174, cleared by /RES): D7 F2, D6 F1 interrupt enable, D5 GATE2,
//   D4 GATE1, D3 GATE0, D0 YM interrupt enable.
// - IACK: D7 / D6 = 1 clears F2 / F1 (74LS175, held until the next STSTB).
// - /INT0 = OR of (request AND enable), 74LS38 open collector.
// - JOY, MOUSE: D5..D0 = FR FL left right up down, active low (4k7 pull-ups),
//   D7 D6 read 1. K4 and K2 are the same Atari DB9 (D0 pin 2, D1 pin 1, D2 pin 4, D3 pin 3,
//   D4 pin 6, D5 pin 9); what is plugged in is a parameter (k4, k2): nothing, a joystick,
//   an Atari ST mouse (D0 XA, D1 XB, D2 YB, D3 YA) or an Amiga mouse (D0 H, D1 V, D2 HQ,
//   D3 VQ). Mouse buttons: left = pin 6, right = pin 9.
// - Sound: YM3014B DAC, Sallen-Key low pass (2k2, 2k2, 6n8, 3n3: 15.3 kHz,
//   Q 0.72), C8 10u + R3 47k high pass; mono on both channels of K3.
#include "chips/i8253.h"
#include "chips/ym3812.h"
#include "core/card.h"

class Mph1v : public Card {
public:
    Mph1v() : Card("MPH-1V") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int irq_lines() override;
    int decode_io(uint8_t port, const char** what) override;
    void describe(nlohmann::json& out) override;
    bool live_param(const char* path) const override;
    bool set_live_param(const char* path, const nlohmann::json& v, std::string& err) override;

    // what is plugged into K4 / K2
    enum Device { DEV_NONE = 0, DEV_JOYSTICK, DEV_MOUSE_ATARI, DEV_MOUSE_AMIGA };
    static const int MOUSE_RATE = 1000;   // quadrature steps per second and axis at most

    static const uint32_t MASTER_HZ = 14318182;   // 14.31818 MHz
    static const uint32_t SAMPLE_HZ = 49716;      // MASTER_HZ / 288

private:
    struct Outs { bool o0, o1, o2; };
    Outs outs(void) const { return Outs{ pit.out(0), pit.out(1), pit.out(2) }; }
    void pit_clock(void);
    void pit_edges(Outs before);   // after something changed the 8253 outputs
    void set_gates(void);
    int16_t filter(int16_t s);
    uint8_t connector(int k);   // what a read of +4 (k = 0, K4) or +5 (k = 1, K2) sees
    void mouse_step(void);
    void update_mouse_flag(void);

    uint8_t base = 0x50;
    I8253 pit;
    Ym3812 ym;
    uint8_t ien = 0;              // IEN register
    bool f1 = false;              // F1 latch (OUT1)
    bool f2 = false;              // F2 latch (OUT2)
    // time
    uint64_t last = 0;            // bus time of the last advance
    uint64_t master_acc = 0;      // master clock remainder * cpu_hz
    uint32_t pit_phase = 0;       // master clocks into the 8253 clock period (256)
    uint32_t ym_phase = 0;        // master clocks into the sample period (288)
    // connectors and the mouse
    int dev[2] = { DEV_JOYSTICK, DEV_MOUSE_ATARI };   // K4, K2
    uint32_t mouse_phase = 0;     // master clocks into the step period
    uint8_t mx = 0, my = 0;       // quadrature phase 0-3 of the X and Y wheels
    // output filter state
    float lp_x1 = 0, lp_x2 = 0, lp_y1 = 0, lp_y2 = 0;
    float hp_x1 = 0, hp_y1 = 0;
};
