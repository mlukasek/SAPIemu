// JPR-1S + BGP-1S + ANK-1M2 (Libor Lasota): BGP-1S colours (Colorace, RGBM), its interrupt at
// line 256 cleared by an opcode fetch from the window; MIKROBIOS 1.02 boots; BOOT reads Intel HEX
// bit by bit from the Redukce34KB01 serial line (19200 Bd, 2 stop bits); Manic Miner shows its
// first cavern in the colours of the photo of the real machine; the car game follows the joystick.
#include <string.h>
#include <map>
#include "cards/bgp1s.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static json machine_cfg(void)
{
    return json::parse(R"({ "name": "test", "bus": { "type": "ARB-1" }, "cards": [
        { "slot": 1, "type": "JPR-1S", "keyboard": { "type": "ANK-1M2", "software": "mikos" },
          "k2_adapter": "redukce34kb01" },
        { "slot": 2, "type": "BGP-1S" } ] })");
}

static Bgp1s* find_bgp(Machine& m)
{
    for (auto& c : m.cards)
        if (Bgp1s* b = dynamic_cast<Bgp1s*>(c.get())) return b;
    return nullptr;
}

static std::map<uint32_t, int> colours(Bgp1s* b)
{
    std::vector<uint32_t> px(288 * 256);
    b->render(px.data(), 0xFF000000u);
    std::map<uint32_t, int> n;
    for (uint32_t c : px) n[c]++;
    return n;
}

static void test_colours(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    Bgp1s* b = find_bgp(m);
    CHECK(b != nullptr);
    if (!b) return;
    memset(b->vram, 0, sizeof(b->vram));
    // Colorace: column 0, lines 0 (even) and 1 (odd): attributes 00/10 = cyan, 6 dots each
    b->vram[0] = 0x3F;
    b->vram[64] = 0xBF;
    // lines 2/3: 01/01 = red; lines 4/5: 00/11 = white
    b->vram[128] = 0x7F;
    b->vram[192] = 0x7F;
    b->vram[256] = 0x3F;
    b->vram[320] = 0xFF;
    std::vector<uint32_t> px(288 * 256);
    b->render(px.data(), 0xFF000000u);
    CHECK_EQ(px[0], 0xFF00FFFFu);
    CHECK_EQ(px[288 + 5], 0xFF00FFFFu);
    CHECK_EQ(px[2 * 288], 0xFFFF0000u);
    CHECK_EQ(px[4 * 288], 0xFFFFFFFFu);
    CHECK_EQ(px[6], 0xFF000000u);
    b->vram[0] = 0x01;   // bit 0 = the left dot
    b->render(px.data(), 0xFF000000u);
    CHECK(px[0] != 0xFF000000u && px[1] == 0xFF000000u);
}

static void test_interrupt(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    Bgp1s* b = find_bgp(m);
    if (!b) return;
    // one request a frame (312 lines of 420 dots at 6.5536 MHz = 19.995 ms)
    int rises = 0;
    bool prev = false;
    uint8_t d;
    for (int i = 0; i < 1000; i++) {   // 1 s in 1 ms steps
        m.bus.now += m.bus.cpu_hz / 1000;
        bool on = b->irq_lines() == IRQ_INT0;
        if (on && !prev) rises++;
        if (on) {   // an opcode fetch from the window clears it, a data read does not
            m.bus.pc = 0x1234;
            b->mem_read(0xC030, d);
            CHECK(b->irq_lines() == IRQ_INT0);
            m.bus.pc = 0xC030;
            b->mem_read(0xC030, d);
            CHECK(b->irq_lines() == 0);
            on = false;
        }
        prev = on;
    }
    printf("BGP-1S interrupt: %d requests in 1 s\n", rises);
    CHECK(rises >= 49 && rises <= 51);
}

// The text of an Intel HEX file sent over the adapter while BOOT reads it.
static void test_boot(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(), err));
    m.run_seconds(2.0);
    CHECK(!m.serials.empty());
    if (m.serials.empty()) return;
    std::string hex;
    CHECK(file_read_text(data_path("media/jpr-1s/auto.hex"), hex));
    m.type_text("BOOT\r");
    m.run_seconds(1.0);
    m.serials[0]->host_send((const uint8_t*)hex.data(), hex.size());
    m.run_seconds(5.0);   // 5 KB at 19200 Bd, 11 bits a character: about 3 s
    CHECK(m.serials[0]->host_pending_rx() <= 2);   // CR LF after the end record: BOOT has finished
    Machine ref;   // the same file loaded directly
    CHECK(ref.load_json(machine_cfg(), err));
    CHECK(ref.load_ihex_text(hex, err));
    int diff = 0;
    for (int a = 0x4400; a <= 0x4BAB; a++)
        if (m.peek((uint16_t)a) != ref.peek((uint16_t)a)) diff++;
    printf("BOOT over Redukce34KB01: %d bytes differ\n", diff);
    CHECK_EQ(diff, 0);
}

static void start(Machine& m, const char* hex_file)
{
    std::string err, hex;
    CHECK(m.load_json(machine_cfg(), err));
    m.run_seconds(2.0);
    CHECK(file_read_text(data_path(hex_file), hex));
    CHECK(m.load_ihex_text(hex, err));
    m.type_text("JUMP 4400\r");
}

static void test_manic_miner(void)
{
    Machine m;
    start(m, "media/jpr-1s/manic_miner.hex");
    m.run_seconds(6.0);
    m.type_text("\r");   // start the game
    m.run_seconds(3.0);
    std::map<uint32_t, int> n = colours(find_bgp(m));
    int magenta = n[0xFFFF00FFu], cyan = n[0xFF00FFFFu], white = n[0xFFFFFFFFu];
    printf("Manic Miner: magenta %d, cyan %d, white %d dots\n", magenta, cyan, white);
    CHECK(magenta > 5000);   // walls and floors of "Hlavni sluj"
    CHECK(cyan > 500);       // the air bar
    CHECK(white > 500);      // name of the cavern, lives
}

static void test_car_joystick(void)
{
    // the same start twice: once the joystick is held left (P1-IN D3), the screen differs
    Machine a, b;
    start(a, "media/jpr-1s/auto.hex");
    start(b, "media/jpr-1s/auto.hex");
    a.run_seconds(3.0);
    b.run_seconds(3.0);
    CHECK(memcmp(find_bgp(a)->vram, find_bgp(b)->vram, 16384) == 0);
    b.bus.joystick[0] = JOY_LEFT;
    a.run_seconds(1.0);
    b.run_seconds(1.0);
    CHECK(memcmp(find_bgp(a)->vram, find_bgp(b)->vram, 16384) != 0);
}

int main(void)
{
    test_init();
    test_colours();
    test_interrupt();
    test_boot();
    test_manic_miner();
    test_car_joystick();
    return TEST_RESULT();
}
