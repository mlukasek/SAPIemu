// MPH-1V sound and joystick card (machines/sapi1v.sapi, ports 50h-57h): the 8253 with its
// clock and cascade, the interrupt latches, the YM3812 timers as AdLib detection sees them,
// a tone and the waveform select enable, the joystick port.
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "core/bus.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static const uint8_t P = 0x50;

static bool load(Machine& m)
{
    std::string text, err;
    CHECK(file_read_text(data_path("machines/sapi1v.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    bool found = false;
    for (json& c : cfg["cards"]) {
        if (c.value("type", std::string()) == "IDE-1") c.erase("ide");   // no disk file needed here
        if (c.value("type", std::string()) == "MPH-1V") found = true;
    }
    CHECK(found);
    if (!m.load_json(cfg, err)) {
        fprintf(stderr, "%s\n", err.c_str());
        return false;
    }
    return true;
}

static void ym(Machine& m, uint8_t reg, uint8_t v)
{
    m.bus.io_write(P + 6, reg);
    m.bus.io_write(P + 7, v);
}

// 82C54: CLK2 = 55930.4 Hz, mode 2 with 56 -> 998.8 Hz on OUT2, each rising edge sets F2
// (the way RAT PLAYER and y1stdemo time their music, MPH-test bit layout)
static void timers(void)
{
    Machine m;
    if (!load(m)) return;
    m.run_seconds(0.05);
    m.bus.io_write(P + 3, 0xB4);   // counter 2, LSB+MSB, mode 2
    // 82C54 read-back of the status: OUT high, NULL COUNT (no count yet), RW 3, mode 2, binary
    m.bus.io_write(P + 3, 0xE8);   // read-back, status only, counter 2
    CHECK_EQ(m.bus.io_read(P + 2), 0xF4);
    m.bus.io_write(P + 2, 56);
    m.bus.io_write(P + 2, 0);
    m.bus.io_write(P + 4, 0x20);   // GATE2, no interrupt yet
    m.run_seconds(0.0005);
    // status and count together: the status first, then the count LSB and MSB
    m.bus.io_write(P + 3, 0xC8);
    CHECK_EQ(m.bus.io_read(P + 2), 0xB4);   // the count is in the counting element now
    uint8_t c_lo = m.bus.io_read(P + 2), c_hi = m.bus.io_read(P + 2);
    CHECK(c_hi == 0 && c_lo >= 1 && c_lo <= 56);
    m.bus.io_write(P + 5, 0x80);   // clear F2
    int n = 0;
    for (int i = 0; i < 2000; i++) {   // 0.1 s in 50 us steps
        m.run_seconds(0.00005);
        if (m.bus.io_read(P + 3) & 0x80) {
            n++;
            m.bus.io_write(P + 5, 0x80);
        }
    }
    printf("F2: %d in 0.1 s\n", n);
    CHECK(n >= 99 && n <= 101);
    CHECK((m.bus.irq_lines() & IRQ_INT0) == 0);   // not enabled
    m.run_seconds(0.002);
    m.bus.io_write(P + 4, 0xA0);   // F2 interrupt enabled
    CHECK((m.bus.irq_lines() & IRQ_INT0) != 0);
    m.bus.io_write(P + 5, 0x80);   // IACK
    CHECK((m.bus.irq_lines() & IRQ_INT0) == 0);
    // counter 1 counts OUT0 (cascade): counter 0 at 998.8 Hz, counter 1 mode 2 with 10 ->
    // OUT1 low 1 ms of every 10 ms, F1 at 99.9 Hz
    m.bus.io_write(P + 3, 0x34);
    m.bus.io_write(P + 0, 56);
    m.bus.io_write(P + 0, 0);
    m.bus.io_write(P + 3, 0x74);
    m.bus.io_write(P + 1, 10);
    m.bus.io_write(P + 1, 0);
    m.bus.io_write(P + 4, 0x18);   // GATE0, GATE1
    m.run_seconds(0.02);
    m.bus.io_write(P + 5, 0x40);
    int low = 0, f1 = 0;
    for (int i = 0; i < 4000; i++) {   // 0.2 s
        m.run_seconds(0.00005);
        uint8_t st = m.bus.io_read(P + 3);
        if (!(st & 0x10)) low++;
        if (st & 0x40) {
            f1++;
            m.bus.io_write(P + 5, 0x40);
        }
    }
    printf("OUT1 low: %d of 4000, F1: %d\n", low, f1);
    CHECK(low >= 370 && low <= 430);
    CHECK(f1 >= 19 && f1 <= 21);
    // counter latch command: the count read back is frozen
    m.bus.io_write(P + 3, 0x00);
    m.run_seconds(0.001);
    uint8_t lo = m.bus.io_read(P + 0), hi = m.bus.io_read(P + 0);
    CHECK(hi == 0 && lo >= 1 && lo <= 56);
    // /RES clears IEN: the gates go low and the counters stop
    m.reset();
    m.bus.io_write(P + 5, 0xC0);
    m.run_seconds(0.01);
    CHECK((m.bus.io_read(P + 3) & 0xC0) == 0);
    CHECK((m.bus.irq_lines() & IRQ_INT0) == 0);
}

// The usual AdLib detection: timers reset, status 00h; timer 1 at FFh, status C0h after 80 us
static void adlib_detect(void)
{
    Machine m;
    if (!load(m)) return;
    m.run_seconds(0.05);
    ym(m, 0x04, 0x60);
    ym(m, 0x04, 0x80);
    uint8_t s1 = m.bus.io_read(P + 6);
    ym(m, 0x02, 0xFF);
    ym(m, 0x04, 0x21);
    m.run_seconds(0.0002);
    uint8_t s2 = m.bus.io_read(P + 6);
    printf("YM status %02X %02X\n", s1, s2);
    CHECK_EQ(s1 & 0xE0, 0x00);
    CHECK_EQ(s2 & 0xE0, 0xC0);
    // the IRQ through IEN D0, read in STATUS D0 (SPLAY waits for it)
    CHECK(m.bus.io_read(P + 3) & 0x01);
    CHECK((m.bus.irq_lines() & IRQ_INT0) == 0);
    m.bus.io_write(P + 4, 0x01);
    CHECK((m.bus.irq_lines() & IRQ_INT0) != 0);
    ym(m, 0x04, 0x80);
    CHECK((m.bus.irq_lines() & IRQ_INT0) == 0);
    // timer 2 counts in 320 us steps: 0 -> 256 * 320 us = 81.9 ms
    ym(m, 0x04, 0x60);
    ym(m, 0x04, 0x80);
    ym(m, 0x03, 0x00);
    ym(m, 0x04, 0x42);   // start timer 2, timer 1 masked
    m.run_seconds(0.080);
    CHECK_EQ(m.bus.io_read(P + 6) & 0xE0, 0x00);
    m.run_seconds(0.004);
    CHECK_EQ(m.bus.io_read(P + 6) & 0xE0, 0xA0);
}

// One operator of channel 0 at 440 Hz: the frequency from the zero crossings, the level, and
// the half sine waveform only with WSE (reg 01h bit 5).
static void tone(void)
{
    Machine m;
    if (!load(m)) return;
    CHECK_EQ(m.bus.audio_rate, 49716);
    m.run_seconds(0.05);
    m.bus.audio_cap = 49716 * 4;
    ym(m, 0x20, 0x01); ym(m, 0x23, 0x01);   // MULT 1
    ym(m, 0x40, 0x3F); ym(m, 0x43, 0x00);   // modulator silent, carrier full
    ym(m, 0x60, 0xF0); ym(m, 0x63, 0xF0);   // fast attack
    ym(m, 0x80, 0x00); ym(m, 0x83, 0x00);
    ym(m, 0xC0, 0x01);                      // additive
    ym(m, 0xE3, 0x01);                      // half sine, ignored without WSE
    ym(m, 0xA0, 0x44);                      // F-number 244h, block 4: 440.0 Hz
    ym(m, 0xB0, 0x32);
    m.run_seconds(0.1);
    m.bus.audio.clear();
    m.run_seconds(1.0);
    std::vector<int16_t> a = m.bus.audio;
    CHECK(a.size() >= 49700 && a.size() <= 49730);
    int cross = 0, mn = 0, mx = 0;
    double sum = 0;
    for (size_t i = 1; i < a.size(); i++) {
        if (a[i - 1] < 0 && a[i] >= 0) cross++;
        if (a[i] < mn) mn = a[i];
        if (a[i] > mx) mx = a[i];
        sum += (double)a[i] * a[i];
    }
    double rms = sqrt(sum / (double)a.size());
    printf("tone: %d crossings, rms %.0f, min %d, max %d\n", cross, rms, mn, mx);
    CHECK(cross >= 439 && cross <= 441);
    CHECK(rms > 1000);
    CHECK(mn < -mx / 2);   // a full sine
    // WSE on: the half sine has no negative half (only the slow drift of its DC through C8 47k)
    ym(m, 0x01, 0x20);
    m.run_seconds(0.05);
    m.bus.audio.clear();
    m.run_seconds(0.2);
    mn = mx = 0;
    for (int16_t s : m.bus.audio) {
        if (s < mn) mn = s;
        if (s > mx) mx = s;
    }
    printf("half sine: min %d, max %d\n", mn, mx);
    CHECK(mx > 1000 && mn > -mx / 4);
    // nobody listens: no samples kept, a recording gets them anyway
    m.bus.audio_cap = 0;
    m.bus.audio.clear();
    m.bus.audio_record(true);
    m.run_seconds(0.1);
    m.bus.audio_record(false);
    CHECK(m.bus.audio.empty());
    CHECK(m.bus.audio_rec.size() >= 4960 && m.bus.audio_rec.size() <= 4980);
}

// The quadrature lines seen while the card sends `steps` of movement (low bits of the port).
static std::vector<int> quad_seq(Machine& m, int port, uint8_t mask)
{
    std::vector<int> seq;
    int last = -1;
    for (int i = 0; i < 80; i++) {   // 20 ms in 0.25 ms steps (one step per ms)
        m.run_seconds(0.00025);
        int v = m.bus.io_read(port) & mask;
        if (v != last) seq.push_back(v);
        last = v;
    }
    return seq;
}

// K4 / K2: joystick, Atari ST and Amiga mouse (quadrature), nothing; set while running
static void controllers(void)
{
    Machine m;
    if (!load(m)) return;
    Card* c = m.find_card("MPH-1V");
    CHECK(c != nullptr);
    if (!c) return;
    // default: K4 joystick, K2 Atari ST mouse
    CHECK(m.bus.mouse_connected);
    CHECK_EQ(m.bus.io_read(P + 4), 0xFF);
    m.bus.joystick[0] = JOY_UP | JOY_FIRE1;
    CHECK_EQ(m.bus.io_read(P + 4), 0xED);
    m.bus.joystick[0] = 0;
    m.run_seconds(0.01);
    uint8_t idle = m.bus.io_read(P + 5);
    CHECK_EQ(idle & 0xF0, 0xF0);   // buttons up, D7 D6 = 1
    // Atari ST: +x -> XA (D0) leads XB (D1): 00 01 11 10 00
    int x0 = idle & 0x03;
    m.bus.mouse_dx = 4;
    std::vector<int> sx = quad_seq(m, P + 5, 0x03);
    CHECK(m.bus.mouse_dx == 0);
    CHECK_EQ((int)sx.size(), 5);
    if (sx.size() == 5 && x0 == 0) {
        CHECK_EQ(sx[1], 0x01);
        CHECK_EQ(sx[2], 0x03);
        CHECK_EQ(sx[3], 0x02);
        CHECK_EQ(sx[4], 0x00);
    }
    // -y (up): the phases backwards, YB (D2) leads YA (D3): 00 04 0C 08 00
    m.bus.mouse_dy = -4;
    std::vector<int> sy = quad_seq(m, P + 5, 0x0C);
    CHECK_EQ((int)sy.size(), 5);
    if (sy.size() == 5) {
        CHECK_EQ(sy[1], 0x04);
        CHECK_EQ(sy[2], 0x0C);
        CHECK_EQ(sy[3], 0x08);
        CHECK_EQ(sy[4], 0x00);
    }
    // buttons: left = pin 6 (D4), right = pin 9 (D5), active low
    m.bus.mouse_buttons = MOUSE_LEFT;
    CHECK_EQ(m.bus.io_read(P + 5) & 0x30, 0x20);
    m.bus.mouse_buttons = MOUSE_RIGHT;
    CHECK_EQ(m.bus.io_read(P + 5) & 0x30, 0x10);
    m.bus.mouse_buttons = 0;
    // a huge movement is limited to 250 ms of steps
    m.bus.mouse_dx = 100000;
    m.run_seconds(0.002);
    CHECK(m.bus.mouse_dx <= 250);
    m.bus.mouse_dx = 0;
    // Amiga: H (pin 2, D0) and HQ (pin 4, D2) are the X wheel
    std::string err;
    CHECK(m.live_param(c, "k2"));
    CHECK(m.set_live_param(c, "k2", "mouse_amiga", err));
    uint8_t a0 = m.bus.io_read(P + 5);
    m.bus.mouse_dx = 1;
    m.run_seconds(0.003);
    uint8_t a1 = m.bus.io_read(P + 5);
    CHECK_EQ((a0 ^ a1) & 0x0F, ((a0 & 0x05) == 0x00 || (a0 & 0x05) == 0x05) ? 0x01 : 0x04);   // one X line changed
    // nothing on K2, a joystick on K4 and K2: the second gets the second host controller
    CHECK(m.set_live_param(c, "k2", "none", err));
    CHECK(!m.bus.mouse_connected);
    CHECK_EQ(m.bus.io_read(P + 5), 0xFF);
    CHECK(m.set_live_param(c, "k2", "joystick", err));
    m.bus.joystick[1] = JOY_LEFT;
    CHECK_EQ(m.bus.io_read(P + 5), 0xF7);
    CHECK_EQ(m.bus.io_read(P + 4), 0xFF);
    CHECK(m.set_live_param(c, "k4", "none", err));
    CHECK_EQ(m.bus.io_read(P + 5), 0xFF);   // now the only joystick: the first controller (idle)
    nlohmann::json* cc = m.card_config(c);
    CHECK(cc && cc->value("k4", std::string()) == "none" && cc->value("k2", std::string()) == "joystick");
    CHECK(!m.set_live_param(c, "k2", "trackball", err));
}

// CP/M from the ZRD-1V: a program in Intel HEX loaded at 0100h and saved like on the real machine
static bool cpm_boot(Machine& m)
{
    if (!load(m)) return false;
    CHECK(run_until_text(m, "Zadej 0-3", 3.0));
    m.run_seconds(0.3);
    m.type_text("1");
    CHECK(run_until_text(m, "\nA>", 10.0));
    m.run_seconds(0.3);
    return true;
}

static bool cpm_save_hex(Machine& m, const std::string& path, const char* name, int pages)
{
    std::string text, err;
    if (!file_read_text(path, text)) return false;
    CHECK(m.load_ihex_text(text, err));
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "SAVE %d %s", pages, name);
    m.type_text(cmd);
    m.run_seconds(1.0);
    CHECK(run_until_text(m, "\nA>", 10.0));
    return true;
}

static double rms_of(const std::vector<int16_t>& a)
{
    double sum = 0;
    for (int16_t s : a) sum += (double)s * s;
    return a.empty() ? 0 : sqrt(sum / (double)a.size());
}

// Board sources outside the repository: ..\SAPI-karty\MPH-1V (since 2026-10-06), else ..\MPH-1V
static std::string board_file(const char* rel)
{
    std::string p = data_path(std::string("../SAPI-karty/MPH-1V/") + rel);
    if (file_exists(p)) return p;
    return data_path(std::string("../MPH-1V/") + rel);
}

// Libor's VGM player with a tune from ..\SAPI-karty\MPH-1V (not in the repository)
static void splay(void)
{
    std::string tune = board_file("SPLAY/bubble.hex");
    if (!file_exists(tune)) {
        printf("splay: %s not found, skipped\n", tune.c_str());
        return;
    }
    Machine m;
    if (!cpm_boot(m)) return;
    CHECK(cpm_save_hex(m, data_path("media/mph-1v/splay.hex"), "SPLAY.COM", 4));
    CHECK(cpm_save_hex(m, tune, "BUBBLE.VGM", 17));
    m.bus.audio_cap = 49716 * 10;
    m.type_text("SPLAY BUBBLE.VGM");
    m.run_seconds(3.0);
    printf("%s\n", screen_text(m).c_str());
    double r = rms_of(m.bus.audio);
    printf("splay: %zu samples, rms %.0f\n", m.bus.audio.size(), r);
    CHECK(r > 300);
}


// RAT PLAYER with a RAD tune from ..\SAPI-karty\MPH-1V (not in the repository), copied to the RAM disk
// like over a serial line: the player checks the 8253 (50 Hz on counter 2, F2) and plays.
static void rat(void)
{
    std::string tune = board_file("RATplayer/smjoop.rad");
    if (!file_exists(tune)) {
        printf("rat: %s not found, skipped\n", tune.c_str());
        return;
    }
    Machine m;
    if (!cpm_boot(m)) return;
    CHECK(cpm_save_hex(m, data_path("media/mph-1v/rat_0_99_93.txt"), "RAT.COM", 18));
    std::string err;
    int bytes = 0;
    CHECK(m.load_binary(tune, 0x0100, err, &bytes));
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "SAVE %d SMJOOP.RAD\r", (bytes + 255) / 256);
    m.type_text(cmd);
    m.run_seconds(1.0);
    CHECK(run_until_text(m, "\nA>", 10.0));
    m.bus.audio_cap = 49716 * 10;
    m.type_text("RAT SMJOOP.RAD\r");
    m.run_seconds(4.0);
    std::string scr = screen_text(m);
    printf("%s\n", scr.c_str());
    CHECK(scr.find("chyba") == std::string::npos);
    double r = rms_of(m.bus.audio);
    printf("rat: %zu samples, rms %.0f\n", m.bus.audio.size(), r);
    CHECK(r > 300);
}

int main(void)
{
    test_init();
    timers();
    adlib_detect();
    tone();
    controllers();
    splay();
    rat();
    return TEST_RESULT();
}
