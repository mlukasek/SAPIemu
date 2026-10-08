#include "cards/dmt1.h"
#include "core/bus.h"
#include "core/plant.h"
#include "core/state.h"

#include <math.h>
#include <stdio.h>

using nlohmann::json;

static const double SETTLE_TAU = 0.1;    // s: within 0.01 % after the 0.5 s of the manual
static const double INT_DELAY = 0.6;     // s: DD5 (R34 C21), 450 ... 850 ms
static const double INT_PULSE = 0.02;    // s: R35 C22, 15 ... 32 ms

bool Dmt1::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_number(cfg, "base", v, 0x80, err)) return false;
    if (v < 0 || v > 0xF0 || (v & 0x0F)) { err = "base must be 00h, 10h ... F0h (SA1-SA4)"; return false; }
    base = (uint8_t)v;
    if (!cfg_bool(cfg, "break_detect", break_detect, true, err)) return false;
    if (!cfg_bool(cfg, "int1", int_jumper, false, err)) return false;
    if (!cfg_bool(cfg, "supply_15v", supply15, false, err)) return false;
    return true;
}

void Dmt1::reset()
{
    // /RES clears nothing on the card (the 8282 has no reset input); the register keeps its value
}

int Dmt1::gain(void) const
{
    // R6-R9 with R1, then NA3 about 10 (RP3 sets one gain exactly): the values the firmware of
    // APS-5 divides by (its table at 18AFh)
    static const int g[4] = { 100, 199, 498, 1000 };
    return g[(reg >> 3) & 3];
}

double Dmt1::input_mv(void)
{
    if (reg & 0x80) return 0;   // no relay closed: the input of the amplifier is open
    double mv = bus && bus->plant ? bus->plant->tc_input_mv(reg & 7) : NAN;
    if (isnan(mv)) return break_detect ? -15.0 : 0.0;   // R3 R4: a broken loop as about -15 mV
    return mv;
}

double Dmt1::target(void)
{
    double u = input_mv() * 1e-3 * gain();
    double lim = supply15 ? 10.0 : 8.0;
    return u > lim ? lim : u < -lim ? -lim : u;
}

double Dmt1::u1(void)
{
    double t = bus ? (double)(bus->now - switched) / bus->cpu_hz : 1e9;
    double tg = target();
    return tg + (from - tg) * exp(-t / SETTLE_TAU);
}

double Dmt1::u2(void)
{
    double c = bus && bus->plant ? bus->plant->block_temp_c() : NAN;
    if (isnan(c)) return 0;    // no sensor transistor
    double u = -c / 10.0;      // 0 ... -5 V for 0 ... 50 degC
    double lim = supply15 ? 10.0 : 8.0;
    return u > lim ? lim : u < -lim ? -lim : u;
}

bool Dmt1::io_write(uint8_t port, uint8_t data)
{
    if ((port & 0xF0) != base) return false;
    double now_u1 = u1();
    if (((data ^ reg) & 0x9F) != 0) {   // relays, gain or channel: the output settles again
        from = now_u1;
        switched = bus->now;
    }
    if ((data & 0x20) && !(reg & 0x20)) int_start = bus->now ? bus->now : 1;
    reg = data;
    return true;
}

int Dmt1::irq_lines()
{
    if (!int_jumper || !int_start) return 0;
    double t = (double)(bus->now - int_start) / bus->cpu_hz;
    if (t >= INT_DELAY + INT_PULSE) {
        int_start = 0;
        return 0;
    }
    return t >= INT_DELAY ? IRQ_INT1 : 0;
}

int Dmt1::decode_io(uint8_t port, const char** what)
{
    if ((port & 0xF0) != base) return 0;
    *what = "amplifier";
    return DEC_WRITE;
}

void Dmt1::describe(json& out)
{
    char b[16];
    snprintf(b, sizeof(b), "%02Xh", base);
    out["base"] = b;
    snprintf(b, sizeof(b), "%02Xh", reg);
    out["register"] = b;
    out["channel"] = (reg & 0x80) ? json(nullptr) : json((reg & 7) + 1);
    out["gain"] = gain();
    out["u1_v"] = round(u1() * 1e4) / 1e4;
    out["u2_v"] = round(u2() * 1e4) / 1e4;
}

std::string Dmt1::live_status()
{
    char b[64];
    if (reg & 0x80) snprintf(b, sizeof(b), "relays off, U1 %.3f V", u1());
    else snprintf(b, sizeof(b), "K%d x%d, U1 %.3f V", (reg & 7) + 1, gain(), u1());
    return b;
}

void Dmt1::state(StateIO& io)
{
    io.v(reg);
    io.v(switched);
    io.v(from);
    io.v(int_start);
}
