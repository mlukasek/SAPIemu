#include "chips/ym3812.h"
#include <string.h>

Ym3812::Ym3812()
{
    reset();
}

void Ym3812::reset(void)
{
    OPL3_Reset(&chip, 49716);   // the native rate, no resampling
    memset(regs, 0, sizeof(regs));
    addr = 0;
    st = 0;
    t1_count = t2_count = 0;
    t1_div = t2_div = 0;
}

void Ym3812::write_data(uint8_t v)
{
    uint8_t a = addr;
    uint8_t old = regs[a];
    regs[a] = v;
    if (a == 0x04) {
        if (v & 0x80) {   // IRQ reset: clears the flags, the rest of the write is ignored
            st = 0;
            regs[a] = old & 0x7F;
            return;
        }
        if ((v & 0x01) && !(old & 0x01)) { t1_count = regs[0x02]; t1_div = 0; }
        if ((v & 0x02) && !(old & 0x02)) { t2_count = regs[0x03]; t2_div = 0; }
        return;
    }
    if (a == 0x01) {   // WSE changed: the waveforms take effect or fall back to sine
        if ((v ^ old) & 0x20)
            for (int r = 0xE0; r <= 0xF5; r++)
                OPL3_WriteReg(&chip, (uint16_t)r, (v & 0x20) ? regs[r] : 0);
        return;
    }
    if (a == 0x02 || a == 0x03) return;   // timer presets, used at the next start / overflow
    if (a >= 0xE0 && a <= 0xF5) {
        OPL3_WriteReg(&chip, a, (regs[0x01] & 0x20) ? v : 0);
        return;
    }
    if (a >= 0xC0 && a <= 0xC8) v &= 0x0F;   // no OPL3 output enables (both channels anyway)
    OPL3_WriteReg(&chip, a, v);
}

uint8_t Ym3812::status(void) const
{
    return (uint8_t)(st | 0x06);   // the low bits read as 110 on a YM3812
}

void Ym3812::timers_step(void)
{
    uint8_t ctl = regs[0x04];
    // timer 1: one step every 80 us (4 samples)
    if (++t1_div >= 4) {
        t1_div = 0;
        if (ctl & 0x01) {
            if (++t1_count >= 256) {
                t1_count = regs[0x02];
                if (!(ctl & 0x40)) st |= 0x80 | 0x40;
            }
        }
        // timer 2: one step every 320 us
        if (++t2_div >= 4) {
            t2_div = 0;
            if (ctl & 0x02) {
                if (++t2_count >= 256) {
                    t2_count = regs[0x03];
                    if (!(ctl & 0x20)) st |= 0x80 | 0x20;
                }
            }
        }
    }
}

void Ym3812::sample(int16_t* lr)
{
    timers_step();
    OPL3_Generate(&chip, lr);
}
