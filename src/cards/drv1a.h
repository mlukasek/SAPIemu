#pragma once
// DRV-1A - relay outputs of SAPI-1 (Tesla Liberec, 6XV 122 82), see docs/desky/DRV-1A.md.
//
// - Eight reed relays KH1-KH8, their contacts S1-S8 on XC2 (S1-S4) and XC3 (S5-S8).
// - One write register MH3212 (DD1): bit n = 1 closes S(n+1). /RES clears it (all contacts open).
// - Address by jumpers, decoded with /IOW by two 3205 (DD2, DD3): 40-47h, 50-57h, 60-67h or
//   70-77h (factory 77h). The card is never read.
// - In APS-5 the contacts switch the solid state relays of the furnace heaters (parameter load).
#include "core/card.h"

class Drv1a : public Card {
public:
    Drv1a() : Card("DRV-1A") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    bool io_write(uint8_t port, uint8_t data) override;
    void describe(nlohmann::json& out) override;
    void state(StateIO& io) override;
    int decode_io(uint8_t port, const char** what) override;
    std::string live_status() override;

    uint8_t contacts(void) const { return latch; }

private:
    void set(uint8_t v);

    uint8_t port = 0x77;
    uint8_t latch = 0;
    bool to_plant = false;   // the contacts switch the heaters of the plant
};
