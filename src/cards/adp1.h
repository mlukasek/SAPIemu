#pragma once
// ADP-1 - A/D converter of SAPI-1 (Tesla Liberec, 6XV 122 81), see docs/desky/ADP-1.md.
//
// - 16 analog inputs S1-S16 (X3) through the multiplexer MAB 16, gain 1 or 10, absolute value and
//   polarity, integrating converter C 520 D: three BCD digits, 10 mV (gain 1) or 1 mV (gain 10)
//   a digit; over the range BBB.
// - Base address by SA1-SA4 = A7-A4 (the base is the negation of the switches). Ports:
//   write +1 = channel (bits 0-3), gain 10 (bit 4), clears READY and starts one conversion;
//   read +1 = READY (bit 7), polarity (bit 4, 1 = minus, follows the input now), MSD (bits 0-3);
//   read +0 = NSD (bits 4-7), LSD (bits 0-3). The value stays until the next start.
// - References on X3: +9.99 V (pin 2) and +0.999 V (pin 1).
// - Inputs: wiring "aps5" = the cables of APS-5: S1 = U1 of DMT-1, S2 = U2 of DMT-1 (terminal block
//   temperature), S5 = the backup battery, S6 = 0 V, S7 = the +9.99 V reference (terminal KAL).
#include "core/card.h"

class Dmt1;

class Adp1 : public Card {
public:
    Adp1() : Card("ADP-1") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    void describe(nlohmann::json& out) override;
    void state(StateIO& io) override;
    int decode_io(uint8_t port, const char** what) override;
    std::string live_status() override;

    double input_v(int ch);   // voltage on S(ch+1) now

private:
    void convert(void);       // finish the conversion now
    Dmt1* dmt(void);

    uint8_t base = 0x30;
    bool aps5 = false;
    double ref_v = 9.99;      // RP1
    double conv_s = 0.02;     // one conversion
    uint8_t ctrl = 0;         // the control register (channel, gain)
    uint64_t started = 0;     // bus time of the start
    bool busy = false;
    uint8_t hi = 0, lo = 0;   // MSD (bits 0-3), NSD LSD
};
