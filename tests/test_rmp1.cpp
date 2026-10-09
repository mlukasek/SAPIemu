// RMP-1 magnetic tape controller with the IZOT SM 5300 (docs/desky/RMP-1.md) and the original programs of
// J. Biba on media/rmp-1/a_lad_izo.img (driver RMP.MAC of Tesla Elstroj 1985 inside):
// - the tape format alone: CRC (ČSN 36 9464, the comp.crc of RMP.MAC), records, SIMH .tap round trip;
// - MTZAP: 255 blocks of the test sentence and a tape mark, MTCTE reads them back;
// - MTDAT: a tape with JSEP labels (VOL1, HDR1/2, EOF1/2, EBCDIC), two files on it, a file back to the
//   floppy, DIRMT lists the tape (a backspace over a block: the start and stop of the drive matter).
// RMP_MAKE=<file>: also save the sample tape made here (media/rmp-1/sapi01_ukazka.tap).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cards/rmp1.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static const char* TAPE = "work/test/rmp_paska.tap";

static void tape_format(void)
{
    // the CRC of RMP.MAC for the 80 B test sentence of MTZAP, as the driver wrote it in SAPIemu
    const char* s = "Toto je prvni zapsany a precteny blok dat pocitacem SAPI-1/CPM na magneticke pasce IZOT CM 5300.01";
    std::vector<uint8_t> d(s, s + 80);
    std::vector<MagTape::Row> rows;
    MagTape::put_record(rows, 1000, d.data(), d.size());
    MagTape::put_mark(rows, 500000);
    MagTape t;
    t.rows = rows;
    t.loaded = true;
    std::vector<MagTape::Record> r = t.records();
    CHECK(r.size() == 2);
    CHECK(!r[0].mark && !r[0].bad && r[0].data == d);
    CHECK(r[1].mark && !r[1].bad);
    CHECK(rows.size() == 80 + 2 + 2);   // data, CRC, LRC, the two rows of the mark
    // a parity error in a row makes the record bad
    t.rows[5].ch ^= 0x100;
    CHECK(t.records()[0].bad);
    // SIMH .tap: written and loaded again (the gaps as RMP.MAC writes them)
    t.rows = rows;
    t.path = data_path("work/test/rmp_format.tap");
    t.dirty = true;
    std::string err;
    CHECK(t.save(err));
    MagTape u;
    CHECK(u.load("work/test/rmp_format.tap", false, err));
    std::vector<MagTape::Record> q = u.records();
    CHECK(q.size() == 2 && q[0].data == d && !q[0].bad && q[1].mark);
    CHECK(q[0].pos > MagTape::BOT_LEN);   // past the BOT marker
}

static Rmp1* rmp_of(Machine& m)
{
    Rmp1* r = (Rmp1*)m.find_card("RMP-1");
    CHECK(r != nullptr);
    return r;
}

// the machine of machines/rmp1_jb_izot.sapi on copies in work/test, a blank tape
static bool jb_with_rmp(Machine& m)
{
    std::string t, err;
    CHECK(file_read_text(data_path("machines/rmp1_jb_izot.sapi"), t));
    json cfg = json::parse(t, nullptr, false, true);
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path("media/sapiman/r1_zrmd4m_271111.img"), img));
    CHECK(file_write_all(data_path("work/test/rmp_a.img"), img.data(), img.size()));
    CHECK(file_read_all(data_path("media/rmp-1/a_lad_izo.img"), img));
    CHECK(file_write_all(data_path("work/test/rmp_b.img"), img.data(), img.size()));
    for (json& c : cfg["cards"]) {
        std::string ty = c.value("type", std::string());
        if (ty == "ZRMD-1") c["mode"] = "nosave";
        if (ty == "RPD-1") {
            c["drive_a"] = { { "image", "work/test/rmp_a.img" } };
            c["drive_b"] = { { "image", "work/test/rmp_b.img" } };
            c["working_copy"] = false;
        }
        if (ty == "RMP-1") {
            c["unit0"] = { { "tape", TAPE } };
            c["working_copy"] = false;
        }
    }
    file_remove(data_path(TAPE));
    CHECK(m.load_json(cfg, err));
    if (!err.empty()) printf("%s\n", err.c_str());
    CHECK(run_until_text(m, "\nA>", 20.0));
    m.run_seconds(0.3);
    return true;
}

// waits until CP/M is back: the last line of the screen is the prompt alone (the command typed stays on the
// screen with an A> before it)
static bool cpm_prompt(Machine& m, double secs)
{
    for (double t = 0; t < secs; t += 0.2) {
        m.run_seconds(0.2);
        std::string s = screen_text(m);
        size_t e = s.find_last_not_of(" \n");
        if (e == std::string::npos) continue;
        size_t b = s.rfind('\n', e);
        b = b == std::string::npos ? 0 : b + 1;
        std::string last = s.substr(b, e + 1 - b);
        if (last.rfind("A>", 0) == 0 && last.size() <= 5) return true;
    }
    printf("NO CP/M PROMPT:\n%s\n", screen_text(m).c_str());
    return false;
}

// waits for a prompt of a program (they stay on the screen: unique ones), then types the answer
static bool answer(Machine& m, const char* prompt, const char* keys, double secs = 60.0)
{
    bool ok = run_until_text(m, prompt, secs);
    if (!ok) printf("NO PROMPT %s:\n%s\n", prompt, screen_text(m).c_str());
    CHECK(ok);
    m.run_seconds(0.3);
    m.type_text(keys);
    return ok;
}

static void mtzap(void)
{
    Machine m;
    if (!jb_with_rmp(m)) return;
    Rmp1* r = rmp_of(m);
    m.type_text("B:MTZAP\r");
    CHECK(cpm_prompt(m, 120.0));
    std::vector<MagTape::Record> recs = r->units[0].tape.records();
    int good = 0;
    for (const MagTape::Record& x : recs)
        if (!x.mark && !x.bad && x.data.size() == 80 && !memcmp(x.data.data(), "Toto je prvni", 13)) good++;
    printf("MTZAP: %d records, %d good blocks\n", (int)recs.size(), good);
    CHECK(good == 255);
    CHECK(recs.size() == 256 && recs.back().mark);
    CHECK(r->units[0].pos == 0 || r->units[0].rewinding);   // rewound (the drive rewinds on its own)
    m.run_seconds(0.5);
    m.type_text("B:MTCTE\r");
    CHECK(cpm_prompt(m, 120.0));
    std::string s = screen_text(m);
    CHECK(s.find("Toto je prvni zapsany") != std::string::npos);
}

// MTDAT 2: a file of the floppy onto the tape as file `n`
static void mtdat_put(Machine& m, const char* file, int n, const char* name)
{
    char k[16];
    CHECK(answer(m, "Zadej funkci", "2\r"));
    CHECK(answer(m, "vstupni diskovy soubor", file));
    m.type_text("\r");
    CHECK(answer(m, "Jmeno pasky", "SAPI01\r"));
    snprintf(k, sizeof(k), "%d\r", n);
    CHECK(answer(m, "Poradi souboru", k));
    CHECK(answer(m, "Delka bloku", "800\r"));
    CHECK(answer(m, "Delka rekordu", "80\r"));
    CHECK(answer(m, "Jmeno souboru", name));
    m.type_text("\r");
    CHECK(answer(m, "(A/N)", "N"));
    m.run_seconds(1.0);
    CHECK(run_until_text(m, "Zadej funkci", 120.0));   // copied: the menu again
}

static void mtdat(void)
{
    Machine m;
    if (!jb_with_rmp(m)) return;
    Rmp1* r = rmp_of(m);
    m.type_text("B:MTDAT\r");
    CHECK(answer(m, "Zadej funkci", "3\r", 20.0));   // INIC: VOL1 SAPI01
    m.run_seconds(3.0);
    m.type_text("SAPI01\r");
    m.run_seconds(1.0);
    mtdat_put(m, "B:MTZAP.MAC", 1, "MTZAP");
    mtdat_put(m, "B:MTDAT.MAN", 2, "MTDATMAN");
    // 5: the list of the tape
    CHECK(answer(m, "Zadej funkci", "5\r"));
    CHECK(run_until_text(m, "Konec pasky", 120.0));
    std::string s = screen_text(m);
    CHECK(s.find("0001  MTZAP") != std::string::npos && s.find("0002  MTDATMAN") != std::string::npos);
    // the labels as JSEP writes them, in EBCDIC
    std::vector<MagTape::Record> recs = r->units[0].tape.records();
    static const uint8_t vol1[4] = { 0xE5, 0xD6, 0xD3, 0xF1 }, hdr1[4] = { 0xC8, 0xC4, 0xD9, 0xF1 };
    int marks = 0, bad = 0, data = 0;
    for (const MagTape::Record& x : recs) {
        if (x.mark) marks++;
        else if (x.bad) bad++;
        else data++;
    }
    printf("MTDAT: %d records, %d tape marks, %d bad\n", (int)recs.size(), marks, bad);
    CHECK(recs.size() == 22 && marks == 7 && bad == 0);
    CHECK(recs[0].data.size() == 80 && !memcmp(recs[0].data.data(), vol1, 4));
    CHECK(recs[1].data.size() == 80 && !memcmp(recs[1].data.data(), hdr1, 4));
    CHECK(recs[4].data.size() == 480);   // MTZAP.MAC: 6 records of 80 B
    CHECK(recs[recs.size() - 1].mark && recs[recs.size() - 2].mark);   // the end: two tape marks
    m.run_seconds(1.0);
    m.type_text("\r");   // the list waits for a key
    // 1: file 2 back to A:, the text as it was (MTDAT converts 7-bit ASCII: the high bit of the Czech
    // letters in KOI8 is lost)
    m.run_seconds(1.0);
    CHECK(answer(m, "Zadej funkci", "1\r"));
    CHECK(answer(m, "Jmeno pasky", "SAPI01\r"));
    CHECK(answer(m, "Poradi souboru na", "2\r"));
    CHECK(answer(m, "Delka bloku", "800\r"));
    CHECK(answer(m, "Delka rekordu", "80\r"));
    CHECK(answer(m, "Jmeno souboru", "MTDATMAN\r"));
    CHECK(answer(m, "diskovy soubor", "A:MTDAT.MAN\r"));
    m.run_seconds(1.0);
    CHECK(answer(m, "Zadej funkci", "4\r", 120.0));   // the end
    CHECK(cpm_prompt(m, 30.0));
    m.run_seconds(0.5);
    m.type_text("TYPE A:MTDAT.MAN\r");
    CHECK(run_until_text(m, "DIRMT", 30.0));
    // DIRMT: the same list (it reads a block past each file and backspaces over it)
    m.run_seconds(3.0);
    m.type_text("B:DIRMT\r");
    CHECK(answer(m, "Paska:", "0", 20.0));
    CHECK(run_until_text(m, "Konec pasky", 120.0));
    s = screen_text(m);
    printf("%s\n", s.c_str());
    CHECK(s.find("Chyba") == std::string::npos);
    CHECK(s.find("0002  MTDATMAN") != std::string::npos);
    const char* make = getenv("RMP_MAKE");
    if (make) {
        std::string err;
        r->units[0].tape.dirty = true;
        std::string keep = r->units[0].tape.path;
        r->units[0].tape.path = make;
        CHECK(r->units[0].tape.save(err));
        r->units[0].tape.path = keep;
        printf("RMP_MAKE: %s\n", make);
    }
}

int main(void)
{
    test_init();
    tape_format();
    const char* part = getenv("RMP_PART");   // ctest runs the parts in parallel (tests/CMakeLists.txt)
    if (!part || !strcmp(part, "mtzap")) mtzap();
    if (!part || !strcmp(part, "mtdat")) mtdat();
    return TEST_RESULT();
}
