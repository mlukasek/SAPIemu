#include "cards/dgd1.h"
#include "core/state.h"
#include "core/bus.h"

#include <stdio.h>

using nlohmann::json;

// Byte clock 1 MHz (8 MHz / 8, Johnson counter); the CPU window is assumed to
// be the first half of every period.
static const double BYTE_NS = 1000.0;
static const double CPU_WINDOW_NS = 500.0;
// DGD-1V: the CPU slot of 4 dots at 25.175 MHz / 2
static const double V_SLOT_NS = 4.0 * 2.0 * 1e9 / 25.175e6;

static const char* const k_map_modes[] = { "ignore", "h", "l", "never" };

static bool map_mode_of(const json& cfg, const char* key, int& out, std::string& err)
{
    std::string m;
    if (!cfg_string(cfg, key, m, "ignore", err)) return false;
    for (int i = 0; i < 4; i++)
        if (m == k_map_modes[i]) { out = i; return true; }
    err = std::string(key) + " must be ignore, h, l or never (jumpers MAPxEN, MAPxLOW)";
    return false;
}

bool Dgd1::set_ink(const std::string& value, std::string& err)
{
    if (!vga) return monitor.set(value, err);
    if (!vga_ink(value, ink_argb)) {
        err = "color must be white, green, amber, red, blue, cyan, yellow or magenta (jumpers S1)";
        return false;
    }
    color = value;
    return true;
}

bool Dgd1::configure(const json& cfg, std::string& err)
{
    if (vga) {
        long long b;
        if (!cfg_number(cfg, "base", b, 0xC000, err)) return false;
        if (b < 0 || b > 0xC000 || (b & 0x3FFF)) { err = "base must be 0000h, 4000h, 8000h or C000h (jumpers S2 /A15 /A14)"; return false; }
        base = (uint16_t)b;
        if (!map_mode_of(cfg, "map1", map_mode[0], err)) return false;
        if (!map_mode_of(cfg, "map2", map_mode[1], err)) return false;
        if (!cfg_bool(cfg, "wait_states", waits, true, err)) return false;
        std::string c;
        if (!cfg_string(cfg, "color", c, "white", err)) return false;
        if (!set_ink(c, err)) return false;
        ram.assign(0x4000, 0x00);
        power_on_fill(ram.data(), ram.size(), 0);
        return true;
    }
    std::string b;
    if (!cfg_string(cfg, "base", b, "C000", err)) return false;
    if (b == "C000" || b == "C000h" || b == "C000H") base = 0xC000;
    else if (b == "0000" || b == "0000h" || b == "0000H") base = 0x0000;
    else {
        err = "base must be C000 or 0000 (switch SP1-2)";
        return false;
    }
    if (!cfg_bool(cfg, "mapping", mapping, false, err)) return false;
    if (!cfg_bool(cfg, "wait_states", waits, true, err)) return false;
    if (!monitor.configure(cfg, err)) return false;
    ram.assign(0x4000, 0x00);
    power_on_fill(ram.data(), ram.size(), 0);
    return true;
}

bool Dgd1::selected(uint16_t addr, int map) const
{
    if ((addr & 0xC000) != base) return false;
    if (!vga) return !mapping || (map & MAP_1);
    for (int i = 0; i < 2; i++) {   // both conditions of S2 must hold
        bool h = (map & (i ? MAP_2 : MAP_1)) != 0;
        if ((map_mode[i] == 1 && !h) || (map_mode[i] == 2 && h) || map_mode[i] == 3) return false;
    }
    return true;
}

int Dgd1::wait_states(void) const
{
    if (!waits) return 0;
    double ns_per_cycle = 1e9 / (double)bus->cpu_hz;
    double t = (double)bus->now * ns_per_cycle;
    if (vga) {   // until the end of the current CPU slot (approximation, see the header)
        double ph = t - V_SLOT_NS * (double)(uint64_t)(t / V_SLOT_NS);
        return (int)((V_SLOT_NS - ph) / ns_per_cycle);
    }
    double phase = t - BYTE_NS * (double)(uint64_t)(t / BYTE_NS);
    if (phase < CPU_WINDOW_NS) return 0;
    return (int)((BYTE_NS - phase) / ns_per_cycle) + 1;
}

bool Dgd1::mem_read(uint16_t addr, uint8_t& data)
{
    if (!selected(addr, bus->map)) return false;
    bus->wait += wait_states();
    data = ram[addr & 0x3FFF];
    return true;
}

bool Dgd1::mem_write(uint16_t addr, uint8_t data)
{
    if (!selected(addr, bus->map)) return false;
    bus->wait += wait_states();
    ram[addr & 0x3FFF] = data;
    return true;
}

bool Dgd1::mem_peek(uint16_t addr, uint8_t& data)
{
    if (!selected(addr, bus->map)) return false;
    data = ram[addr & 0x3FFF];
    return true;
}

int Dgd1::decode_mem(uint16_t addr, int map, const char** what)
{
    if (!selected(addr, map)) return 0;
    *what = "graphics RAM";
    return DEC_READ | DEC_WRITE;
}

void Dgd1::render(uint32_t* argb, uint32_t bg)
{
    const uint32_t fg = vga ? ink_argb : monitor.argb;
    for (int y = 0; y < HEIGHT; y++) {
        const uint8_t* line = &ram[(size_t)y * LINE_BYTES];
        uint32_t* out = argb + (size_t)y * WIDTH;
        for (int x = 0; x < WIDTH; x++)
            out[x] = (line[x >> 3] & (0x80 >> (x & 7))) ? fg : bg;
    }
}

void Dgd1::describe(json& out)
{
    char b[8];
    snprintf(b, sizeof(b), "%04X", base);
    out["base"] = b;
    if (vga) {
        out["map1"] = k_map_modes[map_mode[0]];
        out["map2"] = k_map_modes[map_mode[1]];
        out["color"] = color;
    } else {
        out["mapping"] = mapping;
        out["monitor"] = monitor.name;
    }
    out["wait_states"] = waits;
}

void Dgd1::state(StateIO& io)
{
    io.mem(ram);
}
