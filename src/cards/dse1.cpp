#include "cards/dse1.h"
#include "core/bus.h"
#include "core/log.h"
#include "core/state.h"
#include "core/util.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

using nlohmann::json;

static const double MIN_PULSE_S = 0.001;           // shorter pulses do not program a 2716 / 2732 (assumption)
static const double BYTE_MIN_MS = 45, BYTE_MAX_MS = 55;   // 50 ms +-5 of the data sheets, per byte
// 2708 (Oki MSM2708AS, NTE2708): pulses 0.1-1 ms, N loops over all words with N x width >= 100 ms
static const double MIN_PULSE_2708_S = 0.0001, MAX_PULSE_2708_S = 0.001;
static const double BYTE_MIN_2708_MS = 100;
static const double RUN_GAP_S = 0.5;               // Vpp off 25 V this long ends a programming run

Dse1::~Dse1()
{
    if (!trial) flush();
}

bool Dse1::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_number(cfg, "base", v, 0xF0, err)) return false;
    if (v < 0 || v > 0xF0 || (v & 0x0F)) { err = "base must be 00h, 10h ... F0h (S1)"; return false; }
    base = (uint8_t)v;
    if (!cfg_bool(cfg, "jp1_map", jp1, true, err)) return false;
    std::string s;
    if (!cfg_string(cfg, "vpp", s, "pin50", err)) return false;
    if (s == "pin50") vpp_src = VPP_PIN50;
    else if (s == "converter") vpp_src = VPP_CONVERTER;
    else if (s == "none") vpp_src = VPP_NONE;
    else { err = "vpp must be pin50, converter or none"; return false; }
    if (cfg.contains("x2_cable") && cfg["x2_cable"].is_number_integer()) s = std::to_string(cfg["x2_cable"].get<long long>());
    else if (!cfg_string(cfg, "x2_cable", s, "2716", err)) return false;
    if (s == "2716") x2_cable = CABLE_2716;
    else if (s == "2732") x2_cable = CABLE_2732;
    else if (s == "dual") x2_cable = CABLE_DUAL;
    else if (s == "2708") x2_cable = CABLE_2708;
    else { err = "x2_cable must be 2716, 2732, dual or 2708"; return false; }
    if (!cfg_bool(cfg, "working_copy", work_copy, true, err)) return false;
    std::string img;
    if (cfg.contains("eprom") && cfg["eprom"].is_null()) img.clear();   // no EPROM in the socket
    else if (!cfg_string(cfg, "eprom", img, "media/dse-1/eprom_2716.bin", err)) return false;
    ppi.dev = this;
    power_on_fill(ram, sizeof(ram), 0);
    chip.unload();
    chip.image = img;
    if (!probe && !img.empty()) {
        if (!read_image(img, chip, err)) return false;
        LOGI("%s: EPROM %d %s in the socket", name.c_str(), chip.type, chip.path.c_str());
    }
    return true;
}

std::string Dse1::rom_work_copy(const std::string& image)
{
    std::string norm = image;
    for (char& c : norm) if (c == '\\') c = '/';
    if (norm.rfind("roms/", 0) != 0) return std::string();
    return data_path("work/dse-1/" + norm);   // with its folders: dumps of the same name stay apart
}

// The file the socket works on: an EPROM of media/ through its working copy (work/dse-1/... for media/dse-1/...), a
// dump of roms/ through a copy in work/dse-1/roms/ (the dumps stay as they are), other files as they are (an own
// EPROM).
bool Dse1::read_image(const std::string& image, Eprom27& to, std::string& err)
{
    std::string p = work_copy ? rom_work_copy(image) : std::string();
    if (!p.empty()) {
        if (!file_exists(p)) {
            Eprom27 orig;
            if (!orig.load(data_path(image), err)) return false;
            make_dirs(path_dir(p));
            if (!file_write_new(p, orig.data.data(), orig.data.size())) {
                err = "cannot create working copy " + p;
                return false;
            }
            LOGI("working copy: %s", p.c_str());
        }
    } else if (!media_path(image, work_copy, p, err)) {
        return false;
    }
    if (!to.load(p, err)) return false;
    to.image = image;
    return true;
}

bool Dse1::new_blank(int type, const char* dir, const std::vector<std::string>& in_use, std::string& rel,
                     std::string& err)
{
    size_t n = Eprom27::size_of_type(type);
    if (!n) {
        err = "a new EPROM is a 2708, a 2716 or a 2732";
        return false;
    }
    for (int i = 1; i < 10000; i++) {
        char b[32];
        snprintf(b, sizeof(b), "/eprom_%d.bin", i);
        std::string r = std::string(dir) + b;
        std::string p = data_path(r);
        if (file_exists(p)) {   // a blank one of that size not in a socket is used again (not a new file each time)
            bool used = false;
            for (const std::string& u : in_use) used = used || data_relative(u) == data_relative(p);
            std::vector<uint8_t> old;
            if (used || !file_read_all(p, old) || old.size() != n) continue;
            bool blank = true;
            for (uint8_t x : old) blank = blank && x == 0xFF;
            if (!blank) continue;
            rel = r;
            return true;
        }
        std::vector<uint8_t> d(n, 0xFF);   // erased: all bits 1
        make_dirs(path_dir(p));
        if (!file_write_safe(p, d.data(), d.size())) {
            err = "cannot write " + p;
            return false;
        }
        rel = r;
        return true;
    }
    err = std::string("no free name eprom_N.bin in ") + dir;
    return false;
}

bool Dse1::save_sim_ram(const std::string& path, std::string& err) const
{
    if (!file_write_all(path, ram, sizeof(ram))) {
        err = "cannot write " + path;
        return false;
    }
    return true;
}

bool Dse1::erase(std::string& err)
{
    if (!chip.loaded()) {
        err = "no EPROM in the socket";
        return false;
    }
    if (vpp_level() != EPL_L) {   // as taking it out under voltage: a real chip could be damaged
        char b[96];
        snprintf(b, sizeof(b), "the socket is under voltage (Vpp %d V): the EPROM cannot be taken out to erase it",
                 vpp_volts());
        err = b;
        return false;
    }
    run_end();   // Vpp is 0 V: no pulse is on, a run still waiting for its end ends here
    bool changed = false;
    for (uint8_t& b : chip.data)
        if (b != 0xFF) {
            b = 0xFF;
            changed = true;
        }
    if (changed) {   // saved to its file like any programming
        chip.dirty = true;
        media_written();
    }
    LOGI("%s: EPROM %d erased by UV light (emulator comfort)%s", name.c_str(), chip.type, changed ? "" : ", it was blank");
    update();
    return true;
}

// The chip leaves the socket: written back when programmed.
void Dse1::take_out(void)
{
    std::string err;
    if (battery_files_enabled() && !chip.save(err)) LOGW("%s: %s", name.c_str(), err.c_str());
    chip.unload();
    chip.image.clear();
}

bool Dse1::insert(const std::string& image, std::string& err)
{
    Eprom27 e;   // read first: a bad image leaves the chip in the socket
    if (!read_image(image, e, err)) return false;
    if (vpp_level() != EPL_L)
        LOGW("%s: the EPROM in the socket changed with Vpp %d V (a real chip could be damaged)", name.c_str(), vpp_volts());
    if (pulse_active) pulse_end();
    run_end();
    read_2716_mode = false;
    long_2708 = false;
    if (chip.loaded() && data_relative(chip.path) == data_relative(e.path)) {
        // the same file again: the chip as programmed so far, not the file as it was read (saved first)
        flush();
        e.data = chip.data;
        e.dirty = chip.dirty;
    }
    take_out();
    chip = e;
    LOGI("%s: EPROM %d %s in the socket", name.c_str(), chip.type, chip.path.c_str());
    update();
    return true;
}

void Dse1::eject(void)
{
    if (!chip.loaded()) return;
    if (vpp_level() != EPL_L)
        LOGW("%s: the EPROM taken out of the socket with Vpp %d V (a real chip could be damaged)", name.c_str(), vpp_volts());
    if (pulse_active) pulse_end();
    run_end();
    read_2716_mode = false;
    long_2708 = false;
    take_out();
    update();
}

bool Dse1::media_change(int u, const std::string& image, std::string& err)
{
    if (u != 0) {
        err = "no such socket";
        return false;
    }
    if (image.empty()) {
        eject();
        return true;
    }
    return insert(image, err);
}

void Dse1::flush()
{
    if (!battery_files_enabled()) return;   // tests: work/ stays untouched
    std::string err;
    if (!chip.save(err)) LOGW("%s: %s", name.c_str(), err.c_str());
}

bool Dse1::unsaved()
{
    return battery_files_enabled() && chip.loaded() && chip.dirty;
}

void Dse1::reset()
{
    // /RES clears the 74LS174 (Vpp 0 V, OFF LINE, LEDs off, MAP1 L) and puts the 8255 ports to input. The
    // simulator RAM stays: the write while PC4 floats high (until the first control word) is not modelled.
    regv = 0;
    ppi.reset();
    update();
    run_end();
}

void Dse1::advance(uint64_t now)
{
    // the programming run ends when Vpp has been off 25 V for a while: DSE.COM drops it to 5 V (2732: 0 V) around
    // every pulse to verify the byte, the run goes on
    if (!runs.active || pulse_active || last_vpp == EPL_VPP || !bus || !bus->cpu_hz || now < vpp_off) return;
    if (now - vpp_off >= (uint64_t)(RUN_GAP_S * bus->cpu_hz)) run_end();
}

uint8_t Dse1::vpp_level(void) const
{
    // V1 closes the MAA723 with U0V = 0 (also with U25V = 1); U25V switches the divider P2 / R8 (25 V) or P1 (5 V)
    if (vpp_src == VPP_NONE || !(regv & 0x01)) return EPL_L;
    return (regv & 0x02) ? EPL_VPP : EPL_H;
}

int Dse1::vpp_volts(void) const
{
    static const int v[3] = { 0, 5, 25 };
    return v[vpp_level()];
}

int Dse1::socket_wiring(void) const
{
    if (x2_cable == CABLE_DUAL) return chip.type == Eprom27::T2732 ? CABLE_2732 : CABLE_2716;   // a 2708: the 2716 one
    return x2_cable;
}

int Dse1::socket_type(void) const
{
    int w = socket_wiring();
    return w == CABLE_2732 ? Eprom27::T2732 : w == CABLE_2708 ? Eprom27::T2708 : Eprom27::T2716;
}

double Dse1::min_pulse_s(void) const
{
    return chip.type == Eprom27::T2708 ? MIN_PULSE_2708_S : MIN_PULSE_S;
}

void Dse1::byte_limits(double& lo, double& hi) const
{
    lo = chip.type == Eprom27::T2708 ? BYTE_MIN_2708_MS : BYTE_MIN_MS;
    hi = chip.type == Eprom27::T2708 ? 0 : BYTE_MAX_MS;
}

Dse1::Lines Dse1::lines(void)
{
    Lines l;
    uint8_t ctl = ppi.control();
    uint8_t drv;
    uint8_t pcv = ppi.pc_out(drv);
    l.pc = (uint8_t)((pcv & drv) | ~drv);   // an input pin floats: H
    bool pb_out = !(ctl & 0x02);
    l.addr_driven = pb_out || (drv & 0x0F);
    // PA0-11: PB and PC0-3 of the 8255; with ONLA the 74LS257 adds SA0-11 of the probe (H, no probe): AND
    l.addr = (uint16_t)((((l.pc & 0x0F) << 8) | (pb_out ? ppi.out_b : 0xFF)) & 0xFFF);
    l.addr_known = onla() || (pb_out && (drv & 0x03) == 0x03);
    // the socket, wired by the cable
    uint8_t vpp = vpp_level();
    bool pc6 = (l.pc & 0x40) != 0, pc7 = (l.pc & 0x80) != 0;
    l.pins.a = l.addr & 0x3FF;
    l.pins.p19 = (l.addr & 0x400) ? EPL_H : EPL_L;   // PA10 (2716, 2732)
    int w = socket_wiring();
    if (w == CABLE_2716) {                 // 18 = X2.18 PC6, 20 = X2.12 NOT PC7, 21 = X2.26 Vpp
        l.pins.p18 = pc6 ? EPL_H : EPL_L;
        l.pins.p20 = pc7 ? EPL_L : EPL_H;
        l.pins.p21 = vpp;
    } else if (w == CABLE_2708) {          // reconstructed: 18 = Vpp, 20 = X2.12 shifted to 12 V, 19 +12 V, 21 -5 V
        l.pins.p18 = vpp;
        l.pins.p19 = EPL_12;
        l.pins.p20 = pc7 ? EPL_L : EPL_12;
        l.pins.p21 = EPL_M5;
    } else {                               // 21 = X2.11 PA11, 18 = X2.20 NOR(PC6, PC7), 20 = X2.25 Vpp
        l.pins.p21 = (l.addr & 0x800) ? EPL_H : EPL_L;
        l.pins.p18 = pc6 || pc7 ? EPL_L : EPL_H;
        l.pins.p20 = vpp;
    }
    // PD0-7: the 8255 PA as an output, the RAM read (R and not W), the EPROM; nobody: H
    l.pd = 0xFF;
    if (!(ctl & 0x40) && !(ctl & 0x10)) {   // mode 0 or 1, PA output
        l.pd &= ppi.out_a;
        l.drivers |= DRV_PPI;
    }
    if ((l.pc & 0x20) && !(l.pc & 0x10)) {
        l.pd &= ram[l.addr];
        l.drivers |= DRV_RAM;
    }
    uint8_t d;
    if (chip.output(l.pins, d)) {
        l.pd &= d;
        l.drivers |= DRV_EPROM;
    }
    return l;
}

uint8_t Dse1::pins_a()
{
    return lines().pd;
}

uint8_t Dse1::pins_b()
{
    return (uint8_t)lines().addr;
}

uint8_t Dse1::pins_c()
{
    Lines l = lines();
    return (uint8_t)((l.pc & 0xF0) | ((l.addr >> 8) & 0x0F));
}

void Dse1::warn_once(int bit, const char* fmt, ...)
{
    if (warned & (1u << bit)) return;
    warned |= 1u << bit;
    char b[320];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    LOGW("%s: %s", name.c_str(), b);
}

static const char* driver_names(int d)
{
    switch (d) {
    case 3: return "the 8255 (PA output) and the simulator RAM";
    case 5: return "the 8255 (PA output) and the EPROM";
    case 6: return "the simulator RAM and the EPROM";
    default: return "the 8255 (PA output), the simulator RAM and the EPROM";
    }
}

void Dse1::update(void)
{
    Lines l = lines();
    // W: the 2114 write while PC4 is high (a level), with the address on the lines
    if ((l.pc & 0x10) && l.addr_known) ram[l.addr] = l.pd;
    if (l.drivers & (l.drivers - 1))
        warn_once(W_PD_CLASH + l.drivers, "%s drive PD at once (taken as wired AND)", driver_names(l.drivers));
    if (onla() && l.addr_driven)
        warn_once(W_ONLA, "ON LINE (ONLA) while the 8255 drives the address (PB or PC0-3 outputs)");
    unsigned ov = chip.overvoltage(l.pins);
    if (ov & EPO_25V_P18) warn_once(W_25V_P18, "25 V on pin 18 of the EPROM: on a real chip this could damage it");
    if (ov & EPO_25V_OE)
        warn_once(W_25V_P20, "25 V on /OE (pin 20) of the 2716 in the socket wired for a 2732: on a real chip this could damage it");
    if (ov & EPO_25V_A11)
        warn_once(W_25V_P21, "25 V on A11 (pin 21) of the 2732 in the socket wired for a 2716: on a real chip this could damage it");
    if (ov & EPO_12V_A10)
        warn_once(W_12V_A10, "+12 V (VDD of a 2708) on A10 (pin 19) of the %d in the socket wired for a 2708: on a real chip "
                  "this could damage it", chip.type);
    if (ov & EPO_12V_OE)
        warn_once(W_12V_OE, "+12 V on /OE (pin 20) of the 2716 in the socket wired for a 2708: on a real chip this could damage it");
    if (ov & EPO_M5V_P21)
        warn_once(W_M5V_P21, "-5 V (VBB of a 2708) on pin 21 of the %d in the socket wired for a 2708: on a real chip this "
                  "could damage it", chip.type);
    if (ov & EPO_VBB_PLUS)
        warn_once(W_VBB_PLUS, "%d V on VBB (pin 21) of the 2708 in the socket wired for a %d, it needs -5 V: on a real chip this "
                  "could damage it (it does not read)", l.pins.p21 == EPL_VPP ? 25 : 5, socket_type());
    if (ov & EPO_NO_VBB)
        warn_once(W_2708_NO_VBB, "the 2708 in the socket wired for a %d has 5 V on VCC but 0 V on VBB (pin 21): its data "
                  "sheets want -5 V on VBB before VCC and VDD, a real chip could be damaged (it does not read)", socket_type());
    if (ov & EPO_25V_CSWE)
        warn_once(W_25V_CSWE, "25 V on CS/WE (pin 20) of the 2708 in the socket wired for a 2732: on a real chip this could "
                  "damage it");
    if (chip.type == Eprom27::T2708 && l.pins.p18 == EPL_VPP && l.pins.p20 != EPL_12 && chip.mode(l.pins) != EPM_NO_SUPPLY)
        warn_once(W_2708_PROG_CS, "25 V on PROGRAM (pin 18) of the 2708 while CS/WE is not at 12 V: not a mode of the data "
                  "sheets, nothing is programmed (assumption)");
    // DSE.DOC: during S and R the programming socket must be empty (a 2716 with 0 V on Vpp and /CE low probably
    // loads PD, Nejasnost 3); the emulator reads the RAM right, the log says it once
    if ((l.pc & 0x30) && l.pins.p18 == EPL_L && chip.mode(l.pins) == EPM_NO_VPP)
        warn_once(W_SIM_EPROM, "the simulator RAM used while a 2716 sits in the socket with Vpp 0 V and /CE low: "
                  "DSE.DOC asks for an empty socket during S and R (the real board may read wrong data)");
    // the programming pulse: from the edge that starts it to the write that ends it
    bool prog = chip.mode(l.pins) == EPM_PROGRAM;
    uint16_t c = prog ? chip.cell(l.pins) : 0;
    if (pulse_active && (!prog || c != pulse_addr)) {
        if (prog) warn_once(W_PULSE_ADDR, "the address changed during a programming pulse");
        pulse_end();   // with PD sampled before this write
    }
    if (prog) {
        uint64_t now = bus ? bus->now : 0;
        if (!pulse_active) {
            pulse_active = true;
            n_pulse_starts++;
            pulse_start = now;
            pulse_addr = c;
            pulse_data = 0xFF;
            seg_start = now;
            seg_pd = l.pd;
        } else if (l.pd != seg_pd) {   // PD changed: the stretch before counts when it was long enough
            segment_end(now);
            seg_start = now;
            seg_pd = l.pd;
        }
    }
    uint8_t v = vpp_level();
    if (last_vpp == EPL_VPP && v != EPL_VPP) vpp_off = bus ? bus->now : 0;   // the run ends later (advance)
    if (last_vpp != EPL_VPP && v == EPL_VPP) n_vpp_rises++;
    last_vpp = v;
}

// The data on PD program a cell only when they stay long enough (a real cell takes its charge over time): a stretch
// shorter than the shortest pulse that programs does not count (the previous data for the microseconds between the
// new address and the new data, when a 2708 gets 25 V across addresses from a routine for a 2716).
void Dse1::segment_end(uint64_t now)
{
    double s = bus && bus->cpu_hz && now > seg_start ? (double)(now - seg_start) / bus->cpu_hz : 0;
    if (s >= min_pulse_s()) pulse_data &= seg_pd;
}

void Dse1::pulse_end(void)
{
    uint64_t now = bus ? bus->now : pulse_start;
    uint64_t len = now > pulse_start ? now - pulse_start : 0;
    pulse_active = false;
    last_pulse = len;
    segment_end(now);   // with PD before the write that ends the pulse
    if (!runs.active) {
        runs = Run();
        runs.active = true;
        run_us.assign(4096, 0);
        prev_cell = -1;
    }
    double s = bus && bus->cpu_hz ? (double)len / bus->cpu_hz : 0;
    if (s < min_pulse_s()) {
        runs.short_pulses++;
        return;
    }
    if (chip.type == Eprom27::T2708) {
        long_2708 = s > MAX_PULSE_2708_S;
        if (long_2708)
            warn_once(W_2708_LONG, "a programming pulse of %.2f ms on the 2708: its data sheets give 0.1-1 ms (programmed "
                      "anyway; a routine for a 2716? EPROM.COM of J. Biba programs a 2708 as type 0)", s * 1000.0);
        if (prev_cell == (int32_t)pulse_addr)
            warn_once(W_2708_REPEAT, "programming pulses on one word of the 2708 with no other word in between: its data "
                      "sheets give one pulse to every word in each of N loops (programmed anyway; EPROM.COM of J. Biba "
                      "loops over its range only, a range of one byte does this)");
    }
    prev_cell = pulse_addr;
    if (chip.program(pulse_addr, pulse_data)) media_written();
    if (!runs.pulses || len < runs.min_t) runs.min_t = len;
    if (len > runs.max_t) runs.max_t = len;
    runs.pulses++;
    double us = s * 1e6 + run_us[pulse_addr & 0xFFF];
    run_us[pulse_addr & 0xFFF] = us > 4e9 ? 4000000000u : (uint32_t)(us + 0.5);
}

void Dse1::run_end(void)
{
    if (!runs.active) return;
    runs.active = false;
    runs.bytes = 0;
    uint32_t lo = 0, hi = 0;
    for (uint32_t t : run_us) {
        if (!t) continue;
        if (!runs.bytes || t < lo) lo = t;
        if (t > hi) hi = t;
        runs.bytes++;
    }
    run_us.clear();
    runs.min_byte_ms = lo / 1000.0;
    runs.max_byte_ms = hi / 1000.0;
    double hz = bus && bus->cpu_hz ? (double)bus->cpu_hz : 1.0;
    char shorts[96] = "";
    bool t2708 = chip.type == Eprom27::T2708;
    if (runs.short_pulses)
        snprintf(shorts, sizeof(shorts), ", %u pulse%s shorter than %s ms did not program%s", runs.short_pulses,
                 runs.short_pulses == 1 ? "" : "s", t2708 ? "0.1" : "1", t2708 ? "" : " (assumption)");
    if (!runs.pulses) {
        LOGI("%s: programming: nothing programmed%s", name.c_str(), shorts);
        return;
    }
    LOGI("%s: programming: %u byte%s, %.2f pulses per byte, pulses %.2f-%.2f ms%s", name.c_str(), runs.bytes,
         runs.bytes == 1 ? "" : "s", (double)runs.pulses / runs.bytes, runs.min_t * 1000.0 / hz, runs.max_t * 1000.0 / hz,
         shorts);
    double blo, bhi;
    byte_limits(blo, bhi);
    if (t2708 && runs.min_byte_ms < blo)
        LOGW("%s: the pulses of a byte add up to %.2f-%.2f ms, the data sheets of the 2708 ask for N x pulse width >= 100 ms "
             "(programmed anyway)", name.c_str(), runs.min_byte_ms, runs.max_byte_ms);
    else if (!t2708 && (runs.min_byte_ms < blo || runs.max_byte_ms > bhi))
        LOGW("%s: the pulses of a byte add up to %.2f-%.2f ms, the data sheet asks for 50 ms +-5 (programmed anyway)",
             name.c_str(), runs.min_byte_ms, runs.max_byte_ms);
}

double Dse1::last_pulse_ms(void) const
{
    return bus && bus->cpu_hz ? last_pulse * 1000.0 / bus->cpu_hz : 0;
}

bool Dse1::io_read(uint8_t port, uint8_t& data)
{
    if ((port & 0xF0) != base || (port & 0x08)) return false;
    // base+3 (the control word) and the register: nothing drives the inner bus, the 8286 reads FFh (assumption)
    data = (port & 0x04) ? 0xFF : ppi.read(port & 3);
    if ((port & 0x07) == 0 && chip.type == Eprom27::T2708) {   // the program reads the 2708 (PA as input)
        Lines l = lines();
        if (!(l.drivers & DRV_PPI) && (l.drivers & DRV_EPROM) && l.pins.p18 == EPL_H)
            warn_once(W_2708_READ_5V, "a 2708 read with 5 V on PROGRAM (pin 18): its data sheets want it below VCC in the "
                      "read mode (read anyway; EPROM.COM of J. Biba reads a 2708 as type 0 with 0 V)");
    }
    if ((port & 0x07) == 0 && chip.type == Eprom27::T2732) {   // the program reads the EPROM (PA as input)
        Lines l = lines();
        if (!(l.drivers & DRV_PPI) && l.pins.p18 == EPL_L) {
            int md = chip.mode(l.pins);
            if (md == EPM_OUTPUT_OFF && l.pins.p20 == EPL_H) {   // 5 V on OE/Vpp: its 2716 mode (11h)
                read_2716_mode = true;
                warn_once(W_2732_AS_2716, "a 2732 read with 5 V on OE/Vpp (pin 20): its outputs are off, it reads FFh. "
                          "The program reads it as a 2716: answer A to \"2732?\" (an emulator hint)");
            } else if (md == EPM_READ) {
                read_2716_mode = false;
            }
        }
    }
    return true;
}

bool Dse1::io_write(uint8_t port, uint8_t data)
{
    if ((port & 0xF0) != base || (port & 0x08)) return false;
    if (port & 0x04) regv = data & 0x3F;   // 74LS174: D0-D5
    else ppi.write(port & 3, data);
    update();
    return true;
}

int Dse1::map_lines()
{
    return jp1 && (regv & 0x20) ? MAP_1 : 0;
}

int Dse1::decode_io(uint8_t port, const char** what)
{
    if ((port & 0xF0) != base || (port & 0x08)) return 0;
    static const char* const n[5] = { "8255 PA (data)", "8255 PB (address A0-A7)", "8255 PC (A8-A11, W, R, PGM, R/W)",
                                      "8255 control", "register (Vpp, ON LINE, LED, MAP)" };
    *what = (port & 0x04) ? n[4] : n[port & 3];
    return DEC_READ | DEC_WRITE;   // the 8286 drives the bus on reads of all eight
}

std::vector<std::string> Dse1::warnings(void) const
{
    std::vector<std::string> w;
    for (int d : { 3, 5, 6, 7 })
        if (warned & (1u << (W_PD_CLASH + d))) w.push_back(std::string("PD driven by ") + driver_names(d));
    if (warned & (1u << W_ONLA)) w.push_back("ON LINE while the 8255 drives the address");
    if (warned & (1u << W_25V_P18)) w.push_back("25 V on pin 18 of the EPROM");
    if (warned & (1u << W_25V_P20)) w.push_back("25 V on /OE of a 2716 (socket wired for a 2732)");
    if (warned & (1u << W_25V_P21)) w.push_back("25 V on A11 of a 2732 (socket wired for a 2716)");
    if (warned & (1u << W_PULSE_ADDR)) w.push_back("the address changed during a programming pulse");
    if (warned & (1u << W_SIM_EPROM))
        w.push_back("simulator RAM used with a 2716 in the socket (DSE.DOC: empty for S and R)");
    if (warned & (1u << W_2732_AS_2716)) w.push_back("a 2732 read as a 2716 (5 V on OE/Vpp): answer A to \"2732?\"");
    if (warned & (1u << W_12V_A10)) w.push_back("+12 V on A10 of a 2716 / 2732 (socket wired for a 2708)");
    if (warned & (1u << W_12V_OE)) w.push_back("+12 V on /OE of a 2716 (socket wired for a 2708)");
    if (warned & (1u << W_M5V_P21)) w.push_back("-5 V on pin 21 of a 2716 / 2732 (socket wired for a 2708)");
    if (warned & (1u << W_VBB_PLUS)) w.push_back("5 V or 25 V on VBB of a 2708 (socket wired for a 2716 / 2732)");
    if (warned & (1u << W_25V_CSWE)) w.push_back("25 V on CS/WE of a 2708 (socket wired for a 2732)");
    if (warned & (1u << W_2708_PROG_CS)) w.push_back("25 V on PROGRAM of a 2708 while CS/WE is not at 12 V");
    if (warned & (1u << W_2708_READ_5V)) w.push_back("a 2708 read with 5 V on PROGRAM");
    if (warned & (1u << W_2708_LONG)) w.push_back("a programming pulse longer than 1 ms on a 2708");
    if (warned & (1u << W_2708_REPEAT)) w.push_back("programming pulses on one word of a 2708 with no other word in between");
    if (warned & (1u << W_2708_NO_VBB)) w.push_back("0 V on VBB of a 2708 with VCC on (socket wired for a 2716 / 2732)");
    return w;
}

void Dse1::describe(json& out)
{
    char b[16];
    snprintf(b, sizeof(b), "%02Xh", base);
    out["base"] = b;
    snprintf(b, sizeof(b), "%02Xh", regv);
    out["register"] = b;
    out["vpp_v"] = vpp_volts();
    out["on_line"] = onla() && onld();
    out["onla"] = onla();
    out["onld"] = onld();
    out["led1"] = led1();
    out["led2"] = led2();
    out["map1"] = map_lines() != 0;
    json e;
    e["image"] = chip.image;
    e["path"] = chip.path;
    e["type"] = chip.loaded() ? json(chip.type) : json(nullptr);
    e["dirty"] = chip.dirty;
    e["socket"] = std::to_string(socket_type());
    out["eprom"] = e;
    out["x2_cable"] = x2_cable == CABLE_DUAL ? "dual" : x2_cable == CABLE_2708 ? "2708" : x2_cable == CABLE_2732 ? "2732" : "2716";
    out["pulse_on"] = pulse_active;
    out["last_pulse_ms"] = round(last_pulse_ms() * 1000.0) / 1000.0;
    double hz = bus && bus->cpu_hz ? (double)bus->cpu_hz : 1.0;
    json r;
    r["active"] = runs.active;
    r["pulses"] = runs.pulses;
    r["short_pulses"] = runs.short_pulses;
    r["min_pulse_ms"] = round(runs.min_t * 1e6 / hz) / 1000.0;
    r["max_pulse_ms"] = round(runs.max_t * 1e6 / hz) / 1000.0;
    if (!runs.active) {
        r["bytes"] = runs.bytes;
        r["min_byte_ms"] = round(runs.min_byte_ms * 1000.0) / 1000.0;
        r["max_byte_ms"] = round(runs.max_byte_ms * 1000.0) / 1000.0;
    }
    out["run"] = r;
    out["warnings"] = warnings();
}

std::string Dse1::live_status()
{
    char b[96];
    const char* t = chip.type == Eprom27::T2732 ? "2732" : chip.type == Eprom27::T2716 ? "2716"
                  : chip.type == Eprom27::T2708 ? "2708" : "no EPROM";
    snprintf(b, sizeof(b), "Vpp %d V, %s, %s%s%s", vpp_volts(), onla() && onld() ? "ON LINE" : "OFF LINE", t,
             led1() ? ", LED1" : "", led2() ? ", LED2" : "");
    std::string s = b;
    if (last_pulse) {
        snprintf(b, sizeof(b), ", last pulse %.1f ms", last_pulse_ms());
        s += b;
    }
    return s;
}

void Dse1::state(StateIO& io)
{
    // the EPROM: which one (its image) and the content, programmed while running (1, 2 or 4 KB)
    std::string img = chip.image;
    io.v(img);
    bool unread = false;   // its file cannot be read here: the chip is made of the content in the state
    if (io.loading && io.ok() && img != chip.image) {   // another one was put in since
        std::string err;
        Eprom27 e;
        take_out();
        if (!img.empty() && read_image(img, e, err)) chip = e;
        else if (!img.empty()) {
            LOGW("state: DSE-1 EPROM %s: %s (the content of the state goes into the socket, not into a file)",
                 img.c_str(), err.c_str());
            unread = true;
        }
    }
    std::vector<uint8_t> d = chip.data;
    io.v(d);
    if (io.loading && io.ok()) {
        if (unread) {
            chip.type = Eprom27::type_of_size(d.size());
            if (chip.loaded()) {   // no path: nothing is written
                chip.data = d;
                chip.image = img;
            }
        } else if (d.size() != chip.data.size())
            LOGW("state: DSE-1 EPROM %s has another size than in the state", img.c_str());
        else if (d != chip.data) {   // saved to the working copy like any programming
            chip.data = d;
            chip.dirty = true;
            media_written();
        }
    }
    ppi.state(io);
    io.v(regv);
    io.arr(ram, sizeof(ram));
    io.v(pulse_active);
    io.v(pulse_start);
    io.v(pulse_addr);
    io.v(pulse_data);
    io.v(seg_start);
    io.v(seg_pd);
    io.v(prev_cell);
    io.v(last_pulse);
    io.v(last_vpp);
    io.v(vpp_off);
    io.v(runs.active);
    io.v(runs.pulses);
    io.v(runs.short_pulses);
    io.v(runs.min_t);
    io.v(runs.max_t);
    io.v(runs.bytes);
    io.v(runs.min_byte_ms);
    io.v(runs.max_byte_ms);
    uint32_t n = (uint32_t)run_us.size();
    io.v(n);
    if (io.loading) {
        if (!io.ok() || (n != 0 && n != 4096) || runs.active != (n == 4096)) {   // a run has its 4096 cells
            io.fail("DSE-1: bad programming run");
            return;
        }
        run_us.assign(n, 0);
    }
    for (uint32_t& t : run_us) io.v(t);
    io.v(warned);
    io.v(read_2716_mode);   // the hints of the window go on after a loaded state
    io.v(long_2708);
}
