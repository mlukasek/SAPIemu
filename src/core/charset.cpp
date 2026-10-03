#include "core/charset.h"

#include <string.h>

// Glyph tables were read off the AND-1Z_CharGen.bin character generator
// (see docs/desky/AND-1Z-89.md). Glyphs that are just a full block are
// undefined positions in the ROM.

static const char* const k_frame[32] = {
    "╯", "╭", "╮", "╰", "└", "┌", "┐", "┘",
    "┴", "├", "┬", "┤", "│", "─", "┼", "█",
    "█", "█", "█", "█", "█", "█", "█", "█",
    "█", "█", "█", "█", "█", "█", "█", "█",
};

// C0-FF, KOI-8 cs2 as present in the ROM
static const char* const k_czech[64] = {
    "█", "á", "█", "č", "ď", "ě", "ŕ", "█", // C0
    "ü", "í", "ů", "ĺ", "ľ", "ö", "ň", "ó", // C8
    "ô", "ä", "ř", "š", "ť", "ú", "█", "é", // D0
    "à", "ý", "ž", "█", "ˇ", "█", "¬", "█", // D8
    "´", "Á", "█", "Č", "Ď", "Ě", "Ŕ", "█", // E0
    "Ü", "Í", "Ů", "Ĺ", "Ľ", "Ö", "Ň", "Ó", // E8
    "Ô", "Ä", "Ř", "Š", "Ť", "Ú", "█", "É", // F0
    "À", "Ý", "Ž", "█", "█", "█", "█", "█", // F8
};

static uint32_t k_czech_cp[64];   // code points for reverse lookup (0 = none)
static char k_sextant[64][5];      // UTF-8 for 80-BF
static char k_ascii[128][2];
static bool g_init = false;

static int utf8_encode(uint32_t cp, char* out)
{
    if (cp < 0x80) { out[0] = (char)cp; out[1] = 0; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 0x3F)); out[2] = 0; return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F)); out[3] = 0; return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[3] = (char)(0x80 | (cp & 0x3F)); out[4] = 0;
    return 4;
}

static void init_tables(void)
{
    if (g_init) return;
    for (int i = 0; i < 128; i++) { k_ascii[i][0] = (char)i; k_ascii[i][1] = 0; }
    // Semigraphics: code 80h+p, bit0 = top-left, bit1 = top-right, bit2 = middle-left,
    // bit3 = middle-right, bit4 = bottom-left, bit5 = bottom-right. This is the same
    // ordering as the Unicode "Symbols for Legacy Computing" sextants (U+1FB00..).
    for (int p = 0; p < 64; p++) {
        uint32_t cp;
        if (p == 0) cp = ' ';
        else if (p == 63) cp = 0x2588;
        else if (p == 21) cp = 0x258C;     // left half
        else if (p == 42) cp = 0x2590;     // right half
        else cp = 0x1FB00 + (uint32_t)(p - 1 - (p > 21 ? 1 : 0) - (p > 42 ? 1 : 0));
        utf8_encode(cp, k_sextant[p]);
    }
    for (int i = 0; i < 64; i++) {
        size_t pos = 0;
        std::string s = k_czech[i];
        int32_t cp = utf8_next(s, pos);
        k_czech_cp[i] = (cp == 0x2588 || cp == 0x02C7 || cp == 0x00AC || cp == 0x00B4) ? 0 : (uint32_t)cp;
    }
    g_init = true;
}

const char* sapi_code_to_utf8(uint8_t code)
{
    init_tables();
    if (code < 0x20) return k_frame[code];
    if (code == 0x5E) return "↑";     // up arrow in the ROM
    if (code == 0x7F) return "▒";
    if (code < 0x80) return k_ascii[code];
    if (code < 0xC0) return k_sextant[code - 0x80];
    return k_czech[code - 0xC0];
}

int sapi_code_from_unicode(uint32_t cp)
{
    init_tables();
    if (cp == '\n') return 0x0D;
    if (cp < 0x80) return (int)cp;
    for (int i = 0; i < 64; i++)
        if (k_czech_cp[i] == cp) return 0xC0 + i;
    return -1;
}

int32_t utf8_next(const std::string& s, size_t& pos)
{
    if (pos >= s.size()) return -1;
    uint8_t c = (uint8_t)s[pos++];
    if (c < 0x80) return c;
    int extra;
    int32_t cp;
    if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
    else return -1;
    for (int i = 0; i < extra; i++) {
        if (pos >= s.size()) return -1;
        uint8_t cc = (uint8_t)s[pos++];
        if ((cc & 0xC0) != 0x80) return -1;
        cp = (cp << 6) | (cc & 0x3F);
    }
    return cp;
}

std::string sapi_codes_from_utf8(const std::string& s)
{
    std::string out;
    size_t pos = 0;
    while (pos < s.size()) {
        int32_t cp = utf8_next(s, pos);
        if (cp < 0) continue;
        int code = sapi_code_from_unicode((uint32_t)cp);
        if (code >= 0) out += (char)code;
    }
    return out;
}
