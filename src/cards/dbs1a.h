#pragma once
// DBS-1A - colour semigraphic display of SAPI-1.5 (Tesla Liberec 1990, "Deska Barevné Semigrafiky"),
// see docs/desky/DBS-1A.md.
//
// - 6845 CRTC at ports E0h (address register) and E1h (data register), the pair chosen by the
//   jumpers XT2/XT3 (PROM DD27); only R14-R17 can be read.
// - 4 KB video RAM, WRITE ONLY: even CPU addresses = characters (DD18), odd = attributes (DD17),
//   cell = CPU A1-A11 = 6845 MA0-MA10 (MA11, MA13 not connected, MA12 may select a font set).
//   PROM DD2 maps it into a 4 KB window: B000-BFFF when MAP1 = L and MAP2 = H (standard, XT1
//   1-2, 3-4, 6-7, XT4 19-20); PKT-1 sets MAP2 by OUT 0Eh,03h around each screen write.
// - Character generator EPROM (2716/2732/2764, XT5): A0-A2 = RA0-RA2, A3 = code bit 7, A4-A10 =
//   code bits 0-6, A11/A12 by XT5 (RA3, attribute bit 3, MA12). Bit 7 = leftmost dot. The Tesla
//   EPROM "59" (dump EC1045 2026) confirms this layout: 8 x 12 cell, rows 8-11 in the upper 2 KB.
// - 12 MHz dot clock, 8 dots per character (64 columns) or 6 (80 columns, XT7).
// - Attribute: bits 2-0 foreground, 6-4 background, 3 intensity (Y), 7 = 1 blink (the cell shows
//   the background for 16 of 32 frames). R/G order: the manual and the CP/M text say bit 2 = R,
//   bit 1 = G; the schematic wires bit 1 to R (parameter rgb_order).
// - The 6845 cursor is XORed with the dots.
#include <vector>
#include "core/devices.h"

class Dbs1a : public Card, public DisplayDevice {
public:
    Dbs1a() : Card("DBS-1A") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int decode_io(uint8_t port, const char** what) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

    int screen_width() const override;
    int screen_height() const override;
    double dot_clock_hz() const override { return 12e6; }
    bool text_display() const override { return true; }
    void render(uint32_t* argb, uint32_t bg) override;
    std::vector<std::string> text_rows() override;

private:
    bool window(uint16_t addr, int map) const;
    int cols(void) const;
    int rows(void) const;
    int lines(void) const;   // scan lines per character row
    uint32_t glyph_addr(uint8_t code, uint8_t attr, int ra, unsigned ma) const;
    uint32_t colour(int c, bool y) const;

    std::vector<uint8_t> font;       // the character generator EPROM
    uint8_t chr[2048];               // DD18
    uint8_t attr[2048];              // DD17
    uint8_t r[18];                   // 6845 registers
    uint8_t ar = 0;                  // address register
    uint8_t port_base = 0xE0;
    uint16_t win_base = 0xB000;      // XT4
    bool high_group = false;         // XT1 5-6: C000-FFFF (MAP not decoded)
    bool map1_used = true, map2_used = true;   // XT1 1-2, 3-4
    int xt5 = 1;                     // EPROM addressing (index into the XT5 options)
    int dots = 8;                    // XT7
    bool schematic_rg = false;       // bit 1 = R, bit 2 = G (schematic) instead of the manual
    bool mono = false;               // composite output (grey from RGB, XT6 35-36)
};
