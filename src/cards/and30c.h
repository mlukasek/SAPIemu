#pragma once
// AND-30C - alphanumeric display 80 x 21 with the MC6845 (CM607P on the card) of the set SAPI-IPS (JPR-1Z, AND-30C,
// RAM-64, RFD-Z; the card is dated 1988, its EPROMs are labelled "AND 80"). No documentation survives: the
// schematic "AND-30Cu" was redrawn by reverse engineering by P. Vsijansky (sapi.cz/sapi/and-30c.php, 2020), the
// emulation follows its netlist, see docs/desky/AND-30C.md.
//
// - PROM IC2 (74S287): A0-A4 = AB11-AB15, A5 = /MW, A6 = /MR, A7 = JP8; the dump selects F800-FFFF. Outputs:
//   O2 = DIR of the data buffer 74LS245, O3 = /SEL (also drives MAP2 on the bus: RAM-64 leaves the window),
//   O4 = /WE of the 2114s and R/W of the 6845. /CE2 = RES (off during /RES), /CE1 = X1 pin 7 (/REF) as drawn.
// - 2 KB video RAM (4 x 2114) at F800-FFDF; the 7430 IC6 puts the 6845 at FFE0-FFFF (A5-A10 = 1): RS = A4, so the
//   address register at FFE0-FFEF and the data register at FFF0-FFFF; the RAM cells 7E0-7FF are then not reachable
//   by the CPU (the display still shows them when the 6845 addresses them). 6845 E = FI2 of the bus.
// - The 74157 mux gives the RAM to the CPU at once (no RDY: the trace from DE to the 74LS244 is cut on the card);
//   IC12B (74LS74 with 330 R / 15 nF) blanks the video for about 4.5 us from the start of every CPU access, so a
//   write shows as a short black streak instead of "snow".
// - Character generator 2 x 2716 (IC9 codes 00-7F, IC10 80-FF: D7 picks the EPROM through JP13/JP14), A0-A3 =
//   RA0-RA3, A4-A10 = code bits 0-6; dots = D7..D2 (6 per character, D7 left), 16 bytes per code, row 0 on top.
//   The font is the AND-1Z one with the rows in order.
// - Dot clock 10.8 MHz (LC oscillator with a tunable coil), character clock 1.8 MHz (74164 ring counter); the
//   cursor of the 6845 is ORed with the dots (a solid block on its scan lines).
#include <vector>
#include "core/devices.h"

class And30c : public Card, public DisplayDevice {
public:
    And30c();

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override;
    void describe(nlohmann::json& out) override;
    int decode_mem(uint16_t addr, int map, const char** what) override;

    int screen_width() const override;
    int screen_height() const override;
    double dot_clock_hz() const override { return DOT_HZ; }
    void render(uint32_t* argb, uint32_t bg) override;
    const char* ink_param() const override { return "monitor"; }
    std::string ink() const override { return monitor.name; }
    bool set_ink(const std::string& value, std::string& err) override { return monitor.set(value, err); }
    std::vector<std::string> text_rows() override;

    static constexpr double DOT_HZ = 10.8e6;
    static constexpr int DOTS = 6;
    static constexpr double BLANK_S = 4.5e-6;   // IC12B: 330 R x 15 nF from about 3.5 V down to the 1.4 V threshold

    uint8_t ram[2048];
    uint8_t r[18];                    // 6845 registers
    uint8_t ar = 0;                   // 6845 address register

    // the timing the registers give (0 when not set up): line in us, frame in lines, frequencies
    double line_us(void) const;
    int frame_lines(void) const;

private:
    enum { SEL_NONE, SEL_RAM, SEL_CRTC };
    int select(uint16_t addr, bool write, int map, bool* drive) const;
    int cols(void) const;
    int rows(void) const;
    int lines(void) const;
    void note_access(void);
    uint8_t crtc_read(uint16_t addr) const;

    std::vector<uint8_t> font;        // 4 KB: IC9 (codes 00-7F) and IC10 (80-FF)
    std::vector<uint8_t> prom;        // IC2
    bool ce1_ref = false;             // /CE1 on X1 pin 7 as drawn (else JP10 to GND)
    bool a7_map1 = false;             // JP8: PROM A7 from MAP1 (else GND)
    bool blank_on_access = true;      // IC12B blanks the video on CPU access
    MonitorInk monitor;
    uint64_t reset_at = 0;            // bus time of the last /RES (the 6845 counters start from it)
    static constexpr unsigned ACC_N = 4096;
    uint64_t acc[ACC_N];              // bus times of the recent CPU accesses (ring)
    unsigned acc_head = 0;
};
