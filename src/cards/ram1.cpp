#include "cards/ram1.h"
#include "core/state.h"
#include "core/ihex.h"

#include <string.h>

using nlohmann::json;

bool Ram1::configure(const json& cfg, std::string& err)
{
    long long kb;
    if (!cfg_number(cfg, "fitted_kb", kb, 48, err)) return false;
    if (kb != 16 && kb != 32 && kb != 48) { err = "fitted_kb must be 16, 32 or 48 (16K, 32K, 48K)"; return false; }
    rows = (int)(kb / 16);
    bool a = type == "RAM-1A";
    if (!cfg_bool(cfg, "s1", s1, !a, err)) return false;
    if (!cfg_bool(cfg, "s2", s2, a, err)) return false;
    const char* def = a ? "roms/ram-1a/RAM-1A_D1A_74S287.bin" : "roms/ram-1/RAM-1_D1A_74S287.bin";
    if (!cfg_string(cfg, "prom", prom_file, def, err)) return false;
    std::vector<uint8_t> prom;
    if (!rom_load(prom_file, prom, 256, 0x00, 0, err)) return false;
    if (prom.size() != 256) { err = prom_file + ": the D1A PROM (74S287) has 256 entries"; return false; }
    unsigned page = (s1 ? 0u : 0x40u) | (s2 ? 0u : 0x80u);   // a closed jumper pulls the input low
    for (int b = 0; b < 64; b++) sel[b] = prom[page | (unsigned)b] & 0x0F;
    ram.assign(0xC000, 0x00);
    power_on_fill(ram.data(), ram.size(), 0);
    return true;
}

int Ram1::cell(uint16_t a) const
{
    uint8_t o = sel[a >> 10];
    if (!(o & 1)) return -1;
    int row = (o & 2) ? 0 : (o & 8) ? 1 : (o & 4) ? 2 : -1;
    if (row < 0 || row >= rows) return -2;   // no row strobe or no chips there
    return row * 0x4000 + (a & 0x3FFF);
}

bool Ram1::mem_read(uint16_t a, uint8_t& data)
{
    int c = cell(a);
    if (c == -1) return false;
    data = c >= 0 ? ram[(size_t)c] : 0xFF;
    return true;
}

bool Ram1::mem_write(uint16_t a, uint8_t data)
{
    int c = cell(a);
    if (c == -1) return false;
    if (c >= 0) ram[(size_t)c] = data;
    return true;
}

int Ram1::decode_mem(uint16_t a, int, const char** what)
{
    int c = cell(a);
    if (c == -1) return 0;
    *what = c >= 0 ? "RAM" : "RAM (no chips)";
    return DEC_READ | DEC_WRITE;
}

void Ram1::describe(json& out)
{
    out["fitted_kb"] = rows * 16;
    out["s1"] = s1;
    out["s2"] = s2;
    out["prom"] = prom_file;
}

void Ram1::state(StateIO& io)
{
    io.mem(ram);
}
