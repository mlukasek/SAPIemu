#include "cards/dbs1a.h"
#include "core/bus.h"
#include "core/charset.h"
#include "core/ihex.h"
#include "core/state.h"

#include <stdio.h>
#include <string.h>

using nlohmann::json;

// XT5: how the EPROM pins A11/A12 are wired (font size 2, 4 or 8 KB)
enum {
    XT5_2716,         // 28-33: one set, 8 lines
    XT5_2732,         // 27-28: A11 = RA3 (16 lines), the standard
    XT5_2732_ATTR,    // 28-29: A11 = attribute bit 3 (512 characters, 8 lines)
    XT5_2732_MA12,    // 28-31: A11 = MA12 (two sets chosen by R12 bit 4, 8 lines)
    XT5_2764_ATTR,    // 27-28, 29-30: A11 = RA3, A12 = attribute bit 3
    XT5_2764_MA12,    // 27-28, 30-31: A11 = RA3, A12 = MA12
    XT5_2764_BOTH,    // 28-31, 29-30: A11 = MA12, A12 = attribute bit 3 (8 lines)
    XT5_COUNT
};
static const char* const k_xt5[XT5_COUNT] = { "2716", "2732", "2732_attr", "2732_ma12", "2764_attr", "2764_ma12",
                                               "2764_ma12_attr" };

bool Dbs1a::configure(const json& cfg, std::string& err)
{
    std::string f, x5, rg, out;
    long long port, base;
    if (!cfg_string(cfg, "xt5", x5, "2732", err)) return false;
    xt5 = -1;
    for (int i = 0; i < XT5_COUNT; i++)
        if (x5 == k_xt5[i]) xt5 = i;
    if (xt5 < 0) { err = "xt5 must be 2716, 2732, 2732_attr, 2732_ma12, 2764_attr, 2764_ma12 or 2764_ma12_attr"; return false; }
    size_t size = xt5 == XT5_2716 ? 2048 : xt5 <= XT5_2732_MA12 ? 4096 : 8192;
    if (!cfg_string(cfg, "font", f, "roms/dbs-1a/DBS-1A_CharGen_nahrada.bin", err)) return false;
    if (!rom_load(f, font, size, 0x00, 0, err)) return false;
    font.resize(size, 0x00);
    if (!cfg_number(cfg, "port", port, 0xE0, err)) return false;
    if (port < 0xE0 || port > 0xEE || (port & 1)) { err = "port must be E0h, E2h ... EEh (XT2, XT3)"; return false; }
    port_base = (uint8_t)port;
    if (!cfg_number(cfg, "window", base, 0xB000, err)) return false;
    if (base < 0x8000 || (base & 0x0FFF)) { err = "window must be 8000h, 9000h ... F000h (XT1, XT4)"; return false; }
    win_base = (uint16_t)base;
    high_group = base >= 0xC000;
    if (!cfg_bool(cfg, "map1", map1_used, true, err) || !cfg_bool(cfg, "map2", map2_used, true, err)) return false;
    long long d;
    if (!cfg_number(cfg, "dots", d, 8, err)) return false;
    if (d != 6 && d != 8) { err = "dots must be 8 (64 columns) or 6 (80 columns) (XT7)"; return false; }
    dots = (int)d;
    if (!cfg_string(cfg, "rgb_order", rg, "manual", err)) return false;
    if (rg != "manual" && rg != "schematic") { err = "rgb_order must be manual or schematic"; return false; }
    schematic_rg = rg == "schematic";
    if (!cfg_string(cfg, "output", out, "rgb", err)) return false;
    if (out != "rgb" && out != "composite") { err = "output must be rgb or composite"; return false; }
    mono = out == "composite";
    memset(chr, 0x20, sizeof(chr));
    memset(attr, 0x07, sizeof(attr));
    memset(r, 0, sizeof(r));
    return true;
}

void Dbs1a::reset()
{
    // /RES goes only to the 6845 RESET: its registers and the video RAM keep their contents
}

// PROM DD2: the 4 KB window. 8000-BFFF needs MAP1 = L and MAP2 = H (each when decoded, XT1);
// C000-FFFF ignores MAP.
bool Dbs1a::window(uint16_t addr, int map) const
{
    if ((addr & 0xF000) != win_base) return false;
    if (high_group) return true;
    bool m1 = map1_used ? (map & MAP_1) != 0 : false;
    bool m2 = map2_used ? (map & MAP_2) != 0 : true;
    return !m1 && m2;
}

bool Dbs1a::mem_write(uint16_t addr, uint8_t data)
{
    if (!window(addr, bus->map)) return false;
    unsigned cell = (addr >> 1) & 0x7FF;
    if (addr & 1) attr[cell] = data;
    else chr[cell] = data;
    return true;   // the video RAM is write only: reads are not answered
}

bool Dbs1a::io_read(uint8_t port, uint8_t& data)
{
    if (port != port_base + 1) return false;
    data = (ar >= 14 && ar <= 17) ? r[ar] : 0x00;   // R14-R17 readable (light pen unused)
    return true;
}

bool Dbs1a::io_write(uint8_t port, uint8_t data)
{
    if (port == port_base) {
        ar = (uint8_t)(data & 0x1F);
        return true;
    }
    if (port == port_base + 1) {
        static const uint8_t mask[18] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x1F, 0x7F, 0x7F, 0x03, 0x1F, 0x7F, 0x1F,
                                          0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF };
        if (ar < 16) r[ar] = (uint8_t)(data & mask[ar]);
        return true;
    }
    return false;
}

int Dbs1a::decode_io(uint8_t port, const char** what)
{
    if (port == port_base) { *what = "6845 address"; return DEC_WRITE; }
    if (port == port_base + 1) { *what = "6845 data"; return DEC_READ | DEC_WRITE; }
    return 0;
}

int Dbs1a::decode_mem(uint16_t addr, int map, const char** what)
{
    if (!window(addr, map)) return 0;
    *what = "video RAM (write only)";
    return DEC_WRITE;
}

int Dbs1a::cols(void) const
{
    return (r[1] && r[1] <= 128) ? r[1] : 64;
}

int Dbs1a::rows(void) const
{
    return (r[6] && r[6] <= 64) ? r[6] : 20;
}

int Dbs1a::lines(void) const
{
    return r[1] && r[6] ? (r[9] & 0x1F) + 1 : 12;
}

int Dbs1a::screen_width() const
{
    return cols() * dots;
}

int Dbs1a::screen_height() const
{
    return rows() * lines();
}

uint32_t Dbs1a::glyph_addr(uint8_t code, uint8_t a, int ra, unsigned ma) const
{
    uint32_t base = (uint32_t)(ra & 7) | ((uint32_t)(code >> 7) << 3) | ((uint32_t)(code & 0x7F) << 4);
    unsigned ra3 = (ra >> 3) & 1, at3 = (a >> 3) & 1, ma12 = (ma >> 12) & 1;
    switch (xt5) {
    case XT5_2716: return base;
    case XT5_2732: return base | (ra3 << 11);
    case XT5_2732_ATTR: return base | (at3 << 11);
    case XT5_2732_MA12: return base | (ma12 << 11);
    case XT5_2764_ATTR: return base | (ra3 << 11) | (at3 << 12);
    case XT5_2764_MA12: return base | (ra3 << 11) | (ma12 << 12);
    default: return base | (ma12 << 11) | (at3 << 12);
    }
}

// 3-bit colour with the intensity Y (RGBI monitor) or as grey (composite from RGB through
// G 470 R, R 1 k, B 2k2)
uint32_t Dbs1a::colour(int c, bool y) const
{
    int rr = schematic_rg ? (c >> 1) & 1 : (c >> 2) & 1;
    int gg = schematic_rg ? (c >> 2) & 1 : (c >> 1) & 1;
    int bb = c & 1;
    int on = y ? 0xFF : 0xAA, off = y ? 0x55 : 0x00;
    if (mono) {
        int v = (rr * 28 + gg * 60 + bb * 12) * 255 / 100;
        return 0xFF000000u | (uint32_t)(v << 16) | (uint32_t)(v << 8) | (uint32_t)v;
    }
    return 0xFF000000u | ((uint32_t)(rr ? on : off) << 16) | ((uint32_t)(gg ? on : off) << 8) | (uint32_t)(bb ? on : off);
}

void Dbs1a::render(uint32_t* argb, uint32_t bg)
{
    int w = screen_width(), h = screen_height();
    if (!r[1] || !r[6]) {   // the 6845 not set up yet: no picture
        for (int i = 0; i < w * h; i++) argb[i] = 0xFF000000u;
        return;
    }
    // frame number from the bus time (blinking): 12 MHz dots, R0+1 characters, (R4+1) rows + R5
    double line_s = (r[0] + 1) * dots / 12e6;
    double frame_s = line_s * ((r[4] + 1) * lines() + r[5]);
    uint64_t frame = frame_s > 0 && bus ? (uint64_t)(bus->seconds() / frame_s) : 0;
    bool blik = ((frame >> 4) & 1) != 0;   // CD4024 Q5 clocked by VSYNC
    int cmode = (r[10] >> 5) & 3;
    bool cur_on = cmode == 0 || (cmode == 2 && !((frame >> 3) & 1)) || (cmode == 3 && !((frame >> 4) & 1));
    unsigned start = ((unsigned)r[12] << 8) | r[13];
    unsigned cursor = ((unsigned)r[14] << 8) | r[15];
    int cs = r[10] & 0x1F, ce = r[11] & 0x1F;
    int nc = cols(), nr = rows(), nl = lines();
    for (int row = 0; row < nr; row++)
        for (int col = 0; col < nc; col++) {
            unsigned ma = (start + (unsigned)(row * nc + col)) & 0x3FFF;
            unsigned cell = ma & 0x7FF;
            uint8_t code = chr[cell], a = attr[cell];
            bool y = (a & 0x08) != 0;
            uint32_t fg = colour(a & 7, y), bgc = colour((a >> 4) & 7, y);
            bool hide = (a & 0x80) && blik;   // blinking: the background only
            bool is_cur = cur_on && ma == cursor;
            for (int ra = 0; ra < nl; ra++) {
                uint8_t bits = font[glyph_addr(code, a, ra & 15, ma) % font.size()];   // RA4 not connected
                if (is_cur && ((cs <= ce && ra >= cs && ra <= ce) || (cs > ce && (ra >= cs || ra <= ce)))) bits ^= 0xFF;
                uint32_t* p = argb + (size_t)(row * nl + ra) * w + col * dots;
                for (int x = 0; x < dots; x++) p[x] = (!hide && (bits & (0x80 >> x))) ? fg : bgc;
            }
        }
    (void)bg;
}

std::vector<std::string> Dbs1a::text_rows()
{
    std::vector<std::string> out;
    unsigned start = ((unsigned)r[12] << 8) | r[13];
    int nc = cols(), nr = rows();
    for (int row = 0; row < nr; row++) {
        std::string s;
        for (int col = 0; col < nc; col++) {
            uint8_t c = chr[(start + (unsigned)(row * nc + col)) & 0x7FF];
            s += c < 0x20 ? std::string(" ") : std::string(sapi_code_to_utf8(c));
        }
        out.push_back(s);
    }
    return out;
}

void Dbs1a::describe(json& out)
{
    char b[16];
    out["columns"] = cols();
    out["rows"] = rows();
    out["scan_lines"] = lines();
    snprintf(b, sizeof(b), "%04Xh", (((unsigned)r[12] << 8) | r[13]) & 0x3FFF);
    out["start"] = b;
    snprintf(b, sizeof(b), "%04Xh", (((unsigned)r[14] << 8) | r[15]) & 0x3FFF);
    out["cursor"] = b;
    std::string regs;   // R0..R15 as written
    for (int i = 0; i < 16; i++) {
        snprintf(b, sizeof(b), i ? " %02X" : "%02X", r[i]);
        regs += b;
    }
    out["registers"] = regs;
}

void Dbs1a::state(StateIO& io)
{
    io.arr(chr, sizeof(chr));
    io.arr(attr, sizeof(attr));
    io.arr(r, sizeof(r));
    io.v(ar);
}
