#include "chips/mhb1012.h"
#include <math.h>
#include "core/state.h"
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
    rx_busy = false;   // a character being received is lost
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

void Mhb1012::write_data(uint8_t d, uint64_t now)
{
    thr = d;
    tbre = false;
    thr_at = 0;
    if (clock_grid) {   // the next edge of the 16x clock after the write
        double clk = cycles_per_bit / 16.0;
        double edge = (floor((double)now / clk) + 1.0) * clk;
        thr_at = (uint64_t)ceil(edge);
    }
}

void Mhb1012::rx_inject(uint8_t data, bool framing_error, bool parity_error)
{
    oe = dr;   // previous character not read yet: overrun
    rx_data = (uint8_t)(data & ((1u << word_bits) - 1));
    dr = true;
    fe = framing_error;
    pe = parity_error;
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
    if (!tsr_busy && !tbre && now >= thr_at) {
        // clock_grid: the character starts on its clock edge (or when the previous one ended), not at the
        // moment advance() happens to be called
        uint64_t start = now;
        if (clock_grid) start = thr_at > tx_done_at ? thr_at : tx_done_at;
        tsr = thr;
        tbre = true;
        tsr_busy = true;
        if (tx_log) tx_log->push_back(TxChar{ start, thr });
        tx_done_at = start + (uint64_t)char_cycles();
        if (now >= tx_done_at) {   // advance() came late: the character is out already
            tsr_busy = false;
            uint8_t mask = (uint8_t)((1u << word_bits) - 1);
            if (ch) ch->to_host(tsr & mask);
        }
    }
    // receiver: a character taken from the host is on the line for one character
    // time (start bit .. stop bits) before DR is set
    if (rx_busy && now >= rx_done_at) {
        rx_busy = false;
        oe = dr;   // previous character not read yet: overrun
        uint8_t mask = (uint8_t)((1u << word_bits) - 1);
        rx_data = rx_shift & mask;
        dr = true;
        fe = pe = false;
    }
    if (!rx_busy && ch && rx_allowed && !(ch->pacing && dr)) {
        uint8_t b;
        if (ch->from_host(b)) {
            rx_shift = b;
            rx_busy = true;
            uint64_t start = now > rx_ready_at ? now : rx_ready_at;
            rx_done_at = start + (uint64_t)char_cycles();
            rx_ready_at = rx_done_at;
        }
    }
}

void Mhb1012::state(StateIO& io)
{
    io.v(no_parity);
    io.v(two_stop);
    io.e(word_bits);
    io.v(even_parity);
    io.v(dr);
    io.v(tbre);
    io.v(pe);
    io.v(fe);
    io.v(oe);
    io.v(rx_data);
    io.v(cycles_per_bit);
    io.v(thr);
    io.v(tsr);
    io.v(tsr_busy);
    io.v(tx_done_at);
    io.v(rx_ready_at);
    io.v(rx_busy);
    io.v(rx_shift);
    io.v(rx_done_at);
}
