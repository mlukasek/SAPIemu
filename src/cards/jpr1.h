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
#include <memory>
#include <vector>
#include "core/devices.h"

extern "C" {
#include "i8080.h"
}

class Jpr1 : public CpuCard {
public:
    explicit Jpr1(const char* type = "JPR-1");
    ~Jpr1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    void advance(uint64_t now) override;
    void describe(nlohmann::json& out) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    int decode_io(uint8_t port, const char** what) override;

    int run(int cycles) override;
    void get_regs(CpuRegs& r) override;
    void set_regs(const CpuRegs& r) override;
    uint8_t peek(uint16_t addr) override;
    KeyboardDevice* keyboard() override { return kbd.get(); }
    void set_keyboard(KeyboardDevice* k) override;
    uint32_t clock_hz() const override { return cpu_hz; }

    // CPU memory and port accesses (8080 core callbacks)
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t data);
    uint8_t cpu_in(uint8_t port);
    void cpu_out(uint8_t port, uint8_t data);

private:
    enum Local { L_NONE = 0, L_EPROM, L_RAM, L_P0, L_P1, L_P2, L_ECS, L_UNUSED };
    int local(uint16_t addr) const;      // on-card function at an address
    uint8_t p2_in(void) const;           // interrupt requests and the rest of P2-IN
    void update_interrupt(void);
    void sync(void);

    i8080 cpu;
    uint32_t cpu_hz = 2000000;
    uint64_t next_sync = 0;
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

    // JPR-1A
    uint8_t d2c(uint8_t a_hi) const;     // PROM outputs for A15-A8 in the current BOOT state
    bool model_a = false;
    bool boot = true;                    // BOOT flip-flop (LED V2)
public:
    // JPR-1A: the BOOT LED V2; JPR-1: the LED on the card at P1-OUT D7 (1 = lit, assumed)
    unsigned panel_leds() override { return (model_a ? boot : (p1_out & 0x80) != 0) ? 1u : 0u; }
private:
    bool rst7_jumper = true;             // JP1 1-2: RST 7, else the vector from the bus
    uint8_t d2c_prom[512];
    std::string d2c_file;
};
