#include "cards/adp1.h"
#include "cards/dmt1.h"
#include "core/bus.h"
#include "core/plant.h"
#include "core/state.h"

#include <math.h>
#include <stdio.h>

using nlohmann::json;

bool Adp1::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_number(cfg, "base", v, 0x30, err)) return false;   // the factory setting is not known
    if (v < 0 || v > 0xF0 || (v & 0x0F)) { err = "base must be 00h, 10h ... F0h (SA1-SA4)"; return false; }
    base = (uint8_t)v;
    std::string w;
    if (!cfg_string(cfg, "wiring", w, "none", err)) return false;
    if (w != "none" && w != "aps5") { err = "wiring must be none or aps5"; return false; }
    aps5 = w == "aps5";
    if (!cfg_number(cfg, "ref_mv", v, 9990, err)) return false;
    if (v < 9000 || v > 11000) { err = "ref_mv must be 9000-11000"; return false; }
    ref_v = (double)v / 1000.0;
    if (!cfg_number(cfg, "conversion_ms", v, 20, err)) return false;
    if (v < 1 || v > 1000) { err = "conversion_ms must be 1-1000"; return false; }
    conv_s = (double)v / 1000.0;
    return true;
}

void Adp1::reset()
{
    // /RES clears the registers 74173 and the READY flip-flop (assumption, docs/desky/ADP-1.md)
    hi = lo = 0;
    busy = false;
    ctrl = 0;
}

Dmt1* Adp1::dmt(void)
{
    if (!bus) return nullptr;
    for (Card* c : bus->cards)
        if (Dmt1* d = dynamic_cast<Dmt1*>(c)) return d;
    return nullptr;
}

double Adp1::input_v(int ch)
{
    if (!aps5) return 0;   // nothing connected (an open input of MAB 16: taken as 0 V)
    switch (ch) {
    case 0: { Dmt1* d = dmt(); return d ? d->u1() : 0; }
    case 1: { Dmt1* d = dmt(); return d ? d->u2() : 0; }
    case 4: {
        double b = bus && bus->plant ? bus->plant->battery_v() : NAN;
        return isnan(b) ? 0 : b;
    }
    case 6: return ref_v;   // S7: KAL (S6 = 0 V: the firmware takes S6 as zero, S7 as the reference)
    default: return 0;
    }
}

void Adp1::convert(void)
{
    double u = input_v(ctrl & 0x0F) * ((ctrl & 0x10) ? 10.0 : 1.0);
    long n = lround(fabs(u) * 100.0);   // 10 mV a digit at gain 1
    if (n > 999) {                      // over the range: BBB
        hi = 0x0B;
        lo = 0xBB;
    } else {
        hi = (uint8_t)(n / 100);
        lo = (uint8_t)(((n / 10) % 10) << 4 | (n % 10));
    }
    busy = false;
}

bool Adp1::io_read(uint8_t port, uint8_t& data)
{
    if ((port & 0xF0) != base || (port & 0x0F) > 1) return false;
    if (busy && (double)(bus->now - started) >= conv_s * bus->cpu_hz) convert();
    if (port & 1) {
        bool minus = input_v(ctrl & 0x0F) < 0;   // NA1 follows the input now
        data = (uint8_t)((busy ? 0 : 0x80) | (minus ? 0x10 : 0) | (hi & 0x0F));
    } else {
        data = lo;
    }
    return true;
}

bool Adp1::io_write(uint8_t port, uint8_t data)
{
    if ((port & 0xF0) != base || (port & 0x0F) != 1) return false;
    ctrl = data & 0x1F;
    busy = true;
    started = bus->now;
    return true;
}

int Adp1::decode_io(uint8_t port, const char** what)
{
    if ((port & 0xF0) != base) return 0;
    switch (port & 0x0F) {
    case 0: *what = "A/D data"; return DEC_READ;
    case 1: *what = "A/D control, status"; return DEC_READ | DEC_WRITE;
    default: return 0;
    }
}

void Adp1::describe(json& out)
{
    char b[16];
    snprintf(b, sizeof(b), "%02Xh", base);
    out["base"] = b;
    out["wiring"] = aps5 ? "aps5" : "none";
    out["channel"] = (ctrl & 0x0F) + 1;
    out["gain"] = (ctrl & 0x10) ? 10 : 1;
    out["ready"] = !busy;
    snprintf(b, sizeof(b), "%X%02X", hi & 0x0F, lo);
    out["digits"] = b;
}

std::string Adp1::live_status()
{
    char b[64];
    snprintf(b, sizeof(b), "S%d x%d: %X%02X", (ctrl & 0x0F) + 1, (ctrl & 0x10) ? 10 : 1, hi & 0x0F, lo);
    return b;
}

void Adp1::state(StateIO& io)
{
    io.v(ctrl);
    io.v(started);
    io.v(busy);
    io.v(hi);
    io.v(lo);
}
