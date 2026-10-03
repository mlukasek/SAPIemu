#include "cards/and1v.h"
#include "core/bus.h"
#include "core/charset.h"
#include "core/ihex.h"

#include <string.h>

using nlohmann::json;

bool And1v::configure(const json& cfg, std::string& err)
{
    memset(vram, 0x20, sizeof(vram));
    long long b;
    if (!cfg_number(cfg, "base", b, 0xF800, err)) return false;
    if (b < 0 || b > 0xF800 || (b & 0x7FF)) { err = "base must be a multiple of 800h (jumpers S2 /A11-/A15)"; return false; }
    base = (uint16_t)b;
    std::string m;
    if (!cfg_string(cfg, "map", m, "map1_l", err)) return false;
    if (m == "always") map_mode = 0;
    else if (m == "map1_h") map_mode = 1;
    else if (m == "map1_l") map_mode = 2;
    else if (m == "never") map_mode = 3;
    else { err = "map must be always, map1_h, map1_l or never (MAP1OFF, H/L)"; return false; }
    std::string color;
    if (!cfg_string(cfg, "color", color, "white", err)) return false;
    // S1: red, green (2 bits: GRH 680R, GRL 1k5), blue
    struct C { const char* name; uint32_t argb; };
    static const C colors[] = {
        { "white", 0xFFFFFFFFu }, { "green", 0xFF00FF00u }, { "amber", 0xFFFFAA00u }, { "red", 0xFFFF0000u },
        { "blue", 0xFF0000FFu }, { "cyan", 0xFF00FFFFu }, { "yellow", 0xFFFFFF00u }, { "magenta", 0xFFFF00FFu },
        { "phosphor", 0 },
    };
    own_ink = false;
    bool ok = false;
    for (const C& c : colors)
        if (color == c.name) { ok = true; ink = c.argb; own_ink = c.argb != 0; }
    if (!ok) { err = "color must be white, green, amber, red, blue, cyan, yellow, magenta or phosphor"; return false; }
    if (!cfg_string(cfg, "font", font_file, "roms/and-1v/font_and1v22_1024.bin", err)) return false;
    if (!rom_load(font_file, font, 0x8000, 0x00, 0, err)) return false;
    font.resize(0x8000, 0x00);
    return true;
}

bool And1v::selected(uint16_t a, int map) const
{
    if ((a & 0xF800) != base) return false;
    switch (map_mode) {
    case 0: return true;
    case 1: return (map & MAP_1) != 0;
    case 2: return (map & MAP_1) == 0;
    default: return false;
    }
}

bool And1v::mem_read(uint16_t a, uint8_t& data)
{
    if (!selected(a, bus->map)) return false;
    data = vram[a & 0x7FF];
    return true;
}

bool And1v::mem_write(uint16_t a, uint8_t data)
{
    if (!selected(a, bus->map)) return false;
    vram[a & 0x7FF] = data;
    return true;
}

bool And1v::mem_peek(uint16_t a, uint8_t& data)
{
    return mem_read(a, data);
}

int And1v::decode_mem(uint16_t a, int map, const char** what)
{
    if (!selected(a, map)) return 0;
    *what = "VRAM";
    return DEC_READ | DEC_WRITE;
}

void And1v::render(uint32_t* argb, uint32_t fg, uint32_t bg)
{
    uint32_t on = own_ink ? ink : fg;
    int w = COLS * CELL_W;
    for (int row = 0; row < ROWS; row++)
        for (int m = 0; m < CELL_H; m++) {
            int line = m >> 1;
            unsigned idx = (unsigned)(line < 6 ? line + 2 : line + 4);
            uint32_t* out = argb + (size_t)(row * CELL_H + m) * w;
            for (int col = 0; col < COLS; col++) {
                unsigned code = vram[row * COLS + col];
                for (int half = 0; half < 2; half++) {
                    unsigned addr = 0x4000u | (unsigned)(m & 1) << 13 | (unsigned)half << 12 | code << 4 | idx;
                    uint8_t bits = font[addr & 0x7FFF];
                    for (int x = 0; x < 5; x++)
                        out[col * CELL_W + half * 5 + x] = (bits & (0x08 << x)) ? on : bg;
                }
            }
        }
}

std::vector<std::string> And1v::text_rows()
{
    std::vector<std::string> rows;
    for (int row = 0; row < ROWS; row++) {
        std::string s;
        for (int col = 0; col < COLS; col++) s += sapi_code_to_utf8(vram[row * COLS + col]);
        rows.push_back(s);
    }
    return rows;
}

void And1v::describe(json& out)
{
    char b[8];
    snprintf(b, sizeof(b), "%04X", base);
    out["base"] = b;
    static const char* const modes[] = { "always", "map1_h", "map1_l", "never" };
    out["map"] = modes[map_mode];
    out["font"] = font_file;
}
