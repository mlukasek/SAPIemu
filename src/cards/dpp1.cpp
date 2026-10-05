#include "cards/dpp1.h"
#include "cards/pmd32link.h"
#include "core/bus.h"
#include "core/state.h"

#include <stdio.h>

using nlohmann::json;

Dpp1::Dpp1() : Card("DPP-1") {}

Dpp1::~Dpp1()
{
    flush();
}

bool Dpp1::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_number(cfg, "base", v, 0x28, err)) return false;
    if (v < 0 || v > 0x38 || (v & 7)) {
        err = "base must be 00h, 08h ... 38h (switches 1S1-3S1)";
        return false;
    }
    base = (uint8_t)v;
    if (!cfg_number(cfg, "pmd_channel", v, 2, err)) return false;
    if (v != 1 && v != 2) {
        err = "pmd_channel must be 1 or 2";
        return false;
    }
    pmd_channel = (int)v - 1;
    pmd_enabled = cfg.contains("pmd32sd") && !cfg["pmd32sd"].is_null();
    if (pmd_enabled) {
        pmd.attach(&ppi[pmd_channel]);
        if (!pmd32_configure(pmd, cfg["pmd32sd"], probe, bus->now, name, err)) return false;
    }
    return true;
}

void Dpp1::reset()
{
    ppi[0].reset();
    ppi[1].reset();
    // /RES does not reach the PMD32-SD: it keeps running
}

void Dpp1::advance(uint64_t now)
{
    if (pmd_enabled) pmd.advance(now, bus->cpu_hz);
}

int Dpp1::decode_io(uint8_t port, const char** what)
{
    if ((port & 0xF8) != base) return 0;
    int ch = (port >> 2) & 1;
    if (pmd_enabled && ch == pmd_channel) *what = ch ? "DPP-1 channel 2 (8255), PMD32-SD" : "DPP-1 channel 1 (8255), PMD32-SD";
    else *what = ch ? "DPP-1 channel 2 (8255)" : "DPP-1 channel 1 (8255)";
    return DEC_READ | DEC_WRITE;
}

unsigned Dpp1::panel_leds()
{
    return accessed && bus && bus->now - access_at < bus->cpu_hz / 20 ? 1u : 0u;
}

bool Dpp1::io_read(uint8_t port, uint8_t& data)
{
    if ((port & 0xF8) != base) return false;
    data = ppi[(port >> 2) & 1].read(port & 3);
    accessed = true;
    access_at = bus->now;
    return true;
}

bool Dpp1::io_write(uint8_t port, uint8_t data)
{
    if ((port & 0xF8) != base) return false;
    int ch = (port >> 2) & 1;
    ppi[ch].write(port & 3, data);
    accessed = true;
    access_at = bus->now;
    if (pmd_enabled && ch == pmd_channel) pmd.advance(bus->now, bus->cpu_hz);   // the ATmega reacts within microseconds
    return true;
}

void Dpp1::flush()
{
    pmd.flush();
}

void Dpp1::describe(json& out)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02Xh", base);
    out["base"] = buf;
    out["pmd_channel"] = pmd_channel + 1;
    if (pmd_enabled) pmd32_describe(pmd, out);
}

void Dpp1::state(StateIO& io)
{
    io.v(access_at);
    io.v(accessed);
    ppi[0].state(io);
    ppi[1].state(io);
    io.begin("pmd32sd");
    pmd.state(io);
    io.end();
}
