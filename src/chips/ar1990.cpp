#include "chips/ar1990.h"
#include "core/bus.h"
#include "core/state.h"

#include <stdio.h>

Ar1990Plotter::Ar1990Plotter(uint32_t cpu_hz) : hz(cpu_hz)
{
    mg.power_on();
}

uint64_t Ar1990Plotter::now(void) const
{
    return bus ? bus->now : 0;
}

void Ar1990Plotter::reset()
{
    // /RES of the JPR-1: its port latches go low; the host keeps what it has not sent
    out1 = 0;
    beeper.set(bus, false);
}

void Ar1990Plotter::advance(uint64_t t)
{
    (void)t;
    if (!rdp && !rda() && !host.empty()) {   // the host puts the next byte out
        data = host.front();
        rdp = true;
    }
    beeper.advance(bus);
}

void Ar1990Plotter::press(int b)
{
    if (b >= 0 && b < BUTTONS) held[b] = now() + (uint64_t)(0.15 * hz);
}

void Ar1990Plotter::send(const uint8_t* d, size_t len)
{
    host.insert(host.end(), d, d + len);
}

void Ar1990Plotter::type_char(uint32_t cp)
{
    if (cp == '\n') cp = '\r';
    if (cp < 0x80) host.push_back((uint8_t)cp);
}

uint8_t Ar1990Plotter::p0_in()
{
    uint64_t t = now();
    uint8_t v = 0x03;                        // the stops: not connected to the Minigraf
    if (!color) v |= 0x04;
    if (t >= held[BTN_STOP]) v |= 0x08;
    if (t >= held[BTN_TEST]) v |= 0x10;
    if (t >= held[BTN_READY]) v |= 0x20;
    if (rdp) v |= 0x40;
    if (ascii) v |= 0x80;
    return v;
}

uint8_t Ar1990Plotter::p1_in()
{
    return data;
}

uint8_t Ar1990Plotter::p2_in()
{
    uint8_t v = 0x1E;                        // K3-17, 12, 14, 13: not connected
    if (now() >= held[BTN_JMP1000]) v |= 0x01;
    if (a4) v |= 0x20;
    if (lf_only) v |= 0x40;
    if (cr_only) v |= 0x80;
    return v;
}

void Ar1990Plotter::p0_out(uint8_t value)
{
    mg.bus = bus;
    mg.set_lines(value);
}

void Ar1990Plotter::p1_out(uint8_t value)
{
    uint8_t rise = (uint8_t)(value & ~out1), fall = (uint8_t)(out1 & ~value);
    out1 = value;
    if ((rise & 0x04) && rdp) {              // RDA: taken, the host drops RDP
        rdp = false;
        if (!host.empty()) host.pop_front();
        sent++;
    }
    if ((fall & 0x04) && !host.empty() && !rdp) {
        data = host.front();
        rdp = true;
    }
    if ((rise | fall) & 0x20) beeper.set(bus, (value & 0x20) != 0);
}

std::string Ar1990Plotter::status() const
{
    char b[96];
    snprintf(b, sizeof(b), "AR 1990%s%s%s", led_ready() ? " READY" : "", led_buff() ? " BUFF" : "",
             host.empty() ? "" : " >");
    return b;
}

void Ar1990Plotter::state(StateIO& io)
{
    io.v(host);
    io.v(rdp);
    io.v(data);
    io.v(out1);
    for (uint64_t& h : held) io.v(h);
    io.v(color);
    io.v(ascii);
    io.v(a4);
    io.v(cr_only);
    io.v(lf_only);
    io.v(sent);
    mg.state(io);
    beeper.state(io);
}
