#pragma once
// JPR-1ZI - Z80 CPU card (see docs/desky/JPR-1ZI.md).
//
// - EPROM 2764 visible while BOOT=1 according to decoder PROM D3C; reads come
//   from the EPROM, writes always go to the bus.
// - I/O decoder PROM D6B: port 00h write = BOOT off, ports 01-03h = 3212 P0-P2.
// - Keyboard on connector X2 (P0/P1).
#include <memory>
#include <vector>
#include "chips/printer.h"
#include "core/devices.h"
#include "z80.h"

class Jpr1zi : public CpuCard {
public:
    // "JPR-1ZI", or "JPR-1Z" (ZPS-4): the same card with a 2716 (2 KB, mirrored)
    // and without the XINTA helper (not modelled anyway); decoders D14 = D3C, D10 = D6B
    explicit Jpr1zi(const char* type = "JPR-1ZI");
    // "JPR-1V" (Libor Lasota): fixed TTL decoding equal to D3C 2k + D6B, 28C64 with
    // two 2 KB banks at 0000-07FF chosen by the BIOS jumper, 8 MHz oscillator:
    // bus 2 MHz, CPU 2 or 4 MHz (TURBO)
    ~Jpr1zi() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    void describe(nlohmann::json& out) override;

    int run(int cycles) override;
    void get_regs(CpuRegs& r) override;
    void set_regs(const CpuRegs& r) override;
    uint8_t peek(uint16_t addr) override;
    KeyboardDevice* keyboard() override { return kbd.get(); }
    void set_keyboard(KeyboardDevice* k) override;
    uint32_t clock_hz() const override { return cpu_hz; }
    uint32_t phi2_hz() const override { return bus_hz; }
    bool has_cpu_turbo() const override { return turbo_switch; }
    bool cpu_turbo() const override { return turbo_on; }
    void set_cpu_turbo(bool on) override { turbo_on = on; }
    bool reset_blocked() const override { return hung; }
    // JPR-1V: the TURBO switch and the BIOS jumper (EEPROM bank) work while running, as when
    // they are wired to the buttons of the ZDR panel.
    bool live_param(const char* path) const override;
    bool set_live_param(const char* path, const nlohmann::json& v, std::string& err) override;
    uint32_t cpu_display_hz() const override { return turbo_switch && !turbo_on ? cpu_hz / 2 : cpu_hz; }
    int decode_io(uint8_t port, const char** what) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    Printer* printer() override { return prn.get(); }
    void flush() override { if (prn) prn->flush(); }

    bool boot = true;
    unsigned panel_leds() override { return boot ? 1u : 0u; }   // the BOOT LED (D2), lit from reset to OUT (00h)

private:
    bool rom_selected(uint16_t addr) const { return rom_select(addr, boot); }
    bool rom_select(uint16_t addr, bool boot_ff) const;
    int io_select(uint8_t port) const;   // -1 none, 0 = port 0 (BOOT), 1..3 = P0..P2
    void sync(void);

    z80_t z80;
    uint64_t pins = 0;
    int stall = 0;                // remaining wait-state cycles
    uint64_t next_sync = 0;
    unsigned cpm_ticks = 0;
    uint32_t cpu_hz = 2000000;
    uint32_t bus_hz = 2000000;

    std::vector<uint8_t> eprom;   // 8 KB (2764); JPR-1Z: 2716 mirrored to 8 KB
    unsigned eprom_off = 0;       // JPR-1V: the bank selected by the BIOS jumper
    bool turbo_switch = false;    // JPR-1V: time base osc/2; without TURBO the CPU ticks every other unit
    bool turbo_on = false;
    bool half_phase = false;
    // JPR-1V: an undriven data bus reads as its last byte (instruction fetch, read, write, INTA), measured on
    // the author's board 2026-10-11. Not in the saved state (one byte, the next cycle sets it again).
    bool open_bus_hold = false;
    uint8_t dbus = 0xFF;
    // JPR-1V: STSTB also in the INTA cycle; a card holding RDY then stops the CPU in the INTA cycle for good, and
    // RESET (synchronised with /M1) cannot come (measured 2026-10-11). Only switching off helps. Not in the saved
    // state: a loaded state runs on.
    bool ststb_inta = false;
    bool hung = false;
    unsigned eprom_mask = 0x1FFF;
    std::vector<uint8_t> d3c;     // memory decoder PROM
    std::vector<uint8_t> d6b;     // I/O decoder PROM
    uint8_t port_out[3] = {0, 0, 0};
    std::unique_ptr<KeyboardDevice> kbd;
    // printer on X3 (the Centronics cable of the manual): data P2-OUT, /STROBE P1-OUT1 (the byte
    // is taken at its rising edge), BUSY P2-IN6, pin 16 = P2-IN7 (ECMON drops the bytes at 1)
    std::unique_ptr<Printer> prn;
    bool prn_pin16 = false;
};
