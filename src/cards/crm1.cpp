#include "cards/crm1.h"
#include "core/bus.h"
#include "core/log.h"
#include "core/state.h"
#include "core/util.h"

#include <math.h>
#include <stdio.h>

using nlohmann::json;

bool Crm1::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_number(cfg, "base", v, 0xE000, err)) return false;
    if (v < 0 || v > 0xE000 || (v & 0x1FFF)) { err = "base must be 0000h, 2000h ... E000h (jumpers 1-2 .. 1-9)"; return false; }
    base = (uint16_t)v;
    static const char* const keys[4] = { "kb1", "kb2", "kb3", "kb4" };
    static const int defs[4] = { 0x0000, 0x0400, 0x0800, 0x0C00 };
    for (int i = 0; i < 4; i++) {
        offset[i] = defs[i];
        if (!cfg.contains(keys[i])) continue;
        const json& j = cfg[keys[i]];
        if (j.is_string() && j.get<std::string>() == "none") { offset[i] = -1; continue; }
        if (!cfg_number(cfg, keys[i], v, defs[i], err)) return false;
        if (v < 0 || v > 0x1C00 || (v & 0x3FF)) {
            err = std::string(keys[i]) + " must be 0000h, 0400h ... 1C00h or none (jumpers 10..13 - 14..21)";
            return false;
        }
        offset[i] = (int)v;
    }
    if (!cfg_bool(cfg, "write_protect", protect, false, err)) return false;
    if (!cfg_bool(cfg, "battery", battery, true, err)) return false;
    if (!cfg_string(cfg, "file", file, "work/crm-1/crm.bin", err)) return false;
    if (!cfg_number(cfg, "rdy_ns", v, 590, err)) return false;
    if (v < 0 || v > 10000) { err = "rdy_ns must be 0-10000"; return false; }
    rdy_ns = (int)v;
    ram.assign(4096, 0x00);
    power_on_fill(ram.data(), ram.size(), 0);
    if (battery && !file.empty() && battery_files_enabled() && !probe) {
        std::vector<uint8_t> saved;   // the batteries kept it while the computer was off
        if (file_read_all(data_path(file), saved) && saved.size() == ram.size()) ram = saved;
    }
    return true;
}

int Crm1::chip(uint16_t addr) const
{
    if ((addr & 0xE000) != base) return -1;
    int part = addr & 0x1C00;
    for (int i = 0; i < 4; i++)
        if (offset[i] == part) return i;
    return -1;
}

// RDY low for rdy_ns from the start of the access; the CPU samples it once per clock in the middle
// of T2 and of each wait state: wait states = samples before the end of the pulse.
int Crm1::wait_states(void) const
{
    if (!rdy_ns || !bus) return 0;
    double t = 1e9 / bus->cpu_hz;
    int n = (int)ceil((rdy_ns - t / 2) / t);
    return n > 0 ? n : 0;
}

bool Crm1::mem_read(uint16_t addr, uint8_t& data)
{
    int c = chip(addr);
    if (c < 0) return false;
    data = ram[(size_t)c * 1024 + (addr & 0x3FF)];
    bus->wait += wait_states();
    return true;
}

bool Crm1::mem_peek(uint16_t addr, uint8_t& data)
{
    int c = chip(addr);
    if (c < 0) return false;
    data = ram[(size_t)c * 1024 + (addr & 0x3FF)];
    return true;
}

bool Crm1::mem_write(uint16_t addr, uint8_t data)
{
    int c = chip(addr);
    if (c < 0) return false;
    if (!(protect && c == 0)) ram[(size_t)c * 1024 + (addr & 0x3FF)] = data;
    bus->wait += wait_states();
    return true;
}

void Crm1::flush()
{
    if (!battery || file.empty() || !battery_files_enabled() || probe) return;
    std::string path = data_path(file);
    make_dirs(path_dir(path));
    if (!file_write_all(path, ram.data(), ram.size())) LOGW("%s: cannot save the RAM to %s", name.c_str(), path.c_str());
}

int Crm1::decode_mem(uint16_t addr, int, const char** what)
{
    int c = chip(addr);
    if (c < 0) return 0;
    static const char* const n[4] = { "RAM CS1", "RAM CS2", "RAM CS3", "RAM CS4" };
    *what = n[c];
    return (protect && c == 0) ? DEC_READ : DEC_READ | DEC_WRITE;
}

void Crm1::describe(json& out)
{
    char b[16];
    snprintf(b, sizeof(b), "%04Xh", base);
    out["base"] = b;
    json kb = json::array();
    for (int i = 0; i < 4; i++) {
        if (offset[i] < 0) kb.push_back(nullptr);
        else {
            snprintf(b, sizeof(b), "%04Xh", base + offset[i]);
            kb.push_back(b);
        }
    }
    out["kilobytes"] = kb;
    out["write_protect_cs1"] = protect;
    out["wait_states"] = wait_states();
}

void Crm1::state(StateIO& io)
{
    io.v(ram);
}
