// Address decoders driven by the real PROM dumps: JPR-1ZI D3C/D6B, DMR-1 D2C, AND-1Z D1.
#include "core/machine.h"
#include "core/util.h"
#include "cards/jpr1zi.h"
#include "test_util.h"

using nlohmann::json;

static json base_machine(void)
{
    json m;
    m["name"] = "decoder test";
    m["cards"] = json::array();
    m["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" },
                           { "rom", "roms/mikromon/MikroMon1_3_f000.txt" }, { "decoder", "2k" } });
    return m;
}

static bool ram_at(Machine& m, uint16_t addr)
{
    // RAM is present when a written value reads back
    m.poke(addr, 0x5A);
    if (m.bus.mem_read(addr) != 0x5A) return false;
    m.poke(addr, 0xA5);
    return m.bus.mem_read(addr) == 0xA5;
}

static void test_jpr_boot_rom(void)
{
    Machine m;
    std::string err;
    json cfg = base_machine();
    cfg["cards"].push_back({ { "slot", 2 }, { "type", "DMR-1" }, { "ram_config", 0 } });
    CHECK(m.load_json(cfg, err));
    Jpr1zi* cpu = (Jpr1zi*)m.cpu;
    CHECK(cpu->boot);
    // MikroMon starts with DI (F3h) at 0000h; ROM visible 0000-07FF only (2k decoder)
    CHECK_EQ(m.peek(0x0000), 0xF3);
    m.poke(0x0000, 0x11);              // writes go to the RAM underneath
    CHECK_EQ(m.peek(0x0000), 0xF3);
    m.poke(0x0800, 0x22);
    CHECK_EQ(m.peek(0x0800), 0x22);    // above the ROM window: RAM
    m.bus.io_write(0x00, 0);           // OUT (00h): BOOT off
    CHECK(!cpu->boot);
    CHECK_EQ(m.peek(0x0000), 0x11);
    m.reset();
    CHECK(cpu->boot);
}

static void test_jpr_io(void)
{
    Machine m;
    std::string err;
    json cfg = base_machine();
    cfg["cards"][0]["keyboard"] = { { "type", "EKL-1" } };
    CHECK(m.load_json(cfg, err));
    // EKL-1 idle: STROBE high on P0-IN0, ports 01-03 answered by the CPU card
    CHECK_EQ(m.bus.io_read(0x01) & 1, 1);
    CHECK_EQ(m.bus.io_read(0x04), 0xFF);   // nothing there: open bus
    CHECK_EQ(m.bus.conflicts, 0);
}

static void test_dmr_configs(void)
{
    // expected RAM ranges per JP3 configuration (docs/desky/DMR-1.md)
    struct Probe { uint16_t addr; bool present[8]; };
    const Probe probes[] = {
        { 0x0800, { 1, 1, 1, 0, 0, 0, 0, 0 } },
        { 0x3FFF, { 1, 1, 1, 0, 0, 0, 0, 0 } },
        { 0x4000, { 1, 1, 1, 0, 1, 1, 1, 0 } },
        { 0xEFFF, { 1, 1, 1, 0, 1, 1, 1, 0 } },
        { 0xF000, { 1, 1, 0, 0, 1, 1, 0, 0 } },
        { 0xF7FF, { 1, 1, 0, 0, 1, 1, 0, 0 } },
        { 0xF800, { 1, 0, 0, 0, 1, 0, 0, 0 } },
        { 0xFFFF, { 1, 0, 0, 0, 1, 0, 0, 0 } },
    };
    for (int c = 0; c < 8; c++) {
        Machine m;
        std::string err;
        json cfg = base_machine();
        cfg["cards"].push_back({ { "slot", 2 }, { "type", "DMR-1" }, { "ram_config", c } });
        CHECK(m.load_json(cfg, err));
        for (const Probe& p : probes) {
            bool got = ram_at(m, p.addr);
            if (got != p.present[c])
                fprintf(stderr, "DMR-1 config %d addr %04X: expected %d got %d\n", c, p.addr, p.present[c], got);
            CHECK(got == p.present[c]);
        }
    }
}

static void test_and1z_window(void)
{
    Machine m;
    std::string err;
    json cfg = base_machine();
    cfg["cards"].push_back({ { "slot", 2 }, { "type", "AND-1Z-89" } });
    CHECK(m.load_json(cfg, err));
    CHECK(!ram_at(m, 0xF7FF));
    CHECK(ram_at(m, 0xF800));
    CHECK(ram_at(m, 0xFBFF));
    CHECK(ram_at(m, 0xFC00));
    CHECK(ram_at(m, 0xFFFF));
    m.poke(0xF800, 'A');
    m.poke(0xF800 + 64, 'B');
    std::vector<std::string> rows = m.display->text_rows();
    CHECK_EQ((int)rows.size(), 20);
    CHECK(rows[0][0] == 'A');
    CHECK(rows[1][0] == 'B');
}

int main(void)
{
    test_init();
    test_jpr_boot_rom();
    test_jpr_io();
    test_dmr_configs();
    test_and1z_window();
    return TEST_RESULT();
}
