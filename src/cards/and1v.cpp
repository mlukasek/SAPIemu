#include "cards/and1v.h"
#include "core/state.h"
#include "core/bus.h"
#include "core/charset.h"
#include "core/ihex.h"

#include <string.h>

using nlohmann::json;

bool And1v::configure(const json& cfg, std::string& err)
{
    power_on_fill(vram, sizeof(vram), 0x20);
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
    std::string s1;
    if (!cfg_string(cfg, "color", s1, "white", err)) return false;
    if (!set_ink(s1, err)) return false;
    if (!cfg_string(cfg, "font", font_file, "roms/and-1v/font_and1v22_1024.bin", err)) return false;
    if (!cfg_bool(cfg, "wait_states", waits, true, err)) return false;
    if (!rom_load(font_file, font, 0x8000, 0x00, 0, err)) return false;
    font.resize(0x8000, 0x00);
    return true;
}

// S1: /BLU /RED /GRH /GRL
bool And1v::set_ink(const std::string& value, std::string& err)
{
    if (!vga_ink(value, ink_argb)) {
        err = "color must be white, green, amber, red, blue, cyan, yellow or magenta (jumpers S1)";
        return false;
    }
    color = value;
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

// RDY (sheet 4/7), the same scheme as CGA-1V: the window of the CPU is the half character with VIDEO low,
// every 10 dots of 25.175 MHz. The delays of the gates and flip-flops (74LS) are taken from CGA-1V, where
// they are measured (docs/desky/CGA-1V.md); AND-1V itself is not measured yet (docs/desky/AND-1V.md).
static const double SLOT_NS = 10.0 * 1e9 / 25.175e6;
static const double REQ_NS = 40.0;   // the Z80 edge through IC22A/B, IC5C, IC23B to D of IC16B
static const double REL_NS = 50.0;   // IC16A, IC16B, IC7A, the diode to /WAIT at the Z80

int And1v::wait_states(bool write) const
{
    if (!waits) return 0;
    return bus->window_waits(write, SLOT_NS, SLOT_NS / 2, REQ_NS, REL_NS);
}

bool And1v::mem_read(uint16_t a, uint8_t& data)
{
    if (!selected(a, bus->map)) return false;
    bus->wait += wait_states(false);
    data = vram[a & 0x7FF];
    return true;
}

bool And1v::mem_write(uint16_t a, uint8_t data)
{
    if (!selected(a, bus->map)) return false;
    bus->wait += wait_states(true);
    vram[a & 0x7FF] = data;
    return true;
}

bool And1v::mem_peek(uint16_t a, uint8_t& data)
{
    if (!selected(a, bus->map)) return false;
    data = vram[a & 0x7FF];
    return true;
}

int And1v::decode_mem(uint16_t a, int map, const char** what)
{
    if (!selected(a, map)) return 0;
    *what = "VRAM";
    return DEC_READ | DEC_WRITE;
}

void And1v::render(uint32_t* argb, uint32_t bg)
{
    uint32_t on = ink_argb;
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
    out["color"] = color;
    out["font"] = font_file;
}

void And1v::state(StateIO& io)
{
    io.arr(vram, sizeof(vram));
}
