#pragma once
// CRM-1 - battery backed CMOS RAM 4 KB of SAPI-1 (Tesla Liberec, 6XV 122 83), see docs/desky/CRM-1.md.
//
// - 32 x MHB 1902 (1K x 1): up to four kilobytes CS1-CS4, fitted as needed.
// - DD5 (3205) picks the 8 KB block (jumpers 1-2 .. 1-9, standard 1-9 = E000h), DD4 (3205) its
//   1 KB parts: each CS takes one by its jumper (10..13 - 14..21, standard 0000h, 0400h, 0800h,
//   0C00h: E000-EFFF).
// - SA1 blocks writes into the CS1 kilobyte (on the boards as made it does not work: a PCB and
//   schematic error, fixed on J. Biba's board).
// - The MHB 1902 needs 1000 ns: the one-shot DD6 (74121, R10 5k6) holds RDY low after each access
//   (about 590 ns): one wait state at 2 MHz, two at 4 MHz.
// - The content survives power off (batteries on the board): kept in a file like the DMR-1 SRAM.
#include <string>
#include <vector>
#include "core/card.h"

class Crm1 : public Card {
public:
    Crm1() : Card("CRM-1") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override;
    void flush() override;
    void describe(nlohmann::json& out) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;

private:
    int chip(uint16_t addr) const;   // the kilobyte (CS1..CS4 = 0..3) at addr, -1 none
    int wait_states(void) const;

    std::vector<uint8_t> ram;        // 4 KB, CS1 at 0
    uint16_t base = 0xE000;          // the 8 KB block
    int offset[4] = { 0x0000, 0x0400, 0x0800, 0x0C00 };   // per CS, -1 = not fitted
    bool protect = false;            // SA1
    bool battery = true;
    std::string file;
    int rdy_ns = 590;
};
