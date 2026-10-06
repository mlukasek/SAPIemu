#pragma once
// AND-1Z / AND-1Z-89 - alphanumeric display card (see docs/desky/AND-1Z-89.md).
// 64x20 characters of 6x12 pixels, 2 KB memory-mapped video RAM selected by
// decoder PROM D1 (default F800-FFFF), no attributes, CPU waits via RDY.
#include <vector>
#include "core/devices.h"

class And1z : public Card, public DisplayDevice {
public:
    And1z();

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override;
    void describe(nlohmann::json& out) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;

    int screen_width() const override { return COLS * CELL_W; }
    int screen_height() const override { return ROWS * CELL_H; }
    void render(uint32_t* argb, uint32_t bg) override;
    const char* ink_param() const override { return "monitor"; }
    std::string ink() const override { return monitor.name; }
    bool set_ink(const std::string& value, std::string& err) override { return monitor.set(value, err); }
    std::vector<std::string> text_rows() override;

    static constexpr int COLS = 64;
    static constexpr int ROWS = 20;      // visible rows (vertical PROM DE6)
    static constexpr int CELL_W = 6;
    static constexpr int CELL_H = 12;

    uint8_t vram[2048];

private:
    bool select(uint16_t addr, int map, unsigned& offset) const;
    int wait_states(void) const;

    std::vector<uint8_t> chargen;   // 4 KB, 16 bytes per code, line L=11 on top
    std::vector<uint8_t> prom_d1;   // address decoder
    bool map_enabled = false;       // S1: MAP1 disables the card when "zařazen"
    bool waits = true;              // emulate RDY wait states
    MonitorInk monitor;
};
