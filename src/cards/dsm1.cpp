#include "cards/dsm1.h"
#include "core/state.h"
#include "core/bus.h"
#include "core/log.h"

#include <math.h>
#include <stdio.h>

using nlohmann::json;

static const uint32_t TAPE_BAUD = 2400;
static const int16_t TAPE_AMPLITUDE = 12000;
static const double TAPE_RUNUP_S = 0.05;   // the tape gets to speed after the motor starts
static const uint32_t k_bauds[] = { 50, 75, 110, 150, 300, 600, 1200, 2400, 4800, 9600 };

Dsm1::Dsm1(const char* type) : Card(type)
{
    no_tape = type == std::string("DSM-1V");
    uart.clock_grid = no_tape;   // 6402: the character goes on the edge of the 16x clock (measured)
    uart.tx_log = &tx_chars;
}

bool Dsm1::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_number(cfg, "base", v, 0x10, err)) return false;
    if (v != 0x10 && v != 0x14 && v != 0x18 && v != 0x1C) {
        err = "base must be 10h, 14h, 18h or 1Ch (jumpers 16-12..16-15)";
        return false;
    }
    base = (uint8_t)v;
    if (!cfg_number(cfg, "baud", v, no_tape ? 115200 : 600, err)) return false;
    bool ok = false;
    if (no_tape) ok = v > 0 && 230400 % v == 0 && 230400 / v <= 256;   // 230400 / (256 - N)
    else for (uint32_t b : k_bauds) if (b == (uint32_t)v) ok = true;
    if (!ok) {
        err = no_tape ? "baud must be 230400 / (256 - N), 900..115200 (jumpers S1/S2)"
                      : "baud must be one of 50..9600 (terminal jumper)";
        return false;
    }
    baud = (uint32_t)v;
    if (!cfg_bool(cfg, "int0", int_enabled, true, err)) return false;
    if (!cfg_bool(cfg, "connected", connected_default, true, err)) return false;
    if (!cfg_bool(cfg, "cts", cts_default, true, err)) return false;
    std::string flow;
    if (!cfg_string(cfg, "flow_control", flow, "none", err)) return false;
    if (flow != "none" && flow != "rts_cts") { err = "flow_control must be none or rts_cts"; return false; }
    rts_flow = flow == "rts_cts";

    char buf[64];
    snprintf(buf, sizeof(buf), "%s #%02X", type.c_str(), base);   // as on the front panel
    serial.reset(new SerialChannel(buf));
    serial->connected = connected_default;
    serial->cts = cts_default;
    serial->baud = (int)baud;
    if (cfg.contains("serial")) {
        serial->bridge_cfg = cfg["serial"];
        serial->pacing = cfg["serial"].value("pacing", true);
    }

    // a cassette in the recorder at power on
    std::string tape;
    bool wp;
    if (!cfg_string(cfg, "tape", tape, "", err)) return false;
    if (!cfg_bool(cfg, "tape_write_protect", wp, true, err)) return false;
    if (!tape.empty() && !probe) {
        std::string terr;
        if (deck.load(tape, terr)) deck.write_protect = wp || deck.read_only();
        else LOGW("%s: %s", name.c_str(), terr.c_str());   // an empty recorder, not a broken machine
    }
    return true;
}

bool Dsm1::media_change(int u, const std::string& image, std::string& err)
{
    if (u != 0) {
        err = "no such recorder";
        return false;
    }
    if (image.empty()) {
        deck.eject();
        return true;
    }
    TapeDeck nd;   // read first: a bad file leaves the cassette in the recorder (an MP3 is decoded once)
    if (!nd.load(image, err)) return false;
    bool same = deck.loaded() && deck.path == nd.path && deck.unsaved();
    deck.eject();   // saves what was recorded
    if (same) {     // the same cassette: as just saved, not as it was read before
        if (!deck.load(image, err)) return false;
    } else {
        deck = std::move(nd);
    }
    deck.write_protect = deck.write_protect || deck.read_only();
    return true;
}

void Dsm1::reset()
{
    // /RES clears the modem control register: terminal mode, motor off
    modem_ctrl = 0;
    uart.set_baud(bus->cpu_hz, baud);
    uart.reset(bus->now);
    mr_held = no_tape;   // DSM-1V: /RES sets the MR latch
    tape_time = bus->now;
    tx_chars.clear();
}

bool Dsm1::motor_on(void) const
{
    return (modem_ctrl & 0x20) && deck.loaded() && deck.mode != TapeDeck::STOP;
}

void Dsm1::set_mode(uint8_t ctrl)
{
    bool was = cassette();
    modem_ctrl = ctrl;
    if (cassette() != was) {
        // the UART clock comes from the recorder interface (2400 Bd) or the terminal jumper
        uart.set_baud(bus->cpu_hz, cassette() ? TAPE_BAUD : baud);
        rx_scan = deck.pos;
    }
}

void Dsm1::advance(uint64_t now)
{
    tape_run(now);
    if (cassette()) {
        uart.advance(now, nullptr);   // characters go to / come from the recorder
        // keep the recently sent characters, older ones are on the tape already
        while (tx_chars.size() > 4) tx_chars.erase(tx_chars.begin());
    } else {
        tx_chars.clear();
        if (mr_held) {   // the receiver is held in reset: what comes is lost
            uint8_t b;
            while (serial && serial->from_host(b)) {}
            return;
        }
        uart.advance(now, serial.get(), !rts_flow || (modem_ctrl & 0x01));
    }
}

// ---------------------------------------------------------------- cassette

void Dsm1::tape_run(uint64_t now)
{
    uint64_t t0 = tape_time;
    tape_time = now;
    if (!motor_on()) {
        moving = false;
        return;
    }
    if (!moving) {   // the motor starts: nothing usable is recorded until the tape runs at speed
        moving = true;
        runup = deck.rate * TAPE_RUNUP_S;
    }
    if (now <= t0) return;
    double cps = (double)deck.rate / bus->cpu_hz;   // tape samples per CPU cycle
    double p0 = deck.pos, p1 = p0 + (double)(now - t0) * cps;
    if (deck.mode == TapeDeck::PLAY) {
        double end = (double)deck.samples.size();
        if (p1 > end) p1 = end;   // the end of the tape: the motor runs, nothing moves
        deck.pos = p1;
        runup = 0;
        if (cassette() && (modem_ctrl & 0x08)) tape_receive(p0, p1);
        else rx_scan = p1;
    } else if (deck.mode == TapeDeck::RECORD) {
        tape_record(p0, p1, t0, now);
        deck.pos = p1;
        rx_scan = p1;
    }
}

// UART framing on the demodulated line, in tape samples: a start bit is the
// falling edge, the bits are taken in their middles, the character is complete
// in the middle of the first stop bit.
void Dsm1::tape_receive(double p0, double p1)
{
    double bit = (double)deck.rate / TAPE_BAUD;
    if (rx_scan < p0 - bit * 12) rx_scan = p0;
    for (;;) {
        double s = deck.next_start(rx_scan, p1);
        if (s < 0) {
            rx_scan = p1;
            return;
        }
        int data_bits = uart.frame_bits() - 1 - (uart.two_stop ? 2 : 1);   // data + parity
        double end = s + bit * (1 + data_bits + 0.5);
        if (end > p1) {
            rx_scan = s;   // complete the character when the tape gets there
            return;
        }
        if (deck.line_level(s + bit * 0.5) != 0) {   // a glitch, not a start bit
            rx_scan = s + bit * 0.5;
            continue;
        }
        uint8_t v = 0;
        int parity = 0;
        for (int k = 0; k < uart.word_bits; k++) {
            int b = deck.line_level(s + bit * (1.5 + k));
            if (b) v |= (uint8_t)(1 << k);
            parity ^= b;
        }
        bool pe = false;
        if (!uart.no_parity) {
            int p = deck.line_level(s + bit * (1.5 + uart.word_bits));
            pe = ((parity ^ p) != 0) != uart.even_parity;   // even: data + parity bit even
        }
        bool fe = deck.line_level(end) == 0;
        uart.rx_inject(v, fe, pe);
        rx_scan = end;
    }
}

// The recorder input: the UART line (start, data, parity, stop bits, idle 1)
// XOR the 2400 Hz clock, as a square wave.
void Dsm1::tape_record(double p0, double p1, uint64_t t0, uint64_t t1)
{
    double bitc = uart.bit_cycles();
    double cycles_per_sample = (double)(t1 - t0) / (p1 - p0);
    for (double i = ceil(p0); i < p1; i += 1.0) {
        if (runup > 0) {   // a splice between two recordings: silence, so the reader starts anew
            deck.write(i, i + 1, 0);
            runup -= 1.0;
            continue;
        }
        double t = (double)t0 + (i - p0) * cycles_per_sample;
        int level = 1;
        for (const Mhb1012::TxChar& c : tx_chars) {
            double start = ceil((double)c.start / bitc) * bitc;   // on the bit clock
            int nb = uart.frame_bits();
            if (t < start || t >= start + nb * bitc) continue;
            int k = (int)((t - start) / bitc);
            if (k == 0) level = 0;
            else if (k <= uart.word_bits) level = (c.data >> (k - 1)) & 1;
            else if (!uart.no_parity && k == uart.word_bits + 1) {
                int par = 0;
                for (int b = 0; b < uart.word_bits; b++) par ^= (c.data >> b) & 1;
                level = uart.even_parity ? par : par ^ 1;
            } else level = 1;
        }
        int clk = (int)((uint64_t)(t / (bitc * 0.5)) & 1);
        deck.write(i, i + 1, (level ^ clk) ? TAPE_AMPLITUDE : (int16_t)-TAPE_AMPLITUDE);
    }
}

// ---------------------------------------------------------------- registers

bool Dsm1::io_read(uint8_t port, uint8_t& data)
{
    if ((port & 0xFC) != base) return false;
    switch (port & 3) {
    case 0: {
        // modem status: b7 CTS, b6 DSR, b5 DCD (active = 0), b4 raw received line,
        // b3 PE, b2 FE, b1 OE, b0 INTR
        uint8_t v = 0;
        if (!serial->cts) v |= 0xC0;
        if (!serial->connected) v |= 0x20;
        int line = 1;
        if (cassette() && (modem_ctrl & 0x08) && motor_on() && deck.mode == TapeDeck::PLAY) line = deck.line_level(deck.pos);
        if (line) v |= 0x10;
        if (uart.pe) v |= 0x08;
        if (uart.fe) v |= 0x04;
        if (uart.oe) v |= 0x02;
        if (irq_lines()) v |= 0x01;
        data = v;
        break;
    }
    case 1: data = (uint8_t)(0x3F | (uart.dr ? 0x80 : 0) | (uart.tbre ? 0x40 : 0)); break;
    case 2: data = uart.read_data(); break;
    default: return false;   // base+3 is write only
    }
    return true;
}

bool Dsm1::io_write(uint8_t port, uint8_t data)
{
    if ((port & 0xFC) != base) return false;
    switch (port & 3) {
    case 0:
        if ((data ^ modem_ctrl) & 0x20) LOGD("%s: recorder relay %s", name.c_str(), (data & 0x20) ? "on" : "off");
        advance(bus->now);    // the tape up to now belongs to the old state
        set_mode(data);
        tape_run(bus->now);   // notes a motor start
        break;
    case 1: uart.write_control(data); break;
    case 2:
        if (!mr_held) uart.write_data(data, bus->now);   // DSM-1V in MR: the character is lost
        break;
    case 3:
        uart.reset(bus->now);
        mr_held = no_tape;   // DSM-1V: held until the next /MR
        break;
    }
    return true;
}

int Dsm1::irq_lines()
{
    if (!int_enabled) return 0;
    bool rx = uart.dr && (modem_ctrl & 0x80);
    bool tx = uart.tbre && (modem_ctrl & 0x40);
    return (rx || tx) ? IRQ_INT0 : 0;
}

int Dsm1::decode_io(uint8_t port, const char** what)
{
    if ((port & 0xFC) != base) return 0;
    *what = no_tape ? "UART" : "UART, cassette";   // DSM-1V has no cassette
    return (port & 3) == 3 ? DEC_WRITE : DEC_READ | DEC_WRITE;
}

void Dsm1::flush()
{
    std::string err;
    if (!deck.save(err)) LOGW("%s: %s", name.c_str(), err.c_str());
}

void Dsm1::describe(json& out)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02Xh", base);
    out["base"] = buf;
    out["baud"] = baud;
    out["cassette_mode"] = cassette();
    static const char* const modes[] = { "stop", "play", "record" };
    json t = { { "loaded", deck.loaded() }, { "mode", modes[deck.mode] }, { "motor", motor_on() } };
    if (deck.loaded()) {
        t["file"] = deck.path;
        t["position_s"] = deck.position_seconds();
        t["length_s"] = deck.length_seconds();
        t["write_protect"] = deck.write_protect;
        if (deck.generated) t["program"] = deck.program_info;   // a tape made from a program file
    }
    out["tape"] = t;
}

// ---------------------------------------------------------------- DSM-1V

bool Dsm1v::configure(const json& cfg, std::string& err)
{
    static const char* const keys[2] = { "a", "b" };
    static const int bases[2] = { 0x10, 0x14 };
    for (int i = 0; i < 2; i++) {
        json c = cfg.contains(keys[i]) && cfg[keys[i]].is_object() ? cfg[keys[i]] : json::object();
        if (!cfg_bool(c, "enabled", enabled[i], true, err)) return false;
        if (!c.contains("base")) c["base"] = bases[i];
        if (!c.contains("baud")) c["baud"] = 115200;
        c["int0"] = true;   // wired-OR to /INT0, no jumper; IER/IET in the modem register
        ch[i].bus = bus;
        ch[i].probe = probe;
        ch[i].name = name + (i ? " B" : " A");
        std::string e;
        if (!ch[i].configure(c, e)) {
            err = std::string("channel ") + (i ? "B: " : "A: ") + e;
            return false;
        }
    }
    if (enabled[0] && enabled[1] && ch[0].base_port() == ch[1].base_port()) {
        err = "channels A and B have the same base port";
        return false;
    }
    return true;
}

void Dsm1v::reset()
{
    for (Dsm1& c : ch) c.reset();
}

void Dsm1v::advance(uint64_t now)
{
    for (int i = 0; i < 2; i++)
        if (enabled[i]) ch[i].advance(now);
}

bool Dsm1v::io_read(uint8_t port, uint8_t& data)
{
    for (int i = 0; i < 2; i++)
        if (enabled[i] && ch[i].io_read(port, data)) return true;
    return false;
}

bool Dsm1v::io_write(uint8_t port, uint8_t data)
{
    for (int i = 0; i < 2; i++)
        if (enabled[i] && ch[i].io_write(port, data)) return true;
    return false;
}

bool Dsm1v::mem_read(uint16_t addr, uint8_t& data)
{
    for (Dsm1& c : ch) c.bus_mem_read();
    return false;
}

int Dsm1v::irq_lines()
{
    int l = 0;
    for (int i = 0; i < 2; i++)
        if (enabled[i]) l |= ch[i].irq_lines();
    return l;
}

SerialChannel* Dsm1v::serial_channel(int index)
{
    int n = 0;
    for (int i = 0; i < 2; i++)
        if (enabled[i] && n++ == index) return ch[i].serial_channel(0);
    return nullptr;
}

void Dsm1v::flush()
{
    for (Dsm1& c : ch) c.flush();
}

void Dsm1v::describe(json& out)
{
    for (int i = 0; i < 2; i++) {
        json o;
        o["enabled"] = enabled[i];
        if (enabled[i]) ch[i].describe(o);
        out[i ? "b" : "a"] = o;
    }
}

int Dsm1v::decode_io(uint8_t port, const char** what)
{
    for (int i = 0; i < 2; i++)
        if (enabled[i]) {
            int d = ch[i].decode_io(port, what);
            if (d) return d;
        }
    return 0;
}

// The cassette is referred to (file, position, buttons), not stored.
static void tape_state(StateIO& io, TapeDeck& t)
{
    std::string p = t.path;
    io.media("cassette", p);
    io.v(p);
    // a tape made from a program is made again the same way
    bool gen = t.generated;
    TapeProgram prog = t.program;
    io.v(gen);
    io.v(prog.format);
    io.v(prog.name);
    io.v(prog.address);
    io.v(prog.entry);
    int mode = t.mode;
    io.e(mode);
    double pos = t.pos;
    io.v(pos);
    bool wp = t.write_protect;
    io.v(wp);
    if (!io.loading || !io.ok()) return;
    if (p != t.path || gen != t.generated || (gen && (prog.format != t.program.format || prog.name != t.program.name ||
                                                      prog.address != t.program.address || prog.entry != t.program.entry))) {
        std::string err;
        t.eject();
        bool ok = p.empty() || (gen ? t.load_program(p, prog, err) : t.load(p, err));
        if (!ok) LOGW("state: cassette %s: %s", p.c_str(), err.c_str());
    }
    if (!t.loaded()) return;   // the file is gone: an empty recorder
    t.pos = pos;               // (loading the file rewinds it)
    t.write_protect = wp || t.read_only();
    t.mode = (TapeDeck::Mode)mode;
}

void Dsm1::state(StateIO& io)
{
    io.v(modem_ctrl);
    uart.state(io);
    io.begin("tape");
    tape_state(io, deck);
    io.end();
    io.v(tape_time);
    io.v(rx_scan);
    io.v(moving);
    io.v(runup);
    uint32_t n = (uint32_t)tx_chars.size();
    io.v(n);
    if (io.loading) tx_chars.assign(io.ok() ? n : 0, Mhb1012::TxChar{ 0, 0 });
    for (Mhb1012::TxChar& c : tx_chars) {
        io.v(c.start);
        io.v(c.data);
    }
}

void Dsm1v::state(StateIO& io)
{
    for (int i = 0; i < 2; i++) {
        io.begin(i ? "channel B" : "channel A");
        ch[i].state(io);
        io.end();
    }
}
