#pragma once
// REM-1 - RAM and EPROM card of SAPI-1 (see docs/desky/REM-1.md).
//
// D3B (3205) divides the address space into 8 KB blocks (A15-A13); jumpers
// connect the blocks to the EPROM groups (E, F) and the RAM (R). Disabled
// during /RES.
// - 2708: 8 sockets of 1 KB in block E (pages selected by A10-A12, jumpers
//   A-M, B-N, C-P); 2716: 4 sockets of 2 KB in block E and 4 in block F.
// - RAM: up to 16x 2114 = 8 KB in 1 KB pages from the start of block R.
// The card answers in the whole decoded block; empty sockets read FFh
// (assumption, see the open questions).
#include <vector>
#include "core/card.h"

class Rem1 : public Card {
public:
    Rem1() : Card("REM-1") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override { return mem_read(addr, data); }
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

private:
    int block_e = 0, block_f = -1, block_r = 2;   // 8 KB blocks, -1 = not connected
    bool type_2716 = false;
    std::vector<uint8_t> eprom;   // E block (2708: 8 KB; 2716: E then F, 16 KB), FFh where empty
    std::vector<uint8_t> ram;     // populated 1 KB pages of block R
};
