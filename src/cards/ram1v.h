#pragma once
// RAM-1V rev 1.1 - configurable SRAM card with the MAP1/MAP2 generator (Libor
// Lasota, after RAM-1A), see docs/desky/RAM-1V.md. No PROM: 74LS logic and
// jumpers, all of them parameters here.
//
// - SRAM A (62256) 0000-7FFF, B 8000-FFFF; optional C replaces A at 0000-7FFF
//   while the MAP line chosen by S4 is H (socket meant for a Timekeeper; an
//   M48T35 RTC can be emulated at 7FF8-7FFF of C).
// - 16 KB hole (S5: address A15/A14, MAP1 and MAP2 conditions) and 2 KB hole
//   (S6: address A15-A11, MAP1 condition): the card does not answer there.
// - MAP register (S3: port, write only): one OUT sets MAP1 and MAP2 from data
//   bits chosen by S1/S2 (B1/B2/B4); EN puts them on bus pins 21/22. /RES
//   clears them. The card itself decodes with the MAP lines of the bus.
#include <vector>
#include "core/card.h"

class Ram1v : public Card {
public:
    Ram1v() : Card("RAM-1V") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int map_lines() override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    int decode_io(uint8_t port, const char** what) override;
    void describe(nlohmann::json& out) override;

private:
    enum Cond { C_IGNORE, C_HIGH, C_LOW, C_OFF };
    static bool cond(int c, bool level) { return c == C_IGNORE || (c == C_HIGH && level) || (c == C_LOW && !level); }
    // the chip and offset for an address: 0 A, 1 B, 2 C, -1 nothing
    int chip(uint16_t addr, int map) const;
    uint8_t rtc_read(unsigned reg);

    bool fitted[3] = { true, true, false };
    int c_select = 0;              // S4: 0 never, 1 MAP1 = H, 2 MAP2 = H
    bool c_rtc = false;            // M48T35 Timekeeper in socket C
    int hole16_cond1 = C_HIGH, hole16_cond2 = C_IGNORE;
    uint16_t hole16_base = 0xC000;
    int hole2_cond1 = C_LOW;
    uint16_t hole2_base = 0xF800;
    uint8_t map_port = 0x63;
    bool map_en[2] = { true, true };
    int map_bit[2] = { 6, 7 };
    int map_out = 0;               // MapLine mask of the two flip-flops
    std::vector<uint8_t> mem[3];   // 32 KB each
    uint8_t rtc_ctrl = 0;
};
