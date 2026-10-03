#include "chips/i8255.h"

// Control word (bit 7 = 1): D6-D5 group A mode (00 = 0, 01 = 1, 1x = 2),
// D4 PA input, D3 PC upper input, D2 group B mode, D1 PB input, D0 PC lower input.
// Bit set/reset (bit 7 = 0): D3-D1 bit number, D0 value.

void I8255::reset(void)
{
    write(3, 0x9B);
}

bool I8255::intr_a(void) const
{
    // level: output buffer empty with INTE1, or input buffer full with INTE2
    return (inte1 && !obf_a) || (inte2 && ibf_a);
}

uint8_t I8255::pc_value(void)
{
    uint8_t pins = dev ? dev->pins_c() : 0xFF;
    uint8_t v = 0;
    if (mode2_a()) {
        // PC7 /OBF, PC6 INTE1, PC5 IBF, PC4 INTE2, PC3 INTRA
        v |= obf_a ? 0 : 0x80;
        v |= inte1 ? 0x40 : 0;
        v |= ibf_a ? 0x20 : 0;
        v |= inte2 ? 0x10 : 0;
        v |= intr_a() ? 0x08 : 0;
    } else {
        // mode 0: upper half (PC4-7) per D3, lower half (PC0-3) per D0
        v |= (ctrl & 0x08) ? (pins & 0xF0) : (out_c & 0xF0);
        v |= (ctrl & 0x01) ? (pins & 0x08) : (out_c & 0x08);
    }
    if (ctrl & 0x04) {
        // group B mode 1, port B output: PC2 INTE_B, PC1 /OBF_B, PC0 INTR_B
        v |= inte_b ? 0x04 : 0;
        v |= obf_b ? 0 : 0x02;
        v |= (inte_b && !obf_b) ? 0x01 : 0;
    } else {
        uint8_t lower = (ctrl & 0x01) ? (pins & 0x07) : (out_c & 0x07);
        v |= lower;
    }
    return v;
}

uint8_t I8255::read(int reg)
{
    switch (reg & 3) {
    case 0:
        if (mode2_a()) {
            ibf_a = false;   // RD clears IBF
            return in_a;
        }
        if (ctrl & 0x10) return dev ? dev->pins_a() : 0xFF;
        return out_a;
    case 1:
        if (ctrl & 0x02) return dev ? dev->pins_b() : 0xFF;
        return out_b;
    case 2:
        return pc_value();
    default:
        return 0xFF;
    }
}

void I8255::set_pc_bit(int bit, bool v)
{
    if (mode2_a()) {
        if (bit == 6) { inte1 = v; return; }
        if (bit == 4) { inte2 = v; return; }
        if (bit >= 3) return;          // PC3-PC7 are control lines in mode 2
    }
    if ((ctrl & 0x04) && bit <= 2) {
        if (bit == 2) inte_b = v;
        return;
    }
    uint8_t old = out_c;
    if (v) out_c |= (uint8_t)(1 << bit); else out_c &= (uint8_t)~(1 << bit);
    if (dev && old != out_c) dev->outputs_changed(2, out_c);
}

void I8255::write(int reg, uint8_t value)
{
    switch (reg & 3) {
    case 0:
        out_a = value;
        if (mode2_a()) obf_a = true;   // /OBF low until the peripheral acknowledges
        else if (dev && !(ctrl & 0x10)) dev->outputs_changed(0, out_a);
        break;
    case 1:
        out_b = value;
        if (ctrl & 0x04) obf_b = true;
        else if (dev && !(ctrl & 0x02)) dev->outputs_changed(1, out_b);
        break;
    case 2: {
        // direct write to port C changes only bits configured as outputs
        uint8_t old = out_c;
        out_c = value;
        if (dev && old != out_c) dev->outputs_changed(2, out_c);
        break;
    }
    default:
        if (value & 0x80) {
            // mode set: all output latches and status flip-flops are reset
            ctrl = value;
            out_a = out_b = out_c = 0;
            obf_a = ibf_a = false;
            inte1 = inte2 = false;
            obf_b = inte_b = false;
            if (dev) dev->outputs_changed(2, out_c);
        } else {
            set_pc_bit((value >> 1) & 7, (value & 1) != 0);
        }
        break;
    }
}

uint8_t I8255::ack_a(void)
{
    obf_a = false;
    return out_a;
}

void I8255::strobe_a(uint8_t data)
{
    in_a = data;
    ibf_a = true;
}
