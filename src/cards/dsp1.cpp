#include "cards/dsp1.h"
#include "core/bus.h"
#include "core/devices.h"
#include "core/state.h"
#include "core/util.h"

#include <stdio.h>

using nlohmann::json;

static const char* const k_conn[2] = { "x2", "x3" };   // connector of channel 2, channel 1

double Dsp1::Chan::txc_hz(void) const
{
    if (rate == 7) return ext_hz;
    if (rate == 6) return 3200.0;              // D4C 7493 as /6
    return 153600.0 / (double)(1 << rate);     // 12.288 MHz / 10 / 8, /2 ... /32
}

// SA1 group: "X0" (low) / "X8" (high) / "none", for a function whose low output is `lo`
static bool sa1_choice(const json& cfg, const char* key, const char* def, int lo, int& out, std::string& err)
{
    std::string s;
    if (!cfg_string(cfg, key, s, def, err)) return false;
    static const char* const low[3] = { "X0", "X2", "X4" };
    static const char* const hi[3] = { "X8", "XA", "XC" };
    if (s == "none") out = -1;
    else if (s == low[lo]) out = lo;
    else if (s == hi[lo]) out = lo + 4;
    else {
        err = std::string(key) + " must be " + low[lo] + ", " + hi[lo] + " or none (SA1)";
        return false;
    }
    return true;
}

bool Dsp1::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_number(cfg, "high", v, 4, err)) return false;
    if (v < 0 || v > 15) { err = "high must be 0..F (SA2)"; return false; }
    high = (uint8_t)(v << 4);
    if (!sa1_choice(cfg, "ch2", "X8", 0, sel[0], err) || !sa1_choice(cfg, "ch1", "XA", 1, sel[1], err) ||
        !sa1_choice(cfg, "int", "XC", 2, sel[2], err))
        return false;
    for (int i = 0; i < 2; i++) {
        json c = cfg.contains(k_conn[i]) && cfg[k_conn[i]].is_object() ? cfg[k_conn[i]] : json::object();
        Chan& k = ch[i];
        if (!cfg_number(c, "rate", v, 0, err)) return false;
        if (v < 0 || v > 7) { err = std::string(k_conn[i]) + ".rate must be 0..7 (SA3/SA4)"; return false; }
        k.rate = (int)v;
        if (!cfg_number(c, "ext_hz", v, 0, err)) return false;
        k.ext_hz = (double)(v < 0 ? 0 : v);
        bool connected, cts;
        if (!cfg_bool(c, "connected", connected, true, err) || !cfg_bool(c, "cts", cts, true, err)) return false;
        int port = sel[i] < 0 ? -1 : high | (sel[i] << 1);
        char name[48];
        if (port < 0) snprintf(name, sizeof(name), "%s %s", type.c_str(), i ? "X3" : "X2");
        else snprintf(name, sizeof(name), "%s %s #%02X", type.c_str(), i ? "X3" : "X2", port);
        k.serial.reset(new SerialChannel(name));
        k.serial->connected = connected;
        k.serial->cts = cts;
        if (c.contains("serial")) {
            k.serial->bridge_cfg = c["serial"];
            k.serial->pacing = c["serial"].value("pacing", true);
        }
        double txc = k.txc_hz();
        k.uart.set_clock(bus ? bus->cpu_hz : 2000000, txc > 0 ? txc : 1.0);
        k.serial->baud = (int)(txc / 16.0 + 0.5);
    }
    return true;
}

void Dsp1::reset()
{
    for (Chan& k : ch) {
        k.uart.reset(bus ? bus->now : 0);
        double txc = k.txc_hz();
        k.uart.set_clock(bus ? bus->cpu_hz : 2000000, txc > 0 ? txc : 1.0);
    }
    mask = 0;
}

void Dsp1::advance(uint64_t now)
{
    for (Chan& k : ch)
        if (k.txc_hz() > 0) k.uart.advance(now, k.serial.get());   // no TxC (external, none): no bits move
}

int Dsp1::select(uint8_t port) const
{
    if ((port & 0xF0) != high) return -1;
    int y = (port >> 1) & 7;
    for (int i = 0; i < 3; i++)
        if (sel[i] == y) return i;
    return -1;
}

bool Dsp1::io_read(uint8_t port, uint8_t& data)
{
    int s = select(port);
    if (s < 0) return false;
    advance(bus->now);
    if (s == 2) data = 0xFF;   // the mask register is write only (the 8286 reads an undriven bus)
    else data = ch[s].uart.read(port & 1, ch[s].serial.get());
    return true;
}

bool Dsp1::io_write(uint8_t port, uint8_t data)
{
    int s = select(port);
    if (s < 0) return false;
    advance(bus->now);
    if (s == 2) mask = data & 0x0F;
    else ch[s].uart.write(port & 1, data);
    return true;
}

int Dsp1::irq_lines()
{
    if (!mask) return 0;
    advance(bus->now);
    SerialChannel* s2 = ch[0].serial.get();
    SerialChannel* s1 = ch[1].serial.get();
    bool r = ((mask & 1) && ch[0].uart.rxrdy_pin()) || ((mask & 2) && ch[0].uart.txrdy_pin(s2)) ||
             ((mask & 4) && ch[1].uart.txrdy_pin(s1)) || ((mask & 8) && ch[1].uart.rxrdy_pin());
    return r ? IRQ_INT0 : 0;
}

SerialChannel* Dsp1::serial_channel(int index)
{
    return index >= 0 && index < 2 ? ch[index].serial.get() : nullptr;
}

int Dsp1::decode_io(uint8_t port, const char** what)
{
    int s = select(port);
    if (s < 0) return 0;
    static const char* const n[3][2] = { { "8251 ch. 2 (X2) data", "8251 ch. 2 (X2) control" },
                                         { "8251 ch. 1 (X3) data", "8251 ch. 1 (X3) control" },
                                         { "interrupt mask", "interrupt mask" } };
    *what = n[s][port & 1];
    return s == 2 ? DEC_WRITE : DEC_READ | DEC_WRITE;
}

void Dsp1::describe(json& out)
{
    char b[16];
    for (int i = 0; i < 2; i++) {
        json c;
        if (sel[i] >= 0) {
            snprintf(b, sizeof(b), "%02Xh", high | (sel[i] << 1));
            c["port"] = b;
        }
        c["txc_hz"] = ch[i].txc_hz();
        c["baud_x16"] = ch[i].serial->baud;
        snprintf(b, sizeof(b), "%02Xh", ch[i].uart.command());
        c["command"] = b;
        c["terminal_connected"] = (bool)ch[i].serial->connected;
        out[k_conn[i]] = c;
    }
    out["interrupt_mask"] = mask;
}

void Dsp1::state(StateIO& io)
{
    for (Chan& k : ch) k.uart.state(io);   // the channels (DCD, CTS, waiting bytes): Machine
    io.v(mask);
}
