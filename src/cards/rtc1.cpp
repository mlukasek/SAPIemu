#include "cards/rtc1.h"
#include "core/bus.h"
#include "core/log.h"
#include "core/state.h"
#include "core/util.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

using nlohmann::json;

static const uint32_t MASK = 0xFFFFF;   // 20 bits

// 2 s periods of the clock source: the PC (UTC seconds) or the emulated time
static int64_t periods(bool pc, const Bus* bus)
{
    if (pc) return (int64_t)host_time_now() / 2;
    return bus ? (int64_t)(bus->seconds() / 2.0) : 0;
}

// The local time since Monday 00:00 in 2 s: the counter after the first power on in the PC mode, so
// that a week clock (RTCZ of J. Biba) shows the PC time right away.
static uint32_t week_periods(void)
{
    time_t t = host_time_now();
    struct tm lt;
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    int s = ((lt.tm_wday + 6) % 7) * 86400 + lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
    return (uint32_t)(s / 2);
}

bool Rtc1::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_number(cfg, "base", v, 0xA0, err)) return false;
    if (v < 0 || v > 0xF0 || (v & 0x0F)) { err = "base must be 00h, 10h ... F0h (SA1-SA8, A7 jumper)"; return false; }
    base = (uint8_t)v;
    std::string s;
    if (!cfg_string(cfg, "mode", s, "async", err)) return false;
    if (s != "async" && s != "realtime") { err = "mode must be async or realtime"; return false; }
    realtime = s == "realtime";
    if (!cfg_string(cfg, "int", s, "none", err)) return false;
    if (s == "none") int_line = 0;
    else if (s == "int0") int_line = IRQ_INT0;
    else if (s == "int1") int_line = IRQ_INT1;
    else { err = "int must be none, int0 or int1"; return false; }
    if (!cfg_string(cfg, "time", s, "pc", err)) return false;
    if (s != "pc" && s != "emulated") { err = "time must be pc or emulated"; return false; }
    pc_time = s == "pc";
    if (!cfg_bool(cfg, "battery", battery, true, err)) return false;
    if (!cfg_string(cfg, "file", file, "work/rtc-1/rtc.bin", err)) return false;
    if (!cfg_string(cfg, "xc2", s, "none", err)) return false;
    if (s != "none" && s != "aps5") { err = "xc2 must be none or aps5"; return false; }
    xc2_aps5 = s == "aps5";

    // power on: the batteries kept the counter running (PC time) or holding (emulated time)
    uint32_t start = pc_time ? week_periods() : 0;
    std::vector<uint8_t> saved;
    if (battery && !file.empty() && battery_files_enabled() && !probe && file_read_all(data_path(file), saved) &&
        saved.size() == 16) {
        int64_t val = 0, when = 0;
        memcpy(&val, saved.data(), 8);
        memcpy(&when, saved.data() + 8, 8);
        start = (uint32_t)val;
        if (pc_time) start += (uint32_t)((int64_t)host_time_now() / 2 - when);
    }
    offset = (int64_t)(start & MASK) - periods(pc_time, nullptr);
    user = I8253();
    sys = I8253();
    ppi.reset();
    last = 0;
    phase = 0;
    out1_prev = true;
    return true;
}

void Rtc1::reset()
{
    // /RES resets only the 8255 (all inputs: PC6, PC7 pulled low, no load, no watchdog); the
    // 8253s have no reset pin, the counter is battery backed
    if (bus) clock_system(bus->now);
    ppi.reset();
}

uint32_t Rtc1::value(void)
{
    return (uint32_t)(offset + periods(pc_time, bus)) & MASK;
}

void Rtc1::load(uint32_t v)
{
    offset = (int64_t)(v & MASK) - periods(pc_time, bus);
}

bool Rtc1::pc_out(int bit) const
{
    uint8_t c = ppi.control();
    bool upper = bit >= 4;
    bool output = upper ? !(c & 0x08) : !(c & 0x01);
    return output && (ppi.out_c & (1 << bit));
}

uint8_t Rtc1::pins_c()
{
    uint32_t v = value();
    uint8_t c = (uint8_t)((v >> 16) & 0x07);
    if (!realtime && (v & 0x80000)) c |= 0x08;          // jumper 9-11: bit 19 on PC3
    if (!realtime || sys.out(1)) c |= 0x10;              // PC4: /STB A from OUT1 (realtime), else open
    c |= 0x20;                                           // PC5: XC2/30, open
    return c;                                            // PC6, PC7: pull-downs
}

void Rtc1::clock_system(uint64_t now)
{
    if (now <= last) { last = now; return; }
    uint64_t dt = now - last;
    last = now;
    double hz = bus ? (double)bus->phi2_hz : 2e6, cpu = bus ? (double)bus->cpu_hz : 2e6;
    double t = phase + (double)dt * hz / cpu;
    uint64_t n = (uint64_t)t;
    phase = t - (double)n;
    if (!n) return;
    uint32_t edges = sys.ticks(2, n);                    // CLK0 = CLK1 = OUT2
    if (!edges) return;
    if (xc2_aps5) {                                      // APS-5: XC2/29 (OUT2) to CLK0 and CLK1 of DD5
        user.ticks(0, edges);
        user.ticks(1, edges);
    }
    sys.ticks(0, edges);
    uint32_t falls1 = sys.ticks(1, edges);
    bool o1 = sys.out(1);
    if (realtime && o1 && (falls1 || !out1_prev) && ppi.strobed_in_a())
        ppi.strobe_a(pins_a());                          // end of /STB A: PA latched, INTR A
    out1_prev = o1;
}

void Rtc1::advance(uint64_t now)
{
    clock_system(now);
}

int Rtc1::irq_lines()
{
    if (!int_line) return 0;
    clock_system(bus->now);
    bool req = realtime ? ppi.intr_a() : sys.out(1);    // DD7: /INT low while PC3 (INTR A) / OUT1 is high
    return req ? int_line : 0;
}

bool Rtc1::rtl_line()
{
    clock_system(bus->now);
    return pc_out(7) && sys.out(0);                      // DD7: /RTL low while PC7 and OUT0 are high
}

bool Rtc1::io_read(uint8_t port, uint8_t& data)
{
    if ((port & 0xF0) != base) return false;
    int r = port & 0x0F;
    clock_system(bus->now);
    switch (r >> 2) {
    case 0:
        if (r == 0 && ppi.strobed_in_a() && (!realtime || !sys.out(1)))
            ppi.strobe_a(pins_a());                      // /STB A low (open in async mode): the latch follows
        data = r == 3 ? 0xFF : ppi.read(r);
        break;
    case 1: data = r == 7 ? 0xFF : user.read(r & 3); break;
    case 2: data = r == 11 ? 0xFF : sys.read(r & 3); break;
    default: data = 0xFF; break;                         // +12..15: nothing drives the internal bus
    }
    return true;
}

bool Rtc1::io_write(uint8_t port, uint8_t data)
{
    if ((port & 0xF0) != base) return false;
    int r = port & 0x0F;
    clock_system(bus->now);
    switch (r >> 2) {
    case 0: ppi.write(r, data); break;
    case 1: user.write(r & 3, data); break;
    case 2: sys.write(r & 3, data); break;
    default: {
        // load strobe (DD14: power OK, /IOW, PC6, CS3): the 4029 J inputs see the 8255 pins
        if (!pc_out(6)) break;
        uint8_t c = ppi.control();
        uint32_t v = value();
        bool mode0_a = !(c & 0x60), mode0_b = !(c & 0x04);
        if (mode0_a && !(c & 0x10)) v = (v & ~0xFFu) | ppi.out_a;
        if (mode0_b && !(c & 0x02)) v = (v & ~0xFF00u) | ((uint32_t)ppi.out_b << 8);
        if (mode0_b && !(c & 0x01)) v = (v & ~0x70000u) | ((uint32_t)(ppi.out_c & 0x07) << 16);
        if (realtime) v &= ~0x80000u;                    // jumper 10-11: J4 of DD13 grounded
        else if (mode0_a && !(c & 0x01)) v = (v & ~0x80000u) | ((uint32_t)((ppi.out_c >> 3) & 1) << 19);
        load(v);
        break;
    }
    }
    return true;
}

int Rtc1::decode_io(uint8_t port, const char** what)
{
    if ((port & 0xF0) != base) return 0;
    static const char* const n[4] = { "8255 (real time)", "8253 DD5 (user)", "8253 DD6 (system)", "load real time" };
    int r = port & 0x0F;
    *what = n[r >> 2];
    if (r >> 2 == 3) return DEC_WRITE;
    return DEC_READ | DEC_WRITE;
}

void Rtc1::flush()
{
    if (!battery || file.empty() || !battery_files_enabled() || probe) return;
    int64_t val = value(), when = (int64_t)host_time_now() / 2;
    uint8_t b[16];
    memcpy(b, &val, 8);
    memcpy(b + 8, &when, 8);
    std::string path = data_path(file);
    make_dirs(path_dir(path));
    if (!file_write_all(path, b, sizeof(b))) LOGW("%s: cannot save the clock to %s", name.c_str(), path.c_str());
}

void Rtc1::describe(json& out)
{
    char b[16];
    snprintf(b, sizeof(b), "%02Xh", base);
    out["base"] = b;
    uint32_t v = value();
    out["counter"] = v;
    snprintf(b, sizeof(b), "%ud %02u:%02u:%02u", (v * 2) / 86400, (v * 2) / 3600 % 24, (v * 2) / 60 % 60, (v * 2) % 60);
    out["counter_time"] = b;
    out["watchdog_armed"] = pc_out(7);
    if (xc2_aps5) {
        out["dd5_out0"] = user.out(0);
        out["dd5_out1"] = user.out(1);
    }
}

bool Rtc1::user_out(int n)
{
    if (bus) clock_system(bus->now);
    return user.out(n);
}

void Rtc1::state(StateIO& io)
{
    ppi.state(io);
    user.state(io);
    sys.state(io);
    uint32_t v = value();   // the counter as a value: a loaded state goes on from it
    io.v(v);
    if (io.loading) load(v);
    io.v(last);
    io.v(phase);
    io.v(out1_prev);
}
