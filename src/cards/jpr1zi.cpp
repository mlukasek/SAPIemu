#include "cards/jpr1zi.h"
#include "core/bus.h"
#include "core/ihex.h"
#include "core/log.h"
#include "input/keyboards.h"

#include <string.h>

using nlohmann::json;

// Cards are synchronised and interrupt lines sampled every SYNC_CYCLES CPU cycles.
static const int SYNC_CYCLES = 64;

Jpr1zi::Jpr1zi() : CpuCard("JPR-1ZI")
{
    memset(&z80, 0, sizeof(z80));
}

Jpr1zi::~Jpr1zi() {}

bool Jpr1zi::configure(const json& cfg, std::string& err)
{
    // Clock: 16 MHz crystal divided by 74LS93; jumpers JP2/JP3 select 2 or 4 MHz
    // independently for the CPU and for the bus clock F2.
    long long crystal, cpu_div_mhz, bus_mhz;
    if (!cfg_number(cfg, "crystal_hz", crystal, 16000000, err)) return false;
    if (!cfg_number(cfg, "cpu_mhz", cpu_div_mhz, 2, err)) return false;
    if (!cfg_number(cfg, "bus_mhz", bus_mhz, 2, err)) return false;
    if ((cpu_div_mhz != 2 && cpu_div_mhz != 4) || (bus_mhz != 2 && bus_mhz != 4)) {
        err = "cpu_mhz and bus_mhz must be 2 or 4 (JP2/JP3)";
        return false;
    }
    // QC = crystal/4 (4 MHz), QD = crystal/8 (2 MHz)
    cpu_hz = (uint32_t)(cpu_div_mhz == 4 ? crystal / 4 : crystal / 8);
    bus_hz = (uint32_t)(bus_mhz == 4 ? crystal / 4 : crystal / 8);

    std::string rom, decoder, d6b_file;
    if (!cfg_string(cfg, "rom", rom, "", err)) return false;
    if (!cfg_string(cfg, "decoder", decoder, "roms/jpr-1zi/JPR-1ZI_D3C_2k.bin", err)) return false;
    if (!cfg_string(cfg, "io_decoder", d6b_file, "roms/jpr-1zi/JPR-1ZI_D6B.bin", err)) return false;
    if (decoder == "2k") decoder = "roms/jpr-1zi/JPR-1ZI_D3C_2k.bin";
    if (decoder == "8k") decoder = "roms/jpr-1zi/JPR-1ZI_D3C_8k.bin";

    if (rom.empty()) {
        eprom.assign(8192, 0xFF);
    } else if (!rom_load(rom, eprom, 8192, 0xFF, 0, err)) {
        return false;
    }
    if (!rom_load(decoder, d3c, 256, 0x00, 0, err)) return false;
    if (!rom_load(d6b_file, d6b, 256, 0x0F, 0, err)) return false;

    if (cfg.contains("keyboard") && !cfg["keyboard"].is_null()) {
        const json& k = cfg["keyboard"];
        std::string ktype = k.value("type", std::string("EKL-1"));
        kbd.reset(keyboard_create(ktype));
        if (!kbd) {
            err = "unknown keyboard type '" + ktype + "'";
            return false;
        }
        if (!keyboard_configure(kbd.get(), k, cpu_hz, err)) return false;
    }
    return true;
}

void Jpr1zi::reset()
{
    // /RES: BOOT flip-flop set, 3212 outputs cleared, CPU reset
    pins = z80_reset(&z80);
    boot = true;
    stall = 0;
    memset(port_out, 0, sizeof(port_out));
    next_sync = bus->now;
    stop_hit = false;
    instr_pc = 0;
    if (kbd) {
        kbd->reset();
        kbd->p0_out(port_out[0]);
    }
}

void Jpr1zi::advance(uint64_t now)
{
    if (kbd) kbd->advance(now);
}

// D3C inputs: A0=A13, A1=A12, A2=GND, A3=A14, A4=A15, A5=A11, A6=BOOT, A7=GND;
// output O4 (bit 3) = ROM/RAM.
bool Jpr1zi::rom_selected(uint16_t a) const
{
    unsigned idx = ((a >> 13) & 1) | (((a >> 12) & 1) << 1) | (((a >> 14) & 1) << 3) |
                   (((a >> 15) & 1) << 4) | (((a >> 11) & 1) << 5) | ((boot ? 1u : 0u) << 6);
    return (d3c[idx] & 0x08) != 0;
}

// D6B inputs: A0-A7 of the port address; outputs active low:
// bit3 = port 0 (BOOT off), bit2 = P0, bit1 = P1, bit0 = P2.
int Jpr1zi::io_select(uint8_t port) const
{
    uint8_t o = d6b[port];
    if (!(o & 0x08)) return 0;
    if (!(o & 0x04)) return 1;
    if (!(o & 0x02)) return 2;
    if (!(o & 0x01)) return 3;
    return -1;
}

bool Jpr1zi::io_read(uint8_t port, uint8_t& data)
{
    switch (io_select(port)) {
    case 1: data = kbd ? kbd->p0_in() : 0xFF; return true;
    case 2: data = kbd ? kbd->p1_in() : 0xFF; return true;
    case 3: data = 0xFF; return true;   // P2-IN: printer connector X3, not connected
    default: return false;              // port 00h is write only
    }
}

bool Jpr1zi::io_write(uint8_t port, uint8_t data)
{
    int sel = io_select(port);
    if (sel < 0) return false;
    if (sel == 0) {
        boot = false;
    } else {
        port_out[sel - 1] = data;
        if (sel == 1 && kbd) kbd->p0_out(data);
    }
    return true;
}

void Jpr1zi::sync(void)
{
    bus->advance_all();
    int irq = bus->irq_lines();
    if (irq & IRQ_INT0) pins |= Z80_INT; else pins &= ~Z80_INT;
    if (irq & IRQ_INT1) pins |= Z80_NMI; else pins &= ~Z80_NMI;
    next_sync = bus->now + SYNC_CYCLES;
}

int Jpr1zi::run(int cycles)
{
    stop_hit = false;
    int done = 0;
    while (done < cycles) {
        bus->now++;
        done++;
        if (bus->now >= next_sync) sync();
        if (stall > 0) {   // RDY low: CPU held in a wait state
            stall--;
            continue;
        }
        pins = z80_tick(&z80, pins);

        if (pins & Z80_MREQ) {
            uint16_t addr = Z80_GET_ADDR(pins);
            if (pins & Z80_RD) {
                bus->wait = 0;
                uint8_t d;
                if (rom_selected(addr)) d = eprom[addr & 0x1FFF];
                else d = bus->mem_read(addr);
                Z80_SET_DATA(pins, d);
                stall += bus->wait;
                if ((pins & Z80_M1) && !z80.prefix_active) {
                    // opcode fetch of a new instruction
                    instr_pc = addr;
                    if (step_mode || (bp_exec && bp_exec[addr])) {
                        stop_hit = true;
                        return done;
                    }
                }
            } else if (pins & Z80_WR) {
                bus->wait = 0;
                bus->mem_write(addr, Z80_GET_DATA(pins));
                stall += bus->wait;
            }
        } else if (pins & Z80_IORQ) {
            uint8_t port = (uint8_t)Z80_GET_ADDR(pins);
            if (pins & Z80_M1) {
                // interrupt acknowledge
                uint8_t v = bus->inta();
                Z80_SET_DATA(pins, v);
            } else if (pins & Z80_RD) {
                // on-card ports are served through io_read() like any other card
                uint8_t d = bus->io_read(port);
                Z80_SET_DATA(pins, d);
            } else if (pins & Z80_WR) {
                bus->io_write(port, Z80_GET_DATA(pins));
            }
        }
    }
    return done;
}

void Jpr1zi::get_regs(CpuRegs& r)
{
    r.af = z80.af; r.bc = z80.bc; r.de = z80.de; r.hl = z80.hl;
    r.ix = z80.ix; r.iy = z80.iy; r.sp = z80.sp;
    r.pc = instr_pc;
    r.af2 = z80.af2; r.bc2 = z80.bc2; r.de2 = z80.de2; r.hl2 = z80.hl2;
    r.i = z80.i; r.r = z80.r; r.im = z80.im;
    r.iff1 = z80.iff1; r.iff2 = z80.iff2;
    r.halted = (pins & Z80_HALT) != 0;
}

void Jpr1zi::set_regs(const CpuRegs& r)
{
    z80.af = r.af; z80.bc = r.bc; z80.de = r.de; z80.hl = r.hl;
    z80.ix = r.ix; z80.iy = r.iy; z80.sp = r.sp;
    z80.af2 = r.af2; z80.bc2 = r.bc2; z80.de2 = r.de2; z80.hl2 = r.hl2;
    z80.i = r.i; z80.r = r.r; z80.im = r.im;
    z80.iff1 = r.iff1; z80.iff2 = r.iff2;
    if (r.pc != instr_pc) {
        // restart the instruction fetch at the new address
        pins = z80_prefetch(&z80, r.pc);
        instr_pc = r.pc;
    }
}

uint8_t Jpr1zi::peek(uint16_t addr)
{
    if (rom_selected(addr)) return eprom[addr & 0x1FFF];
    return bus->mem_peek(addr);
}

void Jpr1zi::describe(json& out)
{
    out["cpu_hz"] = cpu_hz;
    out["bus_hz"] = bus_hz;
    out["boot"] = boot;
    out["keyboard"] = kbd ? kbd->type_name() : "none";
}
