#include "cards/and30c.h"
#include "core/bus.h"
#include "core/charset.h"
#include "core/ihex.h"
#include "core/state.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

using nlohmann::json;

And30c::And30c() : Card("AND-30C")
{
    memset(ram, 0, sizeof(ram));
    memset(r, 0, sizeof(r));
    memset(acc, 0, sizeof(acc));
}

bool And30c::configure(const json& cfg, std::string& err)
{
    power_on_fill(ram, sizeof(ram), 0);
    memset(r, 0, sizeof(r));   // the program sets the 6845 up; until then no picture
    ar = 0;
    std::string e0, e1, dec, ce1, a7;
    if (!cfg_string(cfg, "eprom0", e0, "roms/and-30c/AND-30C_IC9_EPROM0.bin", err)) return false;
    if (!cfg_string(cfg, "eprom1", e1, "roms/and-30c/AND-30C_IC10_EPROM1.bin", err)) return false;
    if (!cfg_string(cfg, "decoder", dec, "roms/and-30c/AND-30C_IC2_74S287.bin", err)) return false;
    if (!cfg_string(cfg, "ce1", ce1, "gnd", err)) return false;
    if (ce1 != "gnd" && ce1 != "ref") { err = "ce1 must be gnd (JP10 to GND) or ref (X1 pin 7, as drawn)"; return false; }
    ce1_ref = ce1 == "ref";
    if (!cfg_string(cfg, "prom_a7", a7, "gnd", err)) return false;
    if (a7 != "gnd" && a7 != "map1") { err = "prom_a7 must be gnd or map1 (JP8)"; return false; }
    a7_map1 = a7 == "map1";
    if (!cfg_bool(cfg, "blank_on_access", blank_on_access, true, err)) return false;
    if (!monitor.configure(cfg, err)) return false;
    std::vector<uint8_t> lo, hi;
    if (!rom_load(e0, lo, 2048, 0xFF, 0, err)) return false;
    if (!rom_load(e1, hi, 2048, 0xFF, 0, err)) return false;
    lo.resize(2048, 0xFF);
    hi.resize(2048, 0xFF);
    font = lo;
    font.insert(font.end(), hi.begin(), hi.end());
    if (!rom_load(dec, prom, 256, 0xFF, 0, err)) return false;
    prom.resize(256, 0xFF);
    return true;
}

void And30c::reset()
{
    // /RES goes through IC1B/IC1C to the 6845 RESET (its counters) and switches the PROM off; the registers and
    // the RAM keep their contents
    reset_at = bus ? bus->now : 0;
}

// PROM IC2 for an access with /MR (read) or /MW (write); drive = the card answers on the data bus. The 7430 IC6
// picks the 6845 from the selected window when A5-A10 are all 1.
int And30c::select(uint16_t addr, bool write, int map, bool* drive) const
{
    *drive = false;
    if (ce1_ref) return SEL_NONE;   // /CE1 = /REF: the PROM works only in refresh cycles (none in the emulator)
    unsigned a7 = a7_map1 ? ((map & MAP_1) ? 1u : 0u) : 0u;
    unsigned idx = ((addr >> 11) & 0x1Fu) | (write ? 0x40u : 0x20u) | (a7 << 7);   // A5 = /MW, A6 = /MR
    uint8_t o = prom[idx];
    if (o & 0x04) return SEL_NONE;   // O3 = /SEL
    *drive = write ? (o & 0x08) == 0 : (o & 0x02) == 0;   // O4 = /WE, O2 = DIR (0: towards the bus)
    return (addr & 0x07E0) == 0x07E0 ? SEL_CRTC : SEL_RAM;
}

void And30c::note_access(void)
{
    acc[acc_head % ACC_N] = bus ? bus->now : 0;
    acc_head++;
}

// R14-R17 can be read (R16/R17 light pen: LPSTB not connected, 0); otherwise nothing drives the inner data bus of the
// card and the open 74LS245 reads FFh (assumption)
uint8_t And30c::crtc_read(uint16_t addr) const
{
    if ((addr & 0x10) && ar >= 14 && ar <= 15) return r[ar];
    if ((addr & 0x10) && (ar == 16 || ar == 17)) return 0x00;
    return 0xFF;
}

bool And30c::mem_read(uint16_t addr, uint8_t& data)
{
    bool drive;
    int s = select(addr, false, bus->map, &drive);
    if (s == SEL_NONE || !drive) return false;
    note_access();
    data = s == SEL_CRTC ? crtc_read(addr) : ram[addr & 0x7FF];
    return true;
}

bool And30c::mem_write(uint16_t addr, uint8_t data)
{
    bool drive;
    int s = select(addr, true, bus->map, &drive);
    if (s == SEL_NONE) return false;
    note_access();
    if (!drive) return true;
    if (s == SEL_RAM) {
        ram[addr & 0x7FF] = data;
        return true;
    }
    if (!(addr & 0x10)) {   // RS = A4 = 0: the address register
        ar = (uint8_t)(data & 0x1F);
        return true;
    }
    static const uint8_t mask[16] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x1F, 0x7F, 0x7F, 0x03, 0x1F, 0x7F, 0x1F,
                                      0x3F, 0xFF, 0x3F, 0xFF };
    if (ar < 16) r[ar] = (uint8_t)(data & mask[ar]);
    return true;
}

bool And30c::mem_peek(uint16_t addr, uint8_t& data)
{
    bool drive;
    int s = select(addr, false, bus->map, &drive);
    if (s == SEL_NONE || !drive) return false;
    data = s == SEL_CRTC ? crtc_read(addr) : ram[addr & 0x7FF];
    return true;
}

int And30c::decode_mem(uint16_t addr, int map, const char** what)
{
    bool drive;
    int s = select(addr, false, map, &drive);
    if (s == SEL_NONE) return 0;
    *what = s == SEL_RAM ? "video RAM" : (addr & 0x10) ? "6845 data register" : "6845 address register";
    return DEC_READ | DEC_WRITE;
}

int And30c::cols(void) const
{
    return (r[1] && r[6] && r[1] <= 128) ? r[1] : 80;
}

int And30c::rows(void) const
{
    return (r[1] && r[6] && r[6] <= 64) ? r[6] : 21;
}

int And30c::lines(void) const
{
    return (r[1] && r[6]) ? (r[9] & 0x1F) + 1 : 12;
}

int And30c::screen_width() const
{
    return cols() * DOTS;
}

int And30c::screen_height() const
{
    return rows() * lines();
}

double And30c::line_us(void) const
{
    return r[0] ? (r[0] + 1) * DOTS / DOT_HZ * 1e6 : 0.0;
}

int And30c::frame_lines(void) const
{
    return r[4] ? (r[4] + 1) * ((r[9] & 0x1F) + 1) + (r[5] & 0x1F) : 0;
}

void And30c::render(uint32_t* argb, uint32_t bg)
{
    const uint32_t fg = monitor.argb;
    const int w = screen_width(), h = screen_height();
    for (int i = 0; i < w * h; i++) argb[i] = bg;
    if (!r[1] || !r[6]) return;   // DE never comes: no picture until the 6845 is set up
    const int nc = cols(), nr = rows(), nl = lines();
    const double char_s = DOTS / DOT_HZ, line_s = line_us() * 1e-6, frame_s = line_s * frame_lines();
    const double now_s = bus ? (double)(bus->now - reset_at) / bus->cpu_hz : 0.0;
    const uint64_t frame = frame_s > 0 ? (uint64_t)(now_s / frame_s) : 0;
    int cmode = (r[10] >> 5) & 3;   // 0 steady, 1 off, 2 blink 1/16, 3 blink 1/32 of the field rate
    bool cur_on = cmode == 0 || (cmode == 2 && !((frame >> 3) & 1)) || (cmode == 3 && !((frame >> 4) & 1));
    unsigned start = ((unsigned)(r[12] & 0x3F) << 8) | r[13];
    unsigned cursor = ((unsigned)(r[14] & 0x3F) << 8) | r[15];
    int cs = r[10] & 0x1F, ce = r[11] & 0x1F;
    for (int row = 0; row < nr; row++)
        for (int col = 0; col < nc; col++) {
            unsigned ma = (start + (unsigned)(row * nc + col)) & 0x3FFF;
            uint8_t code = ram[ma & 0x7FF];   // MA11-MA13 are not connected
            const uint8_t* g = &font[(code & 0x80 ? 2048u : 0u) + (code & 0x7Fu) * 16u];
            bool is_cur = cur_on && ma == cursor;
            for (int ra = 0; ra < nl; ra++) {
                uint8_t bits = g[ra & 15] & 0xFC;   // RA4 not connected; D1, D0 not used
                if (is_cur && ((cs <= ce && ra >= cs && ra <= ce) || (cs > ce && (ra >= cs || ra <= ce)))) bits = 0xFC;
                uint32_t* p = argb + (size_t)(row * nl + ra) * w + col * DOTS;
                for (int x = 0; x < DOTS; x++)
                    if (bits & (0x80 >> x)) p[x] = fg;
            }
        }
    if (!blank_on_access || !bus || frame_s <= 0 || line_s <= 0) return;
    // IC12B: every CPU access blanks the video from its start for BLANK_S (an access inside a blank does not make it
    // longer). The accesses of the last frame are drawn where the beam was; the pixels leave the 74165 two
    // character clocks after the 6845 gave their address (74175 delays DE and CURSOR the same).
    const double t_now = (double)bus->now / bus->cpu_hz;
    const int lc = r[0] + 1, fl = frame_lines();
    double blank_until = -1.0;
    unsigned n = acc_head < ACC_N ? acc_head : ACC_N;
    for (unsigned k = 0; k < n; k++) {
        double t = (double)acc[(acc_head - n + k) % ACC_N] / bus->cpu_hz;
        if (t < t_now - frame_s || t > t_now || t < blank_until) continue;
        blank_until = t + BLANK_S;
        double phase = fmod(t - (double)reset_at / bus->cpu_hz, frame_s);
        int line = (int)(phase / line_s);
        double d0 = ((phase - line * line_s) / char_s - 2.0) * DOTS;
        int dots = (int)(BLANK_S * DOT_HZ + 0.5);
        for (int d = 0; d < dots; d++) {
            int dot = (int)floor(d0) + d, ln = line;
            while (dot < 0) { dot += lc * DOTS; ln--; }
            while (dot >= lc * DOTS) { dot -= lc * DOTS; ln++; }
            if (ln < 0 || ln >= fl || ln >= h || dot >= w) continue;   // in the retrace or another frame
            argb[(size_t)ln * w + dot] = bg;
        }
    }
}

std::vector<std::string> And30c::text_rows()
{
    std::vector<std::string> out;
    unsigned start = ((unsigned)(r[12] & 0x3F) << 8) | r[13];
    int nc = cols(), nr = rows();
    for (int row = 0; row < nr; row++) {
        std::string s;
        for (int col = 0; col < nc; col++) s += sapi_code_to_utf8(ram[(start + (unsigned)(row * nc + col)) & 0x7FF]);
        out.push_back(s);
    }
    return out;
}

void And30c::describe(json& out)
{
    char b[96];
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
    out["ce1"] = ce1_ref ? "ref" : "gnd";
    out["prom_a7"] = a7_map1 ? "map1" : "gnd";
    out["blank_on_access"] = blank_on_access;
    out["monitor"] = monitor.name;
    // the video signal the registers give (character clock 1.8 MHz): what a PAL TV needs is about 15.6 kHz, 50 Hz
    double lus = line_us();
    int fl = frame_lines();
    if (lus <= 0 || fl <= 0 || !r[1] || !r[6]) {
        out["video"] = "6845 not set up (no picture)";
        return;
    }
    double hk = 1e3 / lus, vh = hk * 1e3 / fl;
    snprintf(b, sizeof(b), "line %.2f us (%.3f kHz), %d lines, %.2f Hz", lus, hk, fl, vh);
    out["video"] = b;
    std::vector<std::string> warn;
    if (hk < 15.1 || hk > 16.2) warn.push_back("line frequency far from 15.625 kHz");
    if (vh < 45.0 || vh > 55.0) warn.push_back("frame frequency far from 50 Hz");
    if (r[1] >= r[0] + 1) warn.push_back("R1 >= R0+1: no horizontal retrace");
    if ((r[3] & 0x0F) == 0) warn.push_back("R3 horizontal sync width 0 (16 characters on the MC6845)");
    if (r[2] + (r[3] & 0x0F) > r[0] + 1) warn.push_back("horizontal sync beyond the line (R2 + R3 > R0 + 1)");
    if (r[6] > r[4] + 1) warn.push_back("R6 > R4+1: no vertical retrace");
    if (r[7] >= r[4] + 1) warn.push_back("R7 >= R4+1: no vertical sync");
    if (r[6] * lines() > 288) warn.push_back("more than 288 visible lines: cut off on a TV");
    out["video_warnings"] = warn;
}

void And30c::state(StateIO& io)
{
    io.arr(ram, sizeof(ram));
    io.arr(r, sizeof(r));
    io.v(ar);
    io.v(reset_at);
}
