#include "cards/and1.h"
#include "core/bus.h"
#include "core/ihex.h"

#include <string.h>

using nlohmann::json;

static const double LINE_US = 64.0;
static const int FRAME_LINES = 320;
static const double INT_PULSE_US = 640.0;
static const int BLINK_FRAMES = 16;   // half period of blinking (assumption, ~0.65 s period)

And1::And1(const char* type) : Card(type)
{
    memset(vram, 0x20, sizeof(vram));
    for (int b = 0; b < 32; b++) sel_2k[b] = b == 7;   // AND-1: 3800-3FFF (D3A = 3205)
}

bool And1::configure(const json& cfg, std::string& err)
{
    // jumpers 42-43 MHB 2501 (latin) / 41-42 MHB 2502 (cyrillic)
    std::string cs, cg;
    if (!cfg_string(cfg, "charset", cs, "MHB2501", err)) return false;
    if (cs != "MHB2501" && cs != "MHB2502") { err = "charset must be MHB2501 or MHB2502"; return false; }
    cyrillic = cs == "MHB2502";
    if (!cfg_string(cfg, "chargen", cg, ("roms/and-1/" + cs + ".bin").c_str(), err)) return false;
    if (!rom_load(cg, chargen, 512, 0x00, 0, err)) return false;
    long long r;
    if (!cfg_number(cfg, "rows", r, 24, err)) return false;
    if (r != 20 && r != 24) { err = "rows must be 24 (jumper 47-48) or 20 (46-47)"; return false; }
    rows = (int)r;
    if (!cfg_bool(cfg, "int1", int1, true, err)) return false;
    if (!monitor.configure(cfg, err)) return false;
    if (type == "AND-1A") {
        if (!cfg_bool(cfg, "s1", s1_closed, false, err)) return false;
        if (!cfg_string(cfg, "decoder", decoder_file, "roms/and-1a/AND-1A_D3A_74S287.bin", err)) return false;
        std::vector<uint8_t> p;
        if (!rom_load(decoder_file, p, 256, 0xFF, 0, err)) return false;
        if (p.size() != 256) { err = decoder_file + ": the D3A PROM (74S287) has 256 entries"; return false; }
        for (int b = 0; b < 32; b++) {
            unsigned a = (unsigned)b << 11;
            unsigned i = ((a >> 15) & 1) | ((a >> 13) & 1) << 1 | (s1_closed ? 0u : 1u) << 2 | ((a >> 14) & 1) << 3 |
                         ((a >> 11) & 1) << 4 | ((a >> 12) & 1) << 5 | 0xC0u;
            sel_2k[b] = !(p[i] & 0x08);
        }
    }
    return true;
}

// Columns 0-31 of a row: row * 32 + column; columns 32-63 (A5 = 1): the area
// 300-3FF that rows 24-31 would use, 8 bytes per row. Columns 40-63 alias 32-39.
int And1::vram_index(uint16_t a)
{
    int row = (a >> 6) & 31, col = a & 63;
    if (col < 32) return row * 32 + col;
    return 0x300 + row * 8 + (col & 7);
}

bool And1::mem_read(uint16_t a, uint8_t& data)
{
    if (!sel_2k[a >> 11]) return false;
    data = vram[vram_index(a)];
    return true;
}

bool And1::mem_write(uint16_t a, uint8_t data)
{
    if (!sel_2k[a >> 11]) return false;
    vram[vram_index(a)] = data;
    return true;
}

int And1::decode_mem(uint16_t a, int, const char** what)
{
    if (!sel_2k[a >> 11]) return 0;
    *what = "VRAM";
    return DEC_READ | DEC_WRITE;
}

uint64_t And1::frame_cycles(void) const
{
    return (uint64_t)((double)bus->cpu_hz * LINE_US * FRAME_LINES / 1e6);
}

int And1::irq_lines()
{
    if (!int1 || !bus) return 0;
    uint64_t f = frame_cycles();
    uint64_t pulse = (uint64_t)((double)bus->cpu_hz * INT_PULSE_US / 1e6);
    return (bus->now % f) >= f - pulse ? IRQ_INT1 : 0;
}

void And1::render(uint32_t* argb, uint32_t bg)
{
    const uint32_t fg = monitor.argb;
    const int w = screen_width();
    uint64_t frame = bus ? bus->now / frame_cycles() : 0;
    bool blink_on = ((frame / BLINK_FRAMES) & 1) == 0;
    for (int row = 0; row < rows; row++) {
        int col = 0;
        while (col < COLS) {
            uint8_t v = vram[vram_index((uint16_t)(0x3800 + row * 64 + col))];
            int mode = v >> 6;
            const uint8_t* g = &chargen[(v & 0x3F) * 8];
            bool show = mode != 1 || blink_on;
            bool cursor = mode == 2 && blink_on;
            int span = mode == 3 && col + 1 < COLS ? 2 : 1;   // double width takes the next cell
            for (int y = 0; y < CELL_H; y++) {
                uint8_t bits = (show && y < 8) ? g[y] : 0;
                uint32_t* p = argb + (row * CELL_H + y) * w + col * CELL_W;
                for (int x = 0; x < CELL_W * span; x++) {
                    int dot = x / span;
                    bool on = dot < 5 && (bits & (0x10 >> dot));
                    if (cursor && y == 8) on = dot < 5;   // blinking underline
                    p[x] = on ? fg : bg;
                }
            }
            col += span;
        }
    }
}

std::string And1::char_utf8(uint8_t code) const
{
    static const char* const lat =
        "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_ !\"#\xC2\xA4%&'()*+,-./0123456789:;<=>?";
    static const char* const cyr[32] = {
        "Ю", "А", "Б", "Ц", "Д", "Е", "Ф", "Г", "Х", "И", "Й", "К", "Л", "М", "Н", "О",
        "П", "Я", "Р", "С", "Т", "У", "Ж", "В", "Ь", "Ы", "З", "Ш", "Э", "Щ", "Ч", "Ъ",
    };
    code &= 0x3F;
    if (cyrillic && code < 32) return cyr[code];
    // the latin table has the 2-byte "¤" at 24h: walk the UTF-8 string
    const char* p = lat;
    for (int i = 0; i < code; i++) p += ((unsigned char)*p >= 0xC0) ? 2 : 1;
    return std::string(p, ((unsigned char)*p >= 0xC0) ? 2 : 1);
}

std::vector<std::string> And1::text_rows()
{
    std::vector<std::string> out;
    for (int row = 0; row < rows; row++) {
        std::string s;
        for (int col = 0; col < COLS; col++) s += char_utf8(vram[vram_index((uint16_t)(0x3800 + row * 64 + col))]);
        out.push_back(s);
    }
    return out;
}

void And1::describe(json& out)
{
    out["columns"] = COLS;
    out["rows"] = rows;
    out["chargen"] = cyrillic ? "MHB 2502" : "MHB 2501";
    out["int1"] = int1;
    out["monitor"] = monitor.name;
}
