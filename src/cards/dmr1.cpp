#include "cards/dmr1.h"
#include "core/bus.h"
#include "core/ihex.h"
#include "core/log.h"
#include "core/util.h"

#include <stdio.h>
#include <string.h>

using nlohmann::json;

static const uint32_t k_bauds[] = { 75, 150, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400 };

Dmr1::Dmr1() : Card("DMR-1") {}

bool Dmr1::configure(const json& cfg, std::string& err)
{
    long long v;
    std::string prom_file;
    if (!cfg_bool(cfg, "ram", ram_enabled, true, err)) return false;
    if (!cfg_number(cfg, "ram_config", v, 1, err)) return false;
    if (v < 0 || v > 7) { err = "ram_config must be 0..7"; return false; }
    ram_config = (int)v;
    if (!cfg_bool(cfg, "a6_map1", a6_from_map1, false, err)) return false;
    if (!cfg_bool(cfg, "a7_map2", a7_from_map2, false, err)) return false;
    if (!cfg_string(cfg, "decoder", prom_file, "roms/dmr-1/DMR-1_D2C.bin", err)) return false;
    if (!rom_load(prom_file, prom, 512, 0x01, 0, err)) return false;
    ram.assign(131072, 0x00);
    if (!cfg_bool(cfg, "battery", battery, true, err)) return false;
    if (!cfg_string(cfg, "sram_file", sram_file, "work/dmr-1/sram.bin", err)) return false;
    if (battery && ram_enabled && !sram_file.empty() && battery_files_enabled()) {
        // power on with the battery: the SRAM still holds what it had at power off
        std::vector<uint8_t> saved;
        if (file_read_all(data_path(sram_file), saved) && saved.size() == ram.size()) ram = saved;
    }

    if (!cfg_bool(cfg, "uart", uart_enabled, true, err)) return false;
    if (!cfg_number(cfg, "uart_base", v, 0x10, err)) return false;
    if (v != 0x10 && v != 0x14 && v != 0x18 && v != 0x1C) {
        err = "uart_base must be 10h, 14h, 18h or 1Ch (JP2)";
        return false;
    }
    base = (uint8_t)v;
    if (!cfg_number(cfg, "baud", v, 9600, err)) return false;
    bool ok = false;
    for (uint32_t b : k_bauds) if (b == (uint32_t)v) ok = true;
    if (!ok) { err = "baud must be one of 75..38400 (JP8)"; return false; }
    baud = (uint32_t)v;
    if (!cfg_bool(cfg, "int0", int_enabled, true, err)) return false;
    if (!cfg_bool(cfg, "connected", connected_default, true, err)) return false;
    if (!cfg_bool(cfg, "cts", cts_default, true, err)) return false;
    std::string flow;
    if (!cfg_string(cfg, "flow_control", flow, "none", err)) return false;
    if (flow != "none" && flow != "rts_cts") { err = "flow_control must be none or rts_cts"; return false; }
    rts_flow = (flow == "rts_cts");

    char buf[64];
    snprintf(buf, sizeof(buf), "%s #%02X", type.c_str(), base);   // as on the front panel
    serial.reset(new SerialChannel(buf));
    serial->connected = connected_default;
    serial->cts = cts_default;
    serial->baud = (int)baud;
    if (cfg.contains("serial")) {
        serial->bridge_cfg = cfg["serial"];
        serial->pacing = cfg["serial"].value("pacing", true);
    }
    return true;
}

void Dmr1::flush()
{
    if (!battery || !ram_enabled || sram_file.empty() || !battery_files_enabled()) return;
    std::string path = data_path(sram_file);
    make_dirs(path_dir(path));
    if (!file_write_all(path, ram.data(), ram.size())) LOGW("%s: cannot save the SRAM to %s", name.c_str(), path.c_str());
}

void Dmr1::reset()
{
    // /RES does not clear the SRAM (battery backed); the UART is reset.
    modem_ctrl = 0;
    uart.set_baud(bus->cpu_hz, baud);
    uart.reset(bus->now);
}

void Dmr1::advance(uint64_t now)
{
    // with RTS/CTS flow control the host side only sends while SAPI keeps RTS active
    if (uart_enabled) uart.advance(now, serial.get(), !rts_flow || (modem_ctrl & 0x01));
}

// D2C inputs: A0..A5 = A10..A15, A6..A8 = JP3 (A6/A7 optionally MAP1/MAP2).
// Outputs: D0 = /CER, D1 = SRAM A16, D3 = write block.
bool Dmr1::ram_select(uint16_t addr, int map, uint32_t& offset, bool& write_block) const
{
    if (!ram_enabled) return false;
    unsigned a6 = a6_from_map1 ? ((map & MAP_1) ? 1u : 0u) : (unsigned)(ram_config & 1);
    unsigned a7 = a7_from_map2 ? ((map & MAP_2) ? 1u : 0u) : (unsigned)((ram_config >> 1) & 1);
    unsigned a8 = (unsigned)((ram_config >> 2) & 1);
    unsigned idx = (unsigned)(addr >> 10) | (a6 << 6) | (a7 << 7) | (a8 << 8);
    uint8_t o = prom[idx];
    if (o & 0x01) return false;
    offset = (uint32_t)(((o >> 1) & 1) << 16) | addr;
    write_block = (o & 0x08) != 0;
    return true;
}

int Dmr1::decode_mem(uint16_t addr, int map, const char** what)
{
    uint32_t off;
    bool wb;
    if (!ram_select(addr, map, off, wb)) return 0;
    *what = wb ? "RAM (write blocked)" : "RAM";
    return DEC_READ | DEC_WRITE;   // a blocked write is still answered
}

int Dmr1::decode_io(uint8_t port, const char** what)
{
    if (!uart_enabled || (port & 0xFC) != base) return 0;
    *what = "UART";
    return (port & 3) == 3 ? DEC_WRITE : DEC_READ | DEC_WRITE;   // base+3: reset, write only
}

bool Dmr1::mem_read(uint16_t addr, uint8_t& data)
{
    uint32_t off;
    bool wb;
    if (!ram_select(addr, bus->map, off, wb)) return false;
    data = ram[off];
    return true;
}

bool Dmr1::mem_write(uint16_t addr, uint8_t data)
{
    uint32_t off;
    bool wb;
    if (!ram_select(addr, bus->map, off, wb)) return false;
    if (!wb) ram[off] = data;
    return true;
}

bool Dmr1::mem_peek(uint16_t addr, uint8_t& data)
{
    return mem_read(addr, data);
}

bool Dmr1::io_read(uint8_t port, uint8_t& data)
{
    if (!uart_enabled || (port & 0xFC) != base) return false;
    switch (port & 3) {
    case 0: {
        // modem status: b7 CTS, b6 DSR, b5 DCD (active = 0), b4 RI (raw RxD, idle = 1),
        // b3 PE, b2 FE, b1 OE, b0 INTR
        uint8_t v = 0;
        if (!serial->cts) v |= 0xC0;         // CTS, DSR inactive (terminal not ready)
        if (!serial->connected) v |= 0x20;   // DCD inactive (no terminal)
        v |= 0x10;
        if (uart.pe) v |= 0x08;
        if (uart.fe) v |= 0x04;
        if (uart.oe) v |= 0x02;
        if (irq_lines()) v |= 0x01;
        data = v;
        break;
    }
    case 1:
        data = (uint8_t)(0x3F | (uart.dr ? 0x80 : 0) | (uart.tbre ? 0x40 : 0));
        break;
    case 2:
        data = uart.read_data();
        break;
    default:
        return false;   // base+3 is write only
    }
    return true;
}

bool Dmr1::io_write(uint8_t port, uint8_t data)
{
    if (!uart_enabled || (port & 0xFC) != base) return false;
    switch (port & 3) {
    case 0: modem_ctrl = data; break;
    case 1: uart.write_control(data); break;
    case 2: uart.write_data(data); break;
    case 3: uart.reset(bus->now); break;
    }
    return true;
}

int Dmr1::irq_lines()
{
    if (!uart_enabled || !int_enabled) return 0;
    bool rx = uart.dr && (modem_ctrl & 0x80);
    bool tx = uart.tbre && (modem_ctrl & 0x40);
    return (rx || tx) ? IRQ_INT0 : 0;
}

void Dmr1::describe(json& out)
{
    out["ram"] = ram_enabled;
    out["ram_config"] = ram_config;
    out["uart"] = uart_enabled;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02Xh", base);
    out["uart_base"] = buf;
    out["baud"] = baud;
    out["terminal_connected"] = (bool)serial->connected;
    out["cts"] = (bool)serial->cts;
}
