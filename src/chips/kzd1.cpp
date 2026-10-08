#include "chips/kzd1.h"
#include "core/bus.h"
#include "core/log.h"
#include "core/state.h"
#include "core/util.h"

#include <math.h>
#include <string.h>
#include <algorithm>

static const double ACCEL = 9.6;         // m/s2: 48 cm/s in 50 ms, 96 cm/s in 100 ms
static const double PULSE_S = 40e-6;     // 74121: 39k x 1.5n
static const double OVERLOAD_S = 0.03;   // the current comparator integrates "a few hundredths of a second"
static const char MAGIC[8] = { 'K', 'Z', 'D', '1', 'T', 'A', 'P', 'E' };

// ---------------------------------------------------------------- the cassette file
// "KZD1TAPE", u32 version 1, u32 length (um), u8 tab A, u8 tab B, u16 0, u32 count A, u32 count B,
// then the transitions of side A and of side B (u32 position << 1 | track), little endian.

static void put32(std::vector<uint8_t>& d, uint32_t v)
{
    for (int i = 0; i < 4; i++) d.push_back((uint8_t)(v >> (8 * i)));
}

static uint32_t get32(const uint8_t* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool KzdCassette::save(const std::string& path, std::string& err) const
{
    std::vector<uint8_t> d(MAGIC, MAGIC + 8);
    put32(d, 1);
    put32(d, (uint32_t)LENGTH_UM);
    d.push_back(tab[0] ? 1 : 0);
    d.push_back(tab[1] ? 1 : 0);
    d.push_back(0);
    d.push_back(0);
    put32(d, (uint32_t)tr[0].size());
    put32(d, (uint32_t)tr[1].size());
    for (int s = 0; s < 2; s++)
        for (uint32_t v : tr[s]) put32(d, v);
    make_dirs(path_dir(path));
    if (!file_write_all(path, d.data(), d.size())) {
        err = "cannot write " + path;
        return false;
    }
    return true;
}

bool KzdCassette::load(const std::string& path, std::string& err)
{
    std::vector<uint8_t> d;
    if (!file_read_all(path, d)) {
        err = "cannot read " + path;
        return false;
    }
    if (d.size() < 28 || memcmp(d.data(), MAGIC, 8) != 0 || get32(&d[8]) != 1) {
        err = path + ": not a KZD-1 cassette (.kzd)";
        return false;
    }
    size_t n0 = get32(&d[20]), n1 = get32(&d[24]);
    if (d.size() != 28 + 4 * (n0 + n1)) {
        err = path + ": damaged cassette file";
        return false;
    }
    tab[0] = d[16] != 0;
    tab[1] = d[17] != 0;
    const uint8_t* p = d.data() + 28;   // (an empty cassette ends here)
    for (int s = 0; s < 2; s++) {
        size_t n = s ? n1 : n0;
        tr[s].resize(n);
        for (size_t i = 0; i < n; i++, p += 4) tr[s][i] = get32(p);
        std::sort(tr[s].begin(), tr[s].end());
    }
    return true;
}

// ---------------------------------------------------------------- the recorder

double Kzd1::now_s(void) const
{
    return bus ? (double)bus->now / bus->cpu_hz : 0.0;
}

bool Kzd1::insert(const std::string& p, std::string& err)
{
    eject();
    KzdCassette c;
    if (file_exists(p)) {
        if (!c.load(p, err)) return false;
    } else if (battery_files_enabled() && !c.save(p, err)) {   // a new, empty cassette (only in memory for tests)
        return false;
    }
    cas = c;
    file = p;
    has_cassette = true;
    dirty = false;
    side = 0;
    pos_um = KzdCassette::LEADER_UM;   // a cassette goes in wound to the start of side A
    return true;
}

void Kzd1::flush(void)
{
    commit();
    if (!has_cassette || !dirty || !battery_files_enabled()) return;
    std::string err;
    if (cas.save(file, err)) dirty = false;
    else LOGW("KZD-1: %s", err.c_str());
}

void Kzd1::eject(void)
{
    if (!has_cassette) return;
    flush();
    has_cassette = false;
    cas = KzdCassette();
    file.clear();
    stall_since = -1;
}

void Kzd1::flip(void)
{
    if (!has_cassette) return;
    move(now_s());
    commit();
    side ^= 1;
    pos_um = KzdCassette::LENGTH_UM - pos_um;
}

void Kzd1::set_tab(int s, bool present)
{
    if (!has_cassette) return;
    cas.tab[s & 1] = present;
    dirty = true;
}

bool Kzd1::overload(int motor) const
{
    if (stall_since < 0 || motor != stall_motor) return false;
    return now_s() - stall_since >= OVERLOAD_S;
}

const char* Kzd1::motion(void) const
{
    double t = v_target;
    if (t == 0) return v != 0 ? "stopping" : "stop";
    double a = fabs(t);
    if (t > 0) return a > 1.0 ? "rewind forward" : a > 0.5 ? "search forward" : recording() ? "recording" : "forward";
    return a > 1.0 ? "rewind backward" : a > 0.5 ? "search backward" : "backward";
}

void Kzd1::move(double t)
{
    double dt = t - t_last;
    if (dt <= 0) return;
    double t0 = t_last;
    t_last = t;
    const double lo = 0, hi = KzdCassette::LENGTH_UM;
    while (dt > 0) {
        double s = dt, nv = v_target;
        double x0 = pos_um, x1;
        if (v != v_target) {
            double sign = v_target > v ? 1 : -1;
            double tr = fabs(v_target - v) / ACCEL;
            if (tr < s) s = tr;
            else nv = v + sign * ACCEL * s;
            x1 = x0 + (v * s + 0.5 * sign * ACCEL * s * s) * 1e6;
        } else {
            x1 = x0 + v * s * 1e6;
        }
        v = nv;
        if (!has_cassette) x1 = x0;   // no tape: the reels turn freely
        bool stalled = false;
        if (x1 < lo) { x1 = lo; stalled = true; }
        if (x1 > hi) { x1 = hi; stalled = true; }
        if (stalled) v = 0;
        // the head passes the transitions between x0 and x1
        if (x1 != x0) {
            if (recording()) {
                if (rec_active) {
                    rec_lo = std::min(rec_lo, std::min(x0, x1));
                    rec_hi = std::max(rec_hi, std::max(x0, x1));
                }
            } else {
                const std::vector<uint32_t>& tv = cas.tr[side];
                double a = std::min(x0, x1), b = std::max(x0, x1);
                auto first = std::lower_bound(tv.begin(), tv.end(), (uint32_t)ceil(a) << 1);
                auto last = std::upper_bound(tv.begin(), tv.end(), ((uint32_t)floor(b) << 1) | 1u);
                if (first < last) {
                    auto visit = [&](uint32_t e) {
                        double tc = t0 + s * (((double)(e >> 1) - x0) / (x1 - x0));
                        int k = e & 1;
                        if (pulse[k] < 0 || tc >= pulse[k] + PULSE_S) pulse[k] = tc;   // not retriggerable
                    };
                    if (x1 > x0)
                        for (auto it = first; it != last; ++it) visit(*it);
                    else
                        for (auto it = last; it != first;) visit(*--it);
                }
            }
        }
        pos_um = x1;
        // the end of the tape: the pulling motor stalls (forward motor 2, backward motor 1)
        bool at_end = has_cassette && ((x1 <= lo && v_target < 0) || (x1 >= hi && v_target > 0));
        if (at_end) {
            if (stall_since < 0) {
                stall_since = t0 + s;
                stall_motor = v_target > 0 ? 2 : 1;
            }
        } else {
            stall_since = -1;
        }
        t0 += s;
        dt -= s;
    }
}

void Kzd1::commit(void)
{
    if (rec_new.empty() && !(rec_active && rec_hi > rec_lo)) return;
    std::vector<uint32_t>& tv = cas.tr[side];
    double a = rec_lo, b = rec_hi;
    for (uint32_t e : rec_new) {
        a = std::min(a, (double)(e >> 1));
        b = std::max(b, (double)(e >> 1));
    }
    std::sort(rec_new.begin(), rec_new.end());
    auto first = std::lower_bound(tv.begin(), tv.end(), (uint32_t)ceil(a) << 1);
    auto last = std::upper_bound(tv.begin(), tv.end(), ((uint32_t)floor(b) << 1) | 1u);
    std::vector<uint32_t> out;
    out.reserve(tv.size() - (size_t)(last - first) + rec_new.size());
    out.insert(out.end(), tv.begin(), first);
    out.insert(out.end(), rec_new.begin(), rec_new.end());
    out.insert(out.end(), last, tv.end());
    tv.swap(out);
    rec_new.clear();
    rec_lo = rec_hi = pos_um;
    dirty = true;
}

void Kzd1::set_controls(uint8_t value)
{
    move(now_s());
    uint8_t old = pa;
    pa = value;
    bool rec_old = !(old & 0x10), rec = !(value & 0x10);
    if (rec && !rec_old && has_cassette) {   // record (erase) head on
        rec_active = true;
        rec_lo = rec_hi = pos_um;
    }
    if ((old & 0x20) && !(value & 0x20) && rec && has_cassette) {   // D.P.* falling: one transition
        double x = floor(pos_um + 0.5);
        if (x >= KzdCassette::LEADER_UM && x <= KzdCassette::LENGTH_UM - KzdCassette::LEADER_UM)
            rec_new.push_back((uint32_t)x << 1 | ((value & 0x80) ? 1u : 0u));
    }
    if (!rec && rec_old) {
        commit();
        rec_active = false;
    }
    bool f = !(value & 1), ff = !(value & 2), b = !(value & 4), fb = !(value & 8);
    bool fwd = f || ff, bwd = b || fb;
    double sp = 0;
    if (fwd && !bwd) sp = (f && ff) ? 1.44 : ff ? 0.96 : 0.48;
    if (bwd && !fwd) sp = -((b && fb) ? 1.44 : fb ? 0.96 : 0.48);
    if (sp != v_target) {
        if (rec_active) commit();   // the recorded run ends where the motion changes
        v_target = sp;
    }
}

void Kzd1::controls_now(void)
{
    uint8_t value = 0xFF;   // port A as input: the lines float high (all off)
    if (ppi && !(ppi->control() & 0x10)) value = ppi->out_a;
    if (value != pa) set_controls(value);
}

void Kzd1::outputs_changed(int port, uint8_t)
{
    if (port == 0) controls_now();
}

void Kzd1::advance(uint64_t)
{
    controls_now();
    move(now_s());
}

uint8_t Kzd1::pins_b()
{
    controls_now();
    double t = now_s();
    move(t);
    uint8_t v8 = 0x07;   // PB0-PB2 not connected
    if (!recording()) {  // while recording the read amplifiers give nothing (assumption)
        if (pulse[1] >= 0 && t - pulse[1] < PULSE_S) v8 |= 0x80;
        if (pulse[0] >= 0 && t - pulse[0] < PULSE_S) v8 |= 0x40;
    }
    if (overload(1)) v8 |= 0x20;
    if (overload(2)) v8 |= 0x10;
    // ZAZN. U.V.*: the tab switch connects ZAZNAM* back; without the tab (or a cassette) the line floats H
    bool echo = has_cassette && cas.tab[side];
    if (!echo || (pa & 0x10)) v8 |= 0x08;
    return v8;
}

void Kzd1::state(StateIO& io)
{
    if (!io.loading) flush();   // the cassette file matches the state
    std::string p = has_cassette ? file : std::string();
    io.media("KZD-1 cassette", p);
    io.v(p);
    if (io.loading && io.ok() && p != (has_cassette ? file : std::string())) {   // another cassette was inserted since
        std::string err;
        eject();
        if (!p.empty() && !insert(p, err)) LOGW("state: KZD-1 cassette %s: %s", p.c_str(), err.c_str());
    }
    io.v(side);
    io.v(pa);
    io.v(t_last);
    io.v(pos_um);
    io.v(v);
    io.v(v_target);
    io.v(stall_since);
    io.v(stall_motor);
    io.v(pulse[0]);
    io.v(pulse[1]);
    io.v(rec_active);
    io.v(rec_lo);
    io.v(rec_hi);
}
