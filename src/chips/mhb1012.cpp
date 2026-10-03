#include "chips/mhb1012.h"
#include "core/devices.h"

void Mhb1012::set_baud(uint32_t cpu_hz, uint32_t baud)
{
    cycles_per_bit = (double)cpu_hz / (double)(baud ? baud : 1);
}

double Mhb1012::char_cycles(void) const
{
    int bits = 1 + word_bits + (no_parity ? 0 : 1) + (two_stop ? 2 : 1);
    return cycles_per_bit * bits;
}

void Mhb1012::reset(uint64_t now)
{
    dr = false;
    tbre = true;
    pe = fe = oe = false;
    tsr_busy = false;
    rx_ready_at = now;
}

void Mhb1012::write_control(uint8_t cw)
{
    // DSM-1 manual: b0 PI (1 = no parity), b1 SBS (1 = 2 stop bits),
    // b3-2 word length, b4 EPS (1 = even). The word length bit order is not
    // certain (OCR); with b3 = NB1, b2 = NB2 the value 11 is 8 bits.
    no_parity = (cw & 0x01) != 0;
    two_stop = (cw & 0x02) != 0;
    int wl = ((cw >> 3) & 1) | (((cw >> 2) & 1) << 1);
    word_bits = 5 + wl;
    even_parity = (cw & 0x10) != 0;
}

void Mhb1012::write_data(uint8_t d)
{
    thr = d;
    tbre = false;
}

uint8_t Mhb1012::read_data(void)
{
    dr = false;
    return rx_data;
}

void Mhb1012::advance(uint64_t now, SerialChannel* ch, bool rx_allowed)
{
    // transmitter
    if (tsr_busy && now >= tx_done_at) {
        tsr_busy = false;
        uint8_t mask = (uint8_t)((1u << word_bits) - 1);
        if (ch) ch->to_host(tsr & mask);
    }
    if (!tsr_busy && !tbre) {
        tsr = thr;
        tbre = true;
        tsr_busy = true;
        tx_done_at = now + (uint64_t)char_cycles();
    }
    // receiver: one character per character time
    if (ch && rx_allowed && now >= rx_ready_at && !(ch->pacing && dr)) {
        uint8_t b;
        if (ch->from_host(b)) {
            if (dr) oe = true; else oe = false;
            uint8_t mask = (uint8_t)((1u << word_bits) - 1);
            rx_data = b & mask;
            dr = true;
            fe = pe = false;
            rx_ready_at = now + (uint64_t)char_cycles();
        }
    }
}
