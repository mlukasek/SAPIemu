// The plotter of AR 1990 (docs/desky/AR1990.md): JPR-1 with the program of the AR supplement drives a
// Minigraf 0507; commands of the Sharp MZ-1P16 come through the host cable (RDP / RDA).
// - power on: it beeps and waits for READY, then the origin against the left stop;
// - code 04H: the test of the pens (*** MZ-821 *** underlined);
// - media/ar1990/ukazka.txt: the graphics commands (lines, axes, line types, texts) and the text mode;
// - TEST held at power on: the whole character set;
// - the buttons without the window (core/plotter_ctl.h: sapiemu-cli --plotter, MCP plotter).
// AR_PNG=<folder>: also save the drawings as PNG (pictures for the documentation).
#include "chips/ar1990.h"
#include "chips/minigraf.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/plotter_ctl.h"
#include "core/util.h"
#include "test_util.h"

#include <stdlib.h>
#include <string.h>

using nlohmann::json;

static Ar1990Plotter* plotter(Machine& m)
{
    CpuCard* cpu = m.cpu;
    return cpu ? dynamic_cast<Ar1990Plotter*>(cpu->keyboard()) : nullptr;
}

static bool start(Machine& m, bool test_button = false)
{
    std::string t, err;
    CHECK(file_read_text(data_path("machines/plotter_ar1990.sapi"), t));
    json cfg = json::parse(t, nullptr, false, true);
    CHECK(m.load_json(cfg, err));
    if (!err.empty()) printf("%s\n", err.c_str());
    Ar1990Plotter* p = plotter(m);
    CHECK(p != nullptr);
    if (!p) return false;
    if (test_button) p->press(Ar1990Plotter::BTN_TEST);   // held at power on: the whole test
    m.run_seconds(1.0);   // it beeps and waits for READY (as after any stop)
    if (!test_button) CHECK(p->minigraf()->steps == 0);
    p->press(Ar1990Plotter::BTN_READY);
    m.run_seconds(6.0);   // the origin: the carriage against the left stop
    return true;
}

// sends the text and runs until the plotter has taken it and the pen stopped moving
static void plot(Machine& m, const uint8_t* data, size_t len, double max_s)
{
    Ar1990Plotter* p = plotter(m);
    p->send(data, len);
    Minigraf* g = p->minigraf();
    double t = 0;
    uint64_t steps = g->steps;
    int quiet = 0;
    while (t < max_s) {
        m.run_seconds(0.5);
        t += 0.5;
        if (p->pending() == 0 && g->steps == steps) {
            if (++quiet >= 4) break;
        } else quiet = 0;
        steps = g->steps;
    }
    printf("  %.1f s, %llu steps, %d strokes, %llu lost, %llu against the stop\n", t, (unsigned long long)g->steps,
           (int)g->strokes.size(), (unsigned long long)g->lost, (unsigned long long)g->stopped);
}

static void save(Machine& m, const char* name)
{
    const char* dir = getenv("AR_PNG");
    if (!dir) return;
    std::string err;
    Minigraf* g = plotter(m)->minigraf();
    CHECK(g->save_png(std::string(dir) + "/" + name, 8, true, err));
}

static void test_pens(void)
{
    Machine m;
    if (!start(m)) return;
    Ar1990Plotter* p = plotter(m);
    Minigraf* g = p->minigraf();
    printf("after power on: x %.2f mm, %llu steps, %llu against the stop, LED READY %d BUFF %d\n",
           g->x() / 8.0, (unsigned long long)g->steps, (unsigned long long)g->stopped, p->led_ready(), p->led_buff());
    CHECK(g->stopped > 1000);   // the origin: against the left stop (the Minigraf has no switches)
    CHECK(!g->pen_down());
    CHECK(p->led_buff());       // the buffer is empty
    const uint8_t pens = 0x04;  // the test of the pens: *** MZ-821 *** underlined
    plot(m, &pens, 1, 120.0);
    save(m, "ar1990_ptest.png");
    int32_t x0, y0, x1, y1;
    CHECK(g->extent(x0, y0, x1, y1));
    double w = (x1 - x0) / 8.0;   // 14 characters of 40 a line, the underline: 62.5 mm
    printf("  the test of the pens: %.2f x %.2f mm\n", w, (y1 - y0) / 8.0);
    CHECK(w > 60 && w < 65);
    CHECK(g->strokes.size() > 20 && g->lost == 0);
    CHECK(p->sent == 1);
}

static void test_demo(void)
{
    Machine m;
    if (!start(m)) return;
    std::vector<uint8_t> d;
    CHECK(file_read_all(data_path("media/ar1990/ukazka.txt"), d));
    std::string err;
    CHECK(plotter_action(m, "demo", "", err));   // the button Demo of the window, MCP demo, cli --plotter demo
    plot(m, nullptr, 0, 900.0);
    save(m, "ar1990_ukazka.png");
    Ar1990Plotter* p = plotter(m);
    CHECK(p->pending() == 0);
    CHECK(p->sent == d.size());
    Minigraf* g = p->minigraf();
    int32_t x0, y0, x1, y1;
    CHECK(g->extent(x0, y0, x1, y1));
    printf("  the demo: %.2f x %.2f mm\n", (x1 - x0) / 8.0, (y1 - y0) / 8.0);
    CHECK(x1 - x0 == 1360);   // the frame: 170 mm wide (the text lines below are narrower)
    CHECK(g->lost == 0);
}

// TEST held at power on: the origin, then the whole character set (10H-7FH); then the same with the
// button Whole test of the window (TEST held, the RESET of the JPR-1)
static void test_charset(void)
{
    Machine m;
    if (!start(m, true)) return;
    plot(m, nullptr, 0, 300.0);
    Minigraf* g0 = plotter(m)->minigraf();
    size_t first = g0->strokes.size();
    CHECK(g0->lost == 0);
    std::string err;
    CHECK(plotter_action(m, "new-sheet", "", err));
    CHECK(g0->strokes.empty());
    CHECK(plotter_action(m, "whole_test", "", err));   // as MCP names it: TEST held, the RESET of the JPR-1
    plot(m, nullptr, 0, 300.0);
    CHECK(g0->strokes.size() == first);
    save(m, "ar1990_znaky.png");
    Minigraf* g = plotter(m)->minigraf();
    CHECK(g->strokes.size() > 150);   // the reset cleared P0-OUT: the motors jumped to phase 0 (lost steps)
    int32_t x0, y0, x1, y1;
    CHECK(g->extent(x0, y0, x1, y1));
    printf("  the character set: %.2f x %.2f mm\n", (x1 - x0) / 8.0, (y1 - y0) / 8.0);
    CHECK(y1 - y0 > 13 * 128);   // 14 rows of 8 characters, 16 mm apart
}

// The buttons without the window (core/plotter_ctl.h) as sapiemu-cli has them: --plotter ready, the origin,
// --plotter test (the test of the pens: 28 strokes, 62.5 x 12 mm, about 16.5 s), --print-plotter, --plotter-png
static void test_cli_buttons(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/plotter_ar1990.sapi", err));
    Ar1990Plotter* p = plotter(m);
    CHECK(p != nullptr);
    if (!p) return;
    std::string png = data_path("work/test/ar1990_cli.png");
    std::vector<std::string> acts = { "--run-ms", "1000", "--plotter", "ready", "--run-ms", "6000", "--plotter", "test",
                                      "--run-ms", "20000", "--print-plotter", "--plotter-png", png };
    CHECK_EQ(headless_run(m, acts), 0);
    Minigraf* g = p->minigraf();
    CHECK(g->stopped > 1000);   // READY: the origin against the left stop
    CHECK_EQ(g->strokes.size(), 28u);
    int32_t x0, y0, x1, y1;
    CHECK(g->extent(x0, y0, x1, y1));
    double w = (x1 - x0) / 8.0, h = (y1 - y0) / 8.0;
    printf("  the button TEST: %u strokes, %.2f x %.2f mm\n", (unsigned)g->strokes.size(), w, h);
    CHECK(w > 60 && w < 65);
    CHECK(h > 10 && h < 14);
    CHECK(p->led_buff() && !p->led_ready());
    std::string st = plotter_status(m);
    CHECK(st.find("plotter: AR 1990 (ARITMA Minigraf 0507)") != std::string::npos);
    CHECK(st.find("28 strokes") != std::string::npos);
    CHECK(st.find("RDY off, BUFF on; host: 0 B waiting") != std::string::npos);
    std::vector<uint8_t> d;
    CHECK(file_read_all(png, d) && d.size() > 100);
    file_remove(png);
    // a line of commands as the window sends it: \xNN = a byte, CR at the end
    CHECK(plotter_line("\\x02M100,0") == std::string("\x02M100,0\r"));
    CHECK(plotter_line("\\x2Z\\") == std::string("\\x2Z\\\r"));   // not two hex digits: as typed
    size_t before = p->pending();
    CHECK(plotter_action(m, "send", "\\x01", err));
    CHECK_EQ(p->pending(), before + 2);
    CHECK(!plotter_action(m, "pen", "", err));   // the XY 4130 only
    CHECK(!plotter_action(m, "jump", "", err));
    CHECK(err.find("unknown plotter action") != std::string::npos);
    CHECK(!plotter_action(m, "send-file", "media/ar1990/none.txt", err));
    std::vector<std::string> bad = { "--plotter", "pen" };
    CHECK(headless_run(m, bad) != 0);
}

// AR_STATE=<file>: a state in the middle of the demo (the picture of the window in the documentation)
static void state_for_docs(const char* file)
{
    Machine m;
    if (!start(m)) return;
    std::vector<uint8_t> d;
    CHECK(file_read_all(data_path("media/ar1990/ukazka.txt"), d));
    Ar1990Plotter* p = plotter(m);
    p->send(d.data(), d.size());
    for (int i = 0; i < 400 && p->minigraf()->steps < 26000; i++) m.run_seconds(0.25);
    std::string err;
    std::vector<uint8_t> st;
    CHECK(m.save_state(st, "machines/plotter_ar1990.sapi", err));
    CHECK(file_write_all(file, st.data(), st.size()));
}

int main(void)
{
    test_init();
    if (const char* st = getenv("AR_STATE")) {
        state_for_docs(st);
        return TEST_RESULT();
    }
    test_pens();
    test_cli_buttons();
    test_demo();
    test_charset();
    return TEST_RESULT();
}
