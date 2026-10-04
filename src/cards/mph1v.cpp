#include "cards/mph1v.h"
#include "core/bus.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

using nlohmann::json;

// Output low pass (Sallen-Key, unity gain): R4 = R5 = 2k2, C9 = 6n8, C10 = 3n3, as a
// bilinear biquad at the sample rate. High pass: C8 10u into R3 47k.
static float lp_b0, lp_b1, lp_b2, lp_a1, lp_a2, hp_a;

static void filter_init(void)
{
    static bool done = false;
    if (done) return;
    done = true;
    const double pi = 3.14159265358979;
    double r1 = 2200, r2 = 2200, c1 = 6.8e-9, c2 = 3.3e-9;
    double f0 = 1.0 / (2.0 * pi * sqrt(r1 * r2 * c1 * c2));
    double q = sqrt(r1 * r2 * c1 * c2) / (c2 * (r1 + r2));
    double k = tan(pi * f0 / Mph1v::SAMPLE_HZ);
    double n = 1.0 / (1.0 + k / q + k * k);
    lp_b0 = (float)(k * k * n);
    lp_b1 = 2.0f * lp_b0;
    lp_b2 = lp_b0;
    lp_a1 = (float)(2.0 * (k * k - 1.0) * n);
    lp_a2 = (float)((1.0 - k / q + k * k) * n);
    double rc = 47e3 * 10e-6;
    hp_a = (float)(rc / (rc + 1.0 / Mph1v::SAMPLE_HZ));
}

static bool device_of(const json& v, int& out, std::string& err, const char* key)
{
    std::string s = v.is_string() ? v.get<std::string>() : std::string("?");
    if (s == "none") out = Mph1v::DEV_NONE;
    else if (s == "joystick") out = Mph1v::DEV_JOYSTICK;
    else if (s == "mouse_atari") out = Mph1v::DEV_MOUSE_ATARI;
    else if (s == "mouse_amiga") out = Mph1v::DEV_MOUSE_AMIGA;
    else {
        err = std::string(key) + " must be none, joystick, mouse_atari or mouse_amiga";
        return false;
    }
    return true;
}

bool Mph1v::configure(const json& cfg, std::string& err)
{
    long long b;
    if (!cfg_number(cfg, "base", b, 0x50, err)) return false;
    if (b < 0 || b > 0xF8 || (b & 7)) { err = "base must be a multiple of 8 from 00h to F8h (S1)"; return false; }
    base = (uint8_t)b;
    if (!device_of(cfg.value("k4", json("joystick")), dev[0], err, "k4")) return false;
    if (!device_of(cfg.value("k2", json("mouse_atari")), dev[1], err, "k2")) return false;
    update_mouse_flag();
    pit.is_8254 = true;   // CP82C54-10 (parts list)
    filter_init();
    if (bus) bus->audio_rate = SAMPLE_HZ;
    return true;
}

void Mph1v::reset()
{
    // /IRES clears IEN (gates low, interrupts off) and resets the YM3812 (74LS164: /RST
    // stays low 8 clocks of 224 kHz longer). The 8253 and the latches keep their state.
    Outs before = outs();
    ien = 0;
    set_gates();
    pit_edges(before);
    ym.reset();
    if (bus) {
        last = bus->now;
        bus->audio_rate = SAMPLE_HZ;
    }
}

void Mph1v::set_gates(void)
{
    pit.set_gate(0, (ien & 0x08) != 0);
    pit.set_gate(1, (ien & 0x10) != 0);
    pit.set_gate(2, (ien & 0x20) != 0);
}

void Mph1v::pit_edges(Outs b)
{
    if (b.o0 && !pit.out(0)) pit.tick(1);   // CLK1 = OUT0, the 8253 counts on the falling edge
    if (!b.o1 && pit.out(1)) f1 = true;     // 74LS74 clocked by the rising edge
    if (!b.o2 && pit.out(2)) f2 = true;
}

void Mph1v::pit_clock(void)
{
    Outs before = outs();
    pit.tick(0);
    pit.tick(2);
    pit_edges(before);
}

void Mph1v::update_mouse_flag(void)
{
    if (bus) bus->mouse_connected = dev[0] >= DEV_MOUSE_ATARI || dev[1] >= DEV_MOUSE_ATARI;
}

bool Mph1v::live_param(const char* path) const
{
    return !strcmp(path, "k4") || !strcmp(path, "k2");   // plugging a controller in and out
}

bool Mph1v::set_live_param(const char* path, const json& v, std::string& err)
{
    int k = !strcmp(path, "k4") ? 0 : !strcmp(path, "k2") ? 1 : -1;
    if (k < 0) return Card::set_live_param(path, v, err);
    if (!device_of(v, dev[k], err, path)) return false;
    update_mouse_flag();
    return true;
}

// One step of each wheel towards the movement the host still owes (a real mouse cannot turn
// its wheels faster; a program polling the port would miss steps).
void Mph1v::mouse_step(void)
{
    const int cap = MOUSE_RATE / 4;   // at most 250 ms behind the host, then it is dropped
    int& dx = bus->mouse_dx;
    int& dy = bus->mouse_dy;
    if (dx > cap) dx = cap;
    if (dx < -cap) dx = -cap;
    if (dy > cap) dy = cap;
    if (dy < -cap) dy = -cap;
    if (dx > 0) { mx = (uint8_t)((mx + 1) & 3); dx--; }
    else if (dx < 0) { mx = (uint8_t)((mx + 3) & 3); dx++; }
    if (dy > 0) { my = (uint8_t)((my + 1) & 3); dy--; }
    else if (dy < 0) { my = (uint8_t)((my + 3) & 3); dy++; }
}

uint8_t Mph1v::connector(int k)
{
    switch (dev[k]) {
    case DEV_JOYSTICK: {
        // the first joystick connector gets the first host controller, the second the second
        int j = (k == 1 && dev[0] == DEV_JOYSTICK) ? 1 : 0;
        return (uint8_t)(~bus->joystick[j] | 0xC0);
    }
    case DEV_MOUSE_ATARI:
    case DEV_MOUSE_AMIGA: {
        // quadrature: phase 0-3 -> (A, B) = 00, 10, 11, 01; A leads B moving right / down
        static const uint8_t qa[4] = { 0, 1, 1, 0 }, qb[4] = { 0, 0, 1, 1 };
        uint8_t v = 0xFF;
        if (bus->mouse_buttons & MOUSE_LEFT) v &= (uint8_t)~0x10;    // pin 6
        if (bus->mouse_buttons & MOUSE_RIGHT) v &= (uint8_t)~0x20;   // pin 9
        v &= 0xF0;
        uint8_t xa = qa[mx], xb = qb[mx], ya = qa[my], yb = qb[my];
        if (dev[k] == DEV_MOUSE_ATARI)   // pin 2 XA, pin 1 XB, pin 4 YB, pin 3 YA
            v |= (uint8_t)(xa | xb << 1 | yb << 2 | ya << 3);
        else                             // pin 2 H, pin 1 V, pin 4 HQ, pin 3 VQ
            v |= (uint8_t)(xa | ya << 1 | xb << 2 | yb << 3);
        return v;
    }
    default:
        return 0xFF;   // nothing plugged in: the pull-ups
    }
}

int16_t Mph1v::filter(int16_t s)
{
    float x = (float)s;
    float y = lp_b0 * x + lp_b1 * lp_x1 + lp_b2 * lp_x2 - lp_a1 * lp_y1 - lp_a2 * lp_y2;
    lp_x2 = lp_x1;
    lp_x1 = x;
    lp_y2 = lp_y1;
    lp_y1 = y;
    float h = hp_a * (hp_y1 + y - hp_x1);
    hp_x1 = y;
    hp_y1 = h;
    if (h > 32767.0f) h = 32767.0f;
    if (h < -32768.0f) h = -32768.0f;
    return (int16_t)lrintf(h);
}

void Mph1v::advance(uint64_t now)
{
    if (now <= last) return;
    uint64_t d = now - last;
    last = now;
    if (d > (uint64_t)bus->cpu_hz) d = bus->cpu_hz;   // never more than a second at once
    master_acc += d * MASTER_HZ;
    uint64_t ticks = master_acc / bus->cpu_hz;
    master_acc %= bus->cpu_hz;

    pit_phase += (uint32_t)ticks;
    while (pit_phase >= 256) {
        pit_phase -= 256;
        pit_clock();
    }
    if (dev[0] >= DEV_MOUSE_ATARI || dev[1] >= DEV_MOUSE_ATARI) {
        mouse_phase += (uint32_t)ticks;
        while (mouse_phase >= MASTER_HZ / MOUSE_RATE) {
            mouse_phase -= MASTER_HZ / MOUSE_RATE;
            mouse_step();
        }
    }
    ym_phase += (uint32_t)ticks;
    int16_t buf[256];
    size_t n = 0;
    while (ym_phase >= 288) {
        ym_phase -= 288;
        int16_t lr[2];
        ym.sample(lr);
        buf[n++] = filter(lr[0]);
        if (n == 256) {
            bus->audio_put(buf, n);
            n = 0;
        }
    }
    if (n) bus->audio_put(buf, n);
}

bool Mph1v::io_read(uint8_t port, uint8_t& data)
{
    if ((port & 0xF8) != base) return false;
    advance(bus->now);
    switch (port & 7) {
    case 0: case 1: case 2:
        data = pit.read(port & 3);
        break;
    case 3:   // STATUS (74LS244)
        data = 0;
        if (f2) data |= 0x80;
        if (f1) data |= 0x40;
        if (pit.out(2)) data |= 0x20;
        if (pit.out(1)) data |= 0x10;
        if (pit.out(0)) data |= 0x08;
        if (ym.irq()) data |= 0x01;
        break;
    case 4:   // K4 (OVL)
        data = connector(0);
        break;
    case 5:   // K2 (MYS), the same wiring
        data = connector(1);
        break;
    case 6:
        data = ym.status();
        break;
    default:   // the YM3812 does not drive the bus for A0 = 1
        data = 0xFF;
        break;
    }
    return true;
}

bool Mph1v::io_write(uint8_t port, uint8_t data)
{
    if ((port & 0xF8) != base) return false;
    advance(bus->now);
    Outs before = outs();
    switch (port & 7) {
    case 0: case 1: case 2: case 3:
        pit.write(port & 3, data);
        break;
    case 4:
        ien = data & 0xF9;
        set_gates();
        break;
    case 5:   // IACK: the latch is held cleared until the next machine cycle
        if (data & 0x80) f2 = false;
        if (data & 0x40) f1 = false;
        break;
    case 6:
        ym.write_addr(data);
        break;
    case 7:
        ym.write_data(data);
        break;
    }
    pit_edges(before);
    return true;
}

int Mph1v::irq_lines()
{
    bool irq = (f2 && (ien & 0x80)) || (f1 && (ien & 0x40)) || (ym.irq() && (ien & 0x01));
    return irq ? IRQ_INT0 : 0;
}

int Mph1v::decode_io(uint8_t port, const char** what)
{
    if ((port & 0xF8) != base) return 0;
    static const char* names[8] = { "82C54", "82C54", "82C54", "STATUS/82C54", "JOY/IEN", "MOUSE/IACK", "YM3812", "YM3812" };
    *what = names[port & 7];
    return DEC_READ | DEC_WRITE;
}

void Mph1v::describe(json& out)
{
    char t[8];
    snprintf(t, sizeof(t), "%02Xh", base);
    out["base"] = t;
    out["ien"] = ien;
    out["f1"] = f1;
    out["f2"] = f2;
    out["out"] = { pit.out(0), pit.out(1), pit.out(2) };
    out["ym_status"] = ym.status();
    out["int0"] = irq_lines() != 0;
    static const char* devs[4] = { "none", "joystick", "mouse_atari", "mouse_amiga" };
    out["k4"] = devs[dev[0]];
    out["k2"] = devs[dev[1]];
}
