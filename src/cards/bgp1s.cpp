#include "cards/bgp1s.h"
#include "core/state.h"
#include "core/bus.h"

#include <string.h>

using nlohmann::json;

bool Bgp1s::configure(const json& cfg, std::string& err)
{
    power_on_fill(vram, sizeof(vram), 0);
    long long b;
    if (!cfg_number(cfg, "base", b, 0xC000, err)) return false;
    if (b & 0x3FFF || b < 0 || b > 0xC000) { err = "base must be 0000h, 4000h, 8000h or C000h (S3 AD15, AD14)"; return false; }
    base = (uint16_t)b;
    std::string s;
    if (!cfg_string(cfg, "map1", s, "ignore", err)) return false;
    if (s == "ignore") map_mode = 0;
    else if (s == "high") map_mode = 1;
    else if (s == "low") map_mode = 2;
    else { err = "map1 must be ignore (S3 MDIS), high or low (MLEV)"; return false; }
    if (!cfg_string(cfg, "colors", s, "ace", err)) return false;
    if (s != "ace" && s != "rgbm") { err = "colors must be ace (Colorace) or rgbm (S1)"; return false; }
    ace = s == "ace";
    if (!cfg_string(cfg, "int", s, "int0", err)) return false;
    if (s == "int0") int_line = IRQ_INT0;
    else if (s == "int1") int_line = IRQ_INT1;
    else if (s == "none") int_line = 0;
    else { err = "int must be int0, int1 or none (S2)"; return false; }
    if (!cfg_string(cfg, "output", s, "rgb", err)) return false;
    if (!set_ink(s, err)) return false;
    if (!cfg_bool(cfg, "wait_states", waits, true, err)) return false;
    return true;
}

bool Bgp1s::set_ink(const std::string& value, std::string& err)
{
    if (value != "rgb" && value != "mono") { err = "output must be rgb (SCART) or mono (B/W composite)"; return false; }
    mono = value == "mono";
    return true;
}

bool Bgp1s::selected(uint16_t a, int map) const
{
    if ((a & 0xC000) != base) return false;
    if (map_mode == 1) return (map & MAP_1) != 0;
    if (map_mode == 2) return (map & MAP_1) == 0;
    return true;
}

// Called for every bus time the CPU card looks at: a request at each start of line 256.
void Bgp1s::update_int(void)
{
    if (!bus || bus->now <= int_checked) return;
    double frame = (double)bus->cpu_hz * LINE_DOTS * FRAME_LINES / DOT_HZ;   // cycles
    double at = (double)bus->cpu_hz * LINE_DOTS * 256 / DOT_HZ;              // line 256 in the frame
    uint64_t before = (uint64_t)(((double)int_checked - at) / frame + 1e6);
    uint64_t after = (uint64_t)(((double)bus->now - at) / frame + 1e6);
    if (after != before) int_req = true;
    int_checked = bus->now;
}

bool Bgp1s::mem_read(uint16_t a, uint8_t&)
{
    // write only: no data. An opcode fetch from the window (M1) releases the interrupt (IC14A).
    if (selected(a, bus->map) && a == bus->pc) {
        update_int();
        int_req = false;
    }
    return false;
}

bool Bgp1s::mem_write(uint16_t a, uint8_t data)
{
    if (!selected(a, bus->map)) return false;
    vram[a & 0x3FFF] = data;
    if (waits) {
        // RDY until the end of state s5 (the /WE pulse) of the byte period: 1-7 dots
        double dots = (double)bus->now * DOT_HZ / (double)bus->cpu_hz;
        double d = dots - 6.0 * (double)(uint64_t)(dots / 6.0);
        double to_end = d < 5.0 ? 6.0 - d : 12.0 - d;
        bus->wait += (int)(to_end * (double)bus->cpu_hz / DOT_HZ);
    }
    return true;
}

int Bgp1s::irq_lines()
{
    update_int();
    return int_req ? int_line : 0;
}

int Bgp1s::decode_mem(uint16_t a, int map, const char** what)
{
    if (!selected(a, map)) return 0;
    *what = "VRAM (write only)";
    return DEC_WRITE;
}

void Bgp1s::render(uint32_t* argb, uint32_t bg)
{
    static const uint32_t rgb[8] = { 0xFF000000u, 0xFF0000FFu, 0xFF00FF00u, 0xFF00FFFFu,
                                     0xFFFF0000u, 0xFFFF00FFu, 0xFFFFFF00u, 0xFFFFFFFFu };
    for (int y = 0; y < 256; y++) {
        uint32_t* out = argb + (size_t)y * 288;
        const uint8_t* line = vram + y * 64;
        const uint8_t* other = vram + (ace ? (y ^ 1) : y) * 64;   // the auxiliary byte (Colorace: line y ^ 1)
        for (int x = 0; x < 48; x++) {
            uint8_t v = line[x];
            int a = (v >> 6) & 1, b = v >> 7, d = (other[x] >> 6) & 1, c = other[x] >> 7;
            int r = a | d, bl = b | c, g = (!a && !b) || (!c && !d);
            uint32_t ink = mono ? 0xFFFFFFFFu : rgb[r << 2 | g << 1 | bl];
            for (int i = 0; i < 6; i++) out[x * 6 + i] = (v & (1 << i)) ? ink : bg;
        }
    }
}

void Bgp1s::describe(json& out)
{
    char b[8];
    snprintf(b, sizeof(b), "%04Xh", base);
    out["base"] = b;
    static const char* const maps[] = { "ignore", "high", "low" };
    out["map1"] = maps[map_mode];
    out["colors"] = ace ? "ace" : "rgbm";
    out["int"] = int_line == IRQ_INT0 ? "int0" : int_line == IRQ_INT1 ? "int1" : "none";
    out["int_request"] = int_req;
    out["output"] = mono ? "mono" : "rgb";
}

void Bgp1s::state(StateIO& io)
{
    io.arr(vram, sizeof(vram));
    io.v(int_req);
    io.v(int_checked);
}
