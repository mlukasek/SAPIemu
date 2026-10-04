#pragma once
// Yamaha YM3812 (OPL2) on top of the Nuked-OPL3 core. The YMF262 in OPL2 mode sounds the
// same; what differs is done here: the waveform select enable bit (reg 01h bit 5, without
// it every operator plays a sine), the two timers and the status register.
// The chip runs on its own clock (3.579545 MHz on MPH-1V): one sample every 72 clocks,
// timer 1 counts in 80 us steps (288 clocks), timer 2 in 320 us steps.
#include <stdint.h>
#include "opl3.h"

class Ym3812 {
public:
    Ym3812();
    void reset(void);                       // /IC
    void write_addr(uint8_t a) { addr = a; }
    void write_data(uint8_t v);
    uint8_t status(void) const;             // D7 IRQ, D6 T1 flag, D5 T2 flag
    bool irq(void) const { return (st & 0x80) != 0; }
    // Advances the chip by one sample (72 clocks): the timers and one stereo sample.
    void sample(int16_t* lr);
    uint8_t reg(uint8_t r) const { return regs[r]; }

private:
    void timers_step(void);
    opl3_chip chip;
    uint8_t regs[256];
    uint8_t addr = 0;
    uint8_t st = 0;          // status D7..D5
    uint16_t t1_count = 0;   // 8-bit up counters, preset from regs 02h, 03h
    uint16_t t2_count = 0;
    int t2_div = 0;          // timer 2 runs at 1/4 of timer 1
    int t1_div = 0;          // timer 1 step = 4 samples (288 clocks)
};
