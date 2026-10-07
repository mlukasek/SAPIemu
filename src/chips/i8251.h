#pragma once
// Intel 8251 USART (MHB8251), asynchronous mode at the character level with correct timing (RSW-1).
// After a reset (pin or the internal reset command) the first control write is the mode
// instruction, the next ones are commands. The bit rate is TxC / factor (x1, x16, x64); TxC and
// RxC come from the card (RSW-1: 8253 counter 0). Synchronous mode is not emulated: its mode
// instruction is accepted (with the sync characters) but nothing is received or sent.
#include <stdint.h>

class SerialChannel;

struct I8251 {
    // status bits
    enum { ST_TXRDY = 0x01, ST_RXRDY = 0x02, ST_TXE = 0x04, ST_PE = 0x08, ST_OE = 0x10, ST_FE = 0x20,
           ST_SYNDET = 0x40, ST_DSR = 0x80 };
    // command bits
    enum { CMD_TXEN = 0x01, CMD_DTR = 0x02, CMD_RXE = 0x04, CMD_SBRK = 0x08, CMD_ER = 0x10, CMD_RTS = 0x20,
           CMD_IR = 0x40, CMD_EH = 0x80 };

    void reset(uint64_t now);              // RESET pin
    // TxC = RxC frequency (Hz) and the bus time base: sets the bit time with the mode's factor
    void set_clock(uint32_t cpu_hz, double txc_hz);
    uint8_t read(int cd, SerialChannel* ch);   // C/D = 0 data, 1 status
    void write(int cd, uint8_t value);
    // Move characters between the USART and the host channel up to `now`.
    void advance(uint64_t now, SerialChannel* ch);

    bool txrdy_pin(SerialChannel* ch) const;  // TxRDY output: buffer empty, TxEN, CTS
    bool rxrdy_pin(void) const { return rxrdy; }
    bool txe_pin(void) const { return thr_empty && !tsr_busy; }
    uint8_t command(void) const { return cmd; }
    bool async(void) const { return (mode & 3) != 0; }
    int frame_bits(void) const;               // start + data + parity + stop (1.5 counted as 2)
    double bit_cycles(void) const { return cycles_per_bit; }
    void state(class StateIO& io);

private:
    double char_cycles(void) const { return cycles_per_bit * frame_bits(); }
    void update_bit_time(void);

    uint8_t mode = 0x4E;        // async x16, 8 bits, no parity, 1 stop
    uint8_t cmd = 0;
    int expect = 0;             // 0 = mode instruction next, 1 = command, 2/3 = sync characters
    uint32_t cpu_hz = 2000000;
    double txc_hz = 153600.0;
    double cycles_per_bit = 208.3;

    // transmitter
    uint8_t thr = 0, tsr = 0;
    bool thr_empty = true, tsr_busy = false;
    uint64_t tx_done_at = 0;
    // receiver
    uint8_t rx_data = 0, rx_shift = 0;
    bool rxrdy = false, rx_busy = false;
    bool pe = false, oe = false, fe = false;
    uint64_t rx_done_at = 0, rx_ready_at = 0;
};
