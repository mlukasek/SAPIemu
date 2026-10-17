// Grafická jednotka XY 4130 (docs/desky/XY4130.md): the cable (step on the rising edge, the axes and
// directions, the pen), READY from the left stop, the PEN button, the cables mikos and kres; the driver of
// TESLA DIZ in the EPROM behind MIKOS V5 on JPR-1 X3 (ZPS-2): the origin against the left stop and the test
// of the character set.
#include "chips/xy4130.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

static Xy4130* find(Machine& m)
{
    for (auto& c : m.cards)
        if (Xy4130* g = dynamic_cast<Xy4130*>(c->plotter())) return g;
    return nullptr;
}

// one step through the mikos cable (D0 pen, D1 X/Y, D6 +/-, D7 step)
static void pulse(Xy4130& g, int axis, int dir, bool pen)
{
    uint8_t v = (uint8_t)((pen ? 1 : 0) | axis << 1 | dir << 6);
    g.set_lines(v);
    g.set_lines(v | 0x80);
}

static void test_lines(void)
{
    Xy4130 g;
    g.power_on();
    CHECK(!g.ready());
    CHECK(!g.pen_enabled());
    const int32_t x0 = g.x();
    pulse(g, 0, 1, true);   // the carriage to the right, pen asked down: no READY, no PEN
    CHECK_EQ(g.x(), x0 + 1);
    CHECK(!g.pen_down());
    pulse(g, 1, 0, true);   // the paper: +/- low = the pen goes up the sheet
    CHECK_EQ(g.y(), 1);
    pulse(g, 1, 1, true);
    CHECK_EQ(g.y(), 0);
    g.set_lines(0x81);      // STEP stays high: no more steps
    g.set_lines(0x81);
    CHECK_EQ(g.steps, 3u);
    g.set_lines(0x01);      // the falling edge does not step
    CHECK_EQ(g.steps, 3u);
    // to the left stop: READY, and it stays when the carriage leaves
    int n = 0;
    while (g.x() > g.stop_x && n < 100000) {
        pulse(g, 0, 0, false);
        n++;
    }
    CHECK_EQ(g.x(), g.stop_x);
    CHECK(g.ready());
    pulse(g, 0, 0, false);   // against the stop
    CHECK_EQ(g.x(), g.stop_x);
    CHECK_EQ(g.stopped, 1u);
    for (int i = 0; i < 80; i++) pulse(g, 0, 1, false);
    CHECK(g.ready());
    CHECK(!g.pen_down());
    g.pen_button();          // PEN: now the pen follows the line
    CHECK(g.pen_enabled());
    pulse(g, 0, 1, true);
    CHECK(g.pen_down());
    for (int i = 0; i < 10; i++) pulse(g, 1, 0, true);
    CHECK_EQ(g.strokes.size(), 1u);
    g.new_sheet();           // the paper buttons: READY off, the pen up
    CHECK(!g.ready());
    CHECK(!g.pen_down());
    g.pen_button();
    CHECK(!g.pen_enabled());
    // the right stop
    g.power_on();
    for (int i = 0; i < g.travel + 50; i++) pulse(g, 0, 1, false);
    CHECK_EQ(g.x(), g.stop_x + g.travel);
    CHECK(!g.ready());
    // the cable of KRES: D0 step, D2 X/Y, D3 +/-, D5 READY (L = ready), D7 pen
    Xy4130 k;
    k.cable = Xy4130::CABLE_KRES;
    k.power_on();
    CHECK_EQ(k.pins_b(), 0xFF);
    int32_t kx = k.x();
    k.set_lines(0x08);
    k.set_lines(0x09);       // X, + : to the right
    CHECK_EQ(k.x(), kx + 1);
    k.set_lines(0x04);
    k.set_lines(0x05);       // Y, - : the pen goes up the sheet
    CHECK_EQ(k.y(), 1);
    for (int i = 0; i < 1000; i++) {
        k.set_lines(0x00);
        k.set_lines(0x01);
    }
    CHECK(k.ready());
    CHECK_EQ(k.pins_b(), 0xDF);
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

static void wait_still(Machine& m, Xy4130* g, double timeout_s)
{
    uint64_t s = ~0ull;
    for (double t = 0; t < timeout_s && g->steps != s; t += 0.5) {
        s = g->steps;
        m.run_seconds(0.5);
    }
}

static void test_driver(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-2_MIKOS_V5_XY4130.sapi", err));
    CHECK(run_until_text(m, "L=XY4130", 5.0));
    Xy4130* g = find(m);
    CHECK(g != nullptr);
    if (!g) return;
    // the origin: the carriage to the left stop, 80 steps (8 mm) back right
    wait_still(m, g, 30.0);
    printf("origin: x %.1f mm (stop %.1f), y %.1f mm, %llu steps, %llu at the stop, %llu fast\n", g->x() / 10.0,
           g->stop_x / 10.0, g->y() / 10.0, (unsigned long long)g->steps, (unsigned long long)g->stopped,
           (unsigned long long)g->fast);
    CHECK(g->ready());
    CHECK_EQ(g->x(), g->stop_x + 80);
    CHECK(g->stopped > 0);
    CHECK(!g->pen_down());
    CHECK(g->strokes.empty());   // PEN not pressed: nothing drawn
    CHECK(prompt(m));
    g->pen_button();
    // TEST: a new page asks for the paper (a key), the origin again, then the characters 10H-FFH
    m.type_text("G181E\r");
    CHECK(run_until_text(m, "STISKNI", 5.0));
    m.type_text(" ");
    for (int i = 0; i < 1200 && !prompt(m); i++) m.run_seconds(0.5);
    CHECK(prompt(m));
    int32_t x0, y0, x1, y1;
    CHECK(g->extent(x0, y0, x1, y1));
    printf("TEST: %u strokes, x %.1f-%.1f, y %.1f-%.1f mm, %llu steps, %llu fast\n", (unsigned)g->strokes.size(), x0 / 10.0,
           x1 / 10.0, y0 / 10.0, y1 / 10.0, (unsigned long long)g->steps, (unsigned long long)g->fast);
    CHECK(g->strokes.size() >= 600);
    CHECK(fabs((x1 - x0) / 10.0 - 92.8) < 1.0);   // 16 characters a row
    CHECK(fabs((y1 - y0) / 10.0 - 58.0) < 1.0);   // 15 rows
    CHECK_EQ(g->fast, 0u);
    // the text reads from the top left: the first character (10H, a square) is in the corner where the
    // origin is (x 8 mm right of the stop, y = 2460 of the driver at the top), not mirrored
    const std::vector<int32_t>& first = g->strokes[0];
    CHECK(first[0] - x0 < 30);
    CHECK(y1 - first[1] < 50);
    std::string png = data_path("work/test/xy4130.png"), svg = data_path("work/test/xy4130.svg");
    make_dirs(path_dir(png));
    CHECK(g->save_png(png, 4, false, err));
    CHECK(g->save_svg(svg, true, err));
    std::string text;
    CHECK(file_read_text(svg, text));
    CHECK(text.find("stroke-width=\"3.75\"") != std::string::npos);   // the pen 0.375 mm in steps of 0.1 mm
    file_remove(png);
    file_remove(svg);
    // the guide (docs/navod-xy4130.md): a new sheet puts READY off, .G=1803 sets the origin again (MIKOS
    // restarts), media/mikos/XY4130_DEMO.hex from .G=8000 writes its text and draws the frame 100 x 60 mm
    wait_still(m, g, 30.0);
    g->new_sheet();
    CHECK(!g->ready());
    m.type_text("G1803\r");
    m.run_seconds(1.0);
    wait_still(m, g, 30.0);
    CHECK(g->ready());
    CHECK(file_read_text(data_path("media/mikos/XY4130_DEMO.hex"), text));
    CHECK(m.load_ihex_text(text, err, nullptr));
    m.type_text("G8000\r");
    for (int i = 0; i < 240 && g->strokes.empty(); i++) m.run_seconds(0.5);
    wait_still(m, g, 120.0);
    CHECK(g->extent(x0, y0, x1, y1));
    printf("DEMO: %u strokes, %.1f x %.1f mm\n", (unsigned)g->strokes.size(), (x1 - x0) / 10.0, (y1 - y0) / 10.0);
    CHECK_EQ(x1 - x0, 1000);   // the frame 100 mm wide, the text above it within
    CHECK(fabs((y1 - y0) / 10.0 - 78.8) < 1.0);
}

// KRES of BASIC EXP V5.0/G+SZ with the configuration byte 0Dh (XY 4130 on DPP-1, port B 31h): millimetres
// in steps of 0.1 mm, the origin from the left stop
static void test_kres(void)
{
    std::string err, text;
    CHECK(file_read_text(data_path("machines/zps/ZPS-2_MIKOS_V5_Minigraf.sapi"), text));
    nlohmann::json cfg = nlohmann::json::parse(text, nullptr, false, true);
    for (auto& c : cfg["cards"])
        if (c["type"] == "DPP-1") {
            c.erase("minigraf");
            c.erase("minigraf_channel");
            c["xy4130"] = { { "pen_on", true } };   // the PEN button pressed already (the deviation)
        }
    Machine m;
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, "MONITOR V5.0", 5.0));
    m.run_seconds(0.5);
    Xy4130* g = find(m);
    CHECK(g != nullptr);
    if (!g) return;
    CHECK(g->pen_enabled());
    CHECK(file_read_text(data_path("media/mikos/BASIC_EXP_V5.0G_SZ.hex"), text));
    CHECK(m.load_ihex_text(text, err, nullptr));
    m.bus.mem_write(0x7D1C, 0x0D);   // the original configuration: XY 4130, DPP-1, Consul
    m.type_text("B");
    CHECK(run_until_text(m, "READY", 5.0));
    m.run_seconds(0.5);
    // KRES knows no stops: the pen to the left stop first (READY), then the origin 40 mm right of it
    m.type_text("10 KRES #I -220,0,, #I 40,0,,\r20 KRES #K 0,0,30,36\rRUN\r");
    for (int i = 0; i < 240 && g->strokes.empty(); i++) m.run_seconds(0.5);
    wait_still(m, g, 60.0);
    printf("KRES: lines %02X, %llu steps, x %d, ready %d, %u strokes, %llu at the stop, %llu fast\n", g->lines(),
           (unsigned long long)g->steps, (int)g->x(), (int)g->ready(), (unsigned)g->strokes.size(),
           (unsigned long long)g->stopped, (unsigned long long)g->fast);
    CHECK(g->ready());
    CHECK(g->stopped > 0);   // #I runs to the left stop
    CHECK(!g->strokes.empty());
    if (g->strokes.empty()) return;
    const std::vector<int32_t>& s = g->strokes.back();
    int32_t x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
        x0 = std::min(x0, s[i]);
        x1 = std::max(x1, s[i]);
        y0 = std::min(y0, s[i + 1]);
        y1 = std::max(y1, s[i + 1]);
    }
    printf("KRES circle: %.2f x %.2f mm\n", (x1 - x0) / 10.0, (y1 - y0) / 10.0);
    CHECK(fabs((x1 - x0) / 10.0 - 60) < 0.5);
    CHECK(fabs((y1 - y0) / 10.0 - 60) < 0.5);
}

int main(void)
{
    test_init();
    test_lines();
    test_driver();
    test_kres();
    return TEST_RESULT();
}
