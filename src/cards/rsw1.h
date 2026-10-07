#pragma once
// RSW-1 - SAPI-1.5 card "RAM, Sériový port, Watch dog" (Tesla Liberec 1990), see docs/desky/RSW-1.md.
//
// - 64 KB dynamic RAM (4164, RAS-only refresh from the Z80 /REF: Z80 CPU cards only). PROM DD1
//   (74S571) selects it by 1 KB blocks: inputs A10-A15 and A6/A7/A8 from the jumper field XT1
//   (L, H, MAP1 or MAP2 each). The ZPS-7/8 memory map (MAP2 = 1 frees B000-BFFF for the DBS-1
//   video RAM) needs A6 = L, A7 = H, A8 = MAP2 (an assumption, the manual tables disagree).
// - PROM DD2 (74S287) decodes the ports: 18h 8251 data, 19h 8251 command/status, 1Ah 8253
//   counter 2, 1Bh 8253 control, 1Ch counter 0, 1Dh counter 1, 1Eh interrupt mask / BREAK,
//   1Fh watchdog enable (D0).
// - 8251 TxC = RxC = 8253 OUT0, the counters clocked at 12.288 MHz / 10 = 1.2288 MHz.
// - Register 1Eh: D0 TxRDY, D1 RxRDY, D3 TxE interrupt enables, D2 BREAK. The OR of the enabled
//   requests drives bus /INT0 through XT3.
// - Watchdog: 1Fh D0 = 1 opens GATE1/GATE2 of counters 1 and 2 (mode 2, counter 2 clocked by
//   OUT1); the end of counter 2 latches a flip-flop that pulls bus /RTL low through XT2: the
//   CPU card resets the system, /RES clears the flip-flop.
#include <memory>
#include <vector>
#include "chips/i8251.h"
#include "chips/i8253.h"
#include "core/devices.h"

class Rsw1 : public Card {
public:
    Rsw1();

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int irq_lines() override;
    bool rtl_line() override { return wd_reset && wd_rtl; }
    SerialChannel* serial_channel(int index) override { return index == 0 ? serial.get() : nullptr; }
    void describe(nlohmann::json& out) override;
    int decode_io(uint8_t port, const char** what) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;

private:
    bool ram_select(uint16_t addr, int map) const;
    int input_level(int src, int map) const;   // an XT1 source: 0 L, 1 H, 2 MAP1, 3 MAP2
    void update_baud(void);
    void clock_timers(uint64_t now);

    std::vector<uint8_t> ram;
    std::vector<uint8_t> dd1;    // 512 x 4, memory
    std::vector<uint8_t> dd2;    // 256 x 4, I/O
    int xt1[3] = { 0, 1, 3 };    // sources of DD1 inputs A6, A7, A8
    bool int0 = true;            // XT3 13-14
    bool wd_rtl = false;         // XT2 11-12: the watchdog pulls /RTL
    I8251 uart;
    I8253 pit;
    std::unique_ptr<SerialChannel> serial;
    uint8_t mask = 0;            // 1Eh
    bool wd_enable = false;      // DD21A Q: GATE1, GATE2
    bool wd_reset = false;       // DD21B: the watchdog has run out
    uint64_t last = 0;           // bus time the 1.2288 MHz clock is accounted to
    double phase = 0;            // fraction of a clock period
    uint32_t divisor0 = 0;       // counter 0 count the baud rate was computed from
};
