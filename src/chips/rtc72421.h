#pragma once
// Epson RTC-72421 / 72423 real time clock (4-bit registers 0-F), as on ZRMD-1.
// The clock follows the host's local time plus an offset that changes when the
// program sets the time.
#include <stdint.h>

struct Rtc72421 {
    void reset(void);
    uint8_t read(int reg, uint64_t now_cycles, uint32_t cpu_hz);    // low nibble valid
    void write(int reg, uint8_t value, uint64_t now_cycles, uint32_t cpu_hz);
    // Periodic interrupt output (STD.P), level. Updates the IRQ flag.
    bool irq(uint64_t now_cycles, uint32_t cpu_hz);

private:
    void load_fields(void);      // emulated time -> digit registers
    void store_fields(void);     // digit registers -> offset

    int64_t offset = 0;          // emulated - host (seconds)
    uint8_t reg[16] = {0};
    bool hold = false, stop = false, mode24 = true;
    uint8_t cd = 0, ce = 0, cf = 0x04;   // control registers D, E, F
    bool irq_flag = false;
    uint64_t next_irq_cycle = 0;
    bool fields_dirty = false;
};
