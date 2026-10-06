#pragma once
// BGP-1S rev 4.2 - graphics card compatible with PMD-85 (Libor Lasota), see docs/desky/BGP-1S.md.
// The schematic and the BIOS call it BGP-1 too, it is the same card.
//
// - 288x256 dots, 16 KB video RAM: offset = 64 * y + byte (bytes 0-47 visible), bit 0 = the left
//   dot of six, bits 7-6 attribute. No ports, no registers, no character generator.
// - The CPU only writes: reads go to the RAM of another card at the same addresses (the card
//   does not drive the data bus).
// - Window 16 KB (S3, no PROM): 0000 / 4000 / 8000 / C000h, MAP1 high / low / ignored.
// - Colours (S1): RGBM - b7b6 of the byte: 00 green, 01 red, 10 blue, 11 magenta; ACE (Colorace) -
//   the attributes of the same column on the lines 2k and 2k+1 give one of 7 colours to both.
//   R = P(a+d), B = P(b+c), G = P(/a/b + /c/d), a b = b6 b7 of the byte, d c = of the other one.
// - TV output (FRB 30: B/W composite and RGB for SCART), 6.5536 MHz dot clock, 70 bytes a line
//   (64.087 us), 312 lines, 50.01 Hz, not interlaced.
// - A write waits for the CPU slot of the next byte period (RDY, until the end of state s5);
//   reads and I/O never wait.
// - Interrupt at the start of line 256 on /INT0 or /INT1 (S2), held until an opcode fetch (M1)
//   from the window of the card. Not cleared by /RES.
#include "core/devices.h"

class Bgp1s : public Card, public DisplayDevice {
public:
    Bgp1s() : Card("BGP-1S") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    bool mem_read(uint16_t addr, uint8_t& data) override;     // never drives the bus (M1 clears the INT)
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override { return false; }
    int irq_lines() override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

    int screen_width() const override { return 288; }
    int screen_height() const override { return 256; }
    double dot_clock_hz() const override { return 6.5536e6; }
    void render(uint32_t* argb, uint32_t bg) override;
    const char* ink_param() const override { return "output"; }   // which output of K2 the TV gets
    std::string ink() const override { return mono ? "mono" : "rgb"; }
    bool set_ink(const std::string& value, std::string& err) override;
    // the console of MIKROBIOS 1.x (the screen window); the text is drawn by software: no text
    std::vector<std::string> text_rows() override { return std::vector<std::string>(); }

    static constexpr double DOT_HZ = 6.5536e6;
    static constexpr int LINE_DOTS = 70 * 6;
    static constexpr int FRAME_LINES = 312;

    uint8_t vram[16384];

private:
    bool selected(uint16_t a, int map) const;
    void update_int(void);    // the interrupt request at the start of line 256

    uint16_t base = 0xC000;
    int map_mode = 0;         // 0 ignored (MDIS), 1 MAP1 = H, 2 MAP1 = L
    bool ace = true;          // S1
    int int_line = IRQ_INT0;  // S2 (0 = no jumper)
    bool mono = false;        // the TV on the B/W composite output
    bool waits = true;
    bool int_req = false;
    uint64_t int_checked = 0; // bus time up to which the line 256 starts were looked at
};
