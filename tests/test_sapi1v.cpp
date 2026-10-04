// Libor Lasota's V boards (machines/sapi1v.sapi): JPR-1V boots ECMON (bank 1)
// and CP/M from the ZRD-1V RAM disk on the AND-1V, bank 0 is MikroBIOS 3.0;
// the RAM-1V MAP register drives MAP1 and an AND-1V set to MAP1 = L hides;
// DSM-1V gives two serial channels.
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "core/bus.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/png.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static json machine_cfg(void)
{
    std::string text;
    CHECK(file_read_text(data_path("machines/sapi1v.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "IDE-1") c.erase("ide");   // no disk file needed here
    return cfg;
}

static void cpm(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    CHECK(run_until_text(m, "JPR-1Z 62K MON CZ", 3.0));
    CHECK(run_until_text(m, "Zadej 0-3", 3.0));
    m.run_seconds(0.3);
    m.type_text("1");
    CHECK(run_until_text(m, "\nA>", 10.0));
    m.run_seconds(0.3);
    m.type_text("DIR\r");
    CHECK(run_until_text(m, "WM1Z    .HLP", 5.0));
    // DSM-1V: two channels
    CHECK_EQ((int)m.serials.size(), 2);
    printf("%s\n", screen_text(m).c_str());
}

static void mikrobios(void)
{
    json cfg = machine_cfg();
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "JPR-1V") c["bios"] = "fitted";
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, "MikroBIOS 3.0 AllRAM", 3.0));
    CHECK(run_until_text(m, "Falcon Monitor Z80", 3.0));
}

static void map_register(void)
{
    json cfg = machine_cfg();
    for (json& c : cfg["cards"]) {
        std::string t = c.value("type", std::string());
        if (t == "RAM-1V") c["map1_enable"] = true;   // MAP1 = data bit 6 of OUT 63h
        if (t == "AND-1V") c["map"] = "map1_l";
    }
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    m.run_seconds(0.5);
    m.bus.mem_write(0xF800, 0x41);
    CHECK_EQ(m.bus.mem_peek(0xF800), 0x41);   // AND-1V visible with MAP1 = L
    m.bus.io_write(0x63, 0x40);
    CHECK((m.bus.map & MAP_1) != 0);
    CHECK(m.bus.mem_peek(0xF800) != 0x41);    // MAP1 = H: AND-1V off, the hole of RAM-1V: open bus
    m.bus.io_write(0x63, 0x00);
    CHECK((m.bus.map & MAP_1) == 0);
    CHECK_EQ(m.bus.mem_peek(0xF800), 0x41);
}

// JPR-1V TURBO switched while running: CPU 2 MHz -> 4 MHz, the time base stays
static double boot_seconds(bool turbo)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    CHECK(m.cpu->has_cpu_turbo());
    m.cpu->set_cpu_turbo(turbo);
    CHECK_EQ((int)m.cpu->cpu_display_hz(), turbo ? 4000000 : 2000000);
    double t = 0;
    while (t < 5.0 && !screen_contains(m, "Zadej 0-3")) {
        m.run_seconds(0.01);
        t += 0.01;
    }
    return t;
}

static void turbo(void)
{
    double slow = boot_seconds(false), fast = boot_seconds(true);
    printf("ECMON menu after %.2f s at 2 MHz, %.2f s at 4 MHz\n", slow, fast);
    CHECK(slow < 5.0);
    CHECK(fast < slow * 0.8);
}

// CGA-1V: Libor's picture viewers (CP/M .COM at 0100h) saved by SAVE and run;
// CGA_PNG=<dir> keeps the pictures.
static void run_com(Machine& m, const char* hex, int pages, const char* name)
{
    std::string text, err;
    CHECK(file_read_text(data_path(std::string("media/cga-1v/") + hex), text));
    CHECK(m.load_ihex_text(text, err));
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "SAVE %d %s.COM\r", pages, name);
    m.type_text(cmd);
    CHECK(run_until_text(m, "\nA>", 10.0));
    m.run_seconds(0.3);
    snprintf(cmd, sizeof(cmd), "%s\r", name);
    m.type_text(cmd);
}

// Number of different colours on the CGA-1V output.
static int cga_colours(Machine& m, const char* png_name)
{
    DisplayDevice* d = m.find_display("CGA-1V");
    CHECK(d != nullptr);
    if (!d) return 0;
    std::vector<uint32_t> px((size_t)d->screen_width() * d->screen_height());
    d->render(px.data(), 0xFFFFFFFFu, 0xFF000000u);
    const char* dir = getenv("CGA_PNG");
    if (dir) png_write_argb(std::string(dir) + "/" + png_name + ".png", px.data(), d->screen_width(), d->screen_height(), 2, 2);
    std::sort(px.begin(), px.end());
    return (int)(std::unique(px.begin(), px.end()) - px.begin());
}

static void cga_pictures(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    CHECK(run_until_text(m, "Zadej 0-3", 3.0));
    m.run_seconds(0.3);
    m.type_text("1");
    CHECK(run_until_text(m, "\nA>", 10.0));
    m.run_seconds(0.3);
    // EGA: 4 pixels with a common attribute, 32 colours
    run_com(m, "castle.hex", 64, "CASTLE");
    m.run_seconds(3.0);
    int castle = cga_colours(m, "castle");
    // CGA, pages A and B with their own palettes switched every frame: blended
    run_com(m, "rembran2.hex", 127, "REMBRAN2");
    m.run_seconds(6.0);
    int rembran2 = cga_colours(m, "rembran2");
    printf("CGA-1V: CASTLE %d colours, REMBRAN2 %d colours (blended pages)\n", castle, rembran2);
    CHECK(castle > 16);
    CHECK(rembran2 > 4);
}

// VBI request about 60 times a second in STATUS D7, acknowledged by CONFIG D7.
static void cga_vbi(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    m.run_seconds(0.2);
    m.bus.io_write(0x63, 0xC0);   // RAM-1V: MAP1 = MAP2 = H, CGA-1V at C000
    int n = 0;
    for (int i = 0; i < 1000; i++) {
        m.run_seconds(0.001);
        if (m.bus.mem_peek(0xFFFB) & 0x80) {
            n++;
            m.bus.mem_write(0xFFFB, 0x80);
        }
    }
    printf("CGA-1V: %d VBI in 1 s\n", n);
    CHECK(n >= 59 && n <= 61);
    m.bus.mem_write(0xFFFB, 0x88);   // IRQ enabled
    m.run_seconds(0.02);
    CHECK((m.bus.irq_lines() & IRQ_INT0) != 0);
    m.bus.mem_write(0xFFFB, 0x80);   // acknowledged, IRQ disabled
    CHECK((m.bus.irq_lines() & IRQ_INT0) == 0);
    m.bus.io_write(0x63, 0x00);
}

int main(void)
{
    test_init();
    turbo();
    cpm();
    mikrobios();
    map_register();
    cga_vbi();
    cga_pictures();
    return TEST_RESULT();
}
