#include "cards/dpp1.h"
#include "cards/pmd32link.h"
#include "core/bus.h"
#include "core/state.h"
#include "core/util.h"

#include <math.h>
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
    if (!cfg_number(cfg, "printer_channel", v, 1, err)) return false;
    if (v != 1 && v != 2) {
        err = "printer_channel must be 1 or 2";
        return false;
    }
    prn_channel = (int)v - 1;
    if (!cfg_number(cfg, "kzd1_channel", v, 1, err)) return false;
    if (v != 1 && v != 2) {
        err = "kzd1_channel must be 1 or 2";
        return false;
    }
    kzd_channel = (int)v - 1;
    kzd_enabled = cfg.contains("kzd1") && !cfg["kzd1"].is_null();
    if (!cfg_number(cfg, "minigraf_channel", v, 1, err)) return false;
    if (v != 1 && v != 2) {
        err = "minigraf_channel must be 1 or 2";
        return false;
    }
    mg_channel = (int)v - 1;
    mg_enabled = cfg.contains("minigraf") && !cfg["minigraf"].is_null() && cfg["minigraf"] != false;
    pmd_enabled = cfg.contains("pmd32sd") && !cfg["pmd32sd"].is_null();
    prn_enabled = cfg.contains("printer") && !cfg["printer"].is_null();
    if (pmd_enabled && prn_enabled && pmd_channel == prn_channel) {
        err = "the PMD32-SD and the printer cannot share a channel";
        return false;
    }
    if (kzd_enabled && ((pmd_enabled && pmd_channel == kzd_channel) || (prn_enabled && prn_channel == kzd_channel))) {
        err = "the KZD-1 needs its own channel";
        return false;
    }
    if (mg_enabled && ((pmd_enabled && pmd_channel == mg_channel) || (kzd_enabled && kzd_channel == mg_channel))) {
        err = "the Minigraf needs a channel without the PMD32-SD and the KZD-1";
        return false;
    }
    if (prn_enabled && !prn.configure(cfg["printer"], err)) return false;
    if (kzd_enabled) {
        kzd.bus = bus;
        kzd.ppi = &ppi[kzd_channel];
        ppi[kzd_channel].dev = &kzd;
        std::string cas;
        if (!cfg_string(cfg["kzd1"], "cassette", cas, "work/kzd-1/kazeta.kzd", err)) return false;
        if (!probe && !cas.empty() && !kzd.insert(data_path(cas), err)) return false;
    }
    if (mg_enabled) {
        mg.bus = bus;
        mg.ppi = &ppi[mg_channel];
        ppi[mg_channel].dev = &mg;
        mg.power_on();
        mg.lines_now();
    }
    if (pmd_enabled) {
        pmd.attach(&ppi[pmd_channel]);
        pmd.cpm_drive = &bus->cpm_drive;
        if (!pmd32_configure(pmd, cfg["pmd32sd"], probe, bus->now, name, err)) return false;
    }
    return true;
}

void Dpp1::reset()
{
    ppi[0].reset();
    ppi[1].reset();
    if (mg_enabled) mg.lines_now();   // port B is an input: the lines go high
    for (char& c : pmd.cpm_seen) c = 0;   // another system may come
    prn_wait_ack = false;   // /OBF is high after the reset; a byte being printed is acknowledged to nobody
    // /RES does not reach the PMD32-SD: it keeps running
}

void Dpp1::advance(uint64_t now)
{
    if (kzd_enabled) kzd.advance(now);
    if (pmd_enabled) pmd.advance(now, bus->cpu_hz);
    if (prn_enabled) printer_service(now);
}

// mode 1 output: the byte goes to the printer while /OBF is low, its /ACK sets /OBF high again
void Dpp1::printer_service(uint64_t now)
{
    I8255& p = ppi[prn_channel];
    if (prn_wait_ack) {
        if (!prn.ack_due(now)) return;
        prn_wait_ack = false;
        if (p.mode1_a() && p.a_output_full()) p.ack_a();
    }
    if (p.mode1_a() && p.strobed_out_a() && p.a_output_full()) {
        prn.take(p.out_a, now, bus->cpu_hz);
        prn_wait_ack = true;
    }
}

int Dpp1::decode_io(uint8_t port, const char** what)
{
    if ((port & 0xF8) != base) return 0;
    int ch = (port >> 2) & 1;
    if (pmd_enabled && ch == pmd_channel) *what = ch ? "DPP-1 channel 2 (8255), PMD32-SD" : "DPP-1 channel 1 (8255), PMD32-SD";
    else if (kzd_enabled && ch == kzd_channel) *what = ch ? "DPP-1 channel 2 (8255), KZD-1" : "DPP-1 channel 1 (8255), KZD-1";
    else if (mg_enabled && ch == mg_channel) *what = ch ? "DPP-1 channel 2 (8255), Minigraf" : "DPP-1 channel 1 (8255), Minigraf";
    else if (prn_enabled && ch == prn_channel) *what = ch ? "DPP-1 channel 2 (8255), printer" : "DPP-1 channel 1 (8255), printer";
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
    if (prn_enabled) printer_service(bus->now);
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
    if (prn_enabled && ch == prn_channel) {
        if (!ppi[ch].a_output_full()) prn_wait_ack = false;   // a new mode cleared /OBF
        printer_service(bus->now);
    }
    return true;
}

void Dpp1::flush()
{
    if (kzd_enabled) kzd.flush();
    pmd.flush();
    prn.flush();
}

void Dpp1::describe(json& out)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02Xh", base);
    out["base"] = buf;
    out["pmd_channel"] = pmd_channel + 1;
    if (pmd_enabled) pmd32_describe(pmd, out);
    if (prn_enabled)
        out["printer"] = json{ { "channel", prn_channel + 1 }, { "output", prn.path() }, { "bytes", prn.bytes } };
    if (kzd_enabled)
        out["kzd1"] = json{ { "channel", kzd_channel + 1 }, { "cassette", kzd.loaded() ? kzd.path() : std::string() },
                            { "side", kzd.side ? "B" : "A" }, { "position_m", round(kzd.position_m() * 100) / 100 },
                            { "motion", kzd.motion() } };
    if (mg_enabled)
        out["minigraf"] = json{ { "channel", mg_channel + 1 }, { "x_mm", mg.x() / (double)Minigraf::STEPS_PER_MM },
                                { "y_mm", mg.y() / (double)Minigraf::STEPS_PER_MM }, { "pen", mg.pen_down() ? "down" : "up" },
                                { "strokes", mg.strokes.size() }, { "steps", mg.steps }, { "lost_steps", mg.lost }, { "steps_at_stop", mg.stopped } };
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
    io.v(prn_wait_ack);
    prn.state(io);
    if (kzd_enabled) {
        io.begin("kzd1");
        kzd.state(io);
        io.end();
    }
    if (mg_enabled) {
        io.begin("minigraf");
        mg.state(io);
        io.end();
    }
}
