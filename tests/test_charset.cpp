// Character set: semigraphic bit order checked against the character generator ROM,
// KOI-8 cs2 round trip, Intel HEX parser.
#include <string.h>
#include "core/charset.h"
#include "core/ihex.h"
#include "core/util.h"
#include "test_util.h"

static void test_semigraphics(void)
{
    std::vector<uint8_t> rom;
    CHECK(file_read_all(data_path("roms/and-1z-89/AND-1Z_CharGen.bin"), rom));
    CHECK_EQ(rom.size(), 4096);
    if (rom.size() != 4096) return;
    // 80h+p: 2x3 cells of 3x4 pixels; sample the centre pixel of every cell
    for (int p = 0; p < 64; p++) {
        const uint8_t* g = &rom[(0x80 + p) * 16];
        for (int cy = 0; cy < 3; cy++) {
            for (int cx = 0; cx < 2; cx++) {
                int y = cy * 4 + 1, x = cx * 3 + 1;
                bool lit = (g[11 - y] & (0x80 >> x)) != 0;
                bool expected = (p >> (cy * 2 + cx)) & 1;
                if (lit != expected) fprintf(stderr, "semigraphic %02X cell %d,%d\n", 0x80 + p, cx, cy);
                CHECK(lit == expected);
            }
        }
    }
}

static void test_czech(void)
{
    CHECK_EQ(sapi_code_from_unicode(0x017E), 0xDA);   // ž
    CHECK_EQ(sapi_code_from_unicode(0x017D), 0xFA);   // Ž
    CHECK_EQ(sapi_code_from_unicode(0x016F), 0xCA);   // ů
    CHECK_EQ(sapi_code_from_unicode('A'), 'A');
    CHECK_EQ(sapi_code_from_unicode(0x20AC), -1);     // € not available
    std::string codes = sapi_codes_from_utf8("Příliš");
    CHECK_EQ(codes.size(), 6);
    CHECK_EQ((uint8_t)codes[1], 0xD2);
    // every mapped letter converts back to the same text
    for (int c = 0xC0; c < 0x100; c++) {
        std::string s = sapi_code_to_utf8((uint8_t)c);
        std::string back = sapi_codes_from_utf8(s);
        if (back.size() == 1) CHECK_EQ((uint8_t)back[0], c);
    }
    CHECK(strcmp(sapi_code_to_utf8('x'), "x") == 0);
}

static void store(void* user, uint32_t addr, uint8_t v)
{
    uint8_t* mem = (uint8_t*)user;
    if (addr < 256) mem[addr] = v;
}

static void test_ihex(void)
{
    uint8_t mem[256];
    memset(mem, 0, sizeof(mem));
    std::string err;
    // 3 bytes 01 02 03 at 0010h; checksum = -(03+00+10+00+01+02+03) = E7h
    CHECK(ihex_parse(":03001000010203E7\n:00000001FF\n", store, mem, err));
    CHECK_EQ(mem[0x10], 0x01);
    CHECK_EQ(mem[0x11], 0x02);
    CHECK_EQ(mem[0x12], 0x03);
    CHECK(!ihex_parse(":03001000010203E8\n", store, mem, err));   // bad checksum
    // extended linear address record (type 04) as used by the ZRMD-1 .hex images
    memset(mem, 0, sizeof(mem));
    CHECK(ihex_parse(":020000040000FA\n:01002000AA35\n:00000001FF\n", store, mem, err));
    CHECK_EQ(mem[0x20], 0xAA);
}

int main(void)
{
    data_root_init();
    test_semigraphics();
    test_czech();
    test_ihex();
    return TEST_RESULT();
}
