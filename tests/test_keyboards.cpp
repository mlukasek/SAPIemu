// Keyboards on connector X2: EKL-1, Consul 259.11, Consul 262.3, ANK-1 at port
// level, and MikroMon 1.3 working with every parallel keyboard. ANK-1M2 under
// MIKROBASIC (ZPS-1) with its joystick connector: joystick, mouse, plugged in live.
#include <memory>
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "input/keyboards.h"
#include "test_util.h"

static const uint32_t HZ = 2000000;   // 1 ms = 2000 cycles

static KeyboardDevice* make(const char* name)
{
    std::string err;
    KeyboardDevice* k = keyboard_from_preset(name, HZ, err);
    if (!k) fprintf(stderr, "%s\n", err.c_str());
    return k;
}

static void test_ekl1(void)
{
    std::unique_ptr<KeyboardDevice> k(make("EKL-1"));
    CHECK(k != nullptr);
    if (!k) return;
    CHECK_EQ(k->p0_in() & 1, 1);
    k->type_char(0xFD);   // ý -> KOI-8 cs D9h with swapped case = F9h
    k->type_char('a');
    k->advance(0);
    CHECK_EQ(k->p0_in() & 1, 0);
    CHECK_EQ(k->p1_in(), (uint8_t)~0xF9);
    k->advance(100000);
    CHECK_EQ(k->p0_in() & 1, 0);   // latched until acknowledged
    k->p0_out(0x03);
    CHECK_EQ(k->p0_in() & 1, 1);
    k->advance(100100);
    CHECK_EQ(k->p0_in() & 1, 1);   // ACK still high: next key waits
    k->p0_out(0x02);
    k->advance(100200);
    CHECK_EQ(k->p0_in() & 1, 1);   // gap between characters
    k->advance(102100);
    CHECK_EQ(k->p0_in() & 1, 0);
    CHECK_EQ(k->p1_in(), (uint8_t)~0x61);
    // cursor keys in WordStar codes
    CHECK(k->host_key(HK_UP, 0));
    CHECK(!k->host_key(HK_F6, 0));
    CHECK(!k->host_key(HK_KP5, 0));   // keypad digits come as text
}

static void test_consul25911(void)
{
    std::unique_ptr<KeyboardDevice> k(make("Consul 259.11"));
    CHECK(k != nullptr);
    if (!k) return;
    CHECK(std::string(k->type_name()) == "Consul 259.11");
    k->type_char('a');
    k->type_char(0x17E);   // ž: no such key in KOI-7
    CHECK_EQ(k->pending(), 1);
    k->advance(0);
    CHECK_EQ(k->p0_in() & 1, 0);
    CHECK_EQ(k->p1_in(), (uint8_t)(~0x61 | 0x80));   // P1-IN7 = /CLR inactive
    k->p0_out(0x03);
    k->p0_out(0x02);
    CHECK(k->host_key(HK_PAUSE, 0));   // CLEAR
    k->advance(10000);
    CHECK_EQ(k->p1_in() & 0x80, 0);
    k->advance(10000 + 200001);        // held 100 ms
    CHECK_EQ(k->p1_in() & 0x80, 0x80);

    std::unique_ptr<KeyboardDevice> k8(make("Consul 259.11 (KOI-8 cs)"));
    CHECK(k8 != nullptr);
    if (!k8) return;
    k8->type_char(0x17E);   // ž -> DAh with swapped case = FAh, bit 7 on P1-IN7
    k8->advance(0);
    CHECK_EQ(k8->p1_in(), (uint8_t)~0xFA);
    CHECK(!k8->host_key(HK_PAUSE, 0));   // no CLEAR line in this variant
}

static void test_consul2623(void)
{
    // original keyboard: 1 ms strobe pulse, no acknowledge
    std::unique_ptr<KeyboardDevice> k(make("Consul 262.3 (bez 7474)"));
    CHECK(k != nullptr);
    if (!k) return;
    k->type_char('A');
    CHECK(k->host_key(HK_UP, 0));
    k->advance(0);
    CHECK_EQ(k->p0_in() & 1, 0);
    CHECK_EQ(k->p1_in(), (uint8_t)~0x41);
    k->advance(1999);
    CHECK_EQ(k->p0_in() & 1, 0);
    k->advance(2000);
    CHECK_EQ(k->p0_in() & 1, 1);       // pulse over without any ACK
    CHECK_EQ(k->p1_in(), (uint8_t)~0x41);   // data stay on the lines
    k->advance(2000 + 40000);          // 20 ms between characters
    CHECK_EQ(k->p0_in() & 1, 0);
    CHECK_EQ(k->p1_in(), (uint8_t)~0xC1);   // cursor up

    // with the 7474 modification the strobe waits for the acknowledge
    std::unique_ptr<KeyboardDevice> m(make("Consul 262.3"));
    CHECK(m != nullptr);
    if (!m) return;
    CHECK(m->host_key(HK_KP7, 0));
    CHECK(m->host_key(HK_F1, HM_SHIFT));
    CHECK(m->host_key(HK_F6, 0));
    CHECK(!m->host_key(HK_PAUSE, 0));
    m->advance(0);
    CHECK_EQ(m->p1_in(), (uint8_t)~0xB7);
    m->advance(50000);
    CHECK_EQ(m->p0_in() & 1, 0);
    m->p0_out(0x03);
    CHECK_EQ(m->p0_in() & 1, 1);
    m->p0_out(0x02);
    m->advance(52001);
    CHECK_EQ(m->p1_in(), (uint8_t)~0x81);   // shift ROL
    m->p0_out(0x03);
    m->p0_out(0x02);
    m->advance(54002);
    CHECK_EQ(m->p1_in(), (uint8_t)~0xD1);   // key 62
}

static void test_ank1(void)
{
    std::unique_ptr<KeyboardDevice> k(make("ANK-1"));
    CHECK(k != nullptr);
    if (!k) return;
    k->p0_out(0x1F);   // no row selected
    k->type_char('w');   // lower case typed as the W key
    k->type_char('?');   // SHIFT + 2
    k->advance(0);
    CHECK_EQ(k->p0_in(), 0xFF);
    k->p0_out(0x17);     // row D3: 9 O L SP 2 W S Z
    CHECK_EQ(k->p0_in(), 0xFB);
    k->p0_out(0x0F);     // row D4
    CHECK_EQ(k->p0_in(), 0xFF);
    k->advance(80000);   // released after 40 ms
    k->p0_out(0x00);
    CHECK_EQ(k->p0_in(), 0xFF);
    k->advance(160000);  // next key
    k->p0_out(0x0F);
    CHECK_EQ(k->p0_in(), 0xFE);   // SHIFT: row D4, column 0
    k->p0_out(0x17);
    CHECK_EQ(k->p0_in(), 0xF7);   // 2: row D3, column 3
    k->p0_out(0x07);
    CHECK_EQ(k->p0_in(), 0xF6);   // both rows scanned at once
    // INT and T keys on P1-IN, LEDs
    CHECK_EQ(k->p1_in(), 0xFF);
    CHECK(k->host_key(HK_F6, 0));
    CHECK_EQ(k->p1_in(), 0x7F);
    k->p0_out(0xBF);
    CHECK(k->status() == "SHIFT ERR");
    k->p0_out(0x1F);
    CHECK(k->status().empty());
    CHECK(!k->host_key(HK_UP, 0));
}

// ANK-1M2: MIKROBASIC types as with ANK-1; the joystick connector on P1-IN D5..D0 (active low:
// D0 down, D1 up, D2 right, D3 left, D4 fire, D5 fire 2), a mouse plugged in while running.
static void test_ank1m2(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-1.sapi", err));
    CHECK(run_until_text(m, "READY", 2.0));
    CHECK(m.set_keyboard("ANK-1M2", err));
    KeyboardDevice* k = m.keyboard();
    CHECK(k && std::string(k->type_name()) == "ANK-1M2");
    if (!k) return;
    m.type_text("PRINT 12*34+5\r");
    CHECK(run_until_text(m, "413", 5.0));
    CHECK_EQ(k->joyport(), JOY_DEV_JOYSTICK);
    CHECK_EQ(k->p1_in(), 0xFF);
    m.bus.joystick[0] = JOY_UP | JOY_FIRE1;
    CHECK_EQ(k->p1_in(), 0xED);
    m.bus.joystick[0] = JOY_DOWN | JOY_LEFT | JOY_FIRE2;
    CHECK_EQ(k->p1_in(), 0xD6);
    CHECK(k->host_key(HK_F6, 0));   // INT on D7 as with ANK-1
    CHECK_EQ(k->p1_in(), 0x56);
    m.bus.joystick[0] = 0;
    m.run_seconds(0.2);
    CHECK_EQ(k->p1_in(), 0xFF);
    CHECK(!m.bus.mouse_connected);
    // an Atari ST mouse plugged in while running: 3 steps right -> phase 3 (XA 0, XB 1)
    CHECK(m.cpu->set_live_param("keyboard.joyport", nlohmann::json("mouse_atari"), err));
    m.run_seconds(0.01);
    CHECK(m.bus.mouse_connected & MOUSE_PORT_KEYBOARD);
    CHECK_EQ(k->p1_in(), 0xF0);
    m.bus.mouse_dx = 3;
    m.run_seconds(0.02);
    CHECK_EQ(m.bus.mouse_dx, 0);
    CHECK_EQ(k->p1_in(), 0xF2);
    m.bus.mouse_buttons = MOUSE_LEFT;
    CHECK_EQ(k->p1_in(), 0xE2);
    m.bus.mouse_buttons = 0;
    CHECK(m.cpu->set_live_param("keyboard.joyport", nlohmann::json("none"), err));
    m.run_seconds(0.01);
    CHECK(!m.bus.mouse_connected);
    CHECK_EQ(k->p1_in(), 0xFF);
    CHECK(!m.cpu->set_live_param("keyboard.joyport", nlohmann::json("lightpen"), err));
    // back to a mouse, then the plain ANK-1: unplugged with the keyboard
    CHECK(m.cpu->set_live_param("keyboard.joyport", nlohmann::json("mouse_amiga"), err));
    m.run_seconds(0.01);
    CHECK(m.bus.mouse_connected);
    CHECK(m.set_keyboard("ANK-1", err));
    CHECK(!m.bus.mouse_connected);
    CHECK(!m.cpu->set_live_param("keyboard.joyport", nlohmann::json("joystick"), err));   // ANK-1 has none
    m.bus.joystick[0] = JOY_UP;
    CHECK_EQ(m.keyboard()->p1_in(), 0xFF);
    m.bus.joystick[0] = 0;
    // ANK-1M5 (the predecessor) is the same keyboard
    std::unique_ptr<KeyboardDevice> m5(keyboard_from_config(nlohmann::json::parse(R"({"type":"ANK-1M5"})"), HZ, err));
    CHECK(m5 && std::string(m5->type_name()) == "ANK-1M2" && m5->joyport() == JOY_DEV_JOYSTICK);
}

// MikroMon reads every parallel keyboard with the same handshake.
// ANK-1 keys held on the PC (port level): a symbol presses SHIFT first and its key only after
// the program has read the SHIFT row, the key goes up before SHIFT; the game mode holds keys together.
static void test_ank1_held_ports(void)
{
    std::unique_ptr<KeyboardDevice> k(make("ANK-1"));
    CHECK(k != nullptr);
    if (!k) return;
    CHECK(k->held_keys());
    k->advance(0);
    CHECK(k->hold_key(1, ',', HK_NONE, false));   // SHIFT + N
    k->advance(10);
    k->p0_out(0x1D);                 // row D1 (C F R 4 N J U 7) before the SHIFT row was read
    CHECK_EQ(k->p0_in(), 0xFF);      // no N without SHIFT
    k->p0_out(0x0F);                 // row D4
    CHECK_EQ(k->p0_in(), 0xFE);      // SHIFT
    k->p0_out(0x1D);
    CHECK_EQ(k->p0_in(), 0xEF);      // N (column 4)
    k->release_key(1);
    k->advance(100000);              // after min_hold: N up, SHIFT down until N's row is read
    k->p0_out(0x0F);
    CHECK_EQ(k->p0_in(), 0xFE);
    k->p0_out(0x1D);
    CHECK_EQ(k->p0_in(), 0xFF);
    k->advance(100010);
    k->p0_out(0x0F);
    CHECK_EQ(k->p0_in(), 0xFF);      // SHIFT up too
    k->p0_out(0x0D);
    CHECK_EQ(k->p0_in(), 0xFF);      // rows D4 and D1 read without keys
    // a tap shorter than the scan: down until the program has read it
    CHECK(k->hold_key(2, 'w', HK_NONE, false));
    k->release_key(2);
    k->advance(300000);              // the release phase of the comma is over
    k->advance(400000);              // W goes down
    k->p0_out(0x17);
    CHECK_EQ(k->p0_in(), 0xFB);      // W still down: not read yet
    k->advance(500000);
    CHECK_EQ(k->p0_in(), 0xFF);
    // the game mode: SHIFT and O held together, at once
    CHECK(k->hold_key(3, 0, HK_SHIFT, true));
    CHECK(k->hold_key(4, 'o', HK_NONE, true));
    CHECK(!k->hold_key(5, ',', HK_NONE, true));   // symbols only through SHIFT + key
    k->advance(600000);
    k->p0_out(0x07);                 // rows D4 and D3
    CHECK_EQ(k->p0_in(), 0xBE);      // SHIFT (D4 b0) and O (D3 b6)
    k->release_all();
    k->advance(700000);
    CHECK_EQ(k->p0_in(), 0xFF);
}

// MIKROBASIC on ZPS-1 typed with held keys as a person types: keys overlapping, held 30 to
// 90 ms, at odd moments; then a key held for 2 s gives one character (no host autorepeat).
static void test_ank1_held_typing(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-1.sapi", err));
    CHECK(run_until_text(m, "READY", 2.0));
    KeyboardDevice* k = m.keyboard();
    CHECK(k && k->held_keys());
    if (!k) return;
    m.run_seconds(0.5);
    const char* text = "PRINT \"A,B;C?\",7\r";
    unsigned seed = 12345;
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; text[i]; i++) {
            seed = seed * 1103515245u + 12345u;
            double hold = 0.030 + (seed >> 16) % 60 / 1000.0;
            int id = (unsigned char)text[i] + 256 * (i & 1);   // repeated letters: other ids
            bool ok = text[i] == '\r' ? k->hold_key(id, 0, HK_ENTER, false) : k->hold_key(id, (unsigned char)text[i], HK_NONE, false);
            CHECK(ok);
            m.run_seconds(hold * 0.6);
            if (text[i + 1] && text[i + 1] != '\r') {   // the next key goes down before this one is up
                int nid = (unsigned char)text[i + 1] + 256 * ((i + 1) & 1);
                k->hold_key(nid, (unsigned char)text[i + 1], HK_NONE, false);
                m.run_seconds(hold * 0.4);
                k->release_key(id);
                m.run_seconds(0.020 + (seed >> 20) % 40 / 1000.0);
                k->release_key(nid);
                i++;
                m.run_seconds(0.030);
            } else {
                m.run_seconds(hold * 0.4);
                k->release_key(id);
                m.run_seconds(0.030);
            }
        }
        m.run_seconds(1.0);
    }
    std::string s = screen_text(m);
    printf("%s", s.c_str());
    size_t n = 0, pos = 0;
    while ((pos = s.find(">PRINT \"A,B;C?\",7\nA,B;C?", pos)) != std::string::npos) { n++; pos++; }
    CHECK_EQ(n, 3);   // typed and run three times, nothing lost or doubled
    CHECK(s.find("WHAT?") == std::string::npos && s.find('N', s.find("READY")) == s.find("NT"));
    // held for 2 s: one character
    k->hold_key(1, 'Q', HK_NONE, false);
    m.run_seconds(2.0);
    k->release_key(1);
    m.run_seconds(0.5);
    std::string t = screen_text(m);
    CHECK(t.find("QQ") == std::string::npos);
    CHECK(t.find(">Q") != std::string::npos);
}

// MIKROBIOS 1.02 on JPR-1S (BGP-1S graphics, no text to read): a line typed with held keys
// (overlapping, symbols with SHIFT) gives the same picture as the same line typed through the queue.
static bool mikrobios_screen(bool held, std::vector<uint32_t>& px)
{
    Machine m;
    std::string err;
    if (!m.load_file("machines/jpr1s_bgp1s.sapi", err)) return false;
    m.run_seconds(3.0);
    const char* text = "D 4400,44;FF?";
    KeyboardDevice* k = m.keyboard();
    if (!held) {
        m.type_text(text);
    } else {
        unsigned seed = 777;
        for (int i = 0; text[i]; i++) {
            seed = seed * 1103515245u + 12345u;
            int id = (unsigned char)text[i] + 256 * (i & 1);   // a repeated character: another PC key
            k->hold_key(id, (unsigned char)text[i], HK_NONE, false);
            m.run_seconds(0.025 + (seed >> 16) % 50 / 1000.0);
            if (text[i + 1]) {   // the next key goes down before this one is up
                int nid = (unsigned char)text[i + 1] + 256 * ((i + 1) & 1);
                k->hold_key(nid, (unsigned char)text[i + 1], HK_NONE, false);
                m.run_seconds(0.015);
                k->release_key(id);
                m.run_seconds(0.02 + (seed >> 22) % 30 / 1000.0);
                k->release_key(nid);
                i++;
            } else {
                k->release_key(id);
            }
            m.run_seconds(0.03);
        }
    }
    m.run_seconds(8.0 - m.bus.now / (double)m.bus.cpu_hz);   // the same moment (cursor)
    DisplayDevice* d = m.display;
    if (!d) return false;
    px.assign((size_t)d->screen_width() * d->screen_height(), 0);
    d->render(px.data(), 0xFF000000u);
    return true;
}

static void test_ank1_held_mikrobios(void)
{
    std::vector<uint32_t> a, b;
    CHECK(mikrobios_screen(false, a));
    CHECK(mikrobios_screen(true, b));
    CHECK(a == b);
}

static void test_mikromon(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/mikromon.sapi", err));
    if (!err.empty()) fprintf(stderr, "%s\n", err.c_str());
    CHECK(run_until_text(m, "MikroMon 2kB V1.3", 2.0));
    const char* names[] = { "Consul 262.3", "Consul 262.3 (bez 7474)", "Consul 259.11",
                            "Consul 259.11 (KOI-8 cs)", "EKL-1" };
    const char* cmds[] = { "M F100\r", "M F200\r", "M F300\r", "M F400\r", "M F500\r" };
    const char* expect[] = { ">F100:", ">F200:", ">F300:", ">F400:", ">F500:" };
    for (int i = 0; i < 5; i++) {
        CHECK(m.set_keyboard(names[i], err));
        CHECK(std::string(m.keyboard()->type_name()) == names[i]);
        m.type_text(cmds[i]);
        bool ok = run_until_text(m, expect[i], 3.0);
        if (!ok) fprintf(stderr, "no answer with %s\n", names[i]);
        CHECK(ok);
        m.type_text("\x1b");
        m.run_seconds(0.3);
    }
    CHECK(!m.set_keyboard("Consul 999", err));
    printf("%s", screen_text(m).c_str());
}

int main(void)
{
    test_init();
    test_ekl1();
    test_consul25911();
    test_consul2623();
    test_ank1();
    test_ank1m2();
    test_ank1_held_ports();
    test_ank1_held_typing();
    test_ank1_held_mikrobios();
    test_mikromon();
    return TEST_RESULT();
}
