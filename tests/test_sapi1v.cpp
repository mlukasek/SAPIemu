// Libor Lasota's V boards (machines/sapi1v.sapi): JPR-1V boots ECMON (bank 1)
// and CP/M from the ZRD-1V RAM disk on the AND-1V, bank 0 is MikroBIOS 3.0;
// the RAM-1V MAP register drives MAP1 and an AND-1V set to MAP1 = L hides;
// DSM-1V gives two serial channels.
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "cards/zrmd1.h"
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
    for (json& c : cfg["cards"]) {
        if (c.value("type", std::string()) == "IDE-1") c.erase("ide");   // no disk file needed here
        if (c.value("type", std::string()) == "ZRD-1V") c["revision"] = "2.1";   // the author has 1.1
    }
    return cfg;
}

// ZRD-1V rev 1.1 (the author's board): no lock, no LEDs; a read of 61h loads FFh
static void zrd_rev11(void)
{
    json cfg = machine_cfg();
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "ZRD-1V") c["revision"] = "1.1";
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    Card* zrd = m.find_card("ZRD-1V");
    CHECK(zrd != nullptr);
    if (!zrd) return;
    m.bus.io_write(0x61, 0xFF);
    m.bus.io_write(0x62, 0xFF);
    m.bus.io_write(0x63, 0x3F);
    m.run_seconds(0.1);
    m.bus.io_write(0x60, 0x5A);   // like MikroMon: written, no lock
    CHECK(!((Zrmd1*)zrd)->locked());
    m.bus.io_write(0x61, 0xFF);
    CHECK_EQ(m.bus.io_read(0x60), 0x5A);
    CHECK_EQ(zrd->panel_leds(), 0u);
    CHECK(zrd->live_status().empty());
    m.bus.io_write(0x61, 0x10);
    CHECK_EQ(m.bus.io_read(0x61), 0xFF);   // no MAX187: the undriven bus
    m.bus.io_write(0x61, 0xFF);            // the read loaded FFh: 60h reads 3FFFFFh again
    uint8_t want = m.bus.io_read(0x60);
    m.bus.io_write(0x61, 0x10);
    m.bus.io_read(0x61);
    CHECK_EQ(m.bus.io_read(0x60), want);
}

// ZRD-1V rev 2.1: writes to 60h only within 20 ms after an access to 62h; a write outside
// locks the card until /RES (MikroMon does it on purpose); the MAX187 reads the battery.
static void zrd_lock(Machine& m, Card* zrd)
{
    auto at = [&](uint32_t a) {
        m.bus.io_write(0x61, (uint8_t)a);
        m.bus.io_write(0x62, (uint8_t)(a >> 8));
        m.bus.io_write(0x63, (uint8_t)(a >> 16));
    };
    at(0x3FFFF0);
    uint8_t b = m.bus.io_read(0x60);
    at(0x3FFFF0);
    m.bus.io_write(0x60, (uint8_t)~b);   // inside the window: written
    at(0x3FFFF0);
    CHECK_EQ(m.bus.io_read(0x60), (uint8_t)~b);
    at(0x3FFFF0);
    m.bus.io_write(0x60, b);
    // MikroMon: OUT 61h,FF / 62h,FF / 63h,3F, 100 ms, OUT 60h -> locked, nothing written
    at(0x3FFFFF);
    uint8_t last = m.bus.io_read(0x60);
    at(0x3FFFFF);
    m.run_seconds(0.1);
    CHECK(!((Zrmd1*)zrd)->locked());
    m.bus.io_write(0x60, (uint8_t)~last);
    CHECK(((Zrmd1*)zrd)->locked());
    CHECK(zrd->panel_leds() & 1u);
    at(0x3FFFFF);
    m.bus.io_write(0x60, (uint8_t)~last);   // locked: no write even inside the window
    at(0x3FFFFF);
    CHECK_EQ(m.bus.io_read(0x60), last);
    CHECK(zrd->live_status().find("locked by") != std::string::npos);
    // the configuration does not change: the lock is a latch on the board
    nlohmann::json* zc = m.card_config(zrd);
    CHECK(zc && !zc->contains("locked"));
    // /RES unlocks
    m.reset();
    CHECK(!((Zrmd1*)zrd)->locked());
    CHECK(!(zrd->panel_leds() & 1u));
    // MAX187: an access to 62h starts the conversion, reads of 61h clock out EOC, D11..D0 on D5
    std::string err;
    CHECK(m.live_param(zrd, "battery_mv"));
    CHECK(m.set_live_param(zrd, "battery_mv", 2873, err));
    m.run_seconds(0.03);                    // the window of the last access is over
    m.bus.io_write(0x62, 0x00);
    CHECK_EQ(m.bus.io_read(0x61) & 0x20, 0x00);   // converting
    m.run_seconds(0.0001);
    CHECK_EQ(m.bus.io_read(0x61) & 0x20, 0x20);   // end of conversion
    int code = 0;
    for (int i = 0; i < 12; i++) code = code << 1 | ((m.bus.io_read(0x61) >> 5) & 1);
    CHECK_EQ(code, 2873);
    CHECK_EQ(m.bus.io_read(0x61) & 0x20, 0x00);   // zeros after the LSB
    // a read of 61h loads the address register with what it read (FFh / DFh)
    m.bus.io_write(0x61, 0x12);
    uint8_t r = m.bus.io_read(0x61);
    nlohmann::json d;
    zrd->describe(d);
    char want[8];
    snprintf(want, sizeof(want), "%02X", r);
    CHECK(d.value("address", std::string()).substr(4) == want);
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
    // ZRD-1V rev 2.1 panel LEDs: ZAM (red) = the write lock, open; DSK (green) = access, dark now
    Card* zrd = m.find_card("ZRD-1V");
    CHECK(zrd && !(zrd->panel_leds() & 1u) && zrd->panel_led_color(0) == 'r' && zrd->panel_led_color(1) == 'g');
    CHECK(zrd && !(zrd->panel_leds() & 2u));
    m.type_text("DIR\r");
    bool dsk = false;
    for (int i = 0; i < 200 && !dsk; i++) {
        m.run_seconds(0.005);
        dsk = zrd && (zrd->panel_leds() & 2u);
    }
    CHECK(dsk);
    CHECK(run_until_text(m, "WM1Z    .HLP", 5.0));
    // the write mode changes while running (configurator): read only blocks writes
    CHECK(zrd && m.live_param(zrd, "mode") && !m.live_param(zrd, "chips"));
    CHECK(m.set_live_param(zrd, "mode", "readonly", err));
    nlohmann::json* zc = m.card_config(zrd);
    CHECK(zc && zc->value("mode", std::string()) == "readonly");
    m.bus.io_write(0x61, 0x00);
    m.bus.io_write(0x62, 0x00);
    m.bus.io_write(0x63, 0x00);
    uint8_t before = m.bus.io_read(0x60);
    m.bus.io_write(0x61, 0x00);
    m.bus.io_write(0x60, (uint8_t)~before);   // blocked
    m.bus.io_write(0x61, 0x00);
    CHECK_EQ(m.bus.io_read(0x60), before);
    CHECK(!m.set_live_param(zrd, "mode", "fast", err));
    CHECK(m.set_live_param(zrd, "mode", "rw", err));    // tests: rw becomes nosave
    zrd_lock(m, zrd);
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
    // MikroBIOS takes a key on the STROBE pulse and waits until STROBE is released (no ACK):
    // JPR-1V has no 7474 latch, several keys in a row must arrive
    m.run_seconds(0.2);
    m.type_text("D 0\r");
    CHECK(run_until_text(m, "D 0\n", 3.0));
    // the faithful mode of the keyboard switches while running and goes into the description
    CHECK(m.live_param(m.cpu, "keyboard.strict"));
    CHECK(m.set_live_param(m.cpu, "keyboard.strict", true, err));
    CHECK(m.card_config(m.cpu)->at("keyboard").value("strict", false));
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

    // TURBO and BIOS are live parameters (ZDR buttons): the card and the description change
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    CHECK(m.live_param(m.cpu, "turbo") && m.live_param(m.cpu, "bios") && !m.live_param(m.cpu, "osc_hz"));
    CHECK(m.set_live_param(m.cpu, "turbo", true, err) && m.cpu->cpu_turbo());
    CHECK(m.card_config(m.cpu)->value("turbo", false));
    uint8_t bank = m.peek(0x0000);   // BOOT: the EEPROM is at 0000
    std::string other = m.card_config(m.cpu)->value("bios", std::string("fitted")) == "fitted" ? "open" : "fitted";
    CHECK(m.set_live_param(m.cpu, "bios", other, err));
    CHECK(m.peek(0x0000) != bank || m.peek(0x0001) != bank);   // the other bank (MikroBIOS / ECMON)
    CHECK(!m.set_live_param(m.cpu, "bios", "half", err));
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
    d->render(px.data(), 0xFF000000u);
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

// CGA-1V wait states: LDIR into and from the card, T-states per byte against ZRDBENCH on a real SAPI-1 V
// (PoP port 2026-10-08: 4 MHz 23.61 / 22.44, 2 MHz 22.75 / 21.27; memory to memory 21.18).
static double cga_ldir(bool turbo, bool write)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    CHECK(m.set_live_param(m.cpu, "turbo", turbo, err));
    m.run_seconds(0.2);
    m.bus.io_write(0x63, 0xC0);   // RAM-1V: MAP1 = MAP2 = H, CGA-1V at C000
    const uint8_t prog[] = { 0xF3, 0x21, 0x00, (uint8_t)(write ? 0x90 : 0xC0), 0x11, 0x00, (uint8_t)(write ? 0xC0 : 0x90),
                             0x01, 0x00, 0x30, 0xED, 0xB0, 0x76 };   // DI, LD HL / DE, LD BC,3000h, LDIR, HALT
    for (size_t i = 0; i < sizeof(prog); i++) m.poke((uint16_t)(0x8000 + i), prog[i]);
    CpuRegs r;
    m.cpu->get_regs(r);
    r.pc = 0x8000;
    m.cpu->set_regs(r);
    uint64_t t0 = m.bus.now;
    for (int i = 0; i < 100000; i++) {
        m.run_cycles(50);
        m.cpu->get_regs(r);
        if (r.halted) break;
    }
    CHECK(r.halted);
    double w = (double)(m.bus.now - t0) / m.bus.cpu_step / 0x3000 - 21.0;   // LDIR is 21 T without waits
    printf("CGA-1V: LDIR %s at %d MHz, %.2f wait T per byte\n", write ? "into the card" : "from the card", turbo ? 4 : 2, w);
    return w;
}

// wait T per byte on the real card (ZRDBENCH minus its 21.18 without waits)
static void cga_waits(void)
{
    double w4 = cga_ldir(true, true), r4 = cga_ldir(true, false);
    double w2 = cga_ldir(false, true), r2 = cga_ldir(false, false);
    CHECK(fabs(w4 - 2.43) < 0.15);
    CHECK(fabs(r4 - 1.26) < 0.15);
    CHECK(fabs(w2 - 1.57) < 0.4);   // the model gives about 1.27 here (docs/desky/CGA-1V.md)
    CHECK(fabs(r2 - 0.09) < 0.15);
}

int main(void)
{
    test_init();
    cga_waits();
    zrd_rev11();
    turbo();
    cpm();
    mikrobios();
    map_register();
    cga_vbi();
    cga_pictures();
    return TEST_RESULT();
}
