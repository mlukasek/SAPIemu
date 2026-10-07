#include "cards/rsw1.h"
#include "core/bus.h"
#include "core/ihex.h"
#include "core/state.h"

#include <stdio.h>
#include <string.h>

using nlohmann::json;

static const double TIMER_HZ = 1228800.0;   // 12.288 MHz crystal / 74LS90

Rsw1::Rsw1() : Card("RSW-1") {}

static bool xt1_source(const json& cfg, const char* key, const char* def, int& out, std::string& err)
{
    std::string v;
    if (!cfg_string(cfg, key, v, def, err)) return false;
    static const char* const names[4] = { "L", "H", "MAP1", "MAP2" };
    for (int i = 0; i < 4; i++)
        if (v == names[i]) {
            out = i;
            return true;
        }
    err = std::string(key) + " must be L, H, MAP1 or MAP2 (XT1)";
    return false;
}

bool Rsw1::configure(const json& cfg, std::string& err)
{
    std::string f1, f2;
    if (!cfg_string(cfg, "decoder", f1, "roms/rsw-1/RSW-1_DD1_74S571.bin", err)) return false;
    if (!cfg_string(cfg, "io_decoder", f2, "roms/rsw-1/RSW-1_DD2_74S287.bin", err)) return false;
    if (!rom_load(f1, dd1, 512, 0xFF, 0, err) || !rom_load(f2, dd2, 256, 0xFF, 0, err)) return false;
    dd1.resize(512, 0xFF);
    dd2.resize(256, 0xFF);
    if (!xt1_source(cfg, "xt1_a6", "L", xt1[0], err) || !xt1_source(cfg, "xt1_a7", "H", xt1[1], err) ||
        !xt1_source(cfg, "xt1_a8", "MAP2", xt1[2], err))
        return false;
    if (!cfg_bool(cfg, "int0", int0, true, err) || !cfg_bool(cfg, "watchdog_reset", wd_rtl, false, err)) return false;
    bool connected, cts;
    if (!cfg_bool(cfg, "connected", connected, true, err) || !cfg_bool(cfg, "cts", cts, true, err)) return false;
    ram.assign(65536, 0x00);
    serial.reset(new SerialChannel(type + " #18"));
    serial->connected = connected;
    serial->cts = cts;
    if (cfg.contains("serial")) {
        serial->bridge_cfg = cfg["serial"];
        serial->pacing = cfg["serial"].value("pacing", true);
    }
    return true;
}

void Rsw1::reset()
{
    // /RES: the 8251, the mask register and both watchdog flip-flops; the 8253 has no reset pin
    if (bus) clock_timers(bus->now);
    uart.reset(bus ? bus->now : 0);
    mask = 0;
    wd_enable = false;
    wd_reset = false;
    pit.set_gate(0, true);
    pit.set_gate(1, false);
    pit.set_gate(2, false);
    divisor0 = 0;
    update_baud();
}

int Rsw1::input_level(int src, int map) const
{
    switch (src) {
    case 0: return 0;
    case 1: return 1;
    case 2: return (map & MAP_1) ? 1 : 0;
    default: return (map & MAP_2) ? 1 : 0;
    }
}

bool Rsw1::ram_select(uint16_t addr, int map) const
{
    unsigned idx = (unsigned)(addr >> 10) | ((unsigned)input_level(xt1[0], map) << 6) |
                   ((unsigned)input_level(xt1[1], map) << 7) | ((unsigned)input_level(xt1[2], map) << 8);
    return !(dd1[idx] & 0x01);   // O1 = 0: RAM
}

bool Rsw1::mem_read(uint16_t addr, uint8_t& data)
{
    if (!ram_select(addr, bus->map)) return false;
    data = ram[addr];
    return true;
}

bool Rsw1::mem_peek(uint16_t addr, uint8_t& data)
{
    return mem_read(addr, data);
}

bool Rsw1::mem_write(uint16_t addr, uint8_t data)
{
    if (!ram_select(addr, bus->map)) return false;
    ram[addr] = data;
    return true;
}

void Rsw1::update_baud(void)
{
    uint32_t d = pit.divisor(0);
    if (d == divisor0) return;
    divisor0 = d;
    uart.set_clock(bus ? bus->cpu_hz : 2000000, TIMER_HZ / (d ? d : 1));
    if (serial) {
        // the line speed for a host COM port: x16 is what the software uses
        double baud = TIMER_HZ / (d ? d : 1) / 16.0;
        serial->baud = (int)(baud + 0.5);
    }
}

// Counters 1 and 2 run only while the watchdog gates them: clock them then (1.2288 MHz).
void Rsw1::clock_timers(uint64_t now)
{
    if (now <= last) return;
    uint64_t dt = now - last;
    last = now;
    if (!wd_enable) {
        phase = 0;
        return;
    }
    double ticks = phase + (double)dt * TIMER_HZ / (bus ? bus->cpu_hz : 2000000);
    uint64_t n = (uint64_t)ticks;
    phase = ticks - (double)n;
    for (uint64_t i = 0; i < n; i++) {
        bool o1 = pit.out(1), o2 = pit.out(2);
        pit.tick(1);
        if (o1 && !pit.out(1)) pit.tick(2);              // CLK2 = OUT1: counts on its falling edge
        if (!o2 && pit.out(2)) wd_reset = true;          // DD21B clocked by the end of the OUT2 pulse
    }
}

void Rsw1::advance(uint64_t now)
{
    clock_timers(now);
    uart.advance(now, serial.get());
}

int Rsw1::irq_lines()
{
    if (!int0) return 0;
    advance(bus->now);
    bool r = ((mask & 0x01) && uart.txrdy_pin(serial.get())) || ((mask & 0x02) && uart.rxrdy_pin()) ||
             ((mask & 0x08) && uart.txe_pin());
    return r ? IRQ_INT0 : 0;
}

bool Rsw1::io_read(uint8_t port, uint8_t& data)
{
    uint8_t o = dd2[port];
    if (!(o & 0x04)) {   // /S51
        advance(bus->now);
        data = uart.read(port & 1, serial.get());
        return true;
    }
    if (!(o & 0x08)) {   // /S53
        clock_timers(bus->now);
        data = pit.read(port & 3);
        return true;
    }
    return false;        // 1Eh, 1Fh are write only
}

bool Rsw1::io_write(uint8_t port, uint8_t data)
{
    uint8_t o = dd2[port];
    if (!(o & 0x04)) {
        advance(bus->now);
        uart.write(port & 1, data);
        return true;
    }
    if (!(o & 0x08)) {
        clock_timers(bus->now);
        pit.write(port & 3, data);
        update_baud();
        return true;
    }
    if (!(o & 0x02)) {   // /SIR: interrupt mask and BREAK
        mask = (uint8_t)(data & 0x0F);
        return true;
    }
    if (!(o & 0x01)) {   // /SWD: watchdog enable
        clock_timers(bus->now);
        wd_enable = (data & 1) != 0;
        pit.set_gate(1, wd_enable);
        pit.set_gate(2, wd_enable);
        return true;
    }
    return false;
}

int Rsw1::decode_io(uint8_t port, const char** what)
{
    uint8_t o = dd2[port];
    if (!(o & 0x04)) { *what = (port & 1) ? "8251 command/status" : "8251 data"; return DEC_READ | DEC_WRITE; }
    if (!(o & 0x08)) {
        static const char* const n[4] = { "8253 counter 0 (baud)", "8253 counter 1", "8253 counter 2 (watchdog)",
                                          "8253 control" };
        *what = n[port & 3];
        return DEC_READ | DEC_WRITE;
    }
    if (!(o & 0x02)) { *what = "interrupt mask, BREAK"; return DEC_WRITE; }
    if (!(o & 0x01)) { *what = "watchdog"; return DEC_WRITE; }
    return 0;
}

int Rsw1::decode_mem(uint16_t addr, int map, const char** what)
{
    if (!ram_select(addr, map)) return 0;
    *what = "RAM";
    return DEC_READ | DEC_WRITE;
}

void Rsw1::describe(json& out)
{
    char b[8];
    snprintf(b, sizeof(b), "%02Xh", uart.command());
    out["uart_command"] = b;
    out["baud"] = serial->baud;
    out["interrupt_mask"] = mask;
    out["watchdog_enabled"] = wd_enable;
    out["watchdog_expired"] = wd_reset;
    out["terminal_connected"] = (bool)serial->connected;
}

void Rsw1::state(StateIO& io)
{
    io.v(ram);
    uart.state(io);
    pit.state(io);
    io.v(mask);
    io.v(wd_enable);
    io.v(wd_reset);
    io.v(last);
    io.v(phase);
    io.v(divisor0);
}
