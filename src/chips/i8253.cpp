#include "chips/i8253.h"
#include "core/state.h"

I8253::I8253()
{
    reset();
}

void I8253::reset(void)
{
    for (int i = 0; i < 3; i++) {
        bool gate = c[i].gate;
        c[i] = Counter();
        c[i].gate = gate;
    }
}

uint32_t I8253::initial(const Counter& k) const
{
    if (k.bcd) {   // the count is written as four BCD digits (0000 = 10000)
        uint32_t r = k.reload;
        uint32_t d = ((r >> 12) & 15) * 1000 + ((r >> 8) & 15) * 100 + ((r >> 4) & 15) * 10 + (r & 15);
        return d ? d : 10000u;
    }
    if (k.reload) return k.reload;
    return 65536u;
}

uint16_t I8253::value(const Counter& k) const
{
    uint32_t v = k.count;
    if (k.bcd) {   // the counting element holds BCD digits
        uint32_t d = v % 10000;
        return (uint16_t)((d / 1000) << 12 | ((d / 100) % 10) << 8 | ((d / 10) % 10) << 4 | (d % 10));
    }
    return (uint16_t)v;   // 65536 reads as 0
}

uint8_t I8253::read(int n)
{
    Counter& k = c[n];
    if (k.status_latched) {   // 82C54 read-back: the status byte comes first
        k.status_latched = false;
        return k.status;
    }
    uint16_t v = k.latched ? k.latch : value(k);
    uint8_t b;
    if (k.rw == 1) b = (uint8_t)v;
    else if (k.rw == 2) b = (uint8_t)(v >> 8);
    else {
        b = k.read_msb ? (uint8_t)(v >> 8) : (uint8_t)v;
        k.read_msb = !k.read_msb;
        if (!k.read_msb) k.latched = false;   // both bytes of the latch read
        return b;
    }
    k.latched = false;
    return b;
}

void I8253::count_written(Counter& k)
{
    switch (k.mode) {
    case 0:
        k.out = false;
        k.load = true;   // counting (re)starts with the next clock
        k.running = false;
        break;
    case 1:
    case 5:
        break;           // waits for a gate trigger, the new count is used then
    case 2:
    case 3:
        if (!k.running) k.load = true;   // a running counter takes it at the next reload
        break;
    case 4:
        k.load = true;
        k.running = false;
        break;
    }
}

void I8253::latch_count(Counter& k)
{
    if (k.latched) return;   // a second latch before the first is read is ignored
    k.latch = value(k);
    k.latched = true;
}

void I8253::write(int reg, uint8_t v)
{
    if (reg == 3) {   // control word: SC1 SC0 RW1 RW0 M2 M1 M0 BCD
        int n = v >> 6;
        if (n == 3) {
            // 82C54 read-back: D5 = 0 latches the counts, D4 = 0 the status, D3..D1 = counters 2..0
            if (!is_8254) return;   // illegal on the 8253
            for (int i = 0; i < 3; i++) {
                if (!(v & (2 << i))) continue;
                Counter& r = c[i];
                if (!(v & 0x20)) latch_count(r);
                if (!(v & 0x10) && !r.status_latched) {
                    r.status = (uint8_t)((r.out ? 0x80 : 0) | (r.null_count ? 0x40 : 0) | (r.ctl & 0x3F));
                    r.status_latched = true;
                }
            }
            return;
        }
        Counter& k = c[n];
        int rw = (v >> 4) & 3;
        if (rw == 0) {   // counter latch command
            latch_count(k);
            return;
        }
        k.ctl = v;
        k.null_count = true;
        k.status_latched = false;
        k.rw = rw;
        k.mode = (v >> 1) & 7;
        if (k.mode > 5) k.mode -= 4;   // 6, 7 = modes 2, 3
        k.bcd = (v & 1) != 0;
        k.out = k.mode != 0;           // mode 0: OUT low, the others high
        k.running = false;
        k.load = false;
        k.trigger = false;
        k.strobe = false;
        k.write_msb = false;
        k.read_msb = false;
        k.latched = false;
        return;
    }
    Counter& k = c[reg];
    if (k.rw == 1) {
        k.reload = v;
    } else if (k.rw == 2) {
        k.reload = (uint16_t)(v << 8);
    } else if (!k.write_msb) {
        k.lsb = v;
        k.write_msb = true;
        if (k.mode == 0) { k.out = false; k.running = false; }   // writing the LSB stops mode 0
        return;
    } else {
        k.reload = (uint16_t)(k.lsb | v << 8);
        k.write_msb = false;
    }
    k.null_count = true;
    count_written(k);
}

void I8253::set_gate(int n, bool level)
{
    Counter& k = c[n];
    bool rising = level && !k.gate;
    k.gate = level;
    if (rising) {
        if (k.mode == 1 || k.mode == 5) k.trigger = true;
        if (k.mode == 2 || k.mode == 3) k.load = true;   // a rising gate restarts the period
    }
    if (!level && (k.mode == 2 || k.mode == 3)) k.out = true;
}

static uint32_t count_down(uint32_t v, bool bcd)
{
    if (v == 0) return bcd ? 9999u : 65535u;
    return v - 1;
}

void I8253::tick(int n)
{
    Counter& k = c[n];
    if (k.strobe) {   // modes 4, 5: the strobe lasts one clock
        k.strobe = false;
        k.out = true;
    }
    if (k.mode == 1 || k.mode == 5) {
        if (k.trigger) {   // the clock after the gate trigger loads the count
            k.trigger = false;
            k.null_count = false;
            k.count = initial(k);
            k.running = true;
            if (k.mode == 1) k.out = false;
            return;
        }
        if (!k.running) return;
        k.count = count_down(k.count, k.bcd);
        if (k.count == 0) {
            k.running = false;
            if (k.mode == 1) k.out = true;
            else { k.out = false; k.strobe = true; }
        }
        return;
    }
    if (k.load) {
        k.load = false;
        k.null_count = false;
        k.count = initial(k);
        k.running = true;
        if (k.mode == 3) k.count &= ~1u;   // counts by two
        return;
    }
    if (!k.running) return;
    switch (k.mode) {
    case 0:
        if (!k.gate) return;
        k.count = count_down(k.count, k.bcd);
        if (k.count == 0) k.out = true;   // stays high, the counter wraps on
        break;
    case 2:
        if (!k.gate) return;
        k.count = count_down(k.count, k.bcd);
        if (k.count == 1) k.out = false;          // low for one clock before the reload
        else if (k.count == 0) { k.out = true; k.count = initial(k); k.null_count = false; }
        break;
    case 3: {
        if (!k.gate) return;
        k.count = k.count >= 2 ? k.count - 2 : 0;
        if (k.count == 0) {
            k.out = !k.out;
            uint32_t r = initial(k);
            k.null_count = false;
            // an odd count: one more clock in the high half, one less in the low half
            k.count = (r & 1) ? (k.out ? r + 1 : r - 1) : r;
        }
        break;
    }
    case 4:
        if (!k.gate) return;
        k.count = count_down(k.count, k.bcd);
        if (k.count == 0) {
            k.running = false;
            k.out = false;
            k.strobe = true;
        }
        break;
    }
}

uint32_t I8253::ticks(int n, uint64_t clocks)
{
    Counter& k = c[n];
    uint32_t falls = 0;
    while (clocks) {
        bool fast = k.running && k.gate && !k.load && !k.strobe && (k.mode == 2 || k.mode == 3) && k.count >= 2;
        if (fast) {
            // clocks until the next OUT change: mode 3 counts by two to 0, mode 2 goes low at 1
            uint64_t e = k.mode == 3 ? k.count / 2 : k.count - 1;
            if (clocks < e) {
                k.count -= (uint32_t)(k.mode == 3 ? 2 * clocks : clocks);
                return falls;
            }
            k.count -= (uint32_t)(k.mode == 3 ? 2 * (e - 1) : e - 1);
            clocks -= e - 1;
        }
        bool o = k.out;
        tick(n);
        if (o && !k.out) falls++;
        clocks--;
    }
    return falls;
}

void I8253::state(StateIO& io)
{
    for (Counter& k : c) {
        io.e(k.mode);
        io.e(k.rw);
        io.v(k.ctl);
        io.v(k.bcd);
        io.v(k.reload);
        io.v(k.count);
        io.v(k.out);
        io.v(k.gate);
        io.v(k.load);
        io.v(k.running);
        io.v(k.trigger);
        io.v(k.strobe);
        io.v(k.write_msb);
        io.v(k.read_msb);
        io.v(k.latched);
        io.v(k.latch);
        io.v(k.null_count);
        io.v(k.status_latched);
        io.v(k.status);
        io.v(k.lsb);
    }
}
