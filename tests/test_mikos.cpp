// MIKOS V4.0F on ZPS-2 (JPR-1, REM-1, RAM-1/48K, AND-1, DSM-1, ANK-1): the
// monitor starts, a memory block goes to a new tape (.K_A, .K_S) and back
// (.K_L), and real tapes (TOOL 80, BASIC EX) load and run when available.
#include <string.h>
#include "cards/dsm1.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

static const char* REAL_TAPES = "H:/SAPI-1/SAPI_DOKU_2024-01-03/Kazetopáskové vybavení AUDIO WAV/";

static Dsm1* find_dsm(Machine& m)
{
    for (auto& c : m.cards)
        if (Dsm1* d = dynamic_cast<Dsm1*>(c.get())) return d;
    return nullptr;
}

// MIKOS reads a key only some time after printing a question
static bool answer(Machine& m, const char* prompt, const char* keys, double timeout_s)
{
    bool ok = run_until_text(m, prompt, timeout_s);
    m.run_seconds(0.5);
    m.type_text(keys);
    return ok;
}

static bool boot(Machine& m, Dsm1*& dsm)
{
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-2.sapi", err));
    dsm = find_dsm(m);
    CHECK(dsm != nullptr);
    if (!dsm) return false;
    CHECK(run_until_text(m, "MIKOS-1 V4.0F +", 2.0));
    CHECK(run_until_text(m, "SAPI-1 V4.0\n.", 1.0));
    m.run_seconds(0.5);
    return true;
}

// .K_L with any name: header found, confirmed, all blocks read back to the prompt
static int load_tape(Machine& m, Dsm1* dsm, const char* name, int blocks)
{
    m.type_text("KL");
    CHECK(answer(m, "NAME\n:", "*\r", 2.0));
    CHECK(answer(m, "PLAY  DONE?", "Y", 2.0));
    std::string q = std::string(name) + " --(Y/N)?";
    CHECK(answer(m, q.c_str(), "Y", 120.0));
    m.run_seconds(1.0);
    for (int i = 0; i < 300 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(run_until_text(m, "?Y\n\n.", 10.0));
    std::string s = screen_text(m);
    char status[8];
    snprintf(status, sizeof(status), "\" %02X", blocks);
    if (blocks >= 0) CHECK(s.find(status) != std::string::npos);   // the status line shows the last block
    CHECK(s.find("ERROR") == std::string::npos);
    return (int)s.size();
}

static void save_load(void)
{
    Machine m;
    Dsm1* dsm;
    if (!boot(m, dsm)) return;
    for (int i = 0; i < 0x300; i++) m.bus.mem_write((uint16_t)(0x5000 + i), (uint8_t)(i * 7 + 3));

    TapeDeck* t = dsm->tape();
    std::string err;
    CHECK(t->create("work/test/mikos_test.wav", err));
    t->mode = TapeDeck::RECORD;
    m.type_text("KA");
    CHECK(answer(m, "RECORD  DONE?", "Y", 2.0));
    CHECK(answer(m, "NEW  NAME\n:", "DATA\r", 2.0));
    CHECK(run_until_text(m, ":DATA\n\n\n.", 10.0));   // the header is on the tape
    m.run_seconds(0.5);
    m.type_text("KS");
    CHECK(answer(m, ".K_S\n=", "5000 52FF\r", 2.0));
    m.run_seconds(1.0);
    for (int i = 0; i < 120 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(!dsm->motor_on());
    m.run_seconds(0.5);
    printf("%s\n", screen_text(m).c_str());
    CHECK(t->length_seconds() > 3.0);
    CHECK(t->save(err));

    for (int i = 0; i < 0x300; i++) m.bus.mem_write((uint16_t)(0x5000 + i), 0);
    t->mode = TapeDeck::STOP;
    CHECK(t->load("work/test/mikos_test.wav", err));
    t->mode = TapeDeck::PLAY;
    m.type_text("\r");
    m.run_seconds(0.5);
    load_tape(m, dsm, "DATA", -1);
    printf("%s\n", screen_text(m).c_str());
    int bad = 0;
    for (int i = 0; i < 0x300; i++)
        if (m.bus.mem_peek((uint16_t)(0x5000 + i)) != (uint8_t)(i * 7 + 3)) bad++;
    CHECK_EQ(bad, 0);
}

static void real_tape(const char* file, const char* name, int blocks, const char* banner,
                      const char* input = nullptr, const char* expect = nullptr)
{
    std::string path = std::string(REAL_TAPES) + file;
    if (!file_exists(path)) {
        printf("%s not available, skipped\n", file);
        return;
    }
    Machine m;
    Dsm1* dsm;
    if (!boot(m, dsm)) return;
    std::string err;
    TapeDeck* t = dsm->tape();
    CHECK(t->load(path, err));
    t->mode = TapeDeck::PLAY;
    load_tape(m, dsm, name, blocks);
    m.type_text("J");   // checksum and start
    CHECK(run_until_text(m, banner, 3.0));
    if (input) {
        m.run_seconds(0.5);
        m.type_text(input);
        CHECK(run_until_text(m, expect, 5.0));
    }
    printf("%s\n", screen_text(m).c_str());
}

// Monitor V5.0F + MIKOS V5.0 with the Consul 262.3 keyboard: dialogs in Czech
// (NAZEV:, --(A/N)?), the motor starts without asking, "@NAME" writes where the
// tape is, a V4 tape (BASIC EX) loads and runs.
static bool boot_v5(Machine& m, Dsm1*& dsm)
{
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-2_MIKOS_V5.sapi", err));
    dsm = find_dsm(m);
    CHECK(dsm != nullptr);
    if (!dsm) return false;
    CHECK(run_until_text(m, "MIKOS V5.0 (C) TESLA DIZ 1987", 2.0));
    CHECK(run_until_text(m, "MONITOR V5.0\n.", 1.0));
    m.run_seconds(0.5);
    return true;
}

static void v5_save_load(void)
{
    Machine m;
    Dsm1* dsm;
    if (!boot_v5(m, dsm)) return;
    m.type_text("D0 7\r");
    CHECK(run_until_text(m, "0000 C3 8B 01 55 50 C3 06 F0", 2.0));   // the Consul keyboard works
    for (int i = 0; i < 0x200; i++) m.bus.mem_write((uint16_t)(0x5000 + i), (uint8_t)(i * 5 + 1));

    TapeDeck* t = dsm->tape();
    std::string err;
    CHECK(t->create("work/test/mikos5_test.wav", err));
    t->mode = TapeDeck::RECORD;
    m.run_seconds(0.5);
    m.type_text("KS");
    CHECK(answer(m, "NAZEV:", "@DATA\r", 2.0));
    CHECK(answer(m, "@DATA\n\n=", "5000 51FF\r", 10.0));
    m.run_seconds(1.0);
    for (int i = 0; i < 60 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(run_until_text(m, "=5000 =51FF\n\n.", 5.0));
    CHECK(t->save(err));

    for (int i = 0; i < 0x200; i++) m.bus.mem_write((uint16_t)(0x5000 + i), 0);
    t->mode = TapeDeck::STOP;
    CHECK(t->load("work/test/mikos5_test.wav", err));
    t->mode = TapeDeck::PLAY;
    m.type_text("KL");
    CHECK(answer(m, "K:L\n\n NAZEV:", "DATA\r", 2.0));
    m.run_seconds(1.0);
    for (int i = 0; i < 60 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(run_until_text(m, ":DATA\n\nDATA\n\n.", 5.0));
    printf("%s\n", screen_text(m).c_str());
    int bad = 0;
    for (int i = 0; i < 0x200; i++)
        if (m.bus.mem_peek((uint16_t)(0x5000 + i)) != (uint8_t)(i * 5 + 1)) bad++;
    CHECK_EQ(bad, 0);
}

static void v5_real_tape(void)
{
    std::string path = std::string(REAL_TAPES) + "BASIC EX V4.0.mp3";
    if (!file_exists(path)) {
        printf("BASIC EX V4.0.mp3 not available, skipped\n");
        return;
    }
    Machine m;
    Dsm1* dsm;
    if (!boot_v5(m, dsm)) return;
    std::string err;
    TapeDeck* t = dsm->tape();
    CHECK(t->load(path, err));
    t->mode = TapeDeck::PLAY;
    m.type_text("KL");
    CHECK(answer(m, "NAZEV:", "*\r", 2.0));
    CHECK(answer(m, "BASIC --(A/N)?", "A", 60.0));
    m.run_seconds(1.0);
    for (int i = 0; i < 300 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(!dsm->motor_on());
    m.run_seconds(1.0);   // back at the prompt
    CHECK(screen_contains(m, "*\"BASIC      \" 01 1E"));
    m.type_text("J");
    CHECK(run_until_text(m, "READY", 3.0));
    m.run_seconds(0.5);
    m.type_text("PRINT 12*34+5\r");
    CHECK(run_until_text(m, " 413", 5.0));
    printf("%s\n", screen_text(m).c_str());
}

int main(void)
{
    test_init();
    save_load();
    real_tape("tool_80.wav", "TOOL 80", 0x1E, "PEDIT V2.5");
    real_tape("BASIC EX V4.0.mp3", "BASIC", 0x1E, "READY", "PRINT 12*34+5\r", " 413");
    v5_save_load();
    v5_real_tape();
    return TEST_RESULT();
}
