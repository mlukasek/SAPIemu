#include "cards/dsm1.h"
#include "core/bus.h"
#include "core/log.h"

#include <math.h>
#include <stdio.h>

using nlohmann::json;

static const uint32_t TAPE_BAUD = 2400;
static const int16_t TAPE_AMPLITUDE = 12000;
static const uint32_t k_bauds[] = { 50, 75, 110, 150, 300, 600, 1200, 2400, 4800, 9600 };

Dsm1::Dsm1() : Card("DSM-1")
{
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
    if (!cfg_number(cfg, "baud", v, 600, err)) return false;
    bool ok = false;
    for (uint32_t b : k_bauds) if (b == (uint32_t)v) ok = true;
    if (!ok) { err = "baud must be one of 50..9600 (terminal jumper)"; return false; }
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
        if (deck.load(tape, terr)) deck.write_protect = wp || deck.is_mp3;
        else LOGW("%s: %s", name.c_str(), terr.c_str());   // an empty recorder, not a broken machine
    }
    return true;
}

void Dsm1::reset()
{
    // /RES clears the modem control register: terminal mode, motor off
    modem_ctrl = 0;
    uart.set_baud(bus->cpu_hz, baud);
    uart.reset(bus->now);
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
        uart.advance(now, serial.get(), !rts_flow || (modem_ctrl & 0x01));
    }
}

// ---------------------------------------------------------------- cassette

void Dsm1::tape_run(uint64_t now)
{
    uint64_t t0 = tape_time;
    tape_time = now;
    if (now <= t0 || !motor_on()) return;
    double cps = (double)deck.rate / bus->cpu_hz;   // tape samples per CPU cycle
    double p0 = deck.pos, p1 = p0 + (double)(now - t0) * cps;
    if (deck.mode == TapeDeck::PLAY) {
        double end = (double)deck.samples.size();
        if (p1 > end) p1 = end;   // the end of the tape: the motor runs, nothing moves
        deck.pos = p1;
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
        set_mode(data);
        break;
    case 1: uart.write_control(data); break;
    case 2: uart.write_data(data); break;
    case 3: uart.reset(bus->now); break;
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
    *what = "UART, cassette";
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
    }
    out["tape"] = t;
}
