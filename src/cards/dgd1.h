#pragma once
// DGD-1 - graphic display card of SAPI-1 (see docs/desky/DGD-1.md).
//
// - 320x240 dots, 16 KB dynamic RAM (8x 4116) as ordinary memory in one quarter
//   of the address space: switch SP1-2 open = C000H (ZPS-2, ZPS-3), closed =
//   0000H (ZPS-4, SAPI-1Z); SP1-1 closed = with mapping (selected only while
//   MAP1 is active).
// - Line y (0 = top) at base + y * 40h, 40 bytes (320 dots), bit 7 = leftmost
//   dot, 1 = dot lit. 240 lines: base .. base + 3BFFH.
// - 8 MHz dot clock, 1 MHz byte clock (64 us line). The CPU gets the memory in
//   its window of every byte clock period, until then RDY is low (wait states).
// - Its own video output (TV / AZJ-462), next to the AND-1 text display.
// - The address decoder PROM D1A is dumped but the wiring of its inputs is not
//   known yet; the selection follows the table of SP1 in the manual.
//
// DGD-1V (Libor Lasota, rev 1.1, docs/desky/DGD-1V.md): the same memory layout with a
// VGA output (each dot 2 x 2 of 640x480/60 Hz), SRAM 62256, no PROM:
// - S2 (74LS85 IC27): /A15 /A14 = base 0000/4000/8000/C000h (fitted = 0), MAP1EN
//   MAP1LOW and MAP2EN MAP2LOW as on CGA-1V (ignore / H / L / never); /RES disables it.
// - S1: ink /BL /RD /GH /GL (fitted = off), all open = white.
// - Dot clock 25.175 MHz / 2, a byte of 8 dots: 4 dots for the CPU, 4 for the video;
//   RDY waits for the CPU slot (approximated as the end of the current 4-dot slot,
//   like CGA-1V), I/O cycles do not wait.
#include <string.h>
#include <vector>
#include "core/devices.h"

class Dgd1 : public Card, public DisplayDevice {
public:
    explicit Dgd1(const char* type = "DGD-1") : Card(type), vga(strcmp(type, "DGD-1V") == 0) {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

    int screen_width() const override { return WIDTH; }
    int screen_height() const override { return HEIGHT; }
    double dot_clock_hz() const override { return vga ? 7.375e6 : 8e6; }   // VGA: square dots
    bool text_display() const override { return false; }
    void render(uint32_t* argb, uint32_t bg) override;
    const char* ink_param() const override { return vga ? "color" : "monitor"; }   // DGD-1V: jumpers S1
    std::string ink() const override { return vga ? color : monitor.name; }
    bool set_ink(const std::string& value, std::string& err) override;
    std::vector<std::string> text_rows() override { return std::vector<std::string>(); }

    static const int WIDTH = 320;
    static const int HEIGHT = 240;
    static const int LINE_BYTES = 64;   // address step of a line (40 bytes shown)

private:
    bool selected(uint16_t addr, int map) const;
    int wait_states(void) const;

    const bool vga;             // DGD-1V
    uint16_t base = 0xC000;     // SP1-2 (DGD-1V: S2 /A15 /A14)
    bool mapping = false;       // SP1-1
    int map_mode[2] = { 0, 0 }; // DGD-1V S2 MAP1, MAP2: 0 ignore, 1 H, 2 L, 3 never
    bool waits = true;
    MonitorInk monitor;         // DGD-1
    std::string color = "white";   // DGD-1V S1
    uint32_t ink_argb = 0xFFFFFFFFu;
    std::vector<uint8_t> ram;   // 16 KB
};
