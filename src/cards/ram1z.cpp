#include "cards/ram1z.h"
#include "core/state.h"
#include "core/bus.h"
#include "core/ihex.h"

#include <string.h>

using nlohmann::json;

bool Ram1z::configure(const json& cfg, std::string& err)
{
    long long kb;
    if (!cfg_number(cfg, "fitted_kb", kb, 64, err)) return false;
    if (kb != 16 && kb != 32 && kb != 48 && kb != 64) { err = "fitted_kb must be 16, 32, 48 or 64"; return false; }
    rows = (int)(kb / 16);
    if (!cfg_bool(cfg, "s1_1", s1_1, false, err)) return false;
    if (!cfg_bool(cfg, "s1_2", s1_2, true, err)) return false;
    if (!cfg_bool(cfg, "map", map1, false, err)) return false;
    if (!cfg_string(cfg, "prom", prom_file, "roms/ram-1z/RAM-1Z_D3B_74S571.bin", err)) return false;
    std::vector<uint8_t> p;
    if (!rom_load(prom_file, p, 512, 0xFF, 0, err)) return false;
    if (p.size() != 512) { err = prom_file + ": the D3B PROM (74S571) has 512 entries"; return false; }
    memcpy(prom, p.data(), 512);
    ram.assign(0x10000, 0x00);
    power_on_fill(ram.data(), ram.size(), 0);
    return true;
}

int Ram1z::cell(uint16_t a, int map) const
{
    unsigned i = (unsigned)(a >> 10) | (s1_1 ? 0u : 0x40u) | (s1_2 ? 0u : 0x80u) |
                 ((map1 && (map & MAP_1)) ? 0x100u : 0u);
    uint8_t o = prom[i] & 0x0F;
    int row = !(o & 8) ? 0 : !(o & 4) ? 1 : !(o & 2) ? 2 : !(o & 1) ? 3 : -1;
    if (row < 0) return -1;
    if (row >= rows) return -2;
    return row * 0x4000 + (a & 0x3FFF);
}

bool Ram1z::mem_read(uint16_t a, uint8_t& data)
{
    int c = cell(a, bus->map);
    if (c == -1) return false;
    data = c >= 0 ? ram[(size_t)c] : 0xFF;
    return true;
}

bool Ram1z::mem_write(uint16_t a, uint8_t data)
{
    int c = cell(a, bus->map);
    if (c == -1) return false;
    if (c >= 0) ram[(size_t)c] = data;
    return true;
}

bool Ram1z::mem_peek(uint16_t a, uint8_t& data)
{
    return mem_read(a, data);
}

int Ram1z::decode_mem(uint16_t a, int map, const char** what)
{
    int c = cell(a, map);
    if (c == -1) return 0;
    *what = c >= 0 ? "RAM" : "RAM (no chips)";
    return DEC_READ | DEC_WRITE;
}

void Ram1z::describe(json& out)
{
    out["fitted_kb"] = rows * 16;
    out["s1_1"] = s1_1;
    out["s1_2"] = s1_2;
    out["map"] = map1;
    out["prom"] = prom_file;
}

void Ram1z::state(StateIO& io)
{
    io.mem(ram);
}
