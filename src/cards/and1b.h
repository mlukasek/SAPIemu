#pragma once
// AND-1B rev 1.4 - VGA replacement of AND-1 / AND-1A (Libor Lasota), see docs/desky/AND-1B.md.
//
// - 40x24 characters of 7x10 font dots, every dot 2x2 VGA pixels (560x480 in 640x480/60 Hz,
//   25.175 MHz / 2 dot clock, 57 characters a line, 525 lines = 16.641 ms, 60.09 Hz).
// - 1 KB video RAM in a 2 KB window, same layout as AND-1: address = base + row * 40h + column,
//   columns 32-63 folded into the area of rows 24-31 (74LS157 IC18). No wait states, no ports.
// - Window (S2, 74LS85, no PROM): 3800 (7-8 fitted), E800 (5-6), F800 (none), 2800 (both).
// - Byte: D7-D6 attribute, D5-D0 character (MHB 2501 set). The attribute only selects a 1 KB bank
//   of the character generator 27C256 (A13 blink phase, A12 left/right half, A11-A10 = D7-D6), the
//   effects are in the font: 01 blinking, 10 blinking underline, 11 double width (font D7 = 1
//   holds the character for its left half in the next cell). Dots: D6 left .. D0 right.
// - Blinking: frame counter, the blink phase is off 8 frames of 32 (75:25).
// - INT1 (S2 1-2 "50 Hz" / 3-4 "60 Hz", both open = off): pulse from line 480 to 490 (317 us)
//   every frame, in the 50 Hz mode 5 frames of 6.
// - Ink (S1 /R GL GH /B, fitted = off): 15 colours like AND-1V.
// - Not modelled: "snow" (a cell the CPU hits while it is fetched stays black).
#include <vector>
#include "core/devices.h"

class And1b : public Card, public DisplayDevice {
public:
    And1b() : Card("AND-1B") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override { return mem_read(addr, data); }
    int irq_lines() override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

    int screen_width() const override { return 640; }
    int screen_height() const override { return 480; }
    double dot_clock_hz() const override { return 7.375e6; }   // square VGA pixels
    void render(uint32_t* argb, uint32_t bg) override;
    const char* ink_param() const override { return "color"; }   // jumpers S1
    std::string ink() const override { return color; }
    bool set_ink(const std::string& value, std::string& err) override;
    std::vector<std::string> text_rows() override;

    static constexpr int COLS = 40;
    static constexpr int ROWS = 24;
    static constexpr int BORDER = 40;   // black VGA pixels left and right of the 560 wide picture

    static int vram_index(unsigned offset);   // offset in the 2 KB window -> index into the 1 KB RAM
    uint8_t vram[1024];

private:
    bool selected(uint16_t a) const { return (a & 0xF800) == base; }
    uint64_t frame_cycles(void) const;
    uint8_t font_byte(uint8_t code, bool lr, bool blink_off, int row) const;

    std::vector<uint8_t> font;     // 32 KB (27C256), the upper half is read (A14 = 1)
    std::string font_file;
    uint16_t base = 0x3800;
    int int1_mode = 0;             // 0 off, 50, 60
    uint32_t ink_argb = 0xFF00FF00u;
    std::string color = "green";
};
