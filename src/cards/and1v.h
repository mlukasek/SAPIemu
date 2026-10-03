#pragma once
// AND-1V rev 2.3 - VGA alphanumeric display (Libor Lasota, after AND-1Z),
// see docs/desky/AND-1V.md.
//
// - 64x20 characters of 10x24 dots, VGA 640x480/60 Hz (25.175 MHz), one ink
//   colour of 15 on black (jumpers S1 /BLU /RED /GRH /GRL, fitted = off), no
//   attributes, no cursor.
// - 2 KB VRAM, offset = row * 64 + column; rows 20-31 are plain RAM.
// - Decoding: two 74LS85 and jumpers S2 (no PROM): base in 2 KB steps (/A11-
//   /A15), MAP1OFF and H/L: always / when MAP1 = H / when MAP1 = L / never.
//   /RES disables the selection.
// - Character generator 27C256: A0-A3 = line index, A4-A11 = code, A12 = right
//   half, A13 = odd microline, A14 = 1; dots D3 (left) .. D7. Microline m
//   (0-23): line = m >> 1, index = line < 6 ? line + 2 : line + 4.
// - Not modelled: wait states (one CPU slot per character, 0-1 waits at 2 MHz;
//   none on I/O).
#include <vector>
#include "core/devices.h"

class And1v : public Card, public DisplayDevice {
public:
    And1v() : Card("AND-1V") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

    int screen_width() const override { return COLS * CELL_W; }
    int screen_height() const override { return ROWS * CELL_H; }
    double dot_clock_hz() const override { return 7.375e6; }   // square VGA pixels
    void render(uint32_t* argb, uint32_t fg, uint32_t bg) override;
    std::vector<std::string> text_rows() override;

    static const int COLS = 64;
    static const int ROWS = 20;
    static const int CELL_W = 10;
    static const int CELL_H = 24;

private:
    bool selected(uint16_t addr, int map) const;

    uint8_t vram[2048];
    std::vector<uint8_t> font;     // 32 KB (27C256)
    uint16_t base = 0xF800;
    int map_mode = 0;              // 0 always (MAP1OFF), 1 MAP1 = H, 2 MAP1 = L, 3 never
    uint32_t ink = 0xFFFFFFFFu;    // 0 = the GUI phosphor colour
    bool own_ink = false;
    std::string font_file;
};
