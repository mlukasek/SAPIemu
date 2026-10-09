#include "cards/and1b.h"
#include "core/state.h"
#include "core/bus.h"
#include "core/ihex.h"

#include <string.h>

using nlohmann::json;

static const double LINE_US = 399.0 / 12.5875;   // 57 characters of 7 dots: 31.698 us
static const int FRAME_LINES = 525;
static const int INT_FIRST = 480, INT_LAST = 490;   // R24ZAT .. VSYNC (/IRQOFF)

bool And1b::configure(const json& cfg, std::string& err)
{
    power_on_fill(vram, sizeof(vram), 0x20);
    long long b;
    if (!cfg_number(cfg, "base", b, 0x3800, err)) return false;
    if (b != 0x2800 && b != 0x3800 && b != 0xE800 && b != 0xF800) {
        err = "base must be 3800h (S2 7-8), E800h (5-6), F800h (none) or 2800h (both)";
        return false;
    }
    base = (uint16_t)b;
    std::string i;
    if (!cfg_string(cfg, "int1", i, "off", err)) return false;
    if (i == "off") int1_mode = 0;
    else if (i == "50hz") int1_mode = 50;
    else if (i == "60hz") int1_mode = 60;
    else { err = "int1 must be off, 50hz (S2 1-2) or 60hz (S2 3-4)"; return false; }
    std::string c;
    if (!cfg_string(cfg, "color", c, "green", err)) return false;
    if (!set_ink(c, err)) return false;
    if (!cfg_string(cfg, "font", font_file, "roms/and-1b/FontAND1B11.bin", err)) return false;
    if (!rom_load(font_file, font, 0x8000, 0x00, 0, err)) return false;
    if (font.size() == 0x4000) font.insert(font.begin(), font.begin(), font.end());   // a 16 KB file = the upper half
    font.resize(0x8000, 0x00);
    return true;
}

// S1: /R GL GH /B (the same scheme as AND-1V)
bool And1b::set_ink(const std::string& value, std::string& err)
{
    if (!vga_ink(value, ink_argb)) {
        err = "color must be white, green, amber, red, blue, cyan, yellow or magenta (jumpers S1)";
        return false;
    }
    color = value;
    return true;
}

// Columns 0-31: row * 32 + column; columns 32-63 (A5 = 1, IC18): the area of rows 24-31,
// columns 40-63 alias 32-39.
int And1b::vram_index(unsigned o)
{
    int col = (int)(o & 63), row = (int)(o >> 6) & 31;
    if (col < 32) return row * 32 + col;
    return (24 + (row & 7)) * 32 + (row >> 3) * 8 + (col & 7);
}

bool And1b::mem_read(uint16_t a, uint8_t& data)
{
    if (!selected(a)) return false;
    data = vram[vram_index(a & 0x7FF)];
    return true;
}

bool And1b::mem_write(uint16_t a, uint8_t data)
{
    if (!selected(a)) return false;
    vram[vram_index(a & 0x7FF)] = data;
    return true;
}

int And1b::decode_mem(uint16_t a, int, const char** what)
{
    if (!selected(a)) return 0;
    *what = "VRAM";
    return DEC_READ | DEC_WRITE;
}

uint64_t And1b::frame_cycles(void) const
{
    return (uint64_t)((double)bus->cpu_hz * LINE_US * FRAME_LINES / 1e6);
}

int And1b::irq_lines()
{
    if (!int1_mode || !bus) return 0;
    uint64_t f = frame_cycles();
    uint64_t frame = bus->now / f;
    if (int1_mode == 50 && frame % 6 == 5) return 0;   // the 74LS193 skips one pulse of six
    double line = (double)(bus->now % f) * 1e6 / ((double)bus->cpu_hz * LINE_US);
    return line >= INT_FIRST && line < INT_LAST ? IRQ_INT1 : 0;
}

// The 27C256 (upper half): A0-A2 row in the half, A3 half, A4-A11 code (D7-D6 = A11-A10),
// A12 right/left (LR), A13 blink phase.
uint8_t And1b::font_byte(uint8_t code, bool lr, bool blink_off, int row) const
{
    unsigned a = 0x4000u | (blink_off ? 0u : 0x2000u) | (lr ? 0x1000u : 0u) | (unsigned)code << 4 |
                 (unsigned)(row / 5) << 3 | (unsigned)(row % 5);
    return font[a];
}

void And1b::render(uint32_t* argb, uint32_t bg)
{
    const int w = screen_width();
    for (int i = 0; i < w * screen_height(); i++) argb[i] = bg;
    uint64_t frame = bus ? bus->now / frame_cycles() : 0;
    bool blink_off = (frame & 0x18) == 0x18;   // /FLASH = NAND(bit 3, bit 4)
    for (int row = 0; row < ROWS; row++)
        for (int fr = 0; fr < 10; fr++) {
            uint32_t* line0 = argb + (size_t)(row * 20 + fr * 2) * w + BORDER;
            uint32_t* line1 = line0 + w;
            bool hold = false;   // this cell shows the right half of the character latched before
            uint8_t code = 0;
            for (int col = 0; col < COLS; col++) {
                uint8_t bits;
                if (hold) {
                    bits = font_byte(code, false, blink_off, fr);   // LR = 0: the right half
                    hold = false;
                } else {
                    code = vram[vram_index((unsigned)(row * 64 + col))];
                    bits = font_byte(code, false, blink_off, fr);
                    if (bits & 0x80) {   // font D7 = 1 sets LR: this cell gets the left half, the latch holds
                        bits = font_byte(code, true, blink_off, fr);
                        hold = true;     // the byte of the next cell is never shown (in column 39: blanking)
                    }
                }
                for (int x = 0; x < 7; x++) {
                    uint32_t c = (bits & (0x40 >> x)) ? ink_argb : bg;
                    int px = col * 14 + x * 2;
                    line0[px] = line0[px + 1] = line1[px] = line1[px + 1] = c;
                }
            }
        }
}

std::vector<std::string> And1b::text_rows()
{
    static const char* const lat =
        "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_ !\"#\xC2\xA4%&'()*+,-./0123456789:;<=>?";
    std::vector<std::string> out;
    for (int row = 0; row < ROWS; row++) {
        std::string s;
        for (int col = 0; col < COLS; col++) {
            uint8_t c = vram[vram_index((unsigned)(row * 64 + col))] & 0x3F;
            const char* p = lat;   // the 2-byte "¤" at 24h: walk the UTF-8 string
            for (int i = 0; i < c; i++) p += ((unsigned char)*p >= 0xC0) ? 2 : 1;
            s.append(p, ((unsigned char)*p >= 0xC0) ? 2 : 1);
        }
        out.push_back(s);
    }
    return out;
}

void And1b::describe(json& out)
{
    char b[8];
    snprintf(b, sizeof(b), "%04Xh", base);
    out["base"] = b;
    out["columns"] = COLS;
    out["rows"] = ROWS;
    out["int1"] = int1_mode == 0 ? "off" : int1_mode == 50 ? "50hz" : "60hz";
    out["color"] = color;
    out["font"] = font_file;
}

void And1b::state(StateIO& io)
{
    io.arr(vram, sizeof(vram));
}
