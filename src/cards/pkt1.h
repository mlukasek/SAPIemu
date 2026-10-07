#pragma once
// PKT-1 - CPU card of SAPI-1.5 (Tesla Liberec 1990: "Procesor, Klávesnice, Tiskárna"), see
// docs/desky/PKT-1.md.
//
// - Z80A, 16 MHz crystal / 74LS93: CPU 2 MHz (XT5 24-25) or 4 MHz (23-24), bus F2 always 2 MHz.
// - EPROM (2716..27512, XT2 only routes the socket pins) read while BOOT = L where PROM DD8 says:
//   the Tesla DD8 gives 0000-07FF only; reads stay on the card, writes always go to the bus
//   (in the window with the data buffer off: the bus RAM gets an undriven data bus).
// - Register DD7 at port 0Eh (write only, cleared by /RES): D0 = BOOT (0 = EPROM on, bus pin 47),
//   D1 = MAP2, D2 = MAP1 (bus pins 22, 21 through XT1). The ROM BIOS maps the DBS-1 video RAM
//   with MAP2.
// - PROM DD12 decodes the ports: 00-03h 8255, 0Eh the register.
// - 8255: port A printer data (XP3), port B keyboard (XP2, mode 1: /KSTB = PC2 latches the code,
//   IBF = PC1 drives /KACK), PC4 /NUL, PC5 /ZVUK (beeper), PC7 printer /STROBE (the ROM BIOS
//   strobes it by bit set/reset), printer BUSY on PC3 (the fixed board) or PC6 (the original
//   Tesla wiring, which makes the printer unusable).
// - Bus /INT0 -> /INT, /INT1 -> /NMI. /RTL (reset button, RSW-1 watchdog) resets the whole system.
// - XT4: wait states only from RDY, or one more in every machine cycle.
#include <memory>
#include <vector>
#include "chips/i8255.h"
#include "chips/printer.h"
#include "core/devices.h"
#include "z80.h"

class ParallelKeyboard;

class Pkt1 : public CpuCard, public I8255Peripheral {
public:
    Pkt1();
    ~Pkt1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int map_lines() override;
    void describe(nlohmann::json& out) override;

    int run(int cycles) override;
    void get_regs(CpuRegs& r) override;
    void set_regs(const CpuRegs& r) override;
    uint8_t peek(uint16_t addr) override;
    KeyboardDevice* keyboard() override;
    void set_keyboard(KeyboardDevice* k) override;
    uint32_t clock_hz() const override { return cpu_hz; }
    int decode_io(uint8_t port, const char** what) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    Printer* printer() override { return prn.get(); }
    void flush() override { if (prn) prn->flush(); }
    unsigned panel_leds() override { return boot_line() ? 0u : 1u; }   // HL1 lit while BOOT is active

    // I8255Peripheral: the connectors XP2 (keyboard) and XP3 (printer)
    uint8_t pins_b() override;
    uint8_t pins_c() override;
    void outputs_changed(int port, uint8_t value) override;

private:
    bool boot_line(void) const { return !boot_jumper || (reg & 1); }   // bus pin 47, L = EPROM on
    uint8_t dd8_out(uint16_t addr) const { return dd8[(boot_line() ? 4u : 0u) | (((addr >> 11) & 0x1Fu) << 3)]; }
    bool rom_read(uint16_t addr) const { return !(dd8_out(addr) & 0x01); }   // O1 /CSROM
    bool buffer_off(uint16_t addr) const { return (dd8_out(addr) & 0x02) != 0; }   // O2 = 1: not external
    void sync(void);
    void keyboard_lines(void);

    z80_t z80;
    uint64_t pins = 0;
    int stall = 0;
    uint64_t next_sync = 0;
    unsigned cpm_ticks = 0;
    uint32_t cpu_hz = 2000000;
    bool wait_always = false;     // XT4 21-22: one wait state in every machine cycle
    bool boot_jumper = true;      // XT3 6-7: BOOT from the register (5-6: EPROM always off)
    bool map1_out = true, map2_out = true;   // XT1
    bool busy_pc6 = false;        // printer BUSY on PC6 (original Tesla wiring) instead of PC3

    std::vector<uint8_t> eprom;   // the chip (2 KB .. 64 KB)
    unsigned eprom_mask = 0x7FF;
    std::vector<uint8_t> dd8;     // memory decoder PROM
    std::vector<uint8_t> dd12;    // I/O decoder PROM
    uint8_t reg = 0;              // DD7: D0 BOOT, D1 MAP2, D2 MAP1
    I8255 ppi;
    std::unique_ptr<KeyboardDevice> kbd;
    ParallelKeyboard* pkbd = nullptr;   // the same keyboard (strobe and data lines)
    bool kstb_low = false;        // /KSTB as seen last time
    uint8_t kbd_out = 0xFF;       // lines to the keyboard last told: D0 /KACK (IBF), D1 /ZVUK
    std::unique_ptr<Printer> prn;
};
