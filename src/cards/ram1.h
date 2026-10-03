#pragma once
// RAM-1 / RAM-1A - dynamic RAM card of SAPI-1 (see docs/desky/RAM-1.md).
//
// Up to 3 rows of 8x MHB 4116 (16 KB each): RAM-1/16K, /32K, /48K. Decoder
// PROM D1A (MH 74S287, A10-A15 and jumpers S1, S2): SEL per 1 KB, the row by
// A15-A14 (4000-7FFF row 1, 8000-BFFF row 2, C000-FFFF row 3). The factory PROM
// gives (S1 S2): 1 0 = 4000-FFFF (48K), 0 1 = 4000-BFFF (32K), 1 1 = 4000-7FFF
// (16K), 0 0 = 4000-DFFF (40K); 0000-3FFF is never used. No dump of the PROM:
// its content is generated from that table. Hidden refresh in M1, no waits.
#include <vector>
#include "core/card.h"

class Ram1 : public Card {
public:
    Ram1() : Card("RAM-1") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override { return mem_read(addr, data); }
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

private:
    bool selected(uint16_t addr) const { return (prom[addr >> 10] & 1) != 0; }
    uint8_t prom[64];        // SEL for the 64 blocks of 1 KB (the PROM page chosen by S1, S2)
    int rows = 3;            // fitted rows of 4116 (16 KB each)
    bool s1 = true, s2 = false;
    std::vector<uint8_t> ram;   // 48 KB, 4000-FFFF
};
