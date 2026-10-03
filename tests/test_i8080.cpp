// Intel 8080 core (third_party/i8080) with the CP/M CPU test programs and a
// minimal BDOS (functions 2 and 9). Without an argument the quick tests run;
// "8080EXM.COM" runs the exerciser (minutes, release builds only).
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "core/util.h"
#include "test_util.h"

extern "C" {
#include "i8080.h"
}

static uint8_t g_mem[65536];

static uint8_t rd(void*, uint16_t a) { return g_mem[a]; }
static void wr(void*, uint16_t a, uint8_t v) { g_mem[a] = v; }
static uint8_t in(void*, uint8_t) { return 0xFF; }
static void out(void*, uint8_t, uint8_t) {}

// Run a .COM file at 0100h until it jumps to 0000h (warm boot); returns its console output.
static std::string run_com(const char* file, unsigned long long& cycles)
{
    memset(g_mem, 0, sizeof(g_mem));
    std::vector<uint8_t> com;
    if (!file_read_all(data_path(std::string("tests/data/i8080/") + file), com)) {
        fprintf(stderr, "cannot read %s\n", file);
        return std::string();
    }
    memcpy(&g_mem[0x100], com.data(), com.size());
    g_mem[0x0005] = 0xC9;   // BDOS entry: RET (served below before it executes)
    g_mem[0x0006] = 0x00;   // top of the TPA at 0F000h... the tests read the word at 0006h
    g_mem[0x0007] = 0xF0;

    i8080 c;
    i8080_init(&c);
    c.read_byte = rd;
    c.write_byte = wr;
    c.port_in = in;
    c.port_out = out;
    c.pc = 0x0100;
    std::string text;
    for (;;) {
        if (c.pc == 0x0000) break;   // warm boot: the program ended
        if (c.pc == 0x0005) {
            if (c.c == 2) {
                if (c.e != '\r') text += (char)c.e;
            } else if (c.c == 9) {
                for (uint16_t a = (uint16_t)((c.d << 8) | c.e); g_mem[a] != '$'; a++)
                    if (g_mem[a] != '\r') text += (char)g_mem[a];
            }
        }
        i8080_step(&c);
    }
    cycles = c.cyc;
    return text;
}

static bool check(const char* file, const char* must, const char* must_not)
{
    unsigned long long cyc = 0;
    std::string t = run_com(file, cyc);
    printf("---- %s (%llu cycles)\n", file, cyc);
    for (char ch : t)   // CPUTEST also prints NUL characters
        if (ch) putchar(ch);
    putchar('\n');
    bool ok = t.find(must) != std::string::npos && (!must_not || t.find(must_not) == std::string::npos);
    if (!ok) fprintf(stderr, "%s: unexpected output\n", file);
    return ok;
}

int main(int argc, char** argv)
{
    test_init();
    if (argc > 1) {   // the exerciser: every group must report OK
        CHECK(check(argv[1], "Tests complete", "ERROR"));
        return TEST_RESULT();
    }
    CHECK(check("TST8080.COM", "CPU IS OPERATIONAL", nullptr));
    CHECK(check("8080PRE.COM", "8080 Preliminary tests complete", "ERROR"));
    CHECK(check("CPUTEST.COM", "CPU TESTS OK", nullptr));
    return TEST_RESULT();
}
