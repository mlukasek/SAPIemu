#pragma once
// DSP-1 - two channel serial board of SAPI-1 (Tesla Elstroj, 3WN 120 05), see docs/desky/DSP-1.md.
//
// - Upper hex digit of the ports: SA2 (74LS85 D6A, closed switch = address bit 0). Lower digit:
//   74LS138 D5A decodes A3..A1, SA1 joins its outputs to the selects: channel 2 (8251 D3B,
//   connector X2) at X0/X1 or X8/X9, channel 1 (8251 D4B, connector X3) at X2/X3 or XA/XB,
//   the interrupt mask register at X4/X5 or XC/XD. A0 = C/D of the 8251s. J. Biba: 48h, 4Ah, 4Ch.
// - Bit rate per channel (SA3 channel 2, SA4 channel 1): 12.288 MHz / 80 = 153.6 kHz / 2^N
//   (N = 0..5), 3.2 kHz (N = 6) or an external clock (N = 7) on TxC = RxC. 9600 Bd at x16 for N = 0.
// - /INT0 (level, open collector) = mask D0 & RxRDY2 | D1 & TxRDY2 | D2 & TxRDY1 | D3 & RxRDY1
//   (the 8251 pins: TxRDY needs TxEN and /CTS). /RES resets both 8251s and the mask.
// - RS-232 (75150/75154) and the current loop (optocouplers) at once, no switch.
#include <memory>
#include <string>
#include "chips/i8251.h"
#include "core/card.h"

class Dsp1 : public Card {
public:
    Dsp1() : Card("DSP-1") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int irq_lines() override;
    SerialChannel* serial_channel(int index) override;
    void describe(nlohmann::json& out) override;
    void state(StateIO& io) override;
    int decode_io(uint8_t port, const char** what) override;

private:
    struct Chan {
        I8251 uart;
        std::unique_ptr<SerialChannel> serial;
        int rate = 0;              // N of SA3 / SA4 (7 = external clock)
        double ext_hz = 0;         // the external TxC = RxC (pin 15)
        double txc_hz(void) const;
    };
    int select(uint8_t port) const;   // 0 channel 2, 1 channel 1, 2 mask register, -1 none

    Chan ch[2];                    // [0] = channel 2 (X2), [1] = channel 1 (X3)
    uint8_t high = 0x40;           // SA2: the upper hex digit
    int sel[3] = { 4, 5, 6 };      // SA1: 74LS138 output of channel 2, channel 1, the mask (-1 = open)
    uint8_t mask = 0;              // D2B 74LS175
};
