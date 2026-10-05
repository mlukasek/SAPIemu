#include "chips/i8257.h"
#include "core/state.h"

void I8257::reset(void)
{
    mode = 0;
    status = 0;
    flip = false;
}

uint8_t I8257::read(int reg)
{
    reg &= 15;
    if (reg & 8) {   // status (the other register numbers with A3 = 1 are not decoded by the chip)
        uint8_t v = status;
        status &= 0x10;   // TC bits are cleared by reading, the update flag stays
        return v;
    }
    Channel& c = ch[reg >> 1];
    uint16_t v = (reg & 1) ? c.count : c.addr;
    uint8_t b = flip ? (uint8_t)(v >> 8) : (uint8_t)v;
    flip = !flip;
    return b;
}

void I8257::write(int reg, uint8_t value)
{
    reg &= 15;
    if (reg & 8) {
        mode = value;
        flip = false;
        return;
    }
    int n = reg >> 1;
    for (int pass = 0; pass < 2; pass++) {
        // in autoload mode a write to channel 2 also goes to channel 3 (the reload values)
        int k = pass == 0 ? n : 3;
        if (pass == 1 && !(n == 2 && (mode & 0x80))) break;
        uint16_t& r = (reg & 1) ? ch[k].count : ch[k].addr;
        if (!flip) r = (uint16_t)((r & 0xFF00) | value);
        else r = (uint16_t)((r & 0x00FF) | (value << 8));
    }
    flip = !flip;
}

bool I8257::transfer(int c, uint16_t& addr, bool& tc)
{
    if (!enabled(c)) return false;
    Channel& k = ch[c];
    addr = k.addr++;
    tc = (k.count & 0x3FFF) == 0;
    k.count = (uint16_t)((k.count & 0xC000) | ((k.count - 1) & 0x3FFF));
    if (tc) {
        status |= (uint8_t)(1 << c);
        if (c == 2 && (mode & 0x80)) {   // autoload: channel 2 starts again with channel 3's values
            ch[2] = ch[3];
            status |= 0x10;
        } else if (mode & 0x40) {
            mode &= (uint8_t)~(1 << c);   // TC stop
        }
    }
    return true;
}

void I8257::state(StateIO& io)
{
    for (Channel& c : ch) {
        io.v(c.addr);
        io.v(c.count);
    }
    io.v(mode);
    io.v(status);
    io.v(flip);
}
