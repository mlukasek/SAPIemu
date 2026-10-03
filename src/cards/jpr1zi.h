#pragma once
// JPR-1ZI - Z80 CPU card (see docs/desky/JPR-1ZI.md).
//
// - EPROM 2764 visible while BOOT=1 according to decoder PROM D3C; reads come
//   from the EPROM, writes always go to the bus.
// - I/O decoder PROM D6B: port 00h write = BOOT off, ports 01-03h = 3212 P0-P2.
// - Keyboard on connector X2 (P0/P1).
#include <memory>
#include <vector>
#include "core/devices.h"
#include "z80.h"

class Jpr1zi : public CpuCard {
public:
    // "JPR-1ZI", or "JPR-1Z" (ZPS-4): the same card with a 2716 (2 KB, mirrored)
    // and without the XINTA helper (not modelled anyway); decoders D14 = D3C, D10 = D6B
    explicit Jpr1zi(const char* type = "JPR-1ZI");
    ~Jpr1zi() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
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
    int decode_io(uint8_t port, const char** what) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;

    bool boot = true;

private:
    bool rom_selected(uint16_t addr) const { return rom_select(addr, boot); }
    bool rom_select(uint16_t addr, bool boot_ff) const;
    int io_select(uint8_t port) const;   // -1 none, 0 = port 0 (BOOT), 1..3 = P0..P2
    void sync(void);

    z80_t z80;
    uint64_t pins = 0;
    int stall = 0;                // remaining wait-state cycles
    uint64_t next_sync = 0;
    uint32_t cpu_hz = 2000000;
    uint32_t bus_hz = 2000000;

    std::vector<uint8_t> eprom;   // 8 KB (2764); JPR-1Z: 2716 mirrored to 8 KB
    std::vector<uint8_t> d3c;     // memory decoder PROM
    std::vector<uint8_t> d6b;     // I/O decoder PROM
    uint8_t port_out[3] = {0, 0, 0};
    std::unique_ptr<KeyboardDevice> kbd;
};
