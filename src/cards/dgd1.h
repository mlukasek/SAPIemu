#pragma once
// DGD-1 - graphic display card of SAPI-1 (see docs/desky/DGD-1.md).
//
// - 320x240 dots, 16 KB dynamic RAM (8x 4116) as ordinary memory in one quarter
//   of the address space: switch SP1-2 open = C000H (ZPS-2, ZPS-3), closed =
//   0000H (ZPS-4, SAPI-1Z); SP1-1 closed = with mapping (selected only while
//   MAP1 is active).
// - Line y (0 = top) at base + y * 40h, 40 bytes (320 dots), bit 7 = leftmost
//   dot, 1 = dot lit. 240 lines: base .. base + 3BFFH.
// - 8 MHz dot clock, 1 MHz byte clock (64 us line). The CPU gets the memory in
//   its window of every byte clock period, until then RDY is low (wait states).
// - Its own video output (TV / AZJ-462), next to the AND-1 text display.
// - The address decoder PROM D1A is dumped but the wiring of its inputs is not
//   known yet; the selection follows the table of SP1 in the manual.
#include <vector>
#include "core/devices.h"

class Dgd1 : public Card, public DisplayDevice {
public:
    Dgd1() : Card("DGD-1") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

    int screen_width() const override { return WIDTH; }
    int screen_height() const override { return HEIGHT; }
    double dot_clock_hz() const override { return 8e6; }
    bool text_display() const override { return false; }
    void render(uint32_t* argb, uint32_t fg, uint32_t bg) override;
    std::vector<std::string> text_rows() override { return std::vector<std::string>(); }

    static const int WIDTH = 320;
    static const int HEIGHT = 240;
    static const int LINE_BYTES = 64;   // address step of a line (40 bytes shown)

private:
    bool selected(uint16_t addr, int map) const;
    int wait_states(void) const;

    uint16_t base = 0xC000;     // SP1-2
    bool mapping = false;       // SP1-1
    bool waits = true;
    std::vector<uint8_t> ram;   // 16 KB
};
