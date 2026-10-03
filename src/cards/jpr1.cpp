#include "cards/jpr1.h"
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

Jpr1::Jpr1() : CpuCard("JPR-1")
{
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
    }
    return true;
}

void Jpr1::set_keyboard(KeyboardDevice* k)
{
    kbd.reset(k);
    if (kbd) {
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
int Jpr1::local(uint16_t a) const
{
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
    case L_EPROM: return eprom[a];
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
    switch (local(a)) {
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
    return bus->io_read(port);
}

void Jpr1::cpu_out(uint8_t port, uint8_t v)
{
    bus->io_write(port, v);
}

// 3214: the highest active request (R7 highest) interrupts the CPU when the
// 3214 is enabled and, with SGS, the level is above the current status. The
// 3212 then puts RST (7 - level) on the data bus.
void Jpr1::update_interrupt(void)
{
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
}

int Jpr1::run(int cycles)
{
    stop_hit = false;
    bool resume = at_boundary;   // stopped before the instruction at instr_pc: execute it first
    if (cycles > 0) at_boundary = false;
    int done = 0;
    while (done < cycles) {
        if (bus->now >= next_sync) sync();
        if (!resume) {
            // instruction boundary (where the Z80 card stops after the opcode fetch)
            instr_pc = cpu.pc;
            bus->pc = cpu.pc;
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
    case L_EPROM: return eprom[a];
    case L_RAM: return ram[a & 0x3FF];
    case L_P0: return kbd ? kbd->p0_in() : 0xFF;
    case L_P1: return kbd ? kbd->p1_in() : 0xFF;
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

void Jpr1::describe(json& out)
{
    out["cpu"] = "8080";
    out["cpu_hz"] = cpu_hz;
    out["eprom_bytes"] = eprom.size();
    out["ram"] = ram_fitted;
    out["keyboard"] = kbd ? kbd->type_name() : "none";
}
