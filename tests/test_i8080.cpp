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
static std::string run_com(const char* file, unsigned long long& cycles, bool i8085 = false)
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
    c.i8085 = i8085;
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

static bool check(const char* file, const char* must, const char* must_not, bool i8085 = false)
{
    unsigned long long cyc = 0;
    std::string t = run_com(file, cyc, i8085);
    printf("---- %s%s (%llu cycles)\n", file, i8085 ? " (8085)" : "", cyc);
    for (char ch : t)   // CPUTEST also prints NUL characters
        if (ch) putchar(ch);
    putchar('\n');
    bool ok = t.find(must) != std::string::npos && (!must_not || t.find(must_not) == std::string::npos);
    if (!ok) fprintf(stderr, "%s: unexpected output\n", file);
    return ok;
}

// ---------------------------------------------------------------- 8085 mode

// A program at 0000h in 8085 mode; returns the core after `steps` instructions.
static i8080 run85(const std::vector<uint8_t>& prog, int steps)
{
    memset(g_mem, 0, sizeof(g_mem));
    memcpy(g_mem, prog.data(), prog.size());
    i8080 c;
    i8080_init(&c);
    c.read_byte = rd;
    c.write_byte = wr;
    c.port_in = in;
    c.port_out = out;
    c.i8085 = true;
    c.sp = 0x8000;
    for (int i = 0; i < steps; i++) i8080_step(&c);
    return c;
}

// T-states of single instructions (Intel MCS-80/85 table 5-1) and the undocumented ones.
static void test_8085_timing(void)
{
    struct T { std::vector<uint8_t> code; int t; const char* what; } ts[] = {
        { { 0x41 }, 4, "MOV B,C" }, { { 0x04 }, 4, "INR B" }, { { 0x23 }, 6, "INX H" },
        { { 0xC5 }, 12, "PUSH B" }, { { 0xCD, 0x00, 0x10 }, 18, "CALL" }, { { 0xC9 }, 10, "RET" },
        { { 0xC2, 0x00, 0x10 }, 10, "JNZ taken" }, { { 0xCA, 0x00, 0x10 }, 7, "JZ not taken" },
        { { 0xC4, 0x00, 0x10 }, 18, "CNZ taken" }, { { 0xCC, 0x00, 0x10 }, 9, "CZ not taken" },
        { { 0xC0 }, 12, "RNZ taken" }, { { 0xC8 }, 6, "RZ not taken" }, { { 0xFF }, 12, "RST 7" },
        { { 0xE3 }, 16, "XTHL" }, { { 0xE9 }, 6, "PCHL" }, { { 0x76 }, 5, "HLT" },
        { { 0x20 }, 4, "RIM" }, { { 0x08 }, 10, "DSUB" }, { { 0x10 }, 7, "ARHL" }, { { 0x28, 1 }, 10, "LDHI" },
        { { 0xCB }, 6, "RSTV not taken" }, { { 0xDD, 0x00, 0x10 }, 10, "JNK taken" }, { { 0xED }, 10, "LHLX" },
    };
    for (const T& t : ts) {
        i8080 c = run85(t.code, 1);   // flags 0: NZ, K = 0, V = 0
        if ((int)c.cyc != t.t) fprintf(stderr, "8085 %s: %lu T, expected %d\n", t.what, c.cyc, t.t);
        CHECK_EQ((int)c.cyc, t.t);
    }
}

static void test_8085_instructions(void)
{
    // ANA sets AC on the 8085; PUSH PSW holds V (bit 1) and K (bit 5)
    i8080 c = run85({ 0x3E, 0x0F, 0xE6, 0xF0, 0xF5 }, 3);   // MVI A,0Fh; ANI F0h; PUSH PSW
    CHECK(c.hf && c.zf);
    CHECK_EQ(g_mem[0x7FFE], 0x54);   // Z, AC, P; V = 0, K = 0, bit 1 not forced
    c = run85({ 0x3E, 0x7F, 0xC6, 0x01, 0xF5, 0xCB }, 4);   // 7Fh + 1: overflow, V = 1, S = 1, K = 0
    CHECK(c.vf && c.sf && !c.kf);
    CHECK_EQ(g_mem[0x7FFE] & 0x22, 0x02);
    CHECK_EQ(c.pc, 0x40);   // RSTV taken
    c = run85({ 0x21, 0xFF, 0xFF, 0x23 }, 2);   // INX H from FFFFh: K = 1
    CHECK(c.kf);
    // DSUB, LDHI, LDSI, SHLX, LHLX, ARHL, RDEL
    c = run85({ 0x21, 0x34, 0x12, 0x01, 0x35, 0x02, 0x08 }, 3);   // 1234h - 0235h
    CHECK_EQ(c.h, 0x0F);
    CHECK_EQ(c.l, 0xFF);
    CHECK(!c.cf && !c.zf);
    c = run85({ 0x21, 0x00, 0x20, 0x28, 0x10, 0x38, 0x02 }, 3);   // LDHI 10h, LDSI 2
    CHECK_EQ(c.d, 0x80);
    CHECK_EQ(c.e, 0x02);
    c = run85({ 0x11, 0x00, 0x30, 0x21, 0xCD, 0xAB, 0xD9, 0x21, 0, 0, 0xED }, 5);   // SHLX, LHLX
    CHECK_EQ(g_mem[0x3000], 0xCD);
    CHECK_EQ(g_mem[0x3001], 0xAB);
    CHECK_EQ(c.h, 0xAB);
    c = run85({ 0x21, 0x03, 0x80, 0x10 }, 2);   // ARHL 8003h -> C001h, CY = 1
    CHECK_EQ(c.h, 0xC0);
    CHECK_EQ(c.l, 0x01);
    CHECK(c.cf);
    c = run85({ 0x11, 0x01, 0x80, 0x37, 0x18 }, 3);   // STC; RDEL 8001h -> 0003h, CY = 1
    CHECK_EQ(c.d, 0x00);
    CHECK_EQ(c.e, 0x03);
    CHECK(c.cf);
    // RIM and SIM: masks, R7.5, SOD; SID
    c = run85({ 0x3E, 0xCD, 0x30, 0x20 }, 3);   // SIM: SOD 1, SDE, MSE, masks 101b; RIM
    CHECK(c.sod);
    CHECK_EQ(c.masks, 5);
    CHECK_EQ(c.a, 0x05);
}

static void test_8085_interrupts(void)
{
    // RST 7.5: an edge latched under DI, taken after EI and the next instruction (12 T)
    memset(g_mem, 0, sizeof(g_mem));
    const uint8_t prog[] = { 0x3E, 0x08, 0x30, 0xFB, 0x00, 0x00 };   // MVI A,08h; SIM (unmask); EI; NOP
    memcpy(g_mem, prog, sizeof(prog));
    i8080 c;
    i8080_init(&c);
    c.read_byte = rd;
    c.write_byte = wr;
    c.port_in = in;
    c.port_out = out;
    c.i8085 = true;
    c.sp = 0x8000;
    i8085_lines(&c, false, true, false, false);   // the edge comes while masked
    i8080_step(&c);
    i8080_step(&c);   // SIM: unmasked
    i8080_step(&c);   // EI
    CHECK_EQ(c.pc, 4);
    i8080_step(&c);   // NOP (the instruction after EI)
    unsigned long before = c.cyc;
    i8080_step(&c);   // RST 7.5
    CHECK_EQ(c.pc, 0x3C);
    CHECK_EQ((int)(c.cyc - before), 12);
    CHECK(!c.iff && !c.r75_ff);
    // TRAP: taken under DI, the first RIM returns IE before it
    c.iff = 1;
    g_mem[0x3C] = 0x00;
    i8085_lines(&c, true, false, false, false);
    i8080_step(&c);
    CHECK_EQ(c.pc, 0x24);
    g_mem[0x24] = 0x20;   // RIM
    i8080_step(&c);
    CHECK(c.a & 0x08);
    // RST 6.5 / 5.5 are levels; INTR as on the 8080 (RST from the bus)
    c.iff = 1;
    c.masks = 0;
    i8085_lines(&c, false, false, false, true);
    i8080_step(&c);
    CHECK_EQ(c.pc, 0x2C);
}

int main(int argc, char** argv)
{
    test_init();
    if (argc > 2 && !strcmp(argv[2], "8085")) {   // the 8085 exerciser: CRCs of a real D8085AH-1
        CHECK(check(argv[1], "Tests complete", "ERROR", true));
        return TEST_RESULT();
    }
    if (argc > 1) {   // the exerciser: every group must report OK
        CHECK(check(argv[1], "Tests complete", "ERROR"));
        return TEST_RESULT();
    }
    CHECK(check("TST8080.COM", "CPU IS OPERATIONAL", nullptr));
    CHECK(check("8080PRE.COM", "8080 Preliminary tests complete", "ERROR"));
    CHECK(check("CPUTEST.COM", "CPU TESTS OK", nullptr));
    // the 8085 mode: the 8080/8085 diagnostics pass (CPUTEST expects bit 1 of the 8080 PSW)
    CHECK(check("TST8080.COM", "CPU IS OPERATIONAL", nullptr, true));
    CHECK(check("8080PRE.COM", "8080 Preliminary tests complete", "ERROR", true));
    test_8085_timing();
    test_8085_instructions();
    test_8085_interrupts();
    return TEST_RESULT();
}
