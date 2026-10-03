#include "cards/ram1.h"

#include <string.h>

using nlohmann::json;

bool Ram1::configure(const json& cfg, std::string& err)
{
    long long kb;
    if (!cfg_number(cfg, "fitted_kb", kb, 48, err)) return false;
    if (kb != 16 && kb != 32 && kb != 48) { err = "fitted_kb must be 16, 32 or 48 (RAM-1/16K, /32K, /48K)"; return false; }
    rows = (int)(kb / 16);
    if (!cfg_bool(cfg, "s1", s1, true, err)) return false;
    if (!cfg_bool(cfg, "s2", s2, false, err)) return false;
    // factory PROM D1A: the end of the RAM by S1, S2 (table 1 of the manual)
    unsigned end = s1 && !s2 ? 0x10000 : !s1 && s2 ? 0xC000 : s1 && s2 ? 0x8000 : 0xE000;
    for (unsigned b = 0; b < 64; b++) {
        unsigned a = b << 10;
        prom[b] = (a >= 0x4000 && a < end) ? 1 : 0;
    }
    ram.assign(0xC000, 0x00);
    return true;
}

bool Ram1::mem_read(uint16_t a, uint8_t& data)
{
    if (!selected(a)) return false;
    // the row is chosen by A15-A14; a row without chips reads nothing (FFh)
    int row = (a >> 14) - 1;
    data = row < rows ? ram[a - 0x4000] : 0xFF;
    return true;
}

bool Ram1::mem_write(uint16_t a, uint8_t data)
{
    if (!selected(a)) return false;
    int row = (a >> 14) - 1;
    if (row < rows) ram[a - 0x4000] = data;
    return true;
}

int Ram1::decode_mem(uint16_t a, int, const char** what)
{
    if (!selected(a)) return 0;
    *what = ((a >> 14) - 1) < rows ? "RAM" : "RAM (no chips)";
    return DEC_READ | DEC_WRITE;
}

void Ram1::describe(json& out)
{
    out["fitted_kb"] = rows * 16;
    out["s1"] = s1;
    out["s2"] = s2;
}
