// Libor Lasota's V boards (machines/sapi1v.sapi): JPR-1V boots ECMON (bank 1)
// and CP/M from the ZRD-1V RAM disk on the AND-1V, bank 0 is MikroBIOS 3.0;
// the RAM-1V MAP register drives MAP1 and an AND-1V set to MAP1 = L hides;
// DSM-1V gives two serial channels.
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "cards/zrmd1.h"
#include "chips/mhb1012.h"
#include "core/bus.h"
#include "core/headless.h"
#include "core/log.h"
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

// JPR-1V: a read no card answers returns the last byte of the CPU data bus (measured on the author's board
// 2026-10-11, tools/hwbench/openbus.asm: IN A,(9Ah) 9Ah, IN A,(C) 78h, LD A,(C123h) in the RAM-1V hole C1h,
// LD A,(HL) there 7Eh). CP/M on IDE-1 without the card names the disk "XYXY..." (ports 58h, 59h).
static void open_bus(void)
{
    static const uint8_t prog[] = {
        0xF3,                    // di
        0xDB, 0x9A,              // in a,(9Ah)
        0x32, 0x00, 0x90,        // ld (9000h),a
        0x01, 0x9A, 0x5A,        // ld bc,5A9Ah
        0xED, 0x78,              // in a,(c)
        0x32, 0x01, 0x90,        // ld (9001h),a
        0x3E, 0x40, 0xD3, 0x63,  // MAP1 = H, MAP2 = L: the 16 KB hole at C000, no CGA-1V
        0x3A, 0x23, 0xC1,        // ld a,(0C123h)
        0x32, 0x02, 0x90,        // ld (9002h),a
        0x21, 0xF0, 0xD0,        // ld hl,0D0F0h
        0x7E,                    // ld a,(hl)
        0x32, 0x03, 0x90,        // ld (9003h),a
        0xAF, 0xD3, 0x63,        // MAP off
        0x76,                    // halt
    };
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    m.run_seconds(0.2);
    for (size_t i = 0; i < sizeof(prog); i++) m.poke((uint16_t)(0x8000 + i), prog[i]);
    CpuRegs r;
    m.cpu->get_regs(r);
    r.pc = 0x8000;
    m.cpu->set_regs(r);
    for (int i = 0; i < 1000; i++) {
        m.run_cycles(50);
        m.cpu->get_regs(r);
        if (r.halted) break;
    }
    CHECK(r.halted);
    CHECK_EQ(m.bus.mem_peek(0x9000), 0x9A);
    CHECK_EQ(m.bus.mem_peek(0x9001), 0x78);
    CHECK_EQ(m.bus.mem_peek(0x9002), 0xC1);
    CHECK_EQ(m.bus.mem_peek(0x9003), 0x7E);
}

// ZRMD-1: /RES clears only the latch of port 63h (A16-A21); the counters (61h) and 62h keep the address
// (measured on the author's board 2026-10-11, tools/hwbench/zrmd.asm).
static void zrmd1_reset(void)
{
    json cfg = machine_cfg();
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "ZRD-1V") {
            c = json{ { "slot", c["slot"] }, { "type", "ZRMD-1" }, { "image", c["image"] }, { "mode", "nosave" } };
        }
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    m.run_seconds(0.1);
    uint8_t want[4], got[4];
    m.bus.io_write(0x61, 0x10);
    m.bus.io_write(0x62, 0x41);
    m.bus.io_write(0x63, 0x00);
    for (uint8_t& b : want) b = m.bus.io_read(0x60);   // the data at 00:41:10
    m.bus.io_write(0x61, 0x10);
    m.bus.io_write(0x62, 0x41);
    m.bus.io_write(0x63, 0x03);
    m.reset();
    for (uint8_t& b : got) b = m.bus.io_read(0x60);
    CHECK(memcmp(want, got, 4) == 0);
}

static void cpm(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    CHECK(run_until_text(m, "JPR-1Z 62K MON CZ", 3.0));
    CHECK(run_until_text(m, "Zadej 0-3", 3.0));
    m.run_seconds(0.3);
    m.type_text("3");   // HDD (IDE-1): without the RPD-1 the variant 1 waits for it for ever (open bus, JPR-1V)
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
    m.type_text("3");   // HDD (IDE-1): without the RPD-1 the variant 1 waits for it for ever (open bus, JPR-1V)
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

// CGA-1V wait states: LDIR into and from the card, T-states per byte on the author's SAPI-1 V (2026-10-11,
// the ZRDBENCH loops timed by the 82C54 and by the analyzer, tools/hwbench/zrdtimer.asm: 4 MHz 23.608 / 22.757,
// 2 MHz 22.757 / 21.272; memory to memory 21.176; docs/zmeny-podle-hw.md).
// card_hi: the high byte of the card memory, len_hi: of the block length, map: the RAM-1V MAP register
static double card_ldir(bool turbo, bool write, uint8_t card_hi, uint8_t len_hi, uint8_t map, const char* card)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    CHECK(m.set_live_param(m.cpu, "turbo", turbo, err));
    m.run_seconds(0.2);
    m.bus.io_write(0x63, map);
    const uint8_t prog[] = { 0xF3, 0x21, 0x00, (uint8_t)(write ? 0x90 : card_hi), 0x11, 0x00, (uint8_t)(write ? card_hi : 0x90),
                             0x01, 0x00, len_hi, 0xED, 0xB0, 0x76 };   // DI, LD HL / DE, LD BC, LDIR, HALT
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
    double w = (double)(m.bus.now - t0) / m.bus.cpu_step / (len_hi * 256) - 21.0;   // LDIR is 21 T without waits
    printf("%s: LDIR %s at %d MHz, %.2f wait T per byte\n", card, write ? "into the card" : "from the card", turbo ? 4 : 2, w);
    m.bus.io_write(0x63, 0x00);
    return w;
}

static double cga_ldir(bool turbo, bool write)
{
    return card_ldir(turbo, write, 0xC0, 0x30, 0xC0, "CGA-1V");   // MAP1 = MAP2 = H: CGA-1V at C000
}

// wait T per byte on the real card (the measured T per byte minus 21.176 without waits)
static void cga_waits(void)
{
    double w4 = cga_ldir(true, true), r4 = cga_ldir(true, false);
    double w2 = cga_ldir(false, true), r2 = cga_ldir(false, false);
    CHECK(fabs(w4 - 2.43) < 0.1);
    CHECK(fabs(r4 - 1.58) < 0.1);
    CHECK(fabs(w2 - 1.58) < 0.1);
    CHECK(fabs(r2 - 0.10) < 0.1);
}

// AND-1V: the same synchroniser with a window every character (397 ns), open half of it. Measured on the
// author's card 2026-10-11 (tools/hwbench/and1v.asm, 256 B LDIR into / from the card, 4 MHz 6134 / 5730,
// 2 MHz 5873 / 5562 CPU clocks between the markers): the model gives the same within 2 clocks.
static void vrambench(bool turbo, double w_and, double r_and);

static void and1v_waits(void)
{
    double w4 = card_ldir(true, true, 0xF8, 0x08, 0x00, "AND-1V"), r4 = card_ldir(true, false, 0xF8, 0x08, 0x00, "AND-1V");
    double w2 = card_ldir(false, true, 0xF8, 0x08, 0x00, "AND-1V"), r2 = card_ldir(false, false, 0xF8, 0x08, 0x00, "AND-1V");
    CHECK(w4 > r4 && r4 > 0.5 && w4 < 4.0);
    CHECK(w2 >= r2 && w2 > 0.5 && w2 < 3.0);
    vrambench(true, w4, r4);
    vrambench(false, w2, r2);
}

// ZRDBENCH (media/zrd-1v, PoP port) under CP/M against the real SAPI-1 V of the author (2026-10-08):
// T-states per byte or loop turn, calibrated by the 82C54 of MPH-1V. ZRD-1V rev 1.1 as on the author's
// board (rev 2.1 would lock on the long OTIR, which never touches 62h).
static double bench_value(const std::string& s, const char* name)
{
    size_t p = s.find(name);
    CHECK(p != std::string::npos);
    if (p == std::string::npos) return -1.0;
    return atof(s.c_str() + p + strlen(name));
}

// CP/M at the given CPU clock, a program from media/ saved by SAVE and run
static void cpm_program(Machine& m, json cfg, bool turbo, const char* hex, int pages, const char* name, const char* cmd)
{
    std::string err, text;
    CHECK(m.load_json(cfg, err));
    CHECK(m.set_live_param(m.cpu, "turbo", turbo, err));
    CHECK(run_until_text(m, "Zadej 0-3", 6.0));
    m.run_seconds(0.3);
    m.type_text("3");   // HDD (IDE-1): without the RPD-1 the variant 1 waits for it for ever (open bus, JPR-1V)
    CHECK(run_until_text(m, "\nA>", 20.0));
    m.run_seconds(0.3);
    CHECK(file_read_text(data_path(hex), text));
    CHECK(m.load_ihex_text(text, err));
    char line[64];
    snprintf(line, sizeof(line), "SAVE %d %s.COM\r", pages, name);
    m.type_text(line);
    CHECK(run_until_text(m, "\nA>", 10.0));
    m.run_seconds(0.3);
    snprintf(line, sizeof(line), "%s\r", cmd);
    m.type_text(line);
}

static void zrdbench(bool turbo)
{
    json cfg = machine_cfg();
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "ZRD-1V") c["revision"] = "1.1";
    Machine m;
    cpm_program(m, cfg, turbo, "media/zrd-1v/zrdbench.hex", 7, "ZRDBENCH", "ZRDBENCH");
    CHECK(run_until_text(m, "Done.", 30.0));
    std::string s = screen_text(m);
    printf("%s\n", s.c_str());
    struct Row { const char* name; double hw4, hw2, tol2; };
    static const Row rows[] = {
        { "LDIR memory -> memory", 21.17, 21.17, 0.1 },
        { "INIR RAM disk -> memory", 21.14, 21.13, 0.1 },
        { "OTIR memory -> RAM disk", 21.22, 21.22, 0.1 },
        { "loop LD A,(HL) / INC HL", 26.12, 26.12, 0.1 },
        { "loop IN A,(C) from 60h", 25.08, 25.08, 0.1 },
        // the CGA-1V rows as measured 2026-10-11 (ZRDBENCH 2026-10-08 had 22.44 from the card at 4 MHz with
        // ZRD-1V and another card order: the read is on the edge of the window, docs/zmeny-podle-hw.md)
        { "LDIR memory -> CGA-1V", 23.61, 22.76, 0.1 },
        { "LDIR CGA-1V -> memory", 22.76, 21.27, 0.1 },
    };
    double mhz = bench_value(s, "CPU clock:");
    CHECK(fabs(mhz - (turbo ? 4.00 : 1.99)) < 0.015);
    for (const Row& r : rows) {
        double v = bench_value(s, r.name);
        double hw = turbo ? r.hw4 : r.hw2;
        double tol = turbo ? 0.1 : r.tol2;
        if (fabs(v - hw) >= tol) printf("ZRDBENCH %d MHz: %s %.2f, the real SAPI-1 V %.2f\n", turbo ? 4 : 2, r.name, v, hw);
        CHECK(fabs(v - hw) < tol);
    }
}

// VRAMBENCH (media/and-1v): LDIR into and from the AND-1V under CP/M; the card is not measured on the real
// SAPI-1 V yet (docs/desky/AND-1V.md), so only the memory row is held to the hardware and the card rows to
// the model of card_ldir (the loop of 256 B blocks gives the same within 0.15 T).
static void vrambench(bool turbo, double w_and, double r_and)
{
    Machine m;
    cpm_program(m, machine_cfg(), turbo, "media/and-1v/vrambench.hex", 6, "VRAMBENCH", "VRAMBENCH");
    CHECK(run_until_text(m, "Done.", 30.0));
    std::string s = screen_text(m);
    printf("%s\n", s.c_str());
    CHECK(fabs(bench_value(s, "CPU clock:") - (turbo ? 4.00 : 1.99)) < 0.015);
    CHECK(fabs(bench_value(s, "LDIR memory -> memory") - 21.17) < 0.1);
    CHECK(fabs(bench_value(s, "LDIR AND-1V -> memory") - (21.18 + r_and)) < 0.15);
    CHECK(fabs(bench_value(s, "LDIR memory -> AND-1V") - (21.18 + w_and)) < 0.15);
    CHECK(s.find("CP/M") != std::string::npos || s.find("A>") != std::string::npos);   // the screen kept
}

// ZRDTEST (media/zrd-1v) on ZRD-1V rev 2.1: its millisecond loops count CPU clocks (ZRDTEST [MHz]),
// one "ms" is 984 x MHz + 34 T: 0.9925 ms at 4 MHz, 1.001 ms at 2 MHz. The write window of the 74LS123
// (20.0 ms) therefore shows as 20..21 at 4 MHz and 19..20 at 2 MHz; the card locks at the end.
static void zrdtest(bool turbo)
{
    Machine m;
    cpm_program(m, machine_cfg(), turbo, "media/zrd-1v/zrdtest.hex", 6, "ZRDTEST", turbo ? "ZRDTEST" : "ZRDTEST 2");
    CHECK(run_until_text(m, "Konec.", 10.0));
    std::string s = screen_text(m);
    printf("%s\n", s.c_str());
    CHECK(s.find(turbo ? "takt CPU 4 MHz" : "takt CPU 2 MHz") != std::string::npos);
    CHECK(s.find("3000 mV") != std::string::npos);
    CHECK(s.find("IN 61h = DFh") != std::string::npos);
    CHECK(s.find(turbo ? "okno je 20..21 ms" : "okno je 19..20 ms") != std::string::npos);
    Card* zrd = m.find_card("ZRD-1V");
    CHECK(zrd && ((Zrmd1*)zrd)->locked());
}

// Interrupt log: IM 2 with I pointing at the CGA-1V (C000h), the VBI interrupt of the card; nobody answers
// /INTA (FFh), so the vector C0FFh is read from the video memory, which is also reported in the log.
static void int_log_im2(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    m.run_seconds(0.2);
    m.bus.io_write(0x63, 0xC0);   // RAM-1V: MAP1 = MAP2 = H, CGA-1V at C000
    const uint8_t prog[] = { 0xF3, 0xED, 0x5E, 0x3E, 0xC0, 0xED, 0x47, 0xFB, 0x18, 0xFE };   // DI, IM 2, I = C0h, EI, JR $
    for (size_t i = 0; i < sizeof(prog); i++) m.poke((uint16_t)(0x8000 + i), prog[i]);
    m.bus.mem_write(0xC0FF, 0x34);   // the vector in the video RAM: handler 1234h
    m.bus.mem_write(0xC100, 0x12);
    CpuRegs r;
    m.cpu->get_regs(r);
    r.pc = 0x8000;
    m.cpu->set_regs(r);
    m.bus.int_log_enabled = true;
    m.bus.mem_write(0xFFFB, 0x88);   // CGA-1V: IRQ enabled
    for (int i = 0; i < 40 && m.bus.int_log.empty(); i++) m.run_seconds(0.001);
    CHECK(!m.bus.int_log.empty());
    if (m.bus.int_log.empty()) return;
    const IntLogEntry& e = m.bus.int_log[0];
    CHECK_EQ(e.im, 2);
    CHECK_EQ(e.i, 0xC0);
    CHECK_EQ(e.data, 0xFF);
    CHECK_EQ(e.vector, 0xC0FF);
    CHECK_EQ(e.handler, 0x1234);
    CHECK(e.table == "CGA-1V");
    CHECK(e.from.find("CGA-1V") != std::string::npos);
    CHECK(e.pc == 0x8008);
    std::vector<std::string> lines;
    log_recent(lines, 50);
    bool warned = false;
    for (const std::string& l : lines) warned |= l.find("video memory of CGA-1V") != std::string::npos;
    CHECK(warned);
    m.bus.mem_write(0xFFFB, 0x80);
    m.bus.io_write(0x63, 0x00);
}

// ECMON after a reset boots the variant in 003Bh without the menu: a value above 3 is reported in the log
static bool log_has(const char* text)
{
    std::vector<std::string> lines;
    log_recent(lines, 100);
    for (const std::string& l : lines)
        if (l.find(text) != std::string::npos) return true;
    return false;
}

static void ecmon_variant(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    CHECK(run_until_text(m, "Zadej 0-3", 3.0));
    m.run_seconds(0.3);
    m.type_text("3");   // HDD (IDE-1): without the RPD-1 the variant 1 waits for it for ever (open bus, JPR-1V)
    CHECK(run_until_text(m, "\nA>", 10.0));
    CHECK_EQ(m.bus.mem_peek(0x003B), 3);
    m.reset();
    CHECK(!log_has("boots variant"));
    CHECK(run_until_text(m, "\nA>", 10.0));   // variant 3 again, no menu
    m.bus.mem_write(0x003B, 7);
    m.reset();
    CHECK(log_has("boots variant 7 from 003Bh"));
}

// "power_on_memory": "random": RAM, video RAM and the palette hold random bits like real chips; ECMON
// and CP/M start anyway. The default stays zero.
static int nonzero(Machine& m, uint16_t from, int n)
{
    int k = 0;
    for (int i = 0; i < n; i++) k += m.bus.mem_peek((uint16_t)(from + i)) != 0;
    return k;
}

static void power_on_random(void)
{
    Machine z;
    std::string err;
    CHECK(z.load_json(machine_cfg(), err));
    CHECK_EQ(nonzero(z, 0x8000, 4096), 0);
    json cfg = machine_cfg();
    cfg["power_on_memory"] = "random";
    Machine m;
    CHECK(m.load_json(cfg, err));
    CHECK(nonzero(m, 0x8000, 4096) > 3500);   // RAM-1V
    m.bus.io_write(0x63, 0xC0);
    CHECK(nonzero(m, 0xC000, 4096) > 3500);   // CGA-1V
    m.bus.io_write(0x63, 0x00);
    CHECK(run_until_text(m, "Zadej 0-3", 3.0));
    m.run_seconds(0.3);
    m.type_text("3");   // HDD (IDE-1): without the RPD-1 the variant 1 waits for it for ever (open bus, JPR-1V)
    CHECK(run_until_text(m, "\nA>", 10.0));
    cfg["power_on_memory"] = "noise";
    Machine bad;
    CHECK(!bad.load_json(cfg, err));
}

// CGA-1V drawn with the beam: the shown page switched at line 200 tears the picture (VGA line 2y = pixel row y),
// and the switch is listed for the programmer. Frame blending off (it would mix the two pages).
static void run_to_line(Machine& m, int frames_on, int line)
{
    const double line_s = 800.0 / 25.175e6, frame_s = 525.0 * line_s;
    double t = (double)m.bus.now / m.bus.cpu_hz;
    double f = floor(t / frame_s) + frames_on;
    uint64_t target = (uint64_t)((f * frame_s + (line + 0.5) * line_s) * m.bus.cpu_hz);
    if (target > m.bus.now) m.run_cycles(target - m.bus.now);
}

static void cga_beam(void)
{
    json cfg = machine_cfg();
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "CGA-1V") c["frame_blend"] = false;
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    m.run_seconds(0.1);
    m.bus.io_write(0x63, 0xC0);   // RAM-1V: MAP1 = MAP2 = H, CGA-1V at C000
    m.bus.mem_write(0xFFFB, 0x00);   // CPU page A
    for (int i = 0; i < 16000; i++) m.bus.mem_write((uint16_t)(0xC000 + i), 0x00);
    m.bus.mem_write(0xFFFB, 0x02);   // CPU page B
    for (int i = 0; i < 16000; i++) m.bus.mem_write((uint16_t)(0xC000 + i), 0xFF);
    m.bus.mem_write(0xFFFE, 0x1F);   // COLMASK: pixel values 00h (page A) and 1Fh (page B)
    m.bus.mem_write(0xFFFC, 0x00);   // palette from 0: red
    m.bus.mem_write(0xFFFD, 0x3F); m.bus.mem_write(0xFFFD, 0x00); m.bus.mem_write(0xFFFD, 0x00);
    m.bus.mem_write(0xFFFC, 0x1F);   // 1Fh: green
    m.bus.mem_write(0xFFFD, 0x00); m.bus.mem_write(0xFFFD, 0x3F); m.bus.mem_write(0xFFFD, 0x00);
    m.bus.mem_write(0xFFFB, 0x00);   // show page A
    run_to_line(m, 2, 0);
    run_to_line(m, 0, 200);
    m.bus.mem_write(0xFFFB, 0x01);   // show page B at line 200 (pixel row 100)
    run_to_line(m, 1, 100);          // the next frame at row 50
    DisplayDevice* d = m.find_display("CGA-1V");
    CHECK(d != nullptr);
    if (!d) return;
    std::vector<uint32_t> px((size_t)d->screen_width() * d->screen_height());
    d->render(px.data(), 0xFF000000u);
    auto at = [&](int y) { return px[(size_t)y * d->screen_width() + 160] & 0xFFFFFFu; };
    CHECK_EQ(at(10), 0x00FF00u);    // new frame, page B
    CHECK_EQ(at(70), 0xFF0000u);    // the old frame above the switch, page A
    CHECK_EQ(at(150), 0x00FF00u);   // the old frame below the switch, page B
    json desc;
    for (auto& c : m.cards)
        if (c->type == "CGA-1V") c->describe(desc);
    CHECK(desc["mid_frame_switches"].size() >= 1);
    if (!desc["mid_frame_switches"].empty()) {
        json last = desc["mid_frame_switches"].back();
        CHECK(last["line"] == 200);
        CHECK(last["change"].get<std::string>().find("page A -> B") != std::string::npos);
    }
    CHECK(desc["palette_writes_visible"].get<uint64_t>() <= 6);
    m.bus.io_write(0x63, 0x00);
}

// DSM-1V: MR of the UART is held from the reset write (base+3) until a memory read on the bus (/MR); the
// JPR-1V gives no /MR for its own ROM (measured on the real board 2026-10-11, work/hwtest/mr.asm, the line
// carried "CD"): 'A' right after the init and 'B' after a ROM read are lost, 'C' after a RAM read is sent.
static void dsm1v_mr(void)
{
    static const uint8_t prog[] = {
        0xF3,                    // di
        0xD3, 0x13,              // out (13h),a   UART reset: MR held
        0xAF, 0xD3, 0x10,        // xor a / out (10h),a
        0x3E, 0x0D, 0xD3, 0x11,  // ld a,0Dh / out (11h),a   8N1
        0x3E, 0x41, 0xD3, 0x12,  // 'A' (lost)
        0x3A, 0x40, 0x00,        // ld a,(0040h)   ROM: no /MR
        0x3E, 0x42, 0xD3, 0x12,  // 'B' (lost)
        0x3A, 0x00, 0x80,        // ld a,(8000h)   RAM: /MR ends MR
        0x3E, 0x43, 0xD3, 0x12,  // 'C'
        0xDB, 0x11, 0xE6, 0x40, 0x28, 0xFA,   // wait for TBRE
        0x3E, 0x44, 0xD3, 0x12,  // 'D'
        0x18, 0xFE,              // jr $
    };
    std::vector<uint8_t> rom(8192, 0xFF);
    for (size_t b = 0; b < rom.size(); b += 2048) memcpy(&rom[b], prog, sizeof(prog));   // both banks
    std::string err, path = data_path("work/test/dsm1v_mr.bin");
    CHECK(file_write_all(path, rom.data(), rom.size()));
    json cfg = machine_cfg();
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "JPR-1V") c["rom"] = "work/test/dsm1v_mr.bin";
    Machine m;
    CHECK(m.load_json(cfg, err));
    m.run_seconds(0.05);
    std::string got;
    m.serials[0]->host_receive(got, 1 << 16);
    printf("DSM-1V MR: \"%s\"\n", got.c_str());
    CHECK(got == "CD");
}

// The 6402 of the DSM-1V takes the holding register on the next edge of the 16x clock (measured 2026-10-11,
// tools/hwbench/tbre.asm): two writes within one clock period send only the second character; a write while
// a character is in the shift register waits (TBRE 0) and starts when that one ends.
static void uart_clock_grid(void)
{
    std::vector<Mhb1012::TxChar> log;
    Mhb1012 u;
    u.tx_log = &log;
    u.clock_grid = true;
    u.set_baud(4000000, 9600);   // 16x clock: 26.04 cycles
    u.reset(0);
    u.write_data('A', 100);      // the edge is at 105
    u.advance(103, nullptr);
    CHECK(!u.tbre);              // not taken yet
    u.write_data('B', 104);      // overwrites A
    u.advance(110, nullptr);
    CHECK(u.tbre);
    CHECK_EQ((int)log.size(), 1);
    CHECK(log.size() == 1 && log[0].data == 'B' && log[0].start == 105);
    u.write_data('C', 120);      // B is still going out: C waits
    u.advance(200, nullptr);
    CHECK(!u.tbre);
    u.advance(5000, nullptr);
    CHECK(u.tbre);
    CHECK_EQ((int)log.size(), 2);
    if (log.size() == 2) CHECK(log[1].data == 'C' && log[1].start == 105 + (uint64_t)(4000000.0 / 9600 * 10));
    // without the grid (MHB1012 cards): taken at once, both characters go
    std::vector<Mhb1012::TxChar> log2;
    Mhb1012 v;
    v.tx_log = &log2;
    v.set_baud(4000000, 9600);
    v.reset(0);
    v.write_data('A', 100);
    v.advance(103, nullptr);
    v.write_data('B', 104);
    v.advance(20000, nullptr);
    CHECK_EQ((int)log2.size(), 2);
}

int main(void)
{
    test_init();
    uart_clock_grid();
    dsm1v_mr();
    open_bus();
    zrmd1_reset();
    cga_beam();
    power_on_random();
    ecmon_variant();
    int_log_im2();
    cga_waits();
    and1v_waits();
    zrdbench(true);
    zrdbench(false);
    zrdtest(true);
    zrdtest(false);
    zrd_rev11();
    turbo();
    cpm();
    mikrobios();
    map_register();
    cga_vbi();
    cga_pictures();
    return TEST_RESULT();
}
