#include "cards/jpr1.h"
#include "core/state.h"
#include "core/bus.h"
#include "core/ihex.h"
#include "core/log.h"
#include "core/util.h"
#include "input/keyboards.h"

#include <string.h>

using nlohmann::json;

// Cards are synchronised and interrupt lines sampled every SYNC_CYCLES CPU cycles.
static const int SYNC_CYCLES = 64;

static uint8_t cb_read(void* u, uint16_t a) { return ((Jpr1*)u)->cpu_read(a); }
static void cb_write(void* u, uint16_t a, uint8_t v) { ((Jpr1*)u)->cpu_write(a, v); }
static uint8_t cb_in(void* u, uint8_t p) { return ((Jpr1*)u)->cpu_in(p); }
static void cb_out(void* u, uint8_t p, uint8_t v) { ((Jpr1*)u)->cpu_out(p, v); }

Jpr1::Jpr1(const char* type) : CpuCard(type)
{
    model_a = strcmp(type, "JPR-1A") == 0;
    model_s = strcmp(type, "JPR-1S") == 0;
    model_x = strcmp(type, "JPR-1X") == 0;
    memset(d2c_prom, 0xFF, sizeof(d2c_prom));
    memset(ram, 0, sizeof(ram));
    i8080_init(&cpu);
    cpu.read_byte = cb_read;
    cpu.write_byte = cb_write;
    cpu.port_in = cb_in;
    cpu.port_out = cb_out;
    cpu.userdata = this;
}

Jpr1::~Jpr1() {}

// Redukce34KB01 on K2 (JPR-1, JPR-1S): an RS-232 receive line on P1-IN7.
static bool configure_adapter(const json& cfg, std::unique_ptr<SerialChannel>& rx, uint32_t& baud, int& stop,
                              std::string& err)
{
    std::string a;
    if (!cfg_string(cfg, "k2_adapter", a, "none", err)) return false;
    if (a != "none" && a != "redukce34kb01") { err = "k2_adapter must be none or redukce34kb01"; return false; }
    rx.reset();
    if (a == "none") return true;
    long long v;
    if (!cfg_number(cfg, "rx_baud", v, 19200, err)) return false;
    if (v < 50 || v > 115200) { err = "rx_baud must be 50..115200"; return false; }
    baud = (uint32_t)v;
    if (!cfg_number(cfg, "rx_stop_bits", v, 2, err)) return false;
    if (v != 1 && v != 2) { err = "rx_stop_bits must be 1 or 2"; return false; }
    stop = (int)v;
    rx.reset(new SerialChannel("K2 RS-232 (Redukce34KB01)"));
    rx->baud = (int)baud;
    if (cfg.contains("serial")) {
        rx->bridge_cfg = cfg["serial"];
        rx->pacing = cfg["serial"].value("pacing", true);
    }
    return true;
}

// What is plugged into K3 (JPR-1S, JPR-1X): nothing or the diagnostic board.
static bool configure_k3(const json& cfg, bool& diag, std::string& err)
{
    std::string k3;
    if (!cfg_string(cfg, "k3", k3, "none", err)) return false;
    if (k3 != "none" && k3 != "diag") { err = "k3 must be none or diag (the diagnostic board)"; return false; }
    diag = k3 == "diag";
    return true;
}

bool Jpr1::live_param(const char* path) const
{
    return (model_x && (!strcmp(path, "turbo") || !strcmp(path, "bios"))) || CpuCard::live_param(path);
}

bool Jpr1::set_live_param(const char* path, const json& v, std::string& err)
{
    if (model_x && !strcmp(path, "turbo")) {   // the CPU clock follows the jumper at once
        if (!v.is_boolean()) { err = "turbo must be true or false"; return false; }
        turbo_on = v.get<bool>();
        return true;
    }
    if (model_x && !strcmp(path, "bios")) {   // latched at the end of the next reset
        std::string b = v.is_string() ? v.get<std::string>() : std::string();
        if (b != "fitted" && b != "open") { err = "bios must be fitted (lower bank) or open (upper bank)"; return false; }
        bios_fitted = b == "fitted";
        return true;
    }
    return CpuCard::set_live_param(path, v, err);
}

// JPR-1X: the ROM IC11 (A13 = /BIOS, A14 = 1): a 27C256/28C256 image has the banks at 4000h and
// 6000h, a 27C128 image at 0000h and 2000h, a 28C64 image one bank for both.
static bool load_jpr1x_rom(const std::string& rom, std::vector<uint8_t>& banks, std::string& err)
{
    std::vector<uint8_t> img;
    banks.assign(0x4000, 0xFF);
    if (rom.empty()) return true;   // empty socket: the bus
    if (!rom_load(rom, img, 0x8000, 0xFF, 0, err)) return false;
    uint64_t size = 0;   // the chip by the size of the file (Intel HEX: by its highest byte)
    std::string full = data_path(rom);
    std::vector<uint8_t> raw;
    if (file_read_all(full, raw) && !raw.empty() && raw[0] != ':') size = raw.size();
    else
        for (size_t i = img.size(); i > 0; i--)
            if (img[i - 1] != 0xFF) { size = i; break; }
    img.resize((size_t)size);
    if (img.size() > 0x4000) {
        img.resize(0x8000, 0xFF);
        memcpy(banks.data(), &img[0x4000], 0x4000);
    } else if (img.size() > 0x2000) {
        img.resize(0x4000, 0xFF);
        memcpy(banks.data(), img.data(), 0x4000);
    } else {
        img.resize(0x2000, 0xFF);
        memcpy(banks.data(), img.data(), 0x2000);
        memcpy(&banks[0x2000], img.data(), 0x2000);
    }
    return true;
}

bool Jpr1::configure(const json& cfg, std::string& err)
{
    long long crystal;
    if (model_x) {
        long long osc;
        if (!cfg_number(cfg, "osc_hz", osc, 16000000, err)) return false;
        cpu_hz = (uint32_t)(osc / 4);   // the time base: the CPU at 4 MHz (TURBO); the bus 2 MHz
        std::string rom, bios, vec, key;
        if (!cfg_string(cfg, "rom", rom, "roms/jpr-1x/JPR-1X_MIKROBIOS_MIKROBASIC.bin", err)) return false;
        if (!load_jpr1x_rom(rom, eprom, err)) return false;
        x_rom_fitted = !rom.empty();
        if (!cfg_string(cfg, "bios", bios, "fitted", err)) return false;
        if (bios != "fitted" && bios != "open") { err = "bios must be fitted (lower bank) or open (upper bank)"; return false; }
        bios_fitted = bios == "fitted";
        bios_off = bios_fitted ? 0 : 0x2000;
        if (!cfg_bool(cfg, "turbo", turbo_on, false, err)) return false;
        if (!cfg_string(cfg, "int_vector", vec, "rst7", err)) return false;
        if (vec != "rst7" && vec != "level") { err = "int_vector must be rst7 (RSTN/7 open) or level (RSTN/7 fitted)"; return false; }
        rst7_jumper = vec == "rst7";
        if (!cfg_string(cfg, "int_key", key, "none", err)) return false;
        if (key != "none" && key != "r7" && key != "r6") { err = "int_key must be none, r7 (11-10) or r6 (10-R6)"; return false; }
        int_key_r = key == "r7" ? 7 : key == "r6" ? 6 : -1;
        if (!cfg_bool(cfg, "ram", ram_fitted, true, err)) return false;
        if (!configure_k3(cfg, diag, err)) return false;
        uint32_t x = 0x13579BDFu;   // the 6116 holds noise after power on
        for (uint8_t& b : ram) {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            b = (uint8_t)x;
        }
        if (!configure_adapter(cfg, rx, rx_baud, rx_stop, err)) return false;
        if (cfg.contains("keyboard") && !cfg["keyboard"].is_null()) {
            kbd.reset(keyboard_from_config(cfg["keyboard"], cpu_hz, err));
            if (!kbd) return false;
            kbd->bus = bus;
        }
        return true;
    }
    if (!cfg_number(cfg, "crystal_hz", crystal, 18000000, err)) return false;
    cpu_hz = (uint32_t)(crystal / 9);   // 8224
    std::string type, rom;
    if (model_a) {
        if (!cfg_string(cfg, "rom", rom, "", err)) return false;
        eprom.clear();
        if (!rom.empty() && !rom_load(rom, eprom, 0x1000, 0xFF, 0, err)) return false;   // 2x 2716
        if (!cfg_string(cfg, "decoder", d2c_file, "roms/jpr-1a/JPR-1A_D2C_74S571.bin", err)) return false;
        std::vector<uint8_t> p;
        if (!rom_load(d2c_file, p, 512, 0xFF, 0, err)) return false;
        if (p.size() != 512) { err = d2c_file + ": the D2C PROM (74S571) has 512 entries"; return false; }
        memcpy(d2c_prom, p.data(), 512);
        std::string jp1;
        if (!cfg_string(cfg, "jp1", jp1, "1-2", err)) return false;
        if (jp1 != "1-2" && jp1 != "1-3") { err = "jp1 must be 1-2 (RST 7) or 1-3 (vector from the bus)"; return false; }
        rst7_jumper = jp1 == "1-2";
        int_key_jumper = false;
        ram_fitted = false;
        if (cfg.contains("keyboard") && !cfg["keyboard"].is_null()) {
            kbd.reset(keyboard_from_config(cfg["keyboard"], cpu_hz, err));
            if (!kbd) return false;
            kbd->bus = bus;
        }
        return true;
    }
    if (!configure_adapter(cfg, rx, rx_baud, rx_stop, err)) return false;
    if (model_s) {
        if (!cfg_string(cfg, "rom", rom, "roms/jpr-1s/MIKROBIOS_1.02.bin", err)) return false;
        eprom.clear();
        if (!rom.empty() && !rom_load(rom, eprom, 0x2000, 0xFF, 0, err)) return false;   // 28C64
        eprom.resize(0x2000, 0xFF);
        // the SRAM holds noise after power on (as RMD-1): MIKROBIOS keeps a VideoBase found in its
        // RAM when the low 14 bits are zero, a zero RAM would make it 0000h
        sram.resize(0x10000);
        uint32_t x = 0x2468ACE1u;
        for (uint8_t& b : sram) {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            b = (uint8_t)x;
        }
        static const char* const banks[3] = { "ram_4000", "ram_8000", "ram_c000" };
        for (int i = 0; i < 3; i++)
            if (!cfg_bool(cfg, banks[i], bank_en[i], true, err)) return false;
        static const char* const srcs[4] = { "int_int0", "int_int1", "int_p2in0", "int_p2in1" };
        for (int i = 0; i < 4; i++)
            if (!cfg_bool(cfg, srcs[i], int_src[i], i == 1, err)) return false;
        std::string ack;
        if (!cfg_string(cfg, "int_ack", ack, "rst7", err)) return false;
        if (ack != "rst7" && ack != "bus") { err = "int_ack must be rst7 (INT_ADR 2-3) or bus (1-2)"; return false; }
        rst7_jumper = ack == "rst7";
        if (!configure_k3(cfg, diag, err)) return false;
        ram_fitted = true;
        int_key_jumper = false;
        if (cfg.contains("keyboard") && !cfg["keyboard"].is_null()) {
            kbd.reset(keyboard_from_config(cfg["keyboard"], cpu_hz, err));
            if (!kbd) return false;
            kbd->bus = bus;
        }
        return true;
    }
    if (!cfg_string(cfg, "eprom_type", type, "2708", err)) return false;
    if (type != "2708" && type != "2716") { err = "eprom_type must be 2708 or 2716"; return false; }
    eprom_page = type == "2708" ? 0x400 : 0x800;
    if (!cfg_string(cfg, "rom", rom, "", err)) return false;
    eprom.clear();
    if (!rom.empty() && !rom_load(rom, eprom, (size_t)eprom_page * 4, 0xFF, 0, err)) return false;
    if (!cfg_bool(cfg, "ram", ram_fitted, false, err)) return false;
    if (!cfg_bool(cfg, "int_key", int_key_jumper, true, err)) return false;
    if (!cfg_bool(cfg, "wait_state", wait_jumper, false, err)) return false;
    if (cfg.contains("keyboard") && !cfg["keyboard"].is_null()) {
        kbd.reset(keyboard_from_config(cfg["keyboard"], cpu_hz, err));
        if (!kbd) return false;
        kbd->bus = bus;
    }
    return true;
}

void Jpr1::set_keyboard(KeyboardDevice* k)
{
    kbd.reset(k);
    if (kbd) {
        kbd->bus = bus;
        kbd->reset();
        kbd->advance(bus->now);
        kbd->p0_out(p0_out);
    }
}

void Jpr1::reset()
{
    // RESIN of the 8224: the CPU starts at 0000 (the RAM keeps its content);
    // the 3212 outputs are cleared
    i8080_init(&cpu);   // clears the callbacks too
    cpu.read_byte = cb_read;
    cpu.write_byte = cb_write;
    cpu.port_in = cb_in;
    cpu.port_out = cb_out;
    cpu.userdata = this;
    cpu.i8085 = model_x;   // JPR-1X: RESET also masks RST 5.5-7.5, clears SOD (i8080_init)
    if (model_x) {
        bios_off = bios_fitted ? 0 : 0x2000;   // IC25B latches the BIOS jumper at the end of RESET
        x_latched = false;
    }
    p0_out = p1_out = p2_out = 0;
    boot = true;
    irq_enabled = false;
    irq_status = 0;
    irq_sgs = false;
    next_sync = bus->now;
    stop_hit = false;
    at_boundary = false;
    instr_pc = 0;
    if (kbd) {
        kbd->reset();
        kbd->p0_out(p0_out);
    }
}

void Jpr1::advance(uint64_t now)
{
    if (kbd) kbd->advance(now);
}

// D10 decodes the EPROM sockets (MR, A14 = A15 = 0); D9 the 1 KB blocks of
// 2000-3FFF (A13 = 1, A14 = A15 = 0) without MR/MW gating.
// JPR-1A: D2C inputs A0..A8 = A15, A9, A8, A12, A13, A14, A10, A11, BOOT
uint8_t Jpr1::d2c(uint8_t hi) const
{
    unsigned i = (unsigned)((hi >> 7) & 1) | (unsigned)((hi >> 1) & 1) << 1 | (unsigned)(hi & 1) << 2 |
                 (unsigned)((hi >> 4) & 1) << 3 | (unsigned)((hi >> 5) & 1) << 4 | (unsigned)((hi >> 6) & 1) << 5 |
                 (unsigned)((hi >> 2) & 1) << 6 | (unsigned)((hi >> 3) & 1) << 7 | (boot ? 0x100u : 0u);
    return d2c_prom[i] & 0x0F;
}

int Jpr1::local(uint16_t a) const
{
    if (model_s) {   // 74ALS138 (2000-3FFF in 1 KB) and 74ALS153 (quadrants)
        if (a < 0x2000) return L_EPROM;
        if (a < 0x4000) {
            switch ((a >> 10) & 7) {
            case 0: return L_RAM;
            case 1: return L_P0;
            case 2: return L_P1;
            case 3: return L_P2;
            default: return L_NONE;   // 3000-3FFF: the bus (AND-1/AND-1B at 3800)
            }
        }
        return bank_en[(a >> 14) - 1] ? L_BANK : L_NONE;
    }
    if (model_x) {   // IC27 (A13-A15) and IC10 (A10-A12); 3000-FFFF reads from the bus
        if (a < 0x2000) return x_rom_fitted ? L_EPROM : L_NONE;
        if (a >= 0x4000) return L_NONE;
        switch ((a >> 10) & 7) {
        case 0: return ram_fitted ? L_RAM : L_NONE;
        case 1: return L_P0;
        case 2: return L_P1;
        case 3: return L_P2;
        case 4: return L_ECS;
        default: return L_NONE;   // 3400-37FF unused, 3800-3FFF AND-1 on the bus
        }
    }
    if (model_a) {   // memory: only the EPROM while BOOT is set (OE1, OE2 active low)
        if (!boot) return L_NONE;
        uint8_t o = d2c((uint8_t)(a >> 8));
        return (!(o & 4) || !(o & 8)) ? L_EPROM : L_NONE;
    }
    if (a < (uint16_t)(eprom_page * 4) && a < 0x2000) {
        size_t off = a;
        return off < eprom.size() ? L_EPROM : L_NONE;   // empty sockets: the bus
    }
    if ((a & 0xE000) != 0x2000) return L_NONE;
    switch ((a >> 10) & 7) {
    case 0: return ram_fitted ? L_RAM : L_NONE;
    case 1: return L_P0;
    case 2: return L_P1;
    case 3: return L_P2;
    case 4: return L_ECS;
    default: return L_NONE;   // 3400-3FFF: AND-1 VRAM at 3800 and others on the bus
    }
}

uint8_t Jpr1::p2_in(void)
{
    // the diagnostic board on K3: "+" pulls P2-IN0 down, "-" P2-IN7
    uint8_t d = 0xFF;
    if (diag && diag_btn[0]) d &= (uint8_t)~0x01;
    if (diag && diag_btn[1]) d &= (uint8_t)~0x80;
    if (kbd) d &= kbd->p2_in();   // a device on X3 (HRK-1 of APS-5)
    if (model_s) return d;   // K3 with pull-ups
    if (model_x) {   // P2-IN0/1 = bus /INT0, /INT1 (the same wires); P2-IN7 = the INT key with 11-10; the rest K3
        uint8_t v = d;
        if (bus_irq & IRQ_INT0) v &= (uint8_t)~0x01;
        if (bus_irq & IRQ_INT1) v &= (uint8_t)~0x02;
        if (int_key_r == 7 && kbd && !(p1_in() & 0x80)) v &= (uint8_t)~0x80;
        return v;
    }
    // interrupt requests are active low: R0 = /INT0, R1 = /INT1, R7 = INT key
    uint8_t v = kbd ? kbd->p2_in() : 0xFF;
    if (bus_irq & IRQ_INT0) v &= (uint8_t)~0x01;
    if (bus_irq & IRQ_INT1) v &= (uint8_t)~0x02;
    if (int_key_jumper && kbd && !(p1_in() & 0x80)) v &= (uint8_t)~0x80;
    return v;
}

uint8_t Jpr1::p1_in(void)
{
    uint8_t v = kbd ? kbd->p1_in() : 0xFF;
    if (rx) {   // the adapter: K2-7 from the optocoupler, K2-9 (T key) not connected
        v |= 0xC0;
        if (!rx_level()) v &= 0x7F;
    }
    return v;
}

// The line sends the characters from the host one after another at rx_baud: start bit (0),
// 8 data bits LSB first, rx_stop stop bits (1). A character waiting on the host side starts
// as soon as the line is free; nothing waits for the program.
bool Jpr1::rx_level(void)
{
    double bit = (double)cpu_hz / rx_baud;
    uint64_t now = bus->now;
    for (;;) {
        if (rx_active) {
            uint64_t end = rx_start + (uint64_t)(bit * (9 + rx_stop));
            if (now < end) {
                int i = (int)((double)(now - rx_start) / bit);
                if (i == 0) return false;                       // start bit
                if (i <= 8) return ((rx_byte >> (i - 1)) & 1) != 0;
                return true;                                    // stop bits
            }
            rx_active = false;
            rx_free = end;
        }
        uint8_t b;
        if (!rx->from_host(b)) return true;   // idle
        rx_active = true;
        rx_byte = b;
        rx_start = rx_free > now || now - rx_free < (uint64_t)bit ? rx_free : now;   // back to back, or now
    }
}

uint8_t Jpr1::cpu_read(uint16_t a)
{
    if (wait_jumper) extra++;
    switch (local(a)) {
    case L_EPROM:
        if (model_x) return eprom[bios_off + a];
        return a < eprom.size() ? eprom[a] : 0xFF;
    case L_RAM: return model_s ? sram[a] : ram[a & 0x3FF];
    case L_BANK: return sram[a];
    case L_P0: return kbd ? kbd->p0_in() : 0xFF;
    case L_P1: return p1_in();
    case L_P2: return p2_in();
    case L_ECS:
        if (model_x) break;   // JPR-1X: the 3214 is not read, the bus drives the data
        return 0xFF;          // 3214 is write only
    default: break;
    }
    bus->wait = 0;
    uint8_t d = bus->mem_read(a);
    extra += bus->wait;
    return d;
}

void Jpr1::cpu_write(uint16_t a, uint8_t v)
{
    if (wait_jumper) extra++;
    if (model_s) {   // the card takes it and the bus gets it too
        switch (local(a)) {
        case L_RAM:
        case L_BANK: sram[a] = v; break;
        case L_P0:
            p0_out = v;
            if (kbd) kbd->p0_out(v);
            break;
        case L_P1:
            p1_out = v;
            if (kbd) kbd->p1_out(v);
            break;
        case L_P2:
            p2_out = v;
            if (kbd) kbd->p2_out(v);
            break;
        default: break;   // EPROM: /WE held high
        }
        bus->wait = 0;
        bus->mem_write(a, v);
        extra += bus->wait;
        return;
    }
    if (model_x) {   // the card takes it and the bus gets it too (the 74LS245 drives K1 on writes)
        switch (local(a)) {
        case L_RAM: ram[a & 0x3FF] = v; break;
        case L_P0:
            p0_out = v;
            if (kbd) kbd->p0_out(v);
            break;
        case L_P1:
            p1_out = v;
            if (kbd) kbd->p1_out(v);
            break;
        case L_P2:
            p2_out = v;
            if (kbd) kbd->p2_out(v);
            break;
        case L_ECS:
            irq_status = ~v & 7;   // /B2-/B0
            irq_sgs = (v & 0x08) != 0;
            irq_enabled = true;
            update_interrupt();
            break;
        default: break;   // ROM: /WE held high
        }
        bus->wait = 0;
        bus->mem_write(a, v);
        extra += bus->wait;
        return;
    }
    switch (model_a ? (int)L_NONE : local(a)) {   // JPR-1A: writes under the EPROM go to the RAM
    case L_EPROM: return;
    case L_RAM: ram[a & 0x3FF] = v; return;
    case L_P0:
        p0_out = v;
        if (kbd) kbd->p0_out(v);
        return;
    case L_P1:
        if ((p1_out ^ v) & 0x80) LOGD("JPR-1 LED %s", (v & 0x80) ? "on" : "off");
        p1_out = v;
        if (kbd) kbd->p1_out(v);
        return;
    case L_P2:   // printer (APS-5: WD of HRK-1)
        p2_out = v;
        if (kbd) kbd->p2_out(v);
        return;
    case L_ECS:
        // load the current status of the 3214 and enable the next interrupt
        irq_status = ~v & 7;   // /B2-/B0
        irq_sgs = (v & 0x08) != 0;
        irq_enabled = true;
        update_interrupt();
        return;
    default: break;
    }
    bus->wait = 0;
    bus->mem_write(a, v);
    extra += bus->wait;
}

uint8_t Jpr1::cpu_in(uint8_t port)
{
    if (model_a && !(d2c(port) & 1)) {   // PORTY: the 3212 ports of the card
        switch (port & 3) {
        case 1: return kbd ? kbd->p0_in() : 0xFF;
        case 2: return kbd ? kbd->p1_in() : 0xFF;
        default: return 0xFF;   // P2-IN: printer (AC on D6), not connected
        }
    }
    return bus->io_read(port);
}

void Jpr1::cpu_out(uint8_t port, uint8_t v)
{
    if (model_a && !(d2c(port) & 1)) {
        switch (port & 3) {
        case 0:
            if (boot) LOGD("JPR-1A: BOOT off, RAM from 0000");
            boot = false;
            return;
        case 1:
            p0_out = v;
            if (kbd) kbd->p0_out(v);
            return;
        case 2: p1_out = v; return;
        default: p2_out = v; return;   // printer data
        }
    }
    bus->io_write(port, v);
}

// 3214: the highest active request (R7 highest) interrupts the CPU when the 3214 is enabled and
// its level is above the current status; the status inputs /B0-/B2 are active low (a write of D2-D0
// sets the status ~D & 7). D3 = 1 (SGS) lets every level through. The 3212 then puts RST (7 - level)
// on the data bus. Confirmed by the firmware of APS-5: 04h takes R4-R7 only (the key code is on
// R0-R3), 02h only R6-R7 (docs/desky/JPR-1.md).
void Jpr1::update_interrupt(void)
{
    if (model_s) {   // INT_SRC wired OR, level; INT_ADR RST 7 or /INTA on the bus
        uint8_t p2 = p2_in();
        bool req = (int_src[0] && (bus_irq & IRQ_INT0)) || (int_src[1] && (bus_irq & IRQ_INT1)) ||
                   (int_src[2] && !(p2 & 1)) || (int_src[3] && !(p2 & 2));
        cpu.interrupt_pending = req;
        if (req) cpu.interrupt_vector = rst7_jumper ? 0xFF : bus->inta();
        return;
    }
    if (model_x) {
        // 3214 with INTE tied high: an enabled request goes into the 3212 IC7 at once (also under
        // DI), the 3214 waits for the next status write; IC7 holds INTR until the INTA
        if (!x_latched && irq_enabled) {
            uint8_t req = (uint8_t)~p2_in();
            if (int_key_r == 6 && kbd && !(p1_in() & 0x80)) req |= 0x40;   // jumpers 10-R6 and R6-10
            int level = -1;
            for (int i = 7; i >= 0; i--)
                if (req & (1 << i)) { level = i; break; }
            if (level >= 0 && (irq_sgs || level > irq_status)) {
                x_latched = true;
                irq_enabled = false;
                x_vector = rst7_jumper ? 0xFF : (uint8_t)(0xC7 | ((7 - level) << 3));
            }
        }
        cpu.interrupt_pending = x_latched;
        cpu.interrupt_vector = x_vector;
        return;
    }
    if (model_a) {   // INT0 or INT1, no priority logic
        cpu.interrupt_pending = bus_irq != 0;
        if (bus_irq) cpu.interrupt_vector = rst7_jumper ? 0xFF : bus->inta();
        return;
    }
    uint8_t req = (uint8_t)~p2_in();
    int level = -1;
    for (int i = 7; i >= 0; i--)
        if (req & (1 << i)) { level = i; break; }
    bool want = irq_enabled && level >= 0 && (irq_sgs || level > irq_status);
    cpu.interrupt_pending = want;
    if (want) cpu.interrupt_vector = (uint8_t)(0xC7 | ((7 - level) << 3));
}

void Jpr1::sync(void)
{
    bus->advance_all();
    if (bus->rtl() || (kbd && kbd->rtl())) {   // /RTL (pin 1, a watchdog; HRK-1): RESIN of the 8224, /RES of all
        bus->reset();
        return;
    }
    bus_irq = bus->irq_lines();
    update_interrupt();
    next_sync = bus->now + SYNC_CYCLES;
    if (++cpm_ticks >= 1024) {   // about every 30 ms: where is the BIOS of CP/M
        cpm_ticks = 0;
        bus->cpm_poll();
    }
}

int Jpr1::run(int cycles)
{
    stop_hit = false;
    bool resume = at_boundary;   // stopped before the instruction at instr_pc: execute it first
    if (cycles > 0) at_boundary = false;
    int done = 0;
    while (done < cycles) {
        if (bus->now >= next_sync) sync();
        if (bus->hold) {   // a DMA controller had the bus: the CPU waited
            bus->now += bus->hold;
            done += (int)bus->hold;
            bus->hold = 0;
        }
        if (!resume) {
            // instruction boundary (where the Z80 card stops after the opcode fetch)
            instr_pc = cpu.pc;
            bus->pc = cpu.pc;
            if (cpu.pc == bus->cpm_seldsk && cpu.pc && cpu.c < 16) bus->cpm_drive = cpu.c;
            bool step_stop = false;
            if (step_mode) {
                if (step_skip > 0) step_skip--;
                else step_stop = true;
            }
            if (!cpu.halted && (step_stop || (bp_exec && bp_exec[cpu.pc]))) {
                stop_hit = true;
                at_boundary = true;
                return done;
            }
        }
        resume = false;
        bool taking = cpu.interrupt_pending && cpu.iff && cpu.interrupt_delay == 0;
        unsigned long before = cpu.cyc;
        extra = 0;
        if (model_x) cpu.sid = cpu.sod;   // SID wired to SOD
        i8080_step(&cpu);
        if (taking) {
            irq_enabled = false;   // the 3214 waits for the next status write
            x_latched = false;     // JPR-1X: INTA read the vector out of IC7
        }
        // JPR-1X: the time base is 4 MHz, without TURBO a CPU cycle takes two units
        int n = (int)(cpu.cyc - before) * (model_x && !turbo_on ? 2 : 1) + extra;
        if (n <= 0) n = 4;   // HLT: the CPU idles, time goes on
        bus->now += (uint64_t)n;
        done += n;
        if (taking) update_interrupt();
        if (bus->break_request) {   // memory or I/O watchpoint
            stop_hit = true;
            return done;
        }
    }
    return done;
}

void Jpr1::get_regs(CpuRegs& r)
{
    memset(&r, 0, sizeof(r));
    uint8_t f = i8080_flags(&cpu);   // 8085: V in bit 1, K in bit 5
    r.af = (uint16_t)((cpu.a << 8) | f);
    r.bc = (uint16_t)((cpu.b << 8) | cpu.c);
    r.de = (uint16_t)((cpu.d << 8) | cpu.e);
    r.hl = (uint16_t)((cpu.h << 8) | cpu.l);
    r.sp = cpu.sp;
    r.pc = at_boundary ? instr_pc : cpu.pc;
    r.iff1 = r.iff2 = cpu.iff;
    r.halted = cpu.halted;
}

void Jpr1::set_regs(const CpuRegs& r)
{
    cpu.a = (uint8_t)(r.af >> 8);
    uint8_t f = (uint8_t)r.af;
    cpu.sf = (f >> 7) & 1;
    cpu.zf = (f >> 6) & 1;
    cpu.hf = (f >> 4) & 1;
    cpu.pf = (f >> 2) & 1;
    cpu.cf = f & 1;
    cpu.vf = (f >> 1) & 1;
    cpu.kf = (f >> 5) & 1;
    cpu.b = (uint8_t)(r.bc >> 8); cpu.c = (uint8_t)r.bc;
    cpu.d = (uint8_t)(r.de >> 8); cpu.e = (uint8_t)r.de;
    cpu.h = (uint8_t)(r.hl >> 8); cpu.l = (uint8_t)r.hl;
    cpu.sp = r.sp;
    cpu.iff = r.iff1;
    if (r.pc != instr_pc) {
        cpu.pc = r.pc;
        instr_pc = r.pc;
        at_boundary = false;   // the new instruction has not been reached yet
    }
}

uint8_t Jpr1::peek(uint16_t a)
{
    switch (local(a)) {
    case L_EPROM:
        if (model_x) return eprom[bios_off + a];
        return a < eprom.size() ? eprom[a] : 0xFF;
    case L_RAM: return model_s ? sram[a] : ram[a & 0x3FF];
    case L_BANK: return sram[a];
    case L_P0:   // reading the keyboard has side effects (key taken / seen): not from the debugger
    case L_P1: return 0xFF;
    case L_P2: return p2_in();
    case L_ECS: return model_x ? bus->mem_peek(a) : 0xFF;
    default: return bus->mem_peek(a);
    }
}

void Jpr1::poke_local(uint16_t a, uint8_t v)
{
    switch (local(a)) {
    case L_RAM:
        if (model_s) sram[a] = v;
        else ram[a & 0x3FF] = v;
        break;
    case L_BANK: sram[a] = v; break;
    default: break;
    }
}

int Jpr1::decode_mem(uint16_t addr, int, const char** what)
{
    switch (local(addr)) {
    case L_EPROM:
        *what = model_s ? "EEPROM 28C64" : model_x ? (bios_off ? "ROM (upper bank)" : "ROM (lower bank)") : "EPROM";
        return DEC_READ | DEC_ONCARD;
    case L_RAM: *what = "RAM"; return DEC_READ | DEC_WRITE | DEC_ONCARD;
    case L_BANK: *what = "RAM (bank)"; return DEC_READ | DEC_WRITE | DEC_ONCARD;
    case L_P0: *what = "P0 (keyboard)"; return DEC_READ | DEC_WRITE | DEC_ONCARD;
    case L_P1: *what = "P1 (keyboard)"; return DEC_READ | DEC_WRITE | DEC_ONCARD;
    case L_P2: *what = model_s ? "P2 (K3)" : "P2 (interrupts, printer)"; return DEC_READ | DEC_WRITE | DEC_ONCARD;
    case L_ECS: *what = "3214 status"; return DEC_WRITE | DEC_ONCARD;
    default: return 0;
    }
}

int Jpr1::decode_io(uint8_t port, const char** what)
{
    if (!model_a || (d2c(port) & 1)) return 0;
    switch (port & 3) {
    case 0: *what = "BOOT off (EPROM -> RAM)"; return DEC_WRITE;
    case 1: *what = "P0 (keyboard)"; return DEC_READ | DEC_WRITE;
    case 2: *what = "P1 (keyboard, printer)"; return DEC_READ | DEC_WRITE;
    default: *what = "P2 (printer)"; return DEC_READ | DEC_WRITE;
    }
}

void Jpr1::describe(json& out)
{
    if (model_a) out["boot"] = boot;
    out["cpu"] = model_x ? "8085" : "8080";
    out["cpu_hz"] = cpu_display_hz();
    if (model_x) {
        out["turbo"] = turbo_on;
        out["rom_bank"] = bios_off ? "upper" : "lower";
        out["sod"] = (bool)cpu.sod;
    }
    out["eprom_bytes"] = eprom.size();
    out["ram"] = ram_fitted;
    if (model_s) out["ram_banks"] = { bank_en[0], bank_en[1], bank_en[2] };
    if (rx) out["k2_adapter"] = "redukce34kb01";
    if (diag) {
        char b[8];
        snprintf(b, sizeof(b), "%02Xh", p2_out);
        out["k3_diag_left"] = b;
        snprintf(b, sizeof(b), "%02Xh", p1_out);
        out["k3_diag_right"] = b;
    }
    out["keyboard"] = kbd ? kbd->type_name() : "none";
    if (kbd && kbd->joyport() >= 0) out["keyboard_joyport"] = joy_device_name(kbd->joyport());
}

void Jpr1::state(StateIO& io)
{
    // the 8080 core without its callbacks (bit fields one by one)
    uint64_t cyc = cpu.cyc;
    io.v(cyc);
    io.v(cpu.pc);
    io.v(cpu.sp);
    io.v(cpu.a);
    io.v(cpu.b);
    io.v(cpu.c);
    io.v(cpu.d);
    io.v(cpu.e);
    io.v(cpu.h);
    io.v(cpu.l);
    uint8_t f = (uint8_t)((cpu.sf ? 1 : 0) | (cpu.zf ? 2 : 0) | (cpu.hf ? 4 : 0) | (cpu.pf ? 8 : 0) |
                          (cpu.cf ? 0x10 : 0) | (cpu.iff ? 0x20 : 0) | (cpu.halted ? 0x40 : 0) |
                          (cpu.interrupt_pending ? 0x80 : 0));
    io.v(f);
    io.v(cpu.interrupt_vector);
    io.v(cpu.interrupt_delay);
    if (io.loading) {
        cpu.cyc = (unsigned long)cyc;
        cpu.sf = f & 1;
        cpu.zf = (f >> 1) & 1;
        cpu.hf = (f >> 2) & 1;
        cpu.pf = (f >> 3) & 1;
        cpu.cf = (f >> 4) & 1;
        cpu.iff = (f >> 5) & 1;
        cpu.halted = (f >> 6) & 1;
        cpu.interrupt_pending = (f >> 7) & 1;
    }
    io.v(next_sync);
    io.e(extra);
    io.arr(ram, sizeof(ram));
    io.v(p0_out);
    io.v(p1_out);
    io.v(p2_out);
    io.v(irq_enabled);
    io.e(irq_status);
    io.e(bus_irq);
    io.v(boot);
    io.v(at_boundary);
    io.v(instr_pc);
    if (model_s) io.arr(sram.data(), sram.size());   // the type and the adapter are in the description
    if (model_x) {   // the 8085 part of the core, the jumpers that change while running, the 3214 latch
        uint16_t g = (uint16_t)((int)cpu.vf | cpu.kf << 1 | cpu.sod << 2 | cpu.trap << 3 | cpu.rst75 << 4 |
                                cpu.rst65 << 5 | cpu.rst55 << 6 | cpu.trap_ff << 7 | cpu.r75_ff << 8 |
                                cpu.trap_rim << 9 | cpu.trap_ie << 10);
        io.v(g);
        io.v(cpu.masks);
        if (io.loading) {
            cpu.vf = g & 1;
            cpu.kf = (g >> 1) & 1;
            cpu.sod = (g >> 2) & 1;
            cpu.trap = (g >> 3) & 1;
            cpu.rst75 = (g >> 4) & 1;
            cpu.rst65 = (g >> 5) & 1;
            cpu.rst55 = (g >> 6) & 1;
            cpu.trap_ff = (g >> 7) & 1;
            cpu.r75_ff = (g >> 8) & 1;
            cpu.trap_rim = (g >> 9) & 1;
            cpu.trap_ie = (g >> 10) & 1;
            cpu.i8085 = true;
        }
        io.v(turbo_on);
        io.v(bios_fitted);
        io.v(bios_off);
        io.v(x_latched);
        io.v(x_vector);
        io.v(irq_sgs);
    }
    if (rx) {
        io.v(rx_active);
        io.v(rx_byte);
        io.v(rx_start);
        io.v(rx_free);
        rx->state(io);
    }
}
