#pragma once
// CGA-1V rev 1.1 - colour graphics 320x200, VGA output (Libor Lasota),
// see docs/desky/CGA-1V.md.
//
// - 16 KB of the address space (base 0000/4000/8000/C000, jumpers /A15 /A14 in
//   negated form), MAP1/MAP2 each ignored / H / L / never (M1EN M1LO M2EN
//   M2LO); /RES disables the selection (74LS85 IC40).
// - 62256: two video pages A and B of 16 KB. Line y at y * 80, 4 pixels per
//   byte, D7 = leftmost; 16000 bytes shown.
// - Registers at offset 3FC0-3FFF (IA6-IA13 = 1), write: A2 = 0 CONFIG
//   (D0 shown page, D1 CPU page, D2 CGA, D3 IRQ enable, D7 = 1 acknowledges
//   VBI), A2 = 1 Bt476 with RS0/RS1 = A0/A1 (write address, palette data,
//   pixel mask, read address). Writes go into the SRAM as well. Read: SRAM
//   with D7 = VBI request (STATUS).
// - Pixel -> RAMDAC P7..P0: P7:P5 = colour domain (V256:V128:V64, 32 pixel
//   rows), P4..P1 = low nibble of the byte (EGA: attribute of the 4 pixels;
//   CGA: the shifter, P4 = LSB of the pixel, P3..P1 shifted rest), P0 = high
//   nibble bit (EGA ink/paper, CGA MSB of the pixel). COLMASK ANDs P7..P0.
// - VGA 640x480/60 Hz, 25.175 MHz: 800 dots x 525 lines, each pixel 2x2, 400
//   lines shown. VBI request (flip-flop IC9A) at the start of the vertical
//   blanking (line 400), /INT0 when enabled.
// - The CPU gets the SRAM in its window of every 4 pixels (318 ns), RDY low until
//   the window ends (wait states; a write starts with /WR a clock later than a
//   read with /RD, so it waits about a window longer).
// - Frame blending (emulator option): software switching the shown page and the
//   palette every frame (REMBRAN2) relies on the eye mixing both, so when the
//   last two frames (captured at VBI) differ, their average is shown.
// - Beam (option, default on): the picture is drawn row by row as the beam
//   passes (VGA line 2y and 2y+1 = pixel row y), from the state of that moment,
//   so a page, mode, COLMASK or palette change in the middle of a frame and
//   drawing into the shown page tear the picture as on the monitor. Palette
//   writes during the display and switches in the middle of a frame are
//   counted for the programmer (describe), optionally drawn as "snow" dots.
#include <vector>
#include "core/devices.h"

class Cga1v : public Card, public DisplayDevice {
public:
    Cga1v() : Card("CGA-1V") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    bool mem_read(uint16_t addr, uint8_t& data) override;
    bool mem_write(uint16_t addr, uint8_t data) override;
    bool mem_peek(uint16_t addr, uint8_t& data) override;
    int irq_lines() override;
    int decode_mem(uint16_t addr, int map, const char** what) override;
    void describe(nlohmann::json& out) override;

    int screen_width() const override { return WIDTH; }
    int screen_height() const override { return HEIGHT; }
    double dot_clock_hz() const override { return 7.375e6; }   // square pixels (2x2 VGA dots)
    bool text_display() const override { return false; }
    void render(uint32_t* argb, uint32_t bg) override;
    std::vector<std::string> text_rows() override { return std::vector<std::string>(); }

    static const int WIDTH = 320;
    static const int HEIGHT = 200;
    static const int LINE_BYTES = 80;

private:
    bool selected(uint16_t addr, int map) const;
    int wait_states(bool write) const;
    void update_vbi(void);
    void dac_write(int rs, uint8_t data);
    void palette_argb(uint32_t* rgb) const;
    // beam
    int beam_line(uint64_t* frame) const;   // VGA line 0-524 of the beam now (0-399 shown)
    void beam_catch_up(void);               // draw the rows the beam has passed since the last call
    void draw_rows(int y0, int y1);         // into fb from the current state
    void frame_done(void);
    void note_switch(const char* what, int line);

    struct Frame {   // what was shown during a frame: page, mode and colours
        uint8_t page;
        bool cga;
        uint32_t rgb[256];
    };

    uint16_t base = 0xC000;
    int map_mode[2] = { 1, 1 };   // MAP1, MAP2: 0 ignored, 1 H, 2 L, 3 never
    bool waits = true;
    bool blend = true;
    uint8_t ram[0x8000];          // page A 0000-3FFF, page B 4000-7FFF
    uint8_t config = 0;           // IC17: D0 shown page, D1 CPU page, D2 CGA, D3 IRQ enable
    bool vbi = false;             // IC9A: VBI request
    uint64_t vbi_count = 0;       // vertical blankings started so far
    // Bt476
    uint8_t pal[256][3];          // 6-bit R, G, B
    uint8_t dac_addr = 0;
    int dac_step = 0;             // R, G, B
    uint8_t dac_mask = 0xFF;
    Frame frames[2];
    int frame_count = 0;          // frames captured (up to 2), frames[1] is the newest
    // beam (not in the saved state: drawn again from the video RAM)
    bool beam = true;
    bool snow = false;
    std::vector<uint32_t> fb;     // WIDTH x HEIGHT: every row as the beam drew it last
    std::vector<uint32_t> done[2];   // the last two finished frames, done[1] the newest (blending)
    int done_count = 0;
    uint64_t beam_frame = 0;      // frame being drawn
    int beam_row = 0;             // next pixel row of it to draw
    uint32_t rgb[256];            // palette_argb of the current state
    bool rgb_dirty = true;
    uint64_t render_now = ~0ull;  // bus time of the last render(): a write without time passing (debugger) redraws
    bool live_dirty = false;
    // for the programmer: palette writes during the display, switches in the middle of a frame
    uint64_t pal_visible = 0;     // since power on
    uint64_t pal_visible_frame = 0, pal_visible_last = 0;   // in the frame being drawn / the last finished one
    struct Switch { uint64_t frame; int line; std::string what; };
    std::vector<Switch> switches; // the last 16
    bool hinted = false;          // the first palette write during the display went to the log
};
