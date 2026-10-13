#include "cards/and1z.h"
#include "core/state.h"
#include "core/bus.h"
#include "core/charset.h"
#include "core/ihex.h"

#include <string.h>

using nlohmann::json;

// Character clock 600 ns (10 MHz / 6, Johnson counter DC2); QA high (the first half) is the CPU window, the second
// half belongs to the display refresh. The author's card runs at 600.2 ns (QA 1.6660 MHz, line 65.427 us = 109
// characters): taken as measured, which also keeps the window from locking to the CPU clock as a round 600 ns would
// (the 10 MHz and the 8 MHz crystals are independent: on the board an LDIR drifts through the phases).
static const double CHAR_NS = 600.2;
static const double CPU_WINDOW_NS = 300.0;
// RDY (netlist of the schematic): STSTB with /SEL sets the WAIT flip-flop DD1A (RDY low through the 7406). DC1B takes
// the request /READ = /SEL + /MR or /WRITE = /SEL + /MW at the rising edge of QA, the falling edge of QA (end of the
// window) clears WAIT through DC1A/DC1B; an I/O cycle or /RES clears it too (IN/OUT do not wait). Measured on the
// author's board with probes on QA and WAIT (2026-10-13, docs/zmeny-podle-hw.md): /MW comes 24-32 ns after the CPU
// clock edge, /MR 40-48 ns, and the request must be there about 24 ns before the QA edge, so it counts 52 ns (write)
// and 68 ns (read) after the edge; WAIT falls 24-32 ns after the end of the window and RDY 16-32 ns later, with the
// /WAIT set-up of the Z80 that is 56 ns.
static const double REQ_READ_NS = 68.0;
static const double REQ_WRITE_NS = 52.0;
static const double REL_NS = 56.0;

And1z::And1z() : Card("AND-1Z-89")
{
    memset(vram, 0, sizeof(vram));
}

bool And1z::configure(const json& cfg, std::string& err)
{
    power_on_fill(vram, sizeof(vram), 0);
    std::string cg, d1;
    if (!cfg_string(cfg, "chargen", cg, "roms/and-1z-89/AND-1Z_CharGen.bin", err)) return false;
    if (!cfg_string(cfg, "decoder", d1, "roms/and-1z-89/PROM_D1.hex.bin", err)) return false;
    if (!cfg_bool(cfg, "map_enabled", map_enabled, false, err)) return false;
    if (!cfg_bool(cfg, "wait_states", waits, true, err)) return false;
    if (!monitor.configure(cfg, err)) return false;
    if (!rom_load(cg, chargen, 4096, 0x00, 0, err)) return false;
    if (!rom_load(d1, prom_d1, 256, 0xFF, 0, err)) return false;
    return true;
}

// D1 inputs: A0=A15, A1=A11, A2=/RES (high in operation), A3=A14, A4=A13, A5=A12,
// A6=not labelled (assumed high), A7=A10. Outputs: bit0 = /SEL, bit3 = MA10.
bool And1z::select(uint16_t a, int map, unsigned& offset) const
{
    if (map_enabled && (map & MAP_1)) return false;
    unsigned idx = ((a >> 15) & 1) | (((a >> 11) & 1) << 1) | (1u << 2) | (((a >> 14) & 1) << 3) |
                   (((a >> 13) & 1) << 4) | (((a >> 12) & 1) << 5) | (1u << 6) | (((a >> 10) & 1) << 7);
    uint8_t o = prom_d1[idx];
    if (o & 0x01) return false;
    offset = (unsigned)(((o >> 3) & 1) << 10) | (a & 0x3FF);
    return true;
}

int And1z::wait_states(bool write) const
{
    if (!waits) return 0;
    return bus->window_waits(write, CHAR_NS, CPU_WINDOW_NS, write ? REQ_WRITE_NS : REQ_READ_NS, REL_NS);
}

int And1z::decode_mem(uint16_t addr, int map, const char** what)
{
    unsigned off;
    if (!select(addr, map, off)) return 0;
    *what = "VRAM";
    return DEC_READ | DEC_WRITE;
}

bool And1z::mem_read(uint16_t addr, uint8_t& data)
{
    unsigned off;
    if (!select(addr, bus->map, off)) return false;
    data = vram[off];
    bus->wait += wait_states(false);
    return true;
}

bool And1z::mem_write(uint16_t addr, uint8_t data)
{
    unsigned off;
    if (!select(addr, bus->map, off)) return false;
    vram[off] = data;
    bus->wait += wait_states(true);
    return true;
}

bool And1z::mem_peek(uint16_t addr, uint8_t& data)
{
    unsigned off;
    if (!select(addr, bus->map, off)) return false;
    data = vram[off];
    return true;
}

void And1z::render(uint32_t* argb, uint32_t bg)
{
    const uint32_t fg = monitor.argb;
    const int w = screen_width();
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            uint8_t code = vram[row * COLS + col];
            const uint8_t* glyph = &chargen[code * 16];
            for (int y = 0; y < CELL_H; y++) {
                uint8_t bits = glyph[11 - y];   // line counter counts down: L=11 is the top line
                uint32_t* p = argb + (row * CELL_H + y) * w + col * CELL_W;
                for (int x = 0; x < CELL_W; x++)
                    p[x] = (bits & (0x80 >> x)) ? fg : bg;
            }
        }
    }
}

std::vector<std::string> And1z::text_rows()
{
    std::vector<std::string> rows;
    for (int row = 0; row < ROWS; row++) {
        std::string s;
        for (int col = 0; col < COLS; col++) s += sapi_code_to_utf8(vram[row * COLS + col]);
        rows.push_back(s);
    }
    return rows;
}

void And1z::describe(json& out)
{
    out["columns"] = COLS;
    out["rows"] = ROWS;
    out["map_enabled"] = map_enabled;
    out["monitor"] = monitor.name;
}

void And1z::state(StateIO& io)
{
    io.arr(vram, sizeof(vram));
}
