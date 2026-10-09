#include "cards/cga1v.h"
#include "core/state.h"
#include "core/bus.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

using nlohmann::json;

// VGA 640x480/60 Hz: 25.175 MHz, 800 dots per line, 525 lines, 400 shown.
static const double LINE_S = 800.0 / 25.175e6;
static const double FRAME_S = 525.0 * LINE_S;
static const double VBI_S = 400.0 * LINE_S;
// The CPU slot: every 4 pixels of MCLK = 25.175 MHz / 2.
static const double SLOT_NS = 4.0 * 2.0 * 1e9 / 25.175e6;
static const uint16_t REG_OFF = 0x3FC0;

static bool map_mode_of(const json& cfg, const char* key, int& out, std::string& err)
{
    std::string m;
    if (!cfg_string(cfg, key, m, "h", err)) return false;
    if (m == "ignore") out = 0;
    else if (m == "h") out = 1;
    else if (m == "l") out = 2;
    else if (m == "never") out = 3;
    else { err = std::string(key) + " must be ignore, h, l or never (jumpers MxEN, MxLO)"; return false; }
    return true;
}

bool Cga1v::configure(const json& cfg, std::string& err)
{
    long long b;
    if (!cfg_number(cfg, "base", b, 0xC000, err)) return false;
    if (b < 0 || b > 0xC000 || (b & 0x3FFF)) { err = "base must be 0000h, 4000h, 8000h or C000h (jumpers /A15 /A14)"; return false; }
    base = (uint16_t)b;
    if (!map_mode_of(cfg, "map1", map_mode[0], err)) return false;
    if (!map_mode_of(cfg, "map2", map_mode[1], err)) return false;
    if (!cfg_bool(cfg, "wait_states", waits, true, err)) return false;
    if (!cfg_bool(cfg, "frame_blend", blend, true, err)) return false;
    power_on_fill(ram, sizeof(ram), 0);
    power_on_fill(&pal[0][0], sizeof(pal), 0);   // the Bt476 palette too
    return true;
}

void Cga1v::reset()
{
    config = 0;   // IC17 cleared by /IRES; the VBI flip-flop and the RAMDAC are not reset
}

bool Cga1v::selected(uint16_t a, int map) const
{
    if ((a & 0xC000) != base) return false;
    for (int i = 0; i < 2; i++) {
        bool h = (map & (i ? MAP_2 : MAP_1)) != 0;
        switch (map_mode[i]) {
        case 1: if (!h) return false; break;
        case 2: if (h) return false; break;
        case 3: return false;
        }
    }
    return true;
}

// RDY (sheet 4): STSTB with SEL sets IC9B (RDY low). /READ or /WRITE is taken by IC7B at the next rising
// edge of QA, the start of a CPU window (every 4 MCLK, 318 ns); IC7A ends the window with /QA half a period
// later and the rising /CPU clears IC9B. The Z80 samples /WAIT at the falling edge of T2 and of every Tw.
// /RD falls with /MREQ at the falling edge of T1, /WR only at the falling edge of T2, so a write gets its
// window a clock later. REQ_NS + REL_NS (strobe delays, gates, set-up of the flip-flops and of /WAIT) is
// fitted to ZRDBENCH on a real SAPI-1 V (PoP port, 2026-10-08): LDIR into the card 2.43 T per byte more at
// 4 MHz, 1.57 T at 2 MHz, from the card 1.26 T and 0.09 T (docs/desky/CGA-1V.md).
static const double REQ_NS = 40.0;   // the Z80 edge to the D input of IC7B
static const double REL_NS = 50.0;   // the end of the window to /WAIT high at the Z80

int Cga1v::wait_states(bool write) const
{
    if (!waits) return 0;
    double unit = 1e9 / (double)bus->cpu_hz;
    double clk = unit * bus->cpu_step;                     // CPU clock period
    double t2 = (double)bus->now * unit;                   // the access is seen in T2
    double sample = t2 + clk / 2;                          // falling edge of T2: first /WAIT sample
    double req = (write ? sample : sample - clk) + REQ_NS;
    double grant = SLOT_NS * ceil(req / SLOT_NS);          // rising edge of QA
    double release = grant + SLOT_NS / 2 + REL_NS;         // end of the window
    int n = 0;
    while (release > sample + n * clk) n++;
    return n * bus->cpu_step;
}

void Cga1v::update_vbi(void)
{
    double t = (double)bus->now / (double)bus->cpu_hz - VBI_S;
    uint64_t n = t < 0 ? 0 : (uint64_t)(t / FRAME_S) + 1;
    if (n > vbi_count) {
        vbi = true;
        frames[0] = frames[1];   // the frame just shown
        frames[1].page = config & 1;
        frames[1].cga = (config & 4) != 0;
        palette_argb(frames[1].rgb);
        if (frame_count < 2) frame_count++;
    }
    vbi_count = n;
}

bool Cga1v::mem_read(uint16_t a, uint8_t& data)
{
    if (!selected(a, bus->map)) return false;
    bus->wait += wait_states(false);
    return mem_peek(a, data);
}

bool Cga1v::mem_peek(uint16_t a, uint8_t& data)
{
    if (!selected(a, bus->map)) return false;
    uint16_t off = a & 0x3FFF;
    data = ram[(config & 2 ? 0x4000 : 0) | off];
    if (off >= REG_OFF) {   // STATUS: D7 = VBI request (READZ), D0-D6 from the SRAM
        update_vbi();
        data = (uint8_t)((data & 0x7F) | (vbi ? 0x80 : 0));
    }
    return true;
}

bool Cga1v::mem_write(uint16_t a, uint8_t data)
{
    if (!selected(a, bus->map)) return false;
    bus->wait += wait_states(true);
    uint16_t off = a & 0x3FFF;
    ram[(config & 2 ? 0x4000 : 0) | off] = data;   // /WE also for the registers
    if (off >= REG_OFF) {
        if (off & 4) {
            dac_write(off & 3, data);
        } else {
            update_vbi();
            config = (uint8_t)(data & 0x0F);
            if (data & 0x80) vbi = false;   // /IACK
        }
    }
    return true;
}

void Cga1v::dac_write(int rs, uint8_t data)
{
    switch (rs) {
    case 0:   // address register (write mode)
        dac_addr = data;
        dac_step = 0;
        break;
    case 1:   // colour palette: R, G, B, then the address increments
        pal[dac_addr][dac_step] = data & 0x3F;
        if (++dac_step == 3) {
            dac_step = 0;
            dac_addr++;
        }
        break;
    case 2:
        dac_mask = data;
        break;
    default:  // address register (read mode): the colour goes to the read buffer, address + 1
        dac_addr = (uint8_t)(data + 1);
        dac_step = 0;
        break;
    }
}

int Cga1v::irq_lines()
{
    update_vbi();
    return vbi && (config & 8) ? IRQ_INT0 : 0;
}

int Cga1v::decode_mem(uint16_t a, int map, const char** what)
{
    if (!selected(a, map)) return 0;
    *what = (a & 0x3FFF) >= REG_OFF ? "VRAM + registers" : "VRAM";
    return DEC_READ | DEC_WRITE;
}

// RAMDAC colour of every pixel value P7..P0 (COLMASK applied), 6 -> 8 bits
void Cga1v::palette_argb(uint32_t* rgb) const
{
    for (int i = 0; i < 256; i++) {
        const uint8_t* p = pal[i & dac_mask];
        rgb[i] = 0xFF000000u | (uint32_t)(p[0] << 2 | p[0] >> 4) << 16 | (uint32_t)(p[1] << 2 | p[1] >> 4) << 8 |
                 (uint32_t)(p[2] << 2 | p[2] >> 4);
    }
}

// Pixel value P7..P0 of pixel px (0 = left) of byte b on line y.
static inline unsigned pixel_value(unsigned b, int px, int y, bool cga)
{
    unsigned low = (cga ? b << px : b) & 0x0F;   // P4..P1 (IC16)
    unsigned p0 = (b >> (7 - px)) & 1;            // IC15
    return (unsigned)(y >> 5) << 5 | low << 1 | p0;   // P7:P5 = V256:V128:V64
}

void Cga1v::render(uint32_t* argb, uint32_t bg)
{
    const Frame& f0 = frames[0];
    const Frame& f1 = frames[1];
    if (blend && frame_count == 2 &&
        (f0.page != f1.page || f0.cga != f1.cga || memcmp(f0.rgb, f1.rgb, sizeof(f0.rgb)))) {
        const uint8_t* p0 = ram + (f0.page ? 0x4000 : 0);
        const uint8_t* p1 = ram + (f1.page ? 0x4000 : 0);
        for (int y = 0; y < HEIGHT; y++) {
            uint32_t* out = argb + (size_t)y * WIDTH;
            for (int x = 0; x < LINE_BYTES; x++) {
                unsigned b0 = p0[y * LINE_BYTES + x], b1 = p1[y * LINE_BYTES + x];
                for (int px = 0; px < 4; px++) {
                    uint32_t c0 = f0.rgb[pixel_value(b0, px, y, f0.cga)];
                    uint32_t c1 = f1.rgb[pixel_value(b1, px, y, f1.cga)];
                    *out++ = 0xFF000000u | (((c0 & 0xFEFEFEu) >> 1) + ((c1 & 0xFEFEFEu) >> 1));
                }
            }
        }
        return;
    }
    uint32_t rgb[256];   // live state: also shows writes while the CPU is stopped
    palette_argb(rgb);
    const uint8_t* page = ram + (config & 1 ? 0x4000 : 0);
    bool cga = (config & 4) != 0;
    for (int y = 0; y < HEIGHT; y++) {
        const uint8_t* line = page + y * LINE_BYTES;
        uint32_t* out = argb + (size_t)y * WIDTH;
        for (int x = 0; x < LINE_BYTES; x++)
            for (int px = 0; px < 4; px++) *out++ = rgb[pixel_value(line[x], px, y, cga)];
    }
}

void Cga1v::describe(json& out)
{
    char b[8];
    snprintf(b, sizeof(b), "%04X", base);
    out["base"] = b;
    static const char* const modes[] = { "ignore", "h", "l", "never" };
    out["map1"] = modes[map_mode[0]];
    out["map2"] = modes[map_mode[1]];
    out["mode"] = config & 4 ? "CGA" : "EGA";
    out["shown_page"] = config & 1 ? "B" : "A";
    out["cpu_page"] = config & 2 ? "B" : "A";
    out["irq_enabled"] = (config & 8) != 0;
    out["vbi"] = vbi;
    out["frame_blend"] = blend;
    snprintf(b, sizeof(b), "%02X", dac_mask);
    out["colmask"] = b;
}

void Cga1v::state(StateIO& io)
{
    io.arr(ram, sizeof(ram));
    io.v(config);
    io.v(vbi);
    io.v(vbi_count);
    for (int i = 0; i < 256; i++) io.arr(pal[i], 3);
    io.v(dac_addr);
    io.e(dac_step);
    io.v(dac_mask);
    for (Frame& f : frames) {
        io.v(f.page);
        io.v(f.cga);
        for (uint32_t& c : f.rgb) io.v(c);
    }
    io.e(frame_count);
}
