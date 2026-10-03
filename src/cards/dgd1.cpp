#include "cards/dgd1.h"
#include "core/bus.h"

#include <stdio.h>

using nlohmann::json;

// Byte clock 1 MHz (8 MHz / 8, Johnson counter); the CPU window is assumed to
// be the first half of every period.
static const double BYTE_NS = 1000.0;
static const double CPU_WINDOW_NS = 500.0;

bool Dgd1::configure(const json& cfg, std::string& err)
{
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
    ram.assign(0x4000, 0x00);
    return true;
}

bool Dgd1::selected(uint16_t addr, int map) const
{
    if ((addr & 0xC000) != base) return false;
    return !mapping || (map & MAP_1);
}

int Dgd1::wait_states(void) const
{
    if (!waits) return 0;
    double ns_per_cycle = 1e9 / (double)bus->cpu_hz;
    double t = (double)bus->now * ns_per_cycle;
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

void Dgd1::render(uint32_t* argb, uint32_t fg, uint32_t bg)
{
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
    out["mapping"] = mapping;
    out["wait_states"] = waits;
}
