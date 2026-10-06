// Tapes made from programs (core/tapegen): the real ROMs read them. MIKOS V4.0F
// and V5.0 load a HEX file with .K_L and start it with .J (through the JMP put
// before the program for its entry point); MIKROBASIC loads a BASIC program and
// machine code with the CALL line added; the WAV export and --make-tape.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cards/dsm1.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/tapegen.h"
#include "core/util.h"
#include "test_util.h"

static Dsm1* find_dsm(Machine& m)
{
    for (auto& c : m.cards)
        if (Dsm1* d = dynamic_cast<Dsm1*>(c.get())) return d;
    return nullptr;
}

// Intel HEX of `data` at `addr`, 16 bytes a record.
static bool write_hex(const char* path, uint16_t addr, const uint8_t* data, size_t n)
{
    std::string t;
    char b[64];
    for (size_t i = 0; i < n; i += 16) {
        size_t k = n - i < 16 ? n - i : 16;
        uint16_t a = (uint16_t)(addr + i);
        uint8_t sum = (uint8_t)(k + (a >> 8) + (a & 0xFF));
        snprintf(b, sizeof(b), ":%02X%04X00", (unsigned)k, a);
        t += b;
        for (size_t j = 0; j < k; j++) {
            snprintf(b, sizeof(b), "%02X", data[i + j]);
            t += b;
            sum = (uint8_t)(sum + data[i + j]);
        }
        snprintf(b, sizeof(b), "%02X\n", (uint8_t)(0x100 - sum) & 0xFF);
        t += b;
    }
    t += ":00000001FF\n";
    return file_write_text(data_path(path), t);
}

// 5000-50FF a pattern, 5100: MVI A,5Ah / STA 6000h / RET (the entry point)
static const uint16_t PROG_LO = 0x5000, PROG_ENTRY = 0x5100;
static std::vector<uint8_t> program(void)
{
    std::vector<uint8_t> p(0x100);
    for (int i = 0; i < 0x100; i++) p[i] = (uint8_t)(i * 13 + 5);
    static const uint8_t code[] = { 0x3E, 0x5A, 0x32, 0x00, 0x60, 0xC9 };
    p.insert(p.end(), code, code + sizeof(code));
    return p;
}

static int program_diff(Machine& m)
{
    std::vector<uint8_t> p = program();
    int bad = 0;
    for (size_t i = 0; i < p.size(); i++)
        if (m.bus.mem_peek((uint16_t)(PROG_LO + i)) != p[i]) bad++;
    return bad;
}

static void names_and_limits(void)
{
    CHECK(tape_default_name("work/x/manic_miner.hex") == "MANIC_MINER");
    CHECK(tape_default_name("c:\\a.b\\prog") == "PROG");
    CHECK(tape_name_clean("  ab\xc3\xa1" "cdefghijklmn ") == "ABCDEFGHIJK");
    CHECK(tape_is_program("A.HEX") && tape_is_program("a.bin") && !tape_is_program("a.wav"));

    std::vector<uint8_t> mem(0x10000, 0);
    std::vector<int16_t> s;
    int rate = 0;
    std::string info, err;
    TapeProgram p;
    p.name = "X";
    CHECK(!tape_build(mem.data(), 0x4300, 0x4400, p, s, rate, info, err));   // MIKOS RAM below 4400h
    CHECK(err.find("4400h") != std::string::npos);
    p.entry = 0x4500;
    CHECK(!tape_build(mem.data(), 0x4401, 0x4500, p, s, rate, info, err));   // no room for the JMP
    p.format = TAPE_MIKROBASIC;
    p.entry = -1;
    CHECK(!tape_build(mem.data(), 0x4100, 0x4200, p, s, rate, info, err));   // between 40EFh and 4102h
    CHECK(tape_build(mem.data(), 0x4102, 0x4110, p, s, rate, info, err));
    CHECK_EQ(rate, 48000);
    printf("%s\n", info.c_str());
}

// MIKOS reads a key only some time after printing a question
static bool answer(Machine& m, const char* prompt, const char* keys, double timeout_s)
{
    bool ok = run_until_text(m, prompt, timeout_s);
    m.run_seconds(0.5);
    m.type_text(keys);
    return ok;
}

static void wait_motor(Machine& m, Dsm1* dsm)
{
    m.run_seconds(1.0);
    for (int i = 0; i < 120 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(!dsm->motor_on());
}

static bool insert(Dsm1* dsm, const char* path, const TapeProgram& p)
{
    std::string err;
    TapeDeck* t = dsm->tape();
    t->eject();
    bool ok = t->load_program(path, p, err);
    if (!ok) fprintf(stderr, "%s\n", err.c_str());
    CHECK(ok);
    CHECK(t->generated && t->write_protect && t->read_only());
    printf("%s\n", t->program_info.c_str());
    t->mode = TapeDeck::PLAY;
    return ok;
}

static void mikos_v4(void)
{
    std::vector<uint8_t> p = program();
    CHECK(write_hex("work/test/tapegen_prog.hex", PROG_LO, p.data(), p.size()));
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-2.sapi", err));
    Dsm1* dsm = find_dsm(m);
    CHECK(dsm != nullptr);
    if (!dsm) return;
    CHECK(run_until_text(m, "SAPI-1 V4.0\n.", 3.0));
    m.run_seconds(0.5);
    TapeProgram tp;
    tp.name = "testprog";
    tp.entry = PROG_ENTRY;
    if (!insert(dsm, "work/test/tapegen_prog.hex", tp)) return;

    m.type_text("KL");
    CHECK(answer(m, "NAME\n:", "TEST\r", 2.0));   // the name is compared as a prefix
    CHECK(answer(m, "PLAY  DONE?", "Y", 2.0));   // with a name (not *) MIKOS does not ask again
    wait_motor(m, dsm);
    CHECK(run_until_text(m, "*\"TESTPROG   \" 03", 5.0));   // the last block: header, 'B', end of file
    CHECK(!screen_contains(m, "ERROR"));
    printf("%s\n", screen_text(m).c_str());
    CHECK_EQ(program_diff(m), 0);
    CHECK_EQ(m.bus.mem_peek(0x4FFD), 0xC3);   // JMP 5100h before the program
    CHECK_EQ(m.bus.mem_peek(0x4FFE), 0x00);
    CHECK_EQ(m.bus.mem_peek(0x4FFF), 0x51);
    m.type_text("J");   // checksum, 4FFDh, JMP 5100h, RET to the monitor
    m.run_seconds(1.0);
    CHECK_EQ(m.bus.mem_peek(0x6000), 0x5A);
    CHECK(!screen_contains(m, "ERROR"));
}

static void mikos_v5(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-2_MIKOS_V5.sapi", err));
    Dsm1* dsm = find_dsm(m);
    CHECK(dsm != nullptr);
    if (!dsm) return;
    CHECK(run_until_text(m, "MONITOR V5.0\n.", 3.0));
    m.run_seconds(0.5);
    TapeProgram tp;
    tp.name = "V5TEST";
    tp.entry = PROG_ENTRY;
    if (!insert(dsm, "work/test/tapegen_prog.hex", tp)) return;   // written by mikos_v4
    m.type_text("KL");
    CHECK(answer(m, "NAZEV:", "V5TEST\r", 2.0));
    wait_motor(m, dsm);
    CHECK(run_until_text(m, "V5TEST\n\n.", 5.0));
    printf("%s\n", screen_text(m).c_str());
    CHECK_EQ(program_diff(m), 0);
    m.type_text("J");
    m.run_seconds(1.0);
    CHECK_EQ(m.bus.mem_peek(0x6000), 0x5A);

    // a saved state makes the same tape again (name, entry) at the same position
    std::vector<uint8_t> st;
    CHECK(m.save_state(st, "machines/zps/ZPS-2_MIKOS_V5.sapi", err));
    Machine m2;
    CHECK(m2.load_file("machines/zps/ZPS-2_MIKOS_V5.sapi", err));
    CHECK(m2.load_state(st, err));
    Dsm1* d2 = find_dsm(m2);
    if (!d2) return;
    TapeDeck* a = dsm->tape();
    TapeDeck* b = d2->tape();
    CHECK(b->generated);
    CHECK(b->program.name == "V5TEST" && b->program.entry == PROG_ENTRY);
    CHECK(b->program_info == a->program_info);
    CHECK(b->samples == a->samples);
    CHECK(b->pos == a->pos);
}

static void mikrobasic(void)
{
    // a BASIC program as a binary file at 40EFh: line number low, high, text, CR
    static const char* const lines[] = { "10PRINT \"HI\"", "20PRINT 6*7", nullptr };
    std::vector<uint8_t> bas;
    for (int i = 0; lines[i]; i++) {
        int n = (lines[i][0] - '0') * 10 + (lines[i][1] - '0');
        bas.push_back((uint8_t)n);
        bas.push_back(0);
        bas.insert(bas.end(), lines[i] + 2, lines[i] + strlen(lines[i]));
        bas.push_back(0x0D);
    }
    CHECK(file_write_all(data_path("work/test/tapegen_basic.bin"), bas.data(), bas.size()));
    // machine code at 4102h: MVI A,5Ah / STA 4700h / RET
    static const uint8_t code[] = { 0x3E, 0x5A, 0x32, 0x00, 0x47, 0xC9 };
    CHECK(write_hex("work/test/tapegen_code.hex", 0x4102, code, sizeof(code)));

    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-1.sapi", err));
    Dsm1* dsm = find_dsm(m);
    CHECK(dsm != nullptr);
    if (!dsm) return;
    CHECK(run_until_text(m, "READY", 2.0));

    TapeProgram tp;
    tp.format = TAPE_MIKROBASIC;
    tp.address = 0x40EF;
    if (!insert(dsm, "work/test/tapegen_basic.bin", tp)) return;
    m.type_text("LOAD\r");
    CHECK(run_until_text(m, "*TAPEGEN_BAS", 20.0));
    wait_motor(m, dsm);
    CHECK(!screen_contains(m, "ERROR"));
    m.type_text("RUN\r");
    CHECK(run_until_text(m, "42", 5.0));
    CHECK(screen_contains(m, "\nHI"));

    TapeProgram tc;
    tc.format = TAPE_MIKROBASIC;
    tc.name = "CODE";
    if (!insert(dsm, "work/test/tapegen_code.hex", tc)) return;
    m.type_text("NEW\r");
    m.run_seconds(0.5);
    m.type_text("LOAD\r");
    CHECK(run_until_text(m, "*CODE", 20.0));
    wait_motor(m, dsm);
    CHECK(!screen_contains(m, "ERROR"));
    m.type_text("RUN\r");   // 1 CALL 16642, 2 END
    m.run_seconds(2.0);
    CHECK_EQ(m.bus.mem_peek(0x4700), 0x5A);
    printf("%s", screen_text(m).c_str());
}

// The tape exported as WAV is the same tape; --make-tape writes it without a recorder.
static void export_wav(void)
{
    TapeDeck t;
    std::string err;
    TapeProgram tp;
    CHECK(t.load_program("work/test/tapegen_prog.hex", tp, err));
    CHECK(t.export_wav("work/test/tapegen_prog.wav", err));
    TapeDeck w;
    CHECK(w.load("work/test/tapegen_prog.wav", err));
    CHECK_EQ(w.rate, t.rate);
    CHECK_EQ(w.samples.size(), t.samples.size());
    int off = 0;   // reading scales by 32767 / 32768
    for (size_t i = 0; i < w.samples.size() && i < t.samples.size(); i++)
        if (abs(w.samples[i] - t.samples[i]) > 1) off++;
    CHECK_EQ(off, 0);
    CHECK(!w.generated);

    Machine m;
    CHECK(m.load_file("machines/zps/ZPS-1.sapi", err));
    std::string out = data_path("work/test/tapegen_cli.wav");
    file_remove(out);
    std::vector<std::string> a = { "--tape-format", "mikrobasic", "--make-tape", "work/test/tapegen_code.hex", out };
    CHECK_EQ(headless_run(m, a), 0);
    TapeDeck c;
    CHECK(c.load(out, err));
    CHECK(c.length_seconds() > 9.0);   // 8.4 s lead tone, the record, 1.5 s
}

int main(void)
{
    test_init();
    names_and_limits();
    mikos_v4();
    mikos_v5();
    mikrobasic();
    export_wav();
    return TEST_RESULT();
}
