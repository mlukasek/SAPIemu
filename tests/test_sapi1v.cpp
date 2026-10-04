// Libor Lasota's V boards (machines/sapi1v.sapi): JPR-1V boots ECMON (bank 1)
// and CP/M from the ZRD-1V RAM disk on the AND-1V, bank 0 is MikroBIOS 3.0;
// the RAM-1V MAP register drives MAP1 and an AND-1V set to MAP1 = L hides;
// DSM-1V gives two serial channels.
#include <string.h>
#include "core/bus.h"
#include "core/headless.h"
#include "core/machine.h"
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

int main(void)
{
    test_init();
    turbo();
    cpm();
    mikrobios();
    map_register();
    return TEST_RESULT();
}
