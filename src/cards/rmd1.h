#pragma once
// RMD-1 (RMD-1Z) - RAM disk 256 KB (4164) or 1 MB (41256), see docs/desky/RMD-1.md.
//
// Only /IOR, /IOW, A0-A7, D0-D7 and FI2 on X1: no reset, no battery (the
// content is lost at power off). Decoder PROM 74S287 (exchangeable), inputs
// A0-A7, outputs active low: O1 (D0) load address (41h, 45h), O2 (D1) data
// (42h, read and write), O3 (D2) row latch (40h), O4 (D3) prepare read (43h).
// - row latch D0-D3 = A16-A19 (A18/A19 only with 41256)
// - load address: data -> A8-A15, A0-A7 := 00h, or 80h when the port has A2 = 1
// - prepare read: the byte at the address goes to the read latch, address + 1
// - data read: the latch, then the next byte is read, address + 1
// - data write: the byte to the address, address + 1 (no carry into A16)
// The O3/O4 assignment follows the software (the schematic labels them the
// other way round; to be checked on a real PROM).
#include <vector>
#include "core/card.h"

class Rmd1 : public Card {
public:
    Rmd1() : Card("RMD-1") {}
    ~Rmd1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    void flush() override;
    void describe(nlohmann::json& out) override;
    int decode_io(uint8_t port, const char** what) override;

private:
    uint32_t full(void) const { return ((uint32_t)row << 16 | addr) & mask; }

    uint8_t prom[256];
    std::vector<uint8_t> mem;
    uint32_t mask = 0x3FFFF;          // 256 KB
    uint8_t row = 0;                  // A16-A19
    uint16_t addr = 0;                // A0-A15 (74LS193 counters)
    uint8_t latch = 0xFF;             // 74LS373 read latch
    std::string prom_file;
    std::string image;                // optional file keeping the content (not on the real board)
    std::string image_path;
    bool dirty = false;
};
