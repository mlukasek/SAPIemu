#pragma once
// RAM-1 / RAM-1A - dynamic RAM card of SAPI-1 (see docs/desky/RAM-1.md).
//
// Up to 3 rows of 8x MHB 4116 (16 KB each): /16K, /32K, /48K. The decoder PROM
// D1A (MH 74S287, exchangeable to move the RAM) gets A10-A15 on A0-A5 and the
// jumpers S1 on A6, S2 on A7 (closed = 0); its outputs D0 = SEL, D1 = CA0
// (row 1), D3 = CA1 (row 2), D2 = CA2 (row 3), active high. The address within
// a row is A13-A0. RAM-1 and RAM-1A differ only in the PROM:
//   RAM-1 (S1 S2, 1 = closed): 10 = 4000-FFFF, 01 = 4000-BFFF, 11 = 4000-7FFF, 00 = 4000-DFFF
//   RAM-1A: 01 = 0000-BFFF (ZPS-3), 11 = 4000-FFFF, 10 = C000-FFFF, 00 = C000-E7FF
// Hidden refresh in M1, no waits.
#include <vector>
#include "core/card.h"

class Ram1 : public Card {
public:
    explicit Ram1(const char* type) : Card(type) {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override { return mem_read(addr, data); }
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

private:
    // RAM cell of an address: -1 = not selected, -2 = selected row without chips
    int cell(uint16_t addr) const;

    uint8_t sel[64];         // PROM outputs for the 64 blocks of 1 KB (page chosen by S1, S2)
    int rows = 3;            // fitted rows of 4116 (16 KB each)
    bool s1 = true, s2 = false;
    std::string prom_file;
    std::vector<uint8_t> ram;   // 48 KB
};
