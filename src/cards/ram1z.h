#pragma once
// RAM-1Z - dynamic RAM 64 KB for the Z80 cards (ZPS-4, SAPI-1Z), see
// docs/desky/RAM-1Z.md.
//
// 4 rows of 8x 4116 (16 KB each), refresh from the Z80 /RFSH. Decoder PROM D3B
// (74S571, exchangeable): A0-A5 = A10-A15, A6 = S1/1, A7 = S1/2 (closed = 0),
// A8 = MAP1 (S1/4 on) or ground (S1/3 on); outputs active low select the row:
// O4 (D3) row 1 (0000-3FFF), O3 (D2) row 2, O2 (D1) row 3, O1 (D0) row 4.
// Standard content: both closed 64 KB, S1/1 open 62 KB (no F800-FFFF, ZPS-4),
// S1/2 open 60 KB (no F000-FFFF), both open none; MAP1 removes 0000-3FFF.
#include <vector>
#include "core/card.h"

class Ram1z : public Card {
public:
    Ram1z() : Card("RAM-1Z") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

private:
    int cell(uint16_t addr, int map) const;   // -1 not selected, -2 row without chips

    uint8_t prom[512];
    int rows = 4;
    bool s1_1 = false, s1_2 = true;   // closed
    bool map1 = false;                // S1/4 on (MAP1), else S1/3 on (ground)
    std::string prom_file;
    std::vector<uint8_t> ram;         // 64 KB
};
