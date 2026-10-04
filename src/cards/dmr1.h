#pragma once
// DMR-1 - serial interface (MHB1012, DSM-1 compatible registers) + 128 KB SRAM
// (see docs/desky/DMR-1.md).
#include <memory>
#include <vector>
#include "chips/mhb1012.h"
#include "core/devices.h"

class Dmr1 : public Card {
public:
    Dmr1();

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
    SerialChannel* serial_channel(int index) override { return index == 0 ? serial.get() : nullptr; }
    void describe(nlohmann::json& out) override;
    void flush() override;   // battery backed SRAM -> file
    int decode_io(uint8_t port, const char** what) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;

private:
    bool ram_select(uint16_t addr, int map, uint32_t& offset, bool& write_block) const;

    // RAM
    bool ram_enabled = true;
    int ram_config = 1;           // JP3: PROM inputs A8 A7 A6
    bool a6_from_map1 = false;    // JP3: A6 driven by MAP1 instead of a fixed level
    bool a7_from_map2 = false;
    std::vector<uint8_t> ram;     // 128 KB
    bool battery = true;          // JP7: SRAM backed by the battery (MAX691)
    std::string sram_file;        // where the battery keeps the content
    std::vector<uint8_t> prom;    // D2C, 512 bytes

    // serial
    bool uart_enabled = true;
    uint8_t base = 0x10;          // JP2: 10h/14h/18h/1Ch
    uint32_t baud = 9600;         // JP8
    bool int_enabled = true;      // INT0 output connected
    uint8_t modem_ctrl = 0;       // b7 IER, b6 IET, b5 relay, b4 KAZ, b3 RD, b2 BRK, b1 DTR, b0 RTS
    bool connected_default = true;   // terminal connected at power on (DCD active)
    bool cts_default = true;         // terminal ready at power on (CTS, DSR active)
    bool rts_flow = false;           // host side honours RTS (b0 of the modem control register)
    Mhb1012 uart;
    std::unique_ptr<SerialChannel> serial;
};
