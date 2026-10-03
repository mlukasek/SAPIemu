#pragma once
// MHB1012 UART (AY-5-1013 / TR1602 compatible). Character level model with
// correct timing: a character occupies start + data + parity + stop bits at the
// configured baud rate. Board-specific register mapping lives in the card.
#include <stdint.h>

class SerialChannel;

struct Mhb1012 {
    // control word (written through the board): parity inhibit, 2 stop bits,
    // word length 5..8, even parity
    bool no_parity = true;
    bool two_stop = false;
    int word_bits = 8;
    bool even_parity = false;

    // status
    bool dr = false;          // received data available
    bool tbre = true;         // transmit holding register empty
    bool pe = false, fe = false, oe = false;
    uint8_t rx_data = 0;

    void set_baud(uint32_t cpu_hz, uint32_t baud);
    void reset(uint64_t now);
    void write_control(uint8_t cw);   // DSM-1/DMR-1 bit layout
    void write_data(uint8_t d);
    uint8_t read_data(void);
    // Move characters between the UART and the host channel up to `now`.
    void advance(uint64_t now, SerialChannel* ch);
    bool tx_idle(void) const { return tbre && !tsr_busy; }

private:
    double char_cycles(void) const;

    double cycles_per_bit = 208.3;
    uint8_t thr = 0;          // transmit holding register
    uint8_t tsr = 0;          // transmit shift register
    bool tsr_busy = false;
    uint64_t tx_done_at = 0;
    uint64_t rx_ready_at = 0; // earliest time the next received character is complete
};
