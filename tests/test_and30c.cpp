// AND-30C: the decoder after the PROM IC2 (F800-FFDF video RAM, the 6845 at FFE0h/FFF0h with mirrors, the RAM cells
// 7E0-7FF hidden from the CPU, /CE1 and A7 jumpers), the registers, the timing the registers give, the picture
// (glyphs of both EPROMs, the cursor block ORed with the dots) and the blanking of IC12 on a CPU access
// (AND30C_PART=unit). AND30C_PART=cpm: ECMON with the driver for the card (machines/cpm_and30c.sapi, the RAM disk
// only in memory) boots CP/M and a program calls CONOUT of the BIOS with the control codes of the driver.
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "cards/and30c.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static And30c* find_card(Machine& m)
{
    for (auto& c : m.cards)
        if (And30c* d = dynamic_cast<And30c*>(c.get())) return d;
    return nullptr;
}

static bool load(Machine& m, const json& params = json::object())
{
    json cfg;
    cfg["name"] = "test AND-30C";
    cfg["cards"] = json::array();
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    json c = params;
    c["slot"] = 2;
    c["type"] = "AND-30C";
    cfg["cards"].push_back(c);
    std::string err;
    bool ok = m.load_json(cfg, err);
    if (!ok) fprintf(stderr, "load: %s\n", err.c_str());
    CHECK(ok);
    return ok;
}

static void reg(Machine& m, int r, uint8_t v)
{
    m.bus.mem_write(0xFFE0, (uint8_t)r);
    m.bus.mem_write(0xFFF0, v);
}

// the set-up of ECMON for the card (tools/ecmon_and30c.mac, CRTTAB)
static const uint8_t CRTTAB[16] = { 114, 80, 89, 8, 25, 0, 21, 23, 0, 11, 0x4A, 11, 0, 0, 0, 0 };

static void setup(Machine& m)
{
    for (int i = 0; i < 16; i++) reg(m, i, CRTTAB[i]);
}

static void test_decoder(void)
{
    Machine m;
    if (!load(m)) return;
    And30c* d = find_card(m);
    CHECK(d);
    if (!d) return;
    const char* what = nullptr;
    CHECK_EQ(d->decode_mem(0xF7FF, 0, &what), 0);
    CHECK(d->decode_mem(0xF800, 0, &what) != 0 && !strcmp(what, "video RAM"));
    CHECK(d->decode_mem(0xFFDF, 0, &what) != 0 && !strcmp(what, "video RAM"));
    CHECK(d->decode_mem(0xFFE0, 0, &what) != 0 && !strcmp(what, "6845 address register"));
    CHECK(d->decode_mem(0xFFEF, 0, &what) != 0 && !strcmp(what, "6845 address register"));
    CHECK(d->decode_mem(0xFFF0, 0, &what) != 0 && !strcmp(what, "6845 data register"));
    CHECK(d->decode_mem(0xFFFF, 0, &what) != 0 && !strcmp(what, "6845 data register"));
    // the video RAM
    m.bus.mem_write(0xF800, 0x41);
    m.bus.mem_write(0xFFDF, 0x5A);
    CHECK_EQ(d->ram[0], 0x41);
    CHECK_EQ(d->ram[0x7DF], 0x5A);
    CHECK_EQ(m.bus.mem_read(0xF800), 0x41);
    CHECK_EQ(m.bus.mem_read(0xFFDF), 0x5A);
    // the 6845 with its mirrors (RS = A4); the RAM under it is not reached
    d->ram[0x7E5] = 0x77;
    m.bus.mem_write(0xFFE5, 14);
    m.bus.mem_write(0xFFF7, 0x45);
    CHECK_EQ(d->ar, 14);
    CHECK_EQ(d->r[14], 0x05);   // R14 has 6 bits
    CHECK_EQ(d->ram[0x7E5], 0x77);
    CHECK_EQ(m.bus.mem_read(0xFFFF), 0x05);
    m.bus.mem_write(0xFFEA, 15);
    m.bus.mem_write(0xFFF0, 0x23);
    CHECK_EQ(m.bus.mem_read(0xFFF3), 0x23);
    m.bus.mem_write(0xFFE0, 1);       // R1 is write only: nothing drives the inner bus
    m.bus.mem_write(0xFFF0, 80);
    CHECK_EQ(d->r[1], 80);
    CHECK_EQ(m.bus.mem_read(0xFFF0), 0xFF);
    CHECK_EQ(m.bus.mem_read(0xFFE0), 0xFF);
    m.bus.mem_write(0xFFE0, 16);      // light pen: LPSTB not connected
    CHECK_EQ(m.bus.mem_read(0xFFF0), 0x00);

    // /CE1 on /REF as drawn: the PROM only in refresh cycles, the card never answers
    Machine m2;
    if (!load(m2, { { "ce1", "ref" } })) return;
    And30c* d2 = find_card(m2);
    CHECK(d2 && d2->decode_mem(0xF800, 0, &what) == 0 && d2->decode_mem(0xFFE0, 0, &what) == 0);
    // A7 from MAP1: the dump selects only with A7 = 0
    Machine m3;
    if (!load(m3, { { "prom_a7", "map1" } })) return;
    And30c* d3 = find_card(m3);
    CHECK(d3 && d3->decode_mem(0xF800, 0, &what) != 0 && d3->decode_mem(0xF800, MAP_1, &what) == 0);
    std::string err;
    Machine m4;
    json bad;
    bad["cards"] = json::array({ { { "slot", 1 }, { "type", "JPR-1ZI" } }, { { "slot", 2 }, { "type", "AND-30C" }, { "ce1", "x" } } });
    CHECK(!m4.load_json(bad, err));
}

static void test_timing(void)
{
    Machine m;
    if (!load(m)) return;
    And30c* d = find_card(m);
    if (!d) return;
    json out;
    d->describe(out);
    CHECK(out["video"].get<std::string>().find("not set up") != std::string::npos);
    setup(m);
    out = json::object();
    d->describe(out);
    // 115 characters of 6 dots at 10.8 MHz = 63.89 us, 26 rows x 12 lines = 312 lines, 50.1 Hz
    CHECK_EQ(out["columns"].get<int>(), 80);
    CHECK_EQ(out["rows"].get<int>(), 21);
    CHECK_EQ(out["scan_lines"].get<int>(), 12);
    CHECK(out["video"].get<std::string>().find("line 63.89 us") == 0);
    CHECK(out["video"].get<std::string>().find("312 lines") != std::string::npos);
    CHECK_EQ(d->frame_lines(), 312);
    CHECK(fabs(d->line_us() - 63.888888) < 0.001);
    CHECK(out["video_warnings"].empty());
    CHECK_EQ(d->screen_width(), 480);
    CHECK_EQ(d->screen_height(), 252);
    reg(m, 0, 60);   // a line of 61 characters: 29.5 kHz, R1 wider than the line
    out = json::object();
    d->describe(out);
    CHECK(out["video_warnings"].size() >= 2);
}

static void test_picture(void)
{
    Machine m;
    if (!load(m, { { "blank_on_access", false } })) return;   // the set-up writes would blank line 0
    And30c* d = find_card(m);
    if (!d) return;
    setup(m);
    reg(m, 10, 0x20);   // cursor off
    std::vector<uint8_t> e0, e1;
    CHECK(file_read_all(data_path("roms/and-30c/AND-30C_IC9_EPROM0.bin"), e0) && e0.size() == 2048);
    CHECK(file_read_all(data_path("roms/and-30c/AND-30C_IC10_EPROM1.bin"), e1) && e1.size() == 2048);
    if (e0.size() != 2048 || e1.size() != 2048) return;
    memset(d->ram, ' ', sizeof(d->ram));
    m.bus.mem_write(0xF800, 'A');
    m.bus.mem_write(0xF801, 0xC4);   // "d" with a hacek: IC10
    m.bus.mem_write(0xF800 + 80, 'g');   // row 1
    const int w = d->screen_width(), h = d->screen_height();
    std::vector<uint32_t> px((size_t)w * h);
    const uint32_t bg = 0xFF000000u;
    d->render(px.data(), bg);
    int bad = 0;
    for (int ra = 0; ra < 12; ra++)
        for (int x = 0; x < 6; x++) {
            bool a = (e0[0x41 * 16 + ra] & (0x80 >> x)) != 0;
            bool c = (e1[0x44 * 16 + ra] & (0x80 >> x)) != 0;
            bool g = (e0[0x67 * 16 + ra] & (0x80 >> x)) != 0;
            if ((px[(size_t)ra * w + x] != bg) != a) bad++;
            if ((px[(size_t)ra * w + 6 + x] != bg) != c) bad++;
            if ((px[(size_t)(12 + ra) * w + x] != bg) != g) bad++;
        }
    CHECK_EQ(bad, 0);
    // the cursor (steady, lines 10-11) is a block ORed with the dots of cell 2
    reg(m, 10, 0x0A);
    reg(m, 11, 11);
    reg(m, 14, 0);
    reg(m, 15, 2);
    d->render(px.data(), bg);
    for (int ra = 0; ra < 12; ra++)
        for (int x = 0; x < 6; x++) {
            bool lit = px[(size_t)ra * w + 12 + x] != bg;
            if (lit != (ra >= 10)) bad++;
        }
    CHECK_EQ(bad, 0);
    // no picture before the 6845 is set up
    Machine m2;
    if (!load(m2)) return;
    And30c* d2 = find_card(m2);
    memset(d2->ram, 'A', sizeof(d2->ram));
    std::vector<uint32_t> p2((size_t)d2->screen_width() * d2->screen_height(), 0x12345678u);
    d2->render(p2.data(), bg);
    int lit = 0;
    for (uint32_t v : p2) lit += v != bg;
    CHECK_EQ(lit, 0);
}

// IC12B: a CPU access blanks about 4.5 us (48.6 dots) from where the beam is (two characters of the pipeline back)
static void test_blank(bool on)
{
    Machine m;
    if (!load(m, { { "blank_on_access", on } })) return;
    And30c* d = find_card(m);
    if (!d) return;
    setup(m);
    reg(m, 10, 0x20);
    memset(d->ram, 0x7F, sizeof(d->ram));   // the checkerboard: dots on every scan line
    m.bus.now = 0;
    d->reset();
    // the access when the 6845 gives row 2, scan line 0 (line 24), character 12: character 10 leaves the 74165
    const double char_s = 6 / 10.8e6, line_s = d->line_us() * 1e-6, frame_s = line_s * d->frame_lines();
    double t = 10 * frame_s + 24 * line_s + 12 * char_s;
    m.bus.now = (uint64_t)(t * m.bus.cpu_hz + 0.5);
    m.bus.mem_write(0xF900, 0x7F);
    m.bus.now += (uint64_t)(0.002 * m.bus.cpu_hz);   // 2 ms later, in the same frame
    const int w = d->screen_width(), h = d->screen_height();
    std::vector<uint32_t> px((size_t)w * h);
    const uint32_t bg = 0xFF000000u;
    d->render(px.data(), bg);
    std::vector<uint32_t> ref((size_t)w * h);
    Machine m2;   // the same picture without the access (and without the blanks of its set-up writes)
    if (!load(m2, { { "blank_on_access", false } })) return;
    And30c* d2 = find_card(m2);
    for (int i = 0; i < 16; i++) reg(m2, i, CRTTAB[i]);
    reg(m2, 10, 0x20);
    memset(d2->ram, 0x7F, sizeof(d2->ram));
    d2->render(ref.data(), bg);
    int diff = 0, first = -1, last = -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (px[(size_t)y * w + x] != ref[(size_t)y * w + x]) {
                diff++;
                CHECK_EQ(y, 24);
                if (first < 0) first = x;
                last = x;
            }
    if (!on) {
        CHECK_EQ(diff, 0);
        return;
    }
    CHECK(diff > 15);           // the lit dots of the checkerboard in about 49 dots
    CHECK(first >= 60 && first <= 62);
    CHECK(last >= 100 && last <= 109);
    // a frame later the streak is gone
    m.bus.now += (uint64_t)(frame_s * m.bus.cpu_hz);
    d->render(px.data(), bg);
    CHECK(px == ref);
}

// ---------------------------------------------------------------- CP/M with ECMON for the card

static bool load_cpm(Machine& m)
{
    std::string text, err;
    CHECK(file_read_text(data_path("machines/cpm_and30c.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "ZRMD-1") {
            c["mode"] = "nosave";
            c["working_copy"] = false;
        }
    bool ok = m.load_json(cfg, err);
    if (!ok) fprintf(stderr, "cpm_and30c: %s\n", err.c_str());
    CHECK(ok);
    return ok;
}

static void test_cpm(void)
{
    Machine m;
    if (!load_cpm(m)) return;
    And30c* d = find_card(m);
    CHECK(d);
    if (!d) return;
    bool ok = run_until_text(m, "Zadej 0-3", 10.0);
    CHECK(ok);
    if (!ok) {
        fprintf(stderr, "NO MENU:\n%s\n", screen_text(m).c_str());
        return;
    }
    m.run_seconds(0.3);
    m.type_text("3");
    ok = run_until_text(m, "A>", 30.0);
    CHECK(ok);
    if (!ok) {
        fprintf(stderr, "NO CP/M:\n%s\n", screen_text(m).c_str());
        return;
    }
    json out;
    d->describe(out);
    CHECK(out["video"].get<std::string>().find("line 63.89 us") == 0);
    CHECK(out["video_warnings"].empty());
    CHECK_EQ(d->r[1], 80);
    CHECK_EQ(d->r[6], 21);
    // a program that sends a string through CONOUT of the BIOS (BDOS would expand TAB itself), then waits
    static const uint8_t prog[] = {
        0x2A, 0x01, 0x00,         // LHLD 0001h      WBOOT
        0x11, 0x09, 0x00,         // LXI D,9
        0x19,                     // DAD D           CONOUT
        0x22, 0x16, 0x01,         // SHLD 0116h
        0x21, 0x20, 0x01,         // LXI H,0120h
        0x7E,                     // LOOP: MOV A,M
        0xFE, 0x24,               // CPI '$'
        0xCA, 0x1D, 0x01,         // JZ 011Dh
        0xE5,                     // PUSH H
        0x4F,                     // MOV C,A
        0xCD, 0x00, 0x00,         // CALL CONOUT
        0xE1,                     // POP H
        0x23,                     // INX H
        0xC3, 0x0D, 0x01,         // JMP LOOP
        0xC3, 0x1D, 0x01,         // JMP $
    };
    static const char msg[] =
        "\x1D\x1F" "RADEK0\r\n"                     // home, clear
        "\x1B\x22\x20" "LINE2" "\x1B\x23\x20" "LINE3"   // rows 2, 3
        "\x1B\x22\x20\x02" "NEW"                    // insert a line at row 2
        "\x1B\x23\x20\x03"                          // delete row 3 (LINE2): LINE3 back to row 3
        "\x1B\x23\x22\x1E"                          // row 3 from column 2 to the end of the line
        "\x1B\x26\x20" "A\tBC\bD"                   // row 6: tabulator to 8, backspace
        "\x1B\x25\x2A" "XY"                         // row 5, column 10
        "\x1B\x34\x6B" "ABCDEFGHIJ"                 // row 20, column 75: wraps and scrolls
        "$";
    for (size_t i = 0; i < sizeof(prog); i++) m.bus.mem_write((uint16_t)(0x100 + i), prog[i]);
    for (size_t i = 0; i < sizeof(msg) - 1; i++) m.bus.mem_write((uint16_t)(0x120 + i), (uint8_t)msg[i]);
    m.type_text("SAVE 1 T.COM\r");
    m.run_seconds(2.0);
    m.type_text("T\r");
    m.run_seconds(2.0);
    std::vector<std::string> rows = d->text_rows();
    CHECK_EQ(rows.size(), 21);
    if (rows.size() != 21) return;
    auto row = [&](int r) {
        std::string s = rows[r];
        size_t e = s.find_last_not_of(' ');
        return e == std::string::npos ? std::string() : s.substr(0, e + 1);
    };
    // everything one row up after the scroll
    CHECK(row(0).empty());
    CHECK(row(1) == "NEW");
    CHECK(row(2) == "LI");
    CHECK(row(3).empty());
    CHECK(row(4) == "          XY");
    CHECK(row(5) == "A       BD");
    CHECK(row(19) == std::string(75, ' ') + "ABCDE");
    CHECK(row(20) == "FGHIJ");
    unsigned cur = ((unsigned)d->r[14] << 8) | d->r[15];
    CHECK_EQ(cur, 20 * 80 + 5);
    if (row(1) != "NEW" || row(20) != "FGHIJ") fprintf(stderr, "SCREEN:\n%s\n", screen_text(m).c_str());
}

int main(void)
{
    test_init();
    const char* part = getenv("AND30C_PART");
    if (!part || !strcmp(part, "unit")) {
        test_decoder();
        test_timing();
        test_picture();
        test_blank(true);
        test_blank(false);
    }
    if (!part || !strcmp(part, "cpm")) test_cpm();
    return TEST_RESULT();
}
