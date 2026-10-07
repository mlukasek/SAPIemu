#include "cards/pkt1.h"
#include "core/state.h"
#include "core/bus.h"
#include "core/ihex.h"
#include "input/keyboards.h"
#include "input/parallel_kbd.h"

#include <string.h>

using nlohmann::json;

// Cards are synchronised and interrupt lines sampled every SYNC_CYCLES CPU cycles.
static const int SYNC_CYCLES = 64;

Pkt1::Pkt1() : CpuCard("PKT-1")
{
    memset(&z80, 0, sizeof(z80));
    ppi.dev = this;
}

Pkt1::~Pkt1() {}

bool Pkt1::configure(const json& cfg, std::string& err)
{
    long long mhz;
    std::string wait, boot, busy, rom, dec, iodec, chip;
    if (!cfg_number(cfg, "cpu_mhz", mhz, 2, err)) return false;
    if (mhz != 2 && mhz != 4) { err = "cpu_mhz must be 2 or 4 (XT5)"; return false; }
    cpu_hz = (uint32_t)(16000000 / (mhz == 4 ? 4 : 8));
    if (!cfg_string(cfg, "wait", wait, "rdy", err)) return false;
    if (wait != "rdy" && wait != "always") { err = "wait must be rdy or always (XT4)"; return false; }
    wait_always = wait == "always";
    if (!cfg_string(cfg, "boot", boot, "register", err)) return false;
    if (boot != "register" && boot != "off") { err = "boot must be register or off (XT3)"; return false; }
    boot_jumper = boot == "register";
    if (!cfg_bool(cfg, "map1", map1_out, true, err) || !cfg_bool(cfg, "map2", map2_out, true, err)) return false;

    if (!cfg_string(cfg, "rom", rom, "roms/pkt-1/PKT-1_MON-V100690_EPROM.bin", err)) return false;
    if (!cfg_string(cfg, "eprom", chip, "2716", err)) return false;
    static const char* const chips[] = { "2716", "2732", "2764", "27128", "27256", "27512" };
    unsigned size = 0;
    for (int i = 0; i < 6; i++)
        if (chip == chips[i]) size = 2048u << i;
    if (!size) { err = "eprom must be 2716, 2732, 2764, 27128, 27256 or 27512 (XT2)"; return false; }
    eprom_mask = size - 1;
    if (rom.empty()) eprom.assign(size, 0xFF);
    else {
        if (!rom_load(rom, eprom, size, 0xFF, 0, err)) return false;
        eprom.resize(size, 0xFF);
    }
    if (!cfg_string(cfg, "decoder", dec, "roms/pkt-1/PKT-1_DD8_74S287.bin", err)) return false;
    if (!cfg_string(cfg, "io_decoder", iodec, "roms/pkt-1/PKT-1_DD12_74S287.bin", err)) return false;
    if (!rom_load(dec, dd8, 256, 0xFF, 0, err)) return false;
    if (!rom_load(iodec, dd12, 256, 0xFF, 0, err)) return false;
    dd8.resize(256, 0xFF);
    dd12.resize(256, 0xFF);

    if (cfg.contains("keyboard") && !cfg["keyboard"].is_null()) {
        KeyboardDevice* k = keyboard_from_config(cfg["keyboard"], cpu_hz, err);
        if (!k) return false;
        set_keyboard(k);
        if (!kbd) { err = "PKT-1: the keyboard on XP2 must give a code and a strobe (EKL-1, Consul)"; return false; }
    }
    if (cfg.contains("printer") && !cfg["printer"].is_null()) {
        prn.reset(new Printer());
        if (!prn->configure(cfg["printer"], err)) return false;
        if (!cfg_string(cfg["printer"], "busy", busy, "pc3", err)) return false;
        if (busy != "pc3" && busy != "pc6") { err = "printer.busy must be pc3 or pc6"; return false; }
        busy_pc6 = busy == "pc6";
    }
    return true;
}

KeyboardDevice* Pkt1::keyboard()
{
    return kbd.get();
}

void Pkt1::set_keyboard(KeyboardDevice* k)
{
    // XP2 takes a keyboard with a code and a strobe (the 8255 latches the code)
    ParallelKeyboard* p = dynamic_cast<ParallelKeyboard*>(k);
    if (k && !p) {
        delete k;
        k = nullptr;
    }
    kbd.reset(k);
    pkbd = p;
    kstb_low = false;
    kbd_out = 0xFF;
    if (kbd) {
        kbd->bus = bus;
        kbd->reset();
        if (bus) kbd->advance(bus->now);
        keyboard_lines();
    }
}

void Pkt1::reset()
{
    // RESET: the Z80, the 8255 and the bus /RES (DD7 cleared: BOOT active, MAP1 = MAP2 = 0)
    pins = z80_reset(&z80);
    reg = 0;
    stall = 0;
    ppi.reset();
    next_sync = bus->now;
    stop_hit = false;
    at_boundary = false;
    instr_pc = 0;
    if (kbd) {
        kbd->reset();
        kstb_low = false;
        kbd_out = 0xFF;
        keyboard_lines();
    }
}

void Pkt1::advance(uint64_t now)
{
    if (!kbd) return;
    kbd->advance(now);
    // /KSTB (falling edge) latches the code into port B in mode 1
    bool low = !(pkbd->p0_in() & 1);
    if (low && !kstb_low && ppi.strobed_in_b()) ppi.strobe_b(pkbd->p1_in());
    kstb_low = low;
    keyboard_lines();
}

// /KACK (XP2/13) = IBF B through a 7407, /ZVUK (XP2/14) = PC5: told to the keyboard as its
// acknowledge (D0) and beeper (D1, active low) inputs
void Pkt1::keyboard_lines(void)
{
    if (!kbd) return;
    uint8_t v = (uint8_t)((ppi.b_input_full() ? 0x01 : 0) | ((ppi.out_c & 0x20) ? 0x02 : 0));
    if ((ppi.control() & 0x08) && !ppi.mode1_a()) v |= 0x02;   // PC5 an input: /ZVUK pulled up
    if (v != kbd_out) {
        kbd_out = v;
        kbd->p0_out(v);
    }
}

uint8_t Pkt1::pins_b()
{
    return pkbd ? pkbd->p1_in() : 0xFF;   // the code lines, 10k pull-ups
}

uint8_t Pkt1::pins_c()
{
    // PC3 / PC6: printer BUSY (1 = busy) by the wiring; the other one is not connected (pulled up)
    bool busy = prn ? prn->busy(bus->now) : false;
    uint8_t v = 0xFF;
    if (busy_pc6) v = (uint8_t)(busy ? v : (v & ~0x40));
    else if (prn) v = (uint8_t)(busy ? v : (v & ~0x08));
    return v;
}

void Pkt1::outputs_changed(int port, uint8_t value)
{
    if (port == 2) keyboard_lines();   // PC5 /ZVUK
}

bool Pkt1::io_read(uint8_t port, uint8_t& data)
{
    uint8_t o = dd12[port];
    if (!(o & 0x01)) {   // O1: 8255
        if (bus) advance(bus->now);
        data = ppi.read(port & 3);
        if ((port & 3) == 1) keyboard_lines();   // reading port B clears IBF: /KACK
        return true;
    }
    return false;   // 0Eh is write only; other ports are on the bus
}

bool Pkt1::io_write(uint8_t port, uint8_t data)
{
    uint8_t o = dd12[port];
    if (!(o & 0x01)) {
        uint8_t pc7 = ppi.out_c & 0x80;
        ppi.write(port & 3, data);
        // /PSTB: the ROM BIOS strobes the printer by PC7 (low, then high: the byte is taken)
        if (prn && !pc7 && (ppi.out_c & 0x80) && !(ppi.control() & 0x08)) prn->take(ppi.out_a, bus->now, bus->cpu_hz);
        keyboard_lines();
        return true;
    }
    if (!(o & 0x02)) {   // O2: the register DD7 (the bus also sees /IOW, the data buffer is off)
        reg = (uint8_t)(data & 0x07);
        return true;
    }
    return false;
}

int Pkt1::map_lines()
{
    return ((map1_out && (reg & 0x04)) ? MAP_1 : 0) | ((map2_out && (reg & 0x02)) ? MAP_2 : 0);
}

int Pkt1::decode_io(uint8_t port, const char** what)
{
    uint8_t o = dd12[port];
    if (!(o & 0x01)) {
        static const char* const n[4] = { "8255 PA (printer)", "8255 PB (keyboard)", "8255 PC", "8255 control" };
        *what = n[port & 3];
        return DEC_READ | DEC_WRITE;
    }
    if (!(o & 0x02)) { *what = "BOOT, MAP2, MAP1"; return DEC_WRITE; }
    return 0;
}

int Pkt1::decode_mem(uint16_t addr, int, const char** what)
{
    // the EPROM after reset (BOOT active): reads stay on the card, writes go to the bus
    if (!boot_jumper) return 0;
    if (dd8[((addr >> 11) & 0x1Fu) << 3] & 0x01) return 0;
    *what = "EPROM (BOOT)";
    return DEC_READ | DEC_ONCARD;
}

void Pkt1::sync(void)
{
    bus->advance_all();
    // /RTL (reset button, the RSW-1 watchdog): a reset of the whole system at the end of an M1
    bool rtl = false;
    for (Card* c : bus->cards) rtl |= c->rtl_line();
    if (rtl) {
        bus->reset();   // resets this card too: the Z80 starts again
        return;
    }
    int irq = bus->irq_lines();
    if (irq & IRQ_INT0) pins |= Z80_INT; else pins &= ~Z80_INT;
    if (irq & IRQ_INT1) pins |= Z80_NMI; else pins &= ~Z80_NMI;
    next_sync = bus->now + SYNC_CYCLES;
    if (++cpm_ticks >= 1024) {
        cpm_ticks = 0;
        bus->cpm_poll();
    }
}

int Pkt1::run(int cycles)
{
    stop_hit = false;
    if (cycles > 0) at_boundary = false;
    int done = 0;
    while (done < cycles) {
        bus->now++;
        done++;
        if (bus->now >= next_sync) sync();
        if (bus->hold) {   // a DMA controller had the bus: the CPU waited
            bus->now += bus->hold;
            done += (int)bus->hold;
            bus->hold = 0;
        }
        if (stall > 0) {
            stall--;
            continue;
        }
        pins = z80_tick(&z80, pins);

        if (pins & Z80_MREQ) {
            uint16_t addr = Z80_GET_ADDR(pins);
            if (pins & Z80_RD) {
                bus->wait = 0;
                uint8_t d = rom_read(addr) ? eprom[addr & eprom_mask] : bus->mem_read(addr);
                Z80_SET_DATA(pins, d);
                stall += bus->wait + (wait_always ? 1 : 0);
                if ((pins & Z80_M1) && !z80.prefix_active) {
                    instr_pc = addr;
                    bus->pc = addr;
                    if (addr == bus->cpm_seldsk && addr && z80.c < 16) bus->cpm_drive = z80.c;
                    bool step_stop = false;
                    if (step_mode) {
                        if (step_skip > 0) step_skip--;
                        else step_stop = true;
                    }
                    if (step_stop || (bp_exec && bp_exec[addr])) {
                        stop_hit = true;
                        at_boundary = true;
                        return done;
                    }
                }
            } else if (pins & Z80_WR) {
                bus->wait = 0;
                // in the EPROM window the data buffer is off: the bus sees no data (pull-ups)
                bus->mem_write(addr, buffer_off(addr) ? 0xFF : Z80_GET_DATA(pins));
                stall += bus->wait + (wait_always ? 1 : 0);
            }
            if (bus->break_request) {
                stop_hit = true;
                return done;
            }
        } else if (pins & Z80_IORQ) {
            uint8_t port = (uint8_t)Z80_GET_ADDR(pins);
            if (pins & Z80_M1) {
                Z80_SET_DATA(pins, bus->inta());
            } else if (pins & Z80_RD) {
                Z80_SET_DATA(pins, bus->io_read(port));
                if (wait_always) stall++;
            } else if (pins & Z80_WR) {
                bus->io_write(port, Z80_GET_DATA(pins));
                if (wait_always) stall++;
            }
            if (bus->break_request) {
                stop_hit = true;
                return done;
            }
        }
    }
    return done;
}

void Pkt1::get_regs(CpuRegs& r)
{
    r.af = z80.af; r.bc = z80.bc; r.de = z80.de; r.hl = z80.hl;
    r.ix = z80.ix; r.iy = z80.iy; r.sp = z80.sp;
    r.pc = instr_pc;
    r.af2 = z80.af2; r.bc2 = z80.bc2; r.de2 = z80.de2; r.hl2 = z80.hl2;
    r.i = z80.i; r.r = z80.r; r.im = z80.im;
    r.iff1 = z80.iff1; r.iff2 = z80.iff2;
    r.halted = (pins & Z80_HALT) != 0;
}

void Pkt1::set_regs(const CpuRegs& r)
{
    z80.af = r.af; z80.bc = r.bc; z80.de = r.de; z80.hl = r.hl;
    z80.ix = r.ix; z80.iy = r.iy; z80.sp = r.sp;
    z80.af2 = r.af2; z80.bc2 = r.bc2; z80.de2 = r.de2; z80.hl2 = r.hl2;
    z80.i = r.i; z80.r = r.r; z80.im = r.im;
    z80.iff1 = r.iff1; z80.iff2 = r.iff2;
    if (r.pc != instr_pc) {
        pins = z80_prefetch(&z80, r.pc);
        instr_pc = r.pc;
        at_boundary = false;
    }
}

uint8_t Pkt1::peek(uint16_t addr)
{
    if (rom_read(addr)) return eprom[addr & eprom_mask];
    return bus->mem_peek(addr);
}

void Pkt1::describe(json& out)
{
    out["cpu_hz"] = cpu_hz;
    out["boot"] = !boot_line();
    out["map1"] = (reg & 0x04) != 0;
    out["map2"] = (reg & 0x02) != 0;
    out["keyboard"] = kbd ? kbd->type_name() : "none";
    if (prn) out["printer"] = json{ { "output", prn->path() }, { "bytes", prn->bytes } };
}

void Pkt1::state(StateIO& io)
{
    uint32_t size = (uint32_t)sizeof(z80);
    io.v(size);
    if (io.loading && size != sizeof(z80)) { io.fail("Z80 core of another build"); return; }
    io.raw(&z80, sizeof(z80));
    io.v(pins);
    io.e(stall);
    io.v(next_sync);
    io.v(reg);
    ppi.state(io);
    io.v(kstb_low);
    io.v(kbd_out);
    io.v(at_boundary);
    io.v(instr_pc);
    if (prn) prn->state(io);
}
