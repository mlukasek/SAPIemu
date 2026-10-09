// ARITMA Minigraf 0507 on DPP-1 port B: the phase codes, the pen, the stops and lost steps through the
// 8255; the driver V.1-MGF in EPROM (ZPS-2 MIKOS V5): .G=1800 sets the origin against the left stop,
// .G=1809 draws the character set as the real plotter did (sapi.cz obrazky/0507_4.png); PNG, SVG.
#include "chips/minigraf.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

#include <math.h>
#include <string.h>

using nlohmann::json;

static Minigraf* find(Machine& m)
{
    for (auto& c : m.cards)
        if (c->minigraf()) return c->minigraf();
    return nullptr;
}

static void test_codes(void)
{
    json cfg = { { "cards", json::array() } };
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    cfg["cards"].push_back({ { "slot", 2 }, { "type", "DPP-1" }, { "base", "30h" }, { "minigraf", json::object() } });
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    Minigraf* g = find(m);
    CHECK(g != nullptr);
    if (!g) return;
    // port B an input: the lines float high, the pen is down
    CHECK_EQ(g->lines(), 0xFF);
    CHECK(g->pen_down());
    m.bus.io_write(0x33, 0x99);   // PB output: all low, the pen goes up, X and Y step from 7 to 0
    CHECK(!g->pen_down());
    int32_t x0 = g->x(), y0 = g->y();
    CHECK_EQ(x0, Minigraf::STOP_X + 60 * Minigraf::STEPS_PER_MM + 1);
    CHECK_EQ(y0, 1);
    g->new_sheet();
    CHECK(g->strokes.empty());
    CHECK_EQ(g->y(), 0);
    // pen down, 16 steps right (X code up), 8 up the paper (Y code up), back left by 4
    uint8_t xc = 0, yc = 0;
    auto out = [&](bool pen) { m.bus.io_write(0x31, (uint8_t)((pen ? 0x80 : 0) | (yc & 7) << 3 | (xc & 7))); };
    out(true);
    for (int i = 0; i < 16; i++) { xc++; out(true); }
    for (int i = 0; i < 8; i++) { yc++; out(true); }
    for (int i = 0; i < 4; i++) { xc--; out(true); }
    out(false);
    CHECK_EQ(g->x(), x0 + 12);
    CHECK_EQ(g->y(), 8);
    CHECK_EQ(g->strokes.size(), 1u);
    if (!g->strokes.empty()) {   // three straight runs: 4 points
        const std::vector<int32_t>& s = g->strokes[0];
        CHECK_EQ(s.size(), 8u);
        if (s.size() == 8) {
            CHECK_EQ(s[0], x0);
            CHECK_EQ(s[2], x0 + 16);
            CHECK_EQ(s[5], 8);
            CHECK_EQ(s[6], x0 + 12);
        }
    }
    // a slanted line (60 steps, X and Y alternating) stays a few points
    out(true);
    for (int i = 0; i < 40; i++) {
        xc++;
        out(true);
        if (i % 2) {
            yc++;
            out(true);
        }
    }
    out(false);
    CHECK_EQ(g->strokes.size(), 2u);
    if (g->strokes.size() == 2) CHECK(g->strokes[1].size() <= 6u);
    // a jump of the code by 4: the motor cannot tell the way, nothing moves, the step is lost
    uint64_t lost = g->lost;
    int32_t x1 = g->x();
    xc += 4;
    out(false);
    CHECK_EQ(g->x(), x1);
    CHECK_EQ(g->lost, lost + 1);
    // against the left stop: the carriage stays there, the steps are counted
    for (int i = 0; i < 1000 * Minigraf::STEPS_PER_MM / 4; i++) { xc--; out(false); }
    CHECK_EQ(g->x(), Minigraf::STOP_X);
    CHECK(g->stopped > 100);
    CHECK_EQ(g->lost, lost + 1);
}

static bool prompt(Machine& m)
{
    std::vector<std::string> rows = m.display->text_rows();
    for (int i = (int)rows.size() - 1; i >= 0; i--) {
        std::string r = rows[(size_t)i];
        while (!r.empty() && r.back() == ' ') r.pop_back();
        if (!r.empty()) return r == ".";
    }
    return false;
}

static bool command(Machine& m, const char* keys, double timeout_s)
{
    m.type_text(keys);
    m.run_seconds(0.5);
    for (double t = 0; t < timeout_s; t += 0.5) {
        if (prompt(m)) return true;
        m.run_seconds(0.5);
    }
    return false;
}

static void test_driver(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-2_MIKOS_V5_Minigraf.sapi", err));
    CHECK(run_until_text(m, "MONITOR V5.0", 5.0));
    m.run_seconds(0.5);
    Minigraf* g = find(m);
    CHECK(g != nullptr);
    if (!g) return;
    // INIC: control word 99h, the pen to the left stop and 80 steps (10 mm) back right, the paper 1700 steps on
    CHECK(command(m, "G1800\r", 30.0));
    CHECK(!g->pen_down());
    CHECK_EQ(g->x(), Minigraf::STOP_X + 80);
    CHECK(g->stopped > 1000);
    CHECK_EQ(g->lost, 0u);
    CHECK_EQ(g->fast, 0u);   // the origin is set at the speed the plotter takes (400 steps/s)
    // TEST: the character set, 12 rows of 8 as on sapi.cz 0507_4.png
    g->new_sheet();
    CHECK(command(m, "G1809\r", 240.0));
    int32_t x0, y0, x1, y1;
    CHECK(g->extent(x0, y0, x1, y1));
    double w = (x1 - x0) / 8.0, h = (y1 - y0) / 8.0;
    printf("TEST: %u strokes, %.2f x %.2f mm, %llu steps\n", (unsigned)g->strokes.size(), w, h, (unsigned long long)g->steps);
    CHECK(g->strokes.size() >= 120);
    CHECK(fabs(w - 83.75) < 1.0);
    CHECK(fabs(h - 184.75) < 1.0);
    // exports
    std::string png = data_path("work/test/minigraf.png"), svg = data_path("work/test/minigraf.svg");
    CHECK(g->save_png(png, 4, err));
    CHECK(g->save_svg(svg, err));
    std::string text;
    CHECK(file_read_text(svg, text));
    CHECK(text.find("<polyline") != std::string::npos);
    file_remove(png);
    file_remove(svg);
}

int main(void)
{
    test_init();
    test_codes();
    test_driver();
    return TEST_RESULT();
}
