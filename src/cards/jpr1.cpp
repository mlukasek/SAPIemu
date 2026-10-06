#include "cards/jpr1.h"
#include "core/state.h"
#include "core/bus.h"
#include "core/ihex.h"
#include "core/log.h"
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

bool Jpr1::configure(const json& cfg, std::string& err)
{
    long long crystal;
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

uint8_t Jpr1::p2_in(void) const
{
    // interrupt requests are active low: R0 = /INT0, R1 = /INT1, R7 = INT key
    uint8_t v = 0xFF;
    if (bus_irq & IRQ_INT0) v &= (uint8_t)~0x01;
    if (bus_irq & IRQ_INT1) v &= (uint8_t)~0x02;
    if (int_key_jumper && kbd && !(kbd->p1_in() & 0x80)) v &= (uint8_t)~0x80;
    return v;
}

uint8_t Jpr1::cpu_read(uint16_t a)
{
    if (wait_jumper) extra++;
    switch (local(a)) {
    case L_EPROM: return a < eprom.size() ? eprom[a] : 0xFF;
    case L_RAM: return ram[a & 0x3FF];
    case L_P0: return kbd ? kbd->p0_in() : 0xFF;
    case L_P1: return kbd ? kbd->p1_in() : 0xFF;
    case L_P2: return p2_in();
    case L_ECS: return 0xFF;   // 3214 is write only
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
        return;
    case L_P2: p2_out = v; return;   // printer
    case L_ECS:
        // load the current status of the 3214 and enable the next interrupt
        irq_status = v & 7;
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

// 3214: the highest active request (R7 highest) interrupts the CPU when the
// 3214 is enabled and, with SGS, the level is above the current status. The
// 3212 then puts RST (7 - level) on the data bus.
void Jpr1::update_interrupt(void)
{
    if (model_a) {   // INT0 or INT1, no priority logic
        cpu.interrupt_pending = bus_irq != 0;
        if (bus_irq) cpu.interrupt_vector = rst7_jumper ? 0xFF : bus->inta();
        return;
    }
    uint8_t req = (uint8_t)~p2_in();
    int level = -1;
    for (int i = 7; i >= 0; i--)
        if (req & (1 << i)) { level = i; break; }
    bool want = irq_enabled && level >= 0 && (!irq_sgs || level > irq_status);
    cpu.interrupt_pending = want;
    if (want) cpu.interrupt_vector = (uint8_t)(0xC7 | ((7 - level) << 3));
}

void Jpr1::sync(void)
{
    bus->advance_all();
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
        i8080_step(&cpu);
        if (taking) irq_enabled = false;   // the 3214 waits for the next status write
        int n = (int)(cpu.cyc - before) + extra;
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
    uint8_t f = (uint8_t)(((int)cpu.sf << 7) | ((int)cpu.zf << 6) | ((int)cpu.hf << 4) | ((int)cpu.pf << 2) | 0x02 | (int)cpu.cf);
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
    case L_EPROM: return a < eprom.size() ? eprom[a] : 0xFF;
    case L_RAM: return ram[a & 0x3FF];
    case L_P0:   // reading the keyboard has side effects (key taken / seen): not from the debugger
    case L_P1: return 0xFF;
    case L_P2: return p2_in();
    case L_ECS: return 0xFF;
    default: return bus->mem_peek(a);
    }
}

int Jpr1::decode_mem(uint16_t addr, int, const char** what)
{
    switch (local(addr)) {
    case L_EPROM: *what = "EPROM"; return DEC_READ | DEC_ONCARD;
    case L_RAM: *what = "RAM"; return DEC_READ | DEC_WRITE | DEC_ONCARD;
    case L_P0: *what = "P0 (keyboard)"; return DEC_READ | DEC_WRITE | DEC_ONCARD;
    case L_P1: *what = "P1 (keyboard)"; return DEC_READ | DEC_WRITE | DEC_ONCARD;
    case L_P2: *what = "P2 (interrupts, printer)"; return DEC_READ | DEC_WRITE | DEC_ONCARD;
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
    out["cpu"] = "8080";
    out["cpu_hz"] = cpu_hz;
    out["eprom_bytes"] = eprom.size();
    out["ram"] = ram_fitted;
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
}
