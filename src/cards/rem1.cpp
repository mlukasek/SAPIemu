#include "cards/rem1.h"
#include "core/state.h"
#include "core/ihex.h"

using nlohmann::json;

static bool cfg_block(const json& cfg, const char* key, int& out, int def, std::string& err)
{
    if (!cfg.contains(key) || cfg[key].is_null()) {
        out = def;
        return true;
    }
    if (cfg[key].is_string() && cfg[key].get<std::string>() == "none") {
        out = -1;
        return true;
    }
    long long v;
    if (!cfg_number(cfg, key, v, def, err)) return false;
    if (v < 0 || v > 7) {
        err = std::string(key) + " must be 0..7 (8 KB block) or none";
        return false;
    }
    out = (int)v;
    return true;
}

bool Rem1::configure(const json& cfg, std::string& err)
{
    std::string type, rom;
    if (!cfg_string(cfg, "eprom_type", type, "2708", err)) return false;
    if (type != "2708" && type != "2716") { err = "eprom_type must be 2708 or 2716"; return false; }
    type_2716 = type == "2716";
    if (!cfg_block(cfg, "eprom_block", block_e, 0, err)) return false;
    if (!cfg_block(cfg, "eprom_block2", block_f, -1, err)) return false;
    if (!cfg_block(cfg, "ram_block", block_r, 2, err)) return false;
    long long kb;
    if (!cfg_number(cfg, "ram_kb", kb, 2, err)) return false;
    if (kb < 0 || kb > 8) { err = "ram_kb must be 0..8 (pairs of 2114)"; return false; }
    if (!cfg_string(cfg, "rom", rom, "", err)) return false;
    size_t size = type_2716 ? 0x4000 : 0x2000;
    eprom.assign(size, 0xFF);
    if (!rom.empty() && !rom_load(rom, eprom, size, 0xFF, 0, err)) return false;
    ram.assign((size_t)kb * 1024, 0x00);
    return true;
}

// Offset into `eprom` for an address, -1 if not in an EPROM block.
static int eprom_offset(uint16_t a, int block_e, int block_f, bool type_2716)
{
    int blk = a >> 13;
    if (blk == block_e) return a & 0x1FFF;
    if (type_2716 && blk == block_f) return 0x2000 + (a & 0x1FFF);
    return -1;
}

bool Rem1::mem_read(uint16_t a, uint8_t& data)
{
    int off = eprom_offset(a, block_e, block_f, type_2716);
    if (off >= 0) {
        data = eprom[(size_t)off];
        return true;
    }
    if ((a >> 13) == block_r) {
        size_t o = a & 0x1FFF;
        data = o < ram.size() ? ram[o] : 0xFF;
        return true;
    }
    return false;
}

bool Rem1::mem_write(uint16_t a, uint8_t data)
{
    if (eprom_offset(a, block_e, block_f, type_2716) >= 0) return true;   // answered, ignored
    if ((a >> 13) == block_r) {
        size_t o = a & 0x1FFF;
        if (o < ram.size()) ram[o] = data;
        return true;
    }
    return false;
}

int Rem1::decode_mem(uint16_t a, int, const char** what)
{
    if (eprom_offset(a, block_e, block_f, type_2716) >= 0) {
        *what = "EPROM";
        return DEC_READ | DEC_WRITE;
    }
    if ((a >> 13) == block_r) {
        *what = (size_t)(a & 0x1FFF) < ram.size() ? "RAM" : "RAM (empty sockets)";
        return DEC_READ | DEC_WRITE;
    }
    return 0;
}

void Rem1::describe(json& out)
{
    out["eprom_type"] = type_2716 ? "2716" : "2708";
    out["eprom_block"] = block_e;
    out["ram_block"] = block_r;
    out["ram_kb"] = ram.size() / 1024;
}

void Rem1::state(StateIO& io)
{
    io.mem(ram);
}
