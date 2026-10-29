#include "cards/hrk1.h"
#include "cards/rtc1.h"
#include "core/bus.h"
#include "core/plant.h"
#include "core/state.h"

#include <stdio.h>

static uint64_t cyc(double s, uint32_t hz) { return (uint64_t)(s * hz + 0.5); }

Hrk1::Hrk1(uint32_t cpu_hz) : hz(cpu_hz)
{
    press_cycles = cyc(0.08, hz);    // a key held 80 ms
    gap_cycles = cyc(0.12, hz);      // and released 120 ms before the next one
    int_delay = cyc(0.001, hz);      // MH 3ST2 with R6 C6: about 1 ms
    int_len = cyc(224e-6, hz);       // 74121: 0.7 R9 C7
    wd_cycles = cyc(0.051, hz);      // 74123: 0.28 R11 C8 (1 + 0.7 / R11)
    rtl_cycles = cyc(0.0166, hz);    // 0.28 R12 C9
}

uint64_t Hrk1::now(void) const
{
    return bus ? bus->now : 0;
}

void Hrk1::press(uint8_t c)
{
    if (queue.size() < 64) queue.push_back(c & 0x0F);
}

void Hrk1::advance(uint64_t t)
{
    while (!queue.empty() && t >= next_free) {
        uint8_t c = queue.front();
        queue.pop_front();
        uint64_t at = t;   // advance() comes every few dozen cycles
        next_free = at + press_cycles + gap_cycles;
        bool lock_row = c == KEY_R || c == 0x0A || c == 0x0C;   // R1-R3 behind the key switch
        if (lock_row && lock != 2) continue;                       // no contact: nothing happens
        code = c;
        int_at = at + int_delay;
    }
}

uint8_t Hrk1::p0_in()
{
    return bus && bus->plant ? bus->plant->logic_in(0) : 0xFF;
}

uint8_t Hrk1::p1_in()
{
    return bus && bus->plant ? bus->plant->logic_in(1) : 0xFF;
}

void Hrk1::p0_out(uint8_t v)
{
    lo[0] = v;
    if (bus && bus->plant) bus->plant->logic_out(0, v);
}

void Hrk1::p1_out(uint8_t v)
{
    lo[1] = v;
    if (bus && bus->plant) bus->plant->logic_out(1, v);
}

bool Hrk1::relay(void)
{
    return wd_armed && now() < wd_end;
}

bool Hrk1::rtl()
{
    uint64_t t = now();
    if (!powered()) return true;   // no power: the unit stands still (the reset holds everything)
    return wd_armed && t >= wd_end && t < wd_end + rtl_cycles;
}

void Hrk1::outage(double seconds)
{
    uint64_t t = now();
    pfail_at = t ? t : 1;
    power_back = t + cyc(0.1 + (seconds > 0 ? seconds : 0), hz);
}

bool Hrk1::powered(void)
{
    uint64_t t = now();
    if (!pfail_at) return true;
    if (t >= power_back) {   // the mains is back: the supply comes up, the watchdog starts released
        pfail_at = power_back = 0;
        wd_armed = false;
        return true;
    }
    return t < pfail_at + cyc(0.1, hz);   // the warning comes 100 ms before the supply drops
}

double Hrk1::outage_left(void)
{
    uint64_t t = now();
    return pfail_at && t < power_back ? (double)(power_back - t) / hz : 0.0;
}

void Hrk1::p2_out(uint8_t v)
{
    if ((v & 0x80) && !(p2_prev & 0x80)) {   // WD: a rising edge retriggers DD5A
        wd_armed = true;
        wd_end = now() + wd_cycles;
    }
    p2_prev = v;
}

uint8_t Hrk1::p2_in()
{
    uint64_t t = now();
    advance(t);
    uint8_t v = (uint8_t)(0xF0 | code);
    Rtc1* rtc = nullptr;
    if (bus)
        for (Card* c : bus->cards)
            if ((rtc = dynamic_cast<Rtc1*>(c)) != nullptr) break;
    if (rtc) {
        if (!rtc->user_out(1)) v &= (uint8_t)~0x10;   // 1 Hz
        if (!rtc->user_out(0)) v &= (uint8_t)~0x20;   // 50 Hz
    }
    if (int_at && t >= int_at) {
        if (t < int_at + int_len) v &= (uint8_t)~0x40;
        else int_at = 0;
    }
    if (pfail_at) v &= (uint8_t)~0x80;               // /PFAIL of the supply: R7 (RST 0)
    return v;
}

void Hrk1::type_char(uint32_t cp)
{
    if (cp >= '0' && cp <= '9') press((uint8_t)(cp - '0'));
    else if (cp == 'm' || cp == 'M' || cp == 27) press(KEY_M);
    else if (cp == 'r' || cp == 'R') press(KEY_R);
    else if (cp == 8 || cp == 127) press(KEY_BACK);
    else if (cp == '\r' || cp == '\n') press(KEY_ENTER);
}

bool Hrk1::host_key(int key, int)
{
    switch (key) {
    case HK_ENTER: case HK_KP_ENTER: case HK_RIGHT: press(KEY_ENTER); return true;
    case HK_BACKSPACE: case HK_LEFT: case HK_DELETE: press(KEY_BACK); return true;
    case HK_ESCAPE: press(KEY_M); return true;
    default: break;
    }
    if (key >= HK_KP0 && key <= HK_KP9) {
        press((uint8_t)(key - HK_KP0));
        return true;
    }
    return false;
}

// English, whole texts: the status bar translates them with tr() (frontend/i18n.cpp), MCP gives them as they are.
std::string Hrk1::status() const
{
    static const char* const k_text[2][2] = { { "APS-5 key 1", "APS-5 key 1, FAULT" },
                                              { "APS-5 key 2", "APS-5 key 2, FAULT" } };
    bool fault = !(wd_armed && now() < wd_end);   // the LED "porucha"
    return k_text[lock == 2 ? 1 : 0][fault ? 1 : 0];
}

void Hrk1::state(StateIO& io)
{
    io.v(queue);
    io.v(next_free);
    io.v(int_at);
    io.v(code);
    io.v(wd_armed);
    io.v(wd_end);
    io.v(p2_prev);
    io.v(lo[0]);
    io.v(lo[1]);
    io.v(pfail_at);
    io.v(power_back);
    int l = lock;
    io.v(l);
    lock = l;
}
