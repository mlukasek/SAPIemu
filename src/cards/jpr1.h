#pragma once
// JPR-1 - the original CPU card of SAPI-1 (see docs/desky/JPR-1.md).
//
// - Intel 8080A (MHB 8080A + 8224 + 8228), 18 MHz crystal / 9 = 2 MHz.
// - Memory mapped ports (MH3212 latches) decoded by D9 in 2000-3FFF:
//   2000-23FF RAM (2x 2114, optional), 2400 P0, 2800 P1, 2C00 P2, 3000 3214 write.
//   Each port is mirrored over its 1 KB block. 3400-3FFF is left to the bus
//   (AND-1 VRAM at 3800).
// - EPROM sockets D4E-D7E (D10): 2708 in 1 KB pages from 0000, 2716 in 2 KB pages.
//   In ZPS-1 they are empty (MIKROBASIC is on REM-1).
// - ANK-1 keyboard: P0-OUT D4-D0 rows, P0-IN columns, P1-IN D7 INT key, D6 T key.
// - Interrupts: MH3214 with the requests on P2-IN (R0 = bus /INT0, R1 = bus /INT1,
//   R7 = INT key via jumper 10-11) and a 3212 that puts RST (7 - level) on the bus.
//
// JPR-1A (ZPS-3, docs/desky/JPR-1A.md) is the same 8080 with another decoder,
// the PROM D2C (74S571, exchangeable): inputs A15, A9, A8, A12, A13, A14, A10,
// A11, BOOT; outputs active low D0 PORTY, D1 INT/EXT, D2 OE1, D3 OE2.
// - RESET sets the BOOT flip-flop: EPROM (2x 2716, D3B 0000-07FF, D4B 0800-0FFF)
//   is read at 0000-0FFF, writes go to the RAM below; any OUT 00h clears it.
// - Ports P0-P2 are I/O ports 01-03 (keyboard on P0/P1 as on the JPR-1ZI).
// - No 3214: INT0 or INT1 interrupt, jumper JP1 1-2 = RST 7, 1-3 = vector from the bus.
//
// JPR-1S rev 2.2 (Libor Lasota, docs/desky/JPR-1S.md): a JPR-1 remake, the same 8080 and
// memory mapped ports, decoded by a 74ALS138 + 74ALS153 (no PROM):
// - 0000-1FFF 28C64 (8 KB, never written), 2000-23FF RAM, 2400/2800/2C00 P0/P1/P2, 3000-3FFF
//   the bus (no 3214), 4000/8000/C000 16 KB RAM banks, each by a jumper RAM_EN (one 628128).
// - Every write to an address on the card goes to the bus too (8228 /BUSEN grounded): the
//   write-only video RAM of BGP-1S at C000 is read back from bank 3.
// - Interrupt: level request from the jumpers INT_SRC (bus /INT0, /INT1, P2-IN0, P2-IN1,
//   wired OR), response INT_ADR RST 7 (8228) or the vector from the bus (/INTA).
// - P2-IN on K3 with pull-ups (FFh with nothing connected), LED on P1-OUT7.
//
// JPR-1X rev 1.0 (Libor Lasota 2025, docs/desky/JPR-1X.md): a JPR-1 remake with an 80C85
// (8085 mode of the 8080 core), the JPR-1 map with memory mapped ports and the 3214:
// - 0000-1FFF one of two 8 KB banks of a 27C256 (A13 = /BIOS latched at reset: jumper BIOS
//   fitted = the lower bank), 2000-23FF RAM 6116 (1 KB), 2400/2800/2C00 P0/P1/P2, 3000 3214.
// - Every write goes to the bus too; reads from 3000-FFFF come from the bus.
// - 3214 INTE tied high: a request is latched into the vector 3212 even under DI. The vector
//   is RST 7 (RSTN/7 open) or RST (7 - level) (RSTN/7 fitted). INT key on R7 (jumper 11-10),
//   R6 (10-R6) or nowhere (factory).
// - Oscillator 16 MHz: the bus 2 MHz always, the CPU 2 MHz or 4 MHz by the TURBO jumper (live).
//   The time base is 4 MHz; at 2 MHz a CPU cycle takes two units.
// - TRAP, RST 5.5/6.5/7.5 grounded, SID connected to SOD.
//
// Redukce34KB01 (option for JPR-1 and JPR-1S, the cable adapter of the ANK-1M5/M2 keyboard on
// K2): an optocoupler puts an RS-232 receive line on P1-IN7 (idle 1, start bit 0); K2 pins 7
// and 9 of the keyboard (INT, T) are not connected. MIKROBIOS BOOT reads Intel HEX from it
// bit by bit (19200 Bd, 2 stop bits).
#include <memory>
#include <vector>
#include "core/devices.h"

class SerialChannel;

extern "C" {
#include "i8080.h"
}

class Jpr1 : public CpuCard {
public:
    explicit Jpr1(const char* type = "JPR-1");
    ~Jpr1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    void advance(uint64_t now) override;
    void describe(nlohmann::json& out) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    int decode_io(uint8_t port, const char** what) override;

    int run(int cycles) override;
    void get_regs(CpuRegs& r) override;
    void set_regs(const CpuRegs& r) override;
    uint8_t peek(uint16_t addr) override;
    void poke_local(uint16_t addr, uint8_t value) override;
    KeyboardDevice* keyboard() override { return kbd.get(); }
    void set_keyboard(KeyboardDevice* k) override;
    uint32_t clock_hz() const override { return cpu_hz; }
    int cpu_isa() const override { return model_x ? ISA_8085 : ISA_8080; }
    // the diagnostic board on K3 (JPR-1S, JPR-1X)
    bool k3_diag() const override { return diag; }
    uint8_t k3_display(int i) const override { return diag ? (i == 0 ? p2_out : p1_out) : 0; }
    void k3_button(int b, bool down) override { if (diag && b >= 0 && b < 2) diag_btn[b] = down; }
    bool k3_button_down(int b) const override { return diag && b >= 0 && b < 2 && diag_btn[b]; }
    // JPR-1X: TURBO (live) and BIOS (read at reset)
    bool live_param(const char* path) const override;
    bool set_live_param(const char* path, const nlohmann::json& v, std::string& err) override;
    bool has_cpu_turbo() const override { return model_x; }
    bool cpu_turbo() const override { return turbo_on; }
    void set_cpu_turbo(bool on) override { turbo_on = on; }
    uint32_t cpu_display_hz() const override { return model_x && !turbo_on ? cpu_hz / 2 : cpu_hz; }
    SerialChannel* serial_channel(int index) override { return index == 0 ? rx.get() : nullptr; }

    // CPU memory and port accesses (8080 core callbacks)
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t data);
    uint8_t cpu_in(uint8_t port);
    void cpu_out(uint8_t port, uint8_t data);

private:
    enum Local { L_NONE = 0, L_EPROM, L_RAM, L_P0, L_P1, L_P2, L_ECS, L_UNUSED, L_BANK };
    int local(uint16_t addr) const;      // on-card function at an address
    uint8_t p1_in(void);                 // keyboard, or the adapter on K2
    uint8_t p2_in(void);                 // interrupt requests and the rest of P2-IN
    bool rx_level(void);                 // the RS-232 line of the adapter now (1 = idle / stop)
    void update_interrupt(void);
    void sync(void);

    i8080 cpu;
    uint32_t cpu_hz = 2000000;
    uint64_t next_sync = 0;
    unsigned cpm_ticks = 0;
    int extra = 0;                       // wait states of the current instruction
    bool wait_jumper = false;            // 12-14: one wait state per memory access

    std::vector<uint8_t> eprom;          // the populated sockets, from 0000
    int eprom_page = 0x400;              // 2708: 1 KB, 2716: 2 KB
    bool ram_fitted = false;             // 2x 2114 at 2000-23FF
    uint8_t ram[1024];
    uint8_t p0_out = 0, p1_out = 0, p2_out = 0;
    bool int_key_jumper = true;          // 10-11: INT key -> P2-IN7 (interrupt request R7)

    // MH3214
    bool irq_enabled = false;            // set by a write to 3000h, cleared when an interrupt is taken
    int irq_status = 0;                  // current status B0-B2
    bool irq_sgs = false;                // status group select: only levels above the status
    int bus_irq = 0;                     // INT0/INT1 levels from the bus (IrqLine mask)

    std::unique_ptr<KeyboardDevice> kbd;

    // JPR-1S
    bool model_s = false;
    std::vector<uint8_t> sram;           // 628128: 2000-23FF and the banks, indexed by the address
    bool bank_en[3] = { true, true, true };   // RAM_EN: 4000, 8000, C000
    bool int_src[4] = { false, true, false, false };   // INT_SRC: /INT0, /INT1, P2-IN0, P2-IN1

    // Redukce34KB01 on K2: RS-232 receive line on P1-IN7
    std::unique_ptr<SerialChannel> rx;
    uint32_t rx_baud = 19200;
    int rx_stop = 2;
    bool rx_active = false;              // a character is on the line
    uint8_t rx_byte = 0;
    uint64_t rx_start = 0;               // bus time of its start bit
    uint64_t rx_free = 0;                // the line is free (idle) from this time

    // JPR-1A
    uint8_t d2c(uint8_t a_hi) const;     // PROM outputs for A15-A8 in the current BOOT state
    bool model_a = false;
    bool boot = true;                    // BOOT flip-flop (LED V2)
public:
    // JPR-1A: the BOOT LED V2; JPR-1: the LED on the card at P1-OUT D7 (1 = lit, assumed; JPR-1S: so)
    unsigned panel_leds() override { return (model_a ? boot : (p1_out & 0x80) != 0) ? 1u : 0u; }
private:
    bool rst7_jumper = true;             // JP1 1-2 (JPR-1S INT_ADR 2-3, JPR-1X RSTN/7 open): RST 7, else the
                                         // vector from the bus (JPR-1X: RST 7 - level)
    // the diagnostic board on K3
    bool diag = false;
    bool diag_btn[2] = { false, false };   // "+" (P2-IN0), "-" (P2-IN7)
    // JPR-1X
    bool model_x = false;
    bool turbo_on = false;               // TURBO fitted: CPU 4 MHz (else 2 MHz, two time units a cycle)
    bool bios_fitted = true;             // jumper BIOS now
    uint32_t bios_off = 0;               // the bank latched at reset (offset in eprom)
    int int_key_r = -1;                  // INT key on request R7 (11-10), R6 (10-R6), -1 nowhere
    bool x_rom_fitted = true;            // a ROM in IC11 (else its reads come from the bus)
    bool x_latched = false;              // the 3212 IC7 holds a vector for INTR
    uint8_t x_vector = 0xFF;
    uint8_t d2c_prom[512];
    std::string d2c_file;
};
