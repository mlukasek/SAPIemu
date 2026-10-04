#pragma once
// Intel 8253 / 82C54 programmable interval timer: three 16-bit down counters, modes 0-5,
// binary or BCD, LSB/MSB access, counter latch command. The 82C54 (is_8254) adds the
// read-back command (control word with SC = 11) with the status byte: OUT, NULL COUNT and
// the programmed mode. The clocks come from the card: tick(n) is one falling edge of CLKn;
// set_gate(n) drives GATEn; out(n) is OUTn.
#include <stdint.h>

class I8253 {
public:
    I8253();
    void reset(void);                       // power on (the 8253 has no reset pin)
    uint8_t read(int counter);              // A1A0 = 0..2
    void write(int reg, uint8_t value);     // 0..2 count, 3 = control word
    void tick(int counter);                 // one CLK falling edge
    void set_gate(int counter, bool level);
    bool out(int counter) const { return c[counter].out; }
    bool is_8254 = false;

private:
    struct Counter {
        int mode = 0;            // 0..5
        int rw = 3;              // 1 LSB, 2 MSB, 3 LSB then MSB
        uint8_t ctl = 0x36;      // the control word as written (RW, M, BCD for the status byte)
        bool bcd = false;
        uint16_t reload = 0;     // the count written (0 = 65536 / 10000)
        uint32_t count = 0;      // counting element
        bool out = true;
        bool gate = true;
        bool load = false;       // the written count goes in at the next clock
        bool running = false;
        bool trigger = false;    // modes 1, 5: gate rising edge seen
        bool strobe = false;     // modes 4, 5: OUT low for one clock
        bool write_msb = false;  // rw 3: the next byte written is the MSB
        bool read_msb = false;   // rw 3: the next byte read is the MSB
        bool latched = false;
        uint16_t latch = 0;
        bool null_count = true;  // 82C54: a count written but not yet in the counting element
        bool status_latched = false;
        uint8_t status = 0;
        uint8_t lsb = 0;         // rw 3: the LSB written so far
    };
    uint32_t initial(const Counter& k) const;   // the reload value as a count
    uint16_t value(const Counter& k) const;     // current count as read
    void count_written(Counter& k);
    void latch_count(Counter& k);
    Counter c[3];
};
