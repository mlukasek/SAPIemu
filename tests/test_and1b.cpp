// AND-1B (Libor Lasota): ZPS-1 with AND-1B instead of AND-1 runs MIKROBASIC (window 3800h),
// the VRAM layout equals AND-1 for the visible cells, the font banks give blinking and double
// width, INT1 50/60 Hz pulses at the end of the picture.
#include <string.h>
#include "cards/and1b.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static json zps1_with_and1b(const char* int1)
{
    std::string text;
    CHECK(file_read_text(data_path("machines/zps/ZPS-1.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "AND-1") c = json{ { "slot", c["slot"] }, { "type", "AND-1B" }, { "int1", int1 } };
    return cfg;
}

static And1b* find_and1b(Machine& m)
{
    for (auto& c : m.cards)
        if (And1b* a = dynamic_cast<And1b*>(c.get())) return a;
    return nullptr;
}

// lit VGA pixels in a 14x20 cell
static int lit(const std::vector<uint32_t>& px, int row, int col, uint32_t bg)
{
    int n = 0;
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 14; x++)
            if (px[(size_t)(row * 20 + y) * 640 + And1b::BORDER + col * 14 + x] != bg) n++;
    return n;
}

int main(void)
{
    test_init();
    // the visible cells are stored as on AND-1 (columns 32-39 in the area of rows 24-31)
    for (int row = 0; row < 24; row++)
        for (int col = 0; col < 40; col++) {
            int i = And1b::vram_index((unsigned)(row * 64 + col));
            CHECK(i >= 0 && i < 1024);
            for (int r2 = 0; r2 < 24; r2++)
                for (int c2 = 0; c2 < 40; c2++)
                    if ((r2 != row || c2 != col) && And1b::vram_index((unsigned)(r2 * 64 + c2)) == i) CHECK(false);
        }
    CHECK_EQ(And1b::vram_index(0x28), And1b::vram_index(0x20));   // columns 40-63 alias 32-39

    Machine m;
    std::string err;
    CHECK(m.load_json(zps1_with_and1b("off"), err));
    if (!err.empty()) printf("%s\n", err.c_str());
    And1b* d = find_and1b(m);
    CHECK(d != nullptr);
    if (!d) return TEST_RESULT();
    CHECK(run_until_text(m, "READY", 2.0));
    m.type_text("PRINT 12*34+5\r");
    CHECK(run_until_text(m, "413", 5.0));
    printf("%s\n", screen_text(m).c_str());

    // attributes from the font: row 20, columns 0..5: 'A', blinking 'B', underlined 'C', wide 'S' (2 cells)
    const uint32_t bg = 0xFF000000u;
    std::vector<uint32_t> px(640 * 480);
    m.bus.mem_write(0x3800 + 20 * 64 + 0, 0x01);   // A
    m.bus.mem_write(0x3800 + 20 * 64 + 1, 0x42);   // B blinking
    m.bus.mem_write(0x3800 + 20 * 64 + 2, 0x83);   // C blinking underline
    m.bus.mem_write(0x3800 + 20 * 64 + 3, 0xD3);   // S double width
    m.bus.mem_write(0x3800 + 20 * 64 + 4, 0x01);   // hidden by the wide S
    m.bus.mem_write(0x3800 + 20 * 64 + 5, 0x20);   // space
    d->render(px.data(), bg);
    int a = lit(px, 20, 0, bg);
    CHECK(a > 0);
    CHECK(lit(px, 20, 1, bg) > 0);
    CHECK(lit(px, 20, 3, bg) > 0 && lit(px, 20, 4, bg) > 0);   // the two halves of S
    CHECK_EQ(lit(px, 20, 5, bg), 0);
    int under = lit(px, 20, 2, bg);
    // the blink phase is off in frames 24-31 of 32: B disappears, C loses its underline
    uint64_t fc = (uint64_t)(m.bus.cpu_hz * (399.0 / 12.5875) * 525 / 1e6);
    m.run_seconds((double)((24 - (m.bus.now / fc) % 32 + 32) % 32 + 1) * fc / m.bus.cpu_hz);
    d->render(px.data(), bg);
    CHECK_EQ(lit(px, 20, 1, bg), 0);
    CHECK(lit(px, 20, 2, bg) < under);
    CHECK_EQ(lit(px, 20, 0, bg), a);   // a normal character does not blink

    // INT1: off, 60 Hz (every frame), 50 Hz (5 of 6)
    for (const char* mode : { "off", "60hz", "50hz" }) {
        Machine k;
        CHECK(k.load_json(zps1_with_and1b(mode), err));
        And1b* c = find_and1b(k);
        int pulses = 0;
        bool prev = false;
        for (int i = 0; i < 60 * 1000; i++) {   // 600 ms in 10 us steps
            k.bus.now += k.bus.cpu_hz / 100000;
            bool on = c && c->irq_lines() != 0;
            if (on && !prev) pulses++;
            prev = on;
        }
        printf("INT1 %s: %d pulses in 600 ms\n", mode, pulses);
        if (!strcmp(mode, "off")) CHECK_EQ(pulses, 0);
        if (!strcmp(mode, "60hz")) CHECK(pulses >= 35 && pulses <= 37);
        if (!strcmp(mode, "50hz")) CHECK(pulses >= 29 && pulses <= 31);
    }
    return TEST_RESULT();
}
