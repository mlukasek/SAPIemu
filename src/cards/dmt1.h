#pragma once
// DMT-1 - measuring amplifier of SAPI-1 (Tesla Liberec, 6XV 122 85), see docs/desky/DMT-1.md.
//
// - Eight inputs K1-K8 (XC2) switched by relays RP 210 onto the precise amplifier WSH 222, gain
//   100 / 200 / 500 / 1000 by the multiplexer NL1, galvanic separation by a modulator and
//   a transformer; output U1 on XC3/15, at most +-8 V (+-12 V supply) or +-10 V (+-15 V on XC3).
//   Settling after a switch 0.5 s.
// - Temperature of the terminal block: transistor KF 524 on XC3, output U2 0 ... -5 V for
//   0 ... 50 degC.
// - One write register DD1 (MHB 8282), decoded by A7-A4 only (SA1-SA4 = A7-A4, the address is
//   the negation of the switches): 16 mirror ports. Bits: 7 = all relays off, 5 = a rising edge
//   starts DD5: after 600 ms a 20 ms pulse on INT1 (jumper 7-8), 4-3 = gain, 2-0 = channel.
// - Jumper 5-6: an open input (broken thermocouple) shows as about -15 mV.
// - The inputs come from the plant (Bus::plant: thermocouples of APS-5), U1 and U2 go by cables to
//   ADP-1 (Adp1 reads them).
#include "core/card.h"

class Dmt1 : public Card {
public:
    Dmt1() : Card("DMT-1") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    bool io_write(uint8_t port, uint8_t data) override;
    int irq_lines() override;
    void describe(nlohmann::json& out) override;
    void state(StateIO& io) override;
    int decode_io(uint8_t port, const char** what) override;
    std::string live_status() override;

    double u1(void);     // output of the amplifier now (V)
    double u2(void);     // output of the thermometer (V)

private:
    double input_mv(void);    // the selected input now (mV), 0 with the relays off
    double target(void);      // U1 the amplifier settles to
    int gain(void) const;

    uint8_t base = 0x80;
    bool break_detect = true;     // 5-6
    bool int_jumper = false;      // 7-8: INT1 on the bus
    bool supply15 = false;        // +-15 V from XC3 (1-2 and 3-4 open): output up to +-10 V
    uint8_t reg = 0x80;
    uint64_t switched = 0;        // bus time of the last change of channel or gain
    double from = 0;              // U1 at that time
    uint64_t int_start = 0;       // bus time of the rising edge of bit 5 (0 = none)
};
