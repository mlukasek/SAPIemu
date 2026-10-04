#include "cards/mph1v.h"
#include "core/bus.h"

#include <math.h>
#include <stdio.h>

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

bool Mph1v::configure(const json& cfg, std::string& err)
{
    long long b;
    if (!cfg_number(cfg, "base", b, 0x50, err)) return false;
    if (b < 0 || b > 0xF8 || (b & 7)) { err = "base must be a multiple of 8 from 00h to F8h (S1)"; return false; }
    base = (uint8_t)b;
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
    case 4:
        data = (uint8_t)(~bus->joystick[0] | 0xC0);
        break;
    case 5:   // K2 (mouse port, the same wiring): a second controller
        data = (uint8_t)(~bus->joystick[1] | 0xC0);
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
    static const char* names[8] = { "8253", "8253", "8253", "STATUS/8253", "JOY/IEN", "MOUSE/IACK", "YM3812", "YM3812" };
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
}
