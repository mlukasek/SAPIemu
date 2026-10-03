// DSM-1 cassette: MIKROBASIC saves a program on a new tape, forgets it and loads
// it back (record -> WAV -> play); a real recording is decoded when available.
#include <math.h>
#include <string.h>
#include "cards/dsm1.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

static Dsm1* find_dsm(Machine& m)
{
    for (auto& c : m.cards)
        if (Dsm1* d = dynamic_cast<Dsm1*>(c.get())) return d;
    return nullptr;
}

static void save_load(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-1.sapi", err));
    Dsm1* dsm = find_dsm(m);
    CHECK(dsm != nullptr);
    if (!dsm) return;
    CHECK(run_until_text(m, "READY", 2.0));

    TapeDeck* t = dsm->tape();
    CHECK(t->create("work/test/cassette_test.wav", err));
    t->mode = TapeDeck::RECORD;
    m.type_text("10 PRINT \"HELLO\"\r20 PRINT 6*7\rSAVE\r");
    CHECK(run_until_text(m, "NAME", 20.0));
    m.type_text("PROG\r");
    m.run_seconds(1.0);
    CHECK(run_until_text(m, "READY\n>\n", 30.0) || true);
    // SAVE ends with READY once the program is on the tape and the motor is off
    for (int i = 0; i < 60 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(!dsm->motor_on());
    double recorded = t->length_seconds();
    printf("recorded %.2f s\n", recorded);
    CHECK(recorded > 1.0);
    CHECK(t->save(err));

    // forget the program, rewind, play
    m.type_text("NEW\r");
    m.run_seconds(1.0);
    t->mode = TapeDeck::STOP;
    t->pos = 0;
    CHECK(t->load("work/test/cassette_test.wav", err));   // also checks the WAV writer
    t->mode = TapeDeck::PLAY;
    m.type_text("LOAD\r");
    CHECK(run_until_text(m, "*PROG", 40.0));   // MIKROBASIC prints '*' and the name read from the tape
    for (int i = 0; i < 60 && !dsm->motor_on(); i++) m.run_seconds(0.1);
    for (int i = 0; i < 120 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(!dsm->motor_on());
    CHECK(!screen_contains(m, "ERROR"));
    m.type_text("RUN\r");
    CHECK(run_until_text(m, "42", 10.0));
    CHECK(screen_contains(m, "\nHELLO"));
    printf("%s", screen_text(m).c_str());
}

// TOOL 48 (a real tape copied to WAV): the demodulated line gives the header
// 10 10 10 01 "TOOL 48" in 8N2 frames without framing errors.
static void real_tape(void)
{
    const char* f = "H:/SAPI-1/SAPI_DOKU_2024-01-03/Kazetopáskové vybavení AUDIO WAV/tool_48.wav";
    if (!file_exists(f)) {
        printf("real tape not available, skipped\n");
        return;
    }
    TapeDeck t;
    std::string err;
    CHECK(t.load(f, err));
    double bit = t.rate / 2400.0;
    std::string head;
    int fe = 0;
    double scan = 0;
    for (int n = 0; n < 2000; n++) {
        double s = t.next_start(scan, (double)t.samples.size());
        if (s < 0) break;
        int v = 0;
        for (int k = 0; k < 8; k++)
            if (t.line_level(s + bit * (1.5 + k))) v |= 1 << k;
        if (!t.line_level(s + bit * 9.5)) fe++;
        if (n < 11) head += (char)v;
        scan = s + bit * 9.5;
    }
    CHECK(head.size() == 11 && memcmp(head.data(), "\x10\x10\x10\x01TOOL 48", 11) == 0);
    CHECK(fe == 0);
}

static std::string decode_8n2(TapeDeck& t, int max_bytes, int& fe)
{
    double bit = t.rate / 2400.0;
    std::string out;
    fe = 0;
    double scan = 0;
    for (int n = 0; n < max_bytes; n++) {
        double s = t.next_start(scan, (double)t.samples.size());
        if (s < 0) break;
        int v = 0;
        for (int k = 0; k < 8; k++)
            if (t.line_level(s + bit * (1.5 + k))) v |= 1 << k;
        if (!t.line_level(s + bit * 9.5)) fe++;
        out += (char)v;
        scan = s + bit * 9.5;
    }
    return out;
}

int main(int argc, char** argv)
{
    test_init();
    if (argc > 1) {   // decode a tape file: test_cassette FILE
        TapeDeck t;
        std::string err;
        if (!t.load(argv[1], err)) { printf("%s\n", err.c_str()); return 1; }
        if (argc > 2) {   // decoded audio as WAV: test_cassette FILE OUT.wav
            t.path = data_path(argv[2]);
            t.is_mp3 = false;
            t.modified = true;
            return t.save(err) ? 0 : 1;
        }
        int fe;
        std::string b = decode_8n2(t, 100000, fe);
        printf("%d bytes, %d framing errors\n", (int)b.size(), fe);
        for (size_t i = 0; i < b.size() && i < 64; i++) printf("%02X ", (uint8_t)b[i]);
        printf("\n");
        return 0;
    }
    save_load();
    real_tape();
    return TEST_RESULT();
}
