#include "chips/i8251.h"
#include "core/state.h"
#include "core/devices.h"

// Mode instruction (async): D1 D0 baud factor (00 sync, 01 x1, 10 x16, 11 x64), D3 D2 character
// length 5..8, D4 parity enable, D5 even parity, D7 D6 stop bits (01 one, 10 one and a half,
// 11 two). Sync: D6 external sync detect, D7 single sync character.

void I8251::reset(uint64_t now)
{
    mode = 0x4E;
    cmd = 0;
    expect = 0;
    thr_empty = true;
    tsr_busy = false;
    rxrdy = rx_busy = false;
    pe = oe = fe = false;
    rx_ready_at = now;
    update_bit_time();
}

void I8251::set_clock(uint32_t hz, double txc)
{
    cpu_hz = hz;
    txc_hz = txc > 0 ? txc : 1;
    update_bit_time();
}

void I8251::update_bit_time(void)
{
    static const int factor[4] = { 1, 1, 16, 64 };
    cycles_per_bit = (double)cpu_hz * factor[mode & 3] / txc_hz;
}

int I8251::frame_bits(void) const
{
    int data = 5 + ((mode >> 2) & 3);
    int parity = (mode & 0x10) ? 1 : 0;
    int stop = (mode >> 6) == 3 ? 2 : (mode >> 6) == 2 ? 2 : 1;   // 1.5 stop bits rounded up
    return 1 + data + parity + stop;
}

bool I8251::txrdy_pin(SerialChannel* ch) const
{
    bool cts = ch ? ch->cts.load() : false;
    return thr_empty && (cmd & CMD_TXEN) && cts;
}

uint8_t I8251::read(int cd, SerialChannel* ch)
{
    if (!cd) {
        rxrdy = false;
        return rx_data;
    }
    uint8_t s = 0;
    if (thr_empty) s |= ST_TXRDY;
    if (rxrdy) s |= ST_RXRDY;
    if (thr_empty && !tsr_busy) s |= ST_TXE;
    if (pe) s |= ST_PE;
    if (oe) s |= ST_OE;
    if (fe) s |= ST_FE;
    if (ch && ch->cts) s |= ST_DSR;   // the terminal is ready (DSR = CTS of the channel)
    return s;
}

void I8251::write(int cd, uint8_t v)
{
    if (!cd) {
        thr = v;
        thr_empty = false;
        return;
    }
    switch (expect) {
    case 0:   // mode instruction
        mode = v;
        update_bit_time();
        expect = (v & 3) ? 1 : ((v & 0x80) ? 3 : 2);   // sync: one or two sync characters follow
        break;
    case 2:
    case 3:   // sync characters (not used)
        expect = expect == 2 ? 3 : 1;
        break;
    default:  // command instruction
        if (v & CMD_IR) {   // internal reset: the next write is a mode instruction
            expect = 0;
            cmd = 0;
            return;
        }
        if (v & CMD_ER) pe = oe = fe = false;
        cmd = v;
        break;
    }
}

void I8251::advance(uint64_t now, SerialChannel* ch)
{
    if (!async()) return;
    // transmitter: a character leaves the shift register one frame after it went in
    if (tsr_busy && now >= tx_done_at) {
        tsr_busy = false;
        uint8_t mask = (uint8_t)((1u << (5 + ((mode >> 2) & 3))) - 1);
        if (ch) ch->to_host(tsr & mask);
    }
    bool cts = ch ? ch->cts.load() : false;
    if (!tsr_busy && !thr_empty && (cmd & CMD_TXEN) && cts) {
        tsr = thr;
        thr_empty = true;
        tsr_busy = true;
        tx_done_at = now + (uint64_t)char_cycles();
    }
    // receiver: a character from the host is on the line for one frame before RxRDY
    if (rx_busy && now >= rx_done_at) {
        rx_busy = false;
        oe = rxrdy;   // the previous character not read yet: overrun
        rx_data = (uint8_t)(rx_shift & ((1u << (5 + ((mode >> 2) & 3))) - 1));
        rxrdy = true;
    }
    if (!rx_busy && ch && (cmd & CMD_RXE) && !(ch->pacing && rxrdy)) {
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

void I8251::state(StateIO& io)
{
    io.v(mode);
    io.v(cmd);
    io.e(expect);
    io.v(txc_hz);
    io.v(cycles_per_bit);
    io.v(thr);
    io.v(tsr);
    io.v(thr_empty);
    io.v(tsr_busy);
    io.v(tx_done_at);
    io.v(rx_data);
    io.v(rx_shift);
    io.v(rxrdy);
    io.v(rx_busy);
    io.v(pe);
    io.v(oe);
    io.v(fe);
    io.v(rx_done_at);
    io.v(rx_ready_at);
}
