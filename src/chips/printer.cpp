#include "chips/printer.h"
#include "core/card.h"
#include "core/charset.h"
#include "core/log.h"
#include "core/state.h"
#include "core/util.h"

#include <filesystem>
#include <string.h>

using nlohmann::json;

static const size_t MAX_LINES = 5000;

// Unicode for 80h-FFh
static const uint16_t k_cp852[128] = {
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x016F, 0x0107, 0x00E7,
    0x0142, 0x00EB, 0x0150, 0x0151, 0x00EE, 0x0179, 0x00C4, 0x0106,
    0x00C9, 0x0139, 0x013A, 0x00F4, 0x00F6, 0x013D, 0x013E, 0x015A,
    0x015B, 0x00D6, 0x00DC, 0x0164, 0x0165, 0x0141, 0x00D7, 0x010D,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x0104, 0x0105, 0x017D, 0x017E,
    0x0118, 0x0119, 0x00AC, 0x017A, 0x010C, 0x015F, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00C1, 0x00C2, 0x011A,
    0x015E, 0x2563, 0x2551, 0x2557, 0x255D, 0x017B, 0x017C, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x0102, 0x0103,
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x00A4,
    0x0111, 0x0110, 0x010E, 0x00CB, 0x010F, 0x0147, 0x00CD, 0x00CE,
    0x011B, 0x2518, 0x250C, 0x2588, 0x2584, 0x0162, 0x016E, 0x2580,
    0x00D3, 0x00DF, 0x00D4, 0x0143, 0x0144, 0x0148, 0x0160, 0x0161,
    0x0154, 0x00DA, 0x0155, 0x0170, 0x00FD, 0x00DD, 0x0163, 0x00B4,
    0x00AD, 0x02DD, 0x02DB, 0x02C7, 0x02D8, 0x00A7, 0x00F7, 0x00B8,
    0x00B0, 0x00A8, 0x02D9, 0x0171, 0x0158, 0x0159, 0x25A0, 0x00A0,
};
static const uint16_t k_kamenicky[128] = {
    0x010C, 0x00FC, 0x00E9, 0x010F, 0x00E4, 0x010E, 0x0164, 0x010D,
    0x011B, 0x011A, 0x0139, 0x00CD, 0x013E, 0x013A, 0x00C4, 0x00C1,
    0x00C9, 0x017E, 0x017D, 0x00F4, 0x00F6, 0x00D3, 0x016F, 0x00DA,
    0x00FD, 0x00D6, 0x00DC, 0x0160, 0x013D, 0x00DD, 0x0158, 0x0165,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x0148, 0x0147, 0x016E, 0x00D4,
    0x0161, 0x0159, 0x0155, 0x0154, 0x00BC, 0x00A7, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
    0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
    0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
    0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
};

Printer::~Printer()
{
    if (f) fclose(f);
}

bool Printer::configure(const json& p, std::string& err)
{
    long long v;
    std::string cs;
    if (!cfg_string(p, "output", output, "work/printer/tiskarna.prn", err)) return false;
    if (!cfg_number(p, "busy_us", v, 100, err)) return false;
    if (v < 0 || v > 1000000) {
        err = "printer.busy_us must be 0-1000000";
        return false;
    }
    busy_us = (uint32_t)v;
    if (!cfg_string(p, "charset", cs, "koi8cs", err)) return false;
    if (cs == "koi8cs") charset = CS_KOI8CS;
    else if (cs == "kamenicky") charset = CS_KAMENICKY;
    else if (cs == "cp852") charset = CS_CP852;
    else if (cs == "ascii") charset = CS_ASCII;
    else {
        err = "printer.charset must be koi8cs, kamenicky, cp852 or ascii";
        return false;
    }
    return true;
}

std::string Printer::path(void) const
{
    return output.empty() ? std::string() : data_path(output);
}

void Printer::take(uint8_t b, uint64_t now, uint32_t hz)
{
    busy_until = now + (uint64_t)hz * busy_us / 1000000;
    ack_pending = true;
    bytes++;
    if (!f && !open_failed && !output.empty() && battery_files_enabled()) {   // tests: no file
        std::string p = path();
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(p).parent_path(), ec);
        f = fopen(p.c_str(), "ab");
        if (!f) {
            open_failed = true;
            LOGE("printer: cannot write %s", p.c_str());
        }
    }
    if (f) fputc(b, f);
    put(b);
}

bool Printer::ack_due(uint64_t now)
{
    if (!ack_pending || now < busy_until) return false;
    ack_pending = false;
    return true;
}

void Printer::flush(void)
{
    if (f) fflush(f);
}

void Printer::state(StateIO& io)
{
    // the paper and the file are outside the machine
    io.v(busy_until);
    io.v(ack_pending);
}

// ---------------------------------------------------------------- the text view

static void utf8_put(std::string& s, uint32_t cp)
{
    if (cp < 0x80) {
        s += (char)cp;
    } else if (cp < 0x800) {
        s += (char)(0xC0 | (cp >> 6));
        s += (char)(0x80 | (cp & 0x3F));
    } else {
        s += (char)(0xE0 | (cp >> 12));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    }
}

std::string Printer::current_line(void) const
{
    std::string s;
    for (uint32_t cp : cur) utf8_put(s, cp);
    return s;
}

void Printer::glyph(uint32_t cp)
{
    if (col < cur.size()) {
        // printed over: the later character stays, an underline or a space keeps the letter
        if (cur[col] == ' ' || (cp != ' ' && cp != '_')) cur[col] = cp;
    } else {
        while (cur.size() < col) cur.push_back(' ');
        cur.push_back(cp);
    }
    col++;
}

void Printer::end_line(void)
{
    lines.push_back(current_line());
    if (lines.size() > MAX_LINES) lines.erase(lines.begin(), lines.begin() + (lines.size() - MAX_LINES));
    cur.clear();
    col = 0;
}

void Printer::new_sheet(void)
{
    lines.clear();
    cur.clear();
    col = 0;
    generation++;
}

std::string Printer::text(size_t max_lines) const
{
    size_t n = lines.size() + 1, first = n > max_lines ? n - max_lines : 0;
    std::string s;
    for (size_t i = first; i < lines.size(); i++) {
        s += lines[i];
        s += '\n';
    }
    s += current_line();
    return s;
}

// bytes after ESC c (ESC/P); -1 = to a NUL, -2 = a graphics command (its own header)
static int esc_params(uint8_t c)
{
    switch (c) {
    case '!': case '-': case '3': case 'A': case 'J': case 'j': case 'Q': case 'l': case 'R': case 'S':
    case 'U': case 'W': case 'w': case 'x': case 'k': case 't': case 'p': case 'a': case 'r': case 'N':
    case 'C': case 'q': case 'm': case 'i': case 'I': case '+': case 'V': case '/':
        return 1;
    case '$': case '\\': case 'e': case 'f': case 'c':
        return 2;
    case ':': case 'X':
        return 3;
    case 'B': case 'D': case 'b':
        return -1;
    case '*': case 'K': case 'L': case 'Y': case 'Z': case '(':
        return -2;
    default:
        return 0;   // ESC @, E, F, 4, 5, G, H, M, P, g, O, T, 0, 1, 2, 6, 7, 8, 9, < ...
    }
}

void Printer::put(uint8_t b)
{
    generation++;
    switch (esc) {
    case 1: {   // the command after ESC
        esc_cmd = b;
        int n = esc_params(b);
        if (b == 'C') {   // ESC C n, ESC C NUL n
            esc = 7;
        } else if (n > 0) {
            skip = (uint32_t)n;
            esc = 2;
        } else if (n == -1) {
            esc = 3;
        } else if (n == -2) {
            esc = b == '*' || b == '(' ? 4 : 5;
        } else {
            esc = 0;
        }
        return;
    }
    case 2:   // parameters
        if (--skip == 0) esc = 0;
        return;
    case 3:   // tabs to a NUL
        if (b == 0) esc = 0;
        return;
    case 4:   // ESC * m / ESC ( c: one byte, then nL nH
        esc_m = b;
        esc = 5;
        return;
    case 5:   // nL
        esc_lo = b;
        esc = 6;
        return;
    case 6: {   // nH: the data follow
        uint32_t n = esc_lo | ((uint32_t)b << 8);
        if (esc_cmd == '*') n *= esc_m >= 71 ? 6u : esc_m >= 32 ? 3u : 1u;   // columns of 8, 24 or 48 dots
        skip = n;
        esc = n ? 2 : 0;
        return;
    }
    case 7:   // ESC C: page length in lines, or NUL and inches
        if (b == 0) {
            skip = 1;
            esc = 2;
        } else {
            esc = 0;
        }
        return;
    default:
        break;
    }
    switch (b) {
    case 0x1B: esc = 1; return;
    case 0x0D: col = 0; return;
    case 0x0A: end_line(); return;
    case 0x0C:
        if (!cur.empty()) end_line();
        lines.push_back("\f");
        return;
    case 0x09: col = (col / 8 + 1) * 8; return;
    case 0x08: if (col) col--; return;
    default: break;
    }
    if (b < 0x20 || b == 0x7F) return;   // SO, SI, DC2, DC4, CAN, BEL...: no glyph
    if (b < 0x80) {
        glyph(b);
        return;
    }
    switch (charset) {
    case CS_KAMENICKY: glyph(k_kamenicky[b - 0x80]); return;
    case CS_CP852: glyph(k_cp852[b - 0x80]); return;
    case CS_KOI8CS:
        if (b >= 0xC0) {   // the Czech letters of KOI-8 cs2, as on the screen
            const char* u = sapi_code_to_utf8(b);
            size_t pos = 0;
            int32_t cp = utf8_next(u, pos);
            glyph(cp > 0 ? (uint32_t)cp : '?');
            return;
        }
        glyph('?');
        return;
    default:
        glyph('?');
        return;
    }
}
