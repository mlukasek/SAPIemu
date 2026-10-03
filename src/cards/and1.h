#pragma once
// AND-1 - the original alphanumeric display card of SAPI-1 (see docs/desky/AND-1.md).
//
// - 40x24 characters (20 by jumper), 5x7 dots in a 6x10 cell, 6 MHz dot clock,
//   64 us line, 320 lines = 20.48 ms frame.
// - 1 KB video RAM (2x 2114) in the window 3800-3FFF (decoder D3A: A15-A11 =
//   00111), address = 3800 + row * 40h + column; the 64 addresses of a row are
//   folded into 1 KB (columns 32-39 in the area of rows 24-31, assumption).
// - Byte: D7-D6 mode (00 normal, 01 blinking, 10 blinking underline = cursor,
//   11 double width), D5-D0 character of the MHB 2501 (latin) or MHB 2502
//   (cyrillic) character generator.
// - No wait states (RDY not connected). INT1 at the end of every frame
//   (jumper 49-50).
// AND-1A (ZPS-3) is the same display with the decoder D3A as an exchangeable
// PROM 74S287 (2 KB steps): inputs A15, A13, S1, A14, A11, A12, A6/A7 tied high,
// output O4 (D3) = /SEL. Standard content: E800-EFFF, jumper 44-45 (S1)
// closed = 3800-3FFF (MIKROBASIC, MIKOS).
#include <vector>
#include "core/devices.h"

class And1 : public Card, public DisplayDevice {
public:
    explicit And1(const char* type = "AND-1");

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override { return mem_read(addr, data); }
    int irq_lines() override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

    int screen_width() const override { return COLS * CELL_W; }
    int screen_height() const override { return rows * CELL_H; }
    double dot_clock_hz() const override { return 6e6; }
    void render(uint32_t* argb, uint32_t fg, uint32_t bg) override;
    std::vector<std::string> text_rows() override;

    static const int COLS = 40;
    static const int CELL_W = 6;
    static const int CELL_H = 10;

    uint8_t vram[1024];

private:
    static int vram_index(uint16_t addr);   // folding of 3800-3FFF into 1 KB
    uint64_t frame_cycles(void) const;
    std::string char_utf8(uint8_t code) const;

    std::vector<uint8_t> chargen;   // 64 characters x 8 rows, bits 4-0 = columns from the left
    bool cyrillic = false;          // MHB 2502
    int rows = 24;                  // jumpers 47-48 (24) / 46-47 (20)
    bool int1 = true;               // jumper 49-50: INT1 at the end of the frame
    bool sel_2k[32];                // the decoder: selected 2 KB blocks
    bool s1_closed = false;         // AND-1A jumper 44-45
    std::string decoder_file;       // AND-1A D3A PROM
};
