// CRM-1: the 4 KB battery backed RAM, its jumpers (8 KB block, 1 KB parts of CS1-CS4), SA1,
// the RDY wait states and the content kept in a file across power off.
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static json machine(const json& crm, bool fast = false)
{
    json cfg = { { "cards", json::array() } };
    json cpu = { { "slot", 1 }, { "type", "JPR-1ZI" } };
    if (fast) { cpu["cpu_mhz"] = 4; cpu["bus_mhz"] = 4; }
    cfg["cards"].push_back(cpu);
    json c = crm;
    c["slot"] = 2;
    c["type"] = "CRM-1";
    cfg["cards"].push_back(c);
    return cfg;
}

static void test_default(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine({ { "battery", false } }), err));
    for (int i = 0; i < 4; i++) m.poke((uint16_t)(0xE000 + i * 0x400), (uint8_t)(0x11 * (i + 1)));
    m.poke(0xEFFF, 0xA5);
    for (int i = 0; i < 4; i++) CHECK_EQ(m.peek((uint16_t)(0xE000 + i * 0x400)), 0x11 * (i + 1));
    CHECK_EQ(m.peek(0xEFFF), 0xA5);
    CHECK_EQ(m.peek(0xF000), 0xFF);   // the upper half of the block: no chip selected
    CHECK_EQ(m.peek(0xDFFF), 0xFF);
    // RDY after each access: 590 ns = 1 wait state at 2 MHz
    m.bus.wait = 0;
    m.bus.mem_read(0xE123);
    CHECK_EQ(m.bus.wait, 1);
    m.bus.wait = 0;
    m.bus.mem_read(0xF123);
    CHECK_EQ(m.bus.wait, 0);
}

static void test_jumpers(void)
{
    Machine m;
    std::string err;
    // block 2000h, CS1 at +1C00h, CS2 at +0000h, CS3 and CS4 not fitted, SA1 on
    CHECK(m.load_json(machine({ { "battery", false }, { "base", "2000h" }, { "kb1", "1C00h" }, { "kb2", "0000h" },
                                { "kb3", "none" }, { "kb4", "none" }, { "write_protect", true } }), err));
    m.poke(0x2000, 0x42);        // CS2
    m.poke(0x3C00, 0x43);        // CS1: write protected
    m.poke(0x2400, 0x44);        // nothing there
    CHECK_EQ(m.peek(0x2000), 0x42);
    CHECK_EQ(m.peek(0x3C00), 0x00);
    CHECK_EQ(m.peek(0x2400), 0xFF);
    // bad jumpers
    Machine b;
    CHECK(!b.load_json(machine({ { "base", "3000h" } }), err));
    CHECK(!b.load_json(machine({ { "kb1", "0500h" } }), err));
}

static void test_wait_4mhz(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine({ { "battery", false } }, true), err));
    m.bus.wait = 0;
    m.bus.mem_write(0xE000, 1);
    CHECK_EQ(m.bus.wait, 2);
    Machine z;
    CHECK(z.load_json(machine({ { "battery", false }, { "rdy_ns", 0 } }, true), err));
    z.bus.wait = 0;
    z.bus.mem_read(0xE000);
    CHECK_EQ(z.bus.wait, 0);
}

static void test_battery(void)
{
    battery_files_enable(true);
    std::string file = "work/test/crm1_test.bin";
    file_remove(data_path(file));
    json crm = { { "file", file } };
    std::string err;
    {
        Machine m;
        CHECK(m.load_json(machine(crm), err));
        m.poke(0xE7FF, 0x5A);
    }   // power off: flushed
    {
        Machine m;
        CHECK(m.load_json(machine(crm), err));
        CHECK_EQ(m.peek(0xE7FF), 0x5A);
    }
    crm["battery"] = false;
    {
        Machine m;
        CHECK(m.load_json(machine(crm), err));
        CHECK_EQ(m.peek(0xE7FF), 0x00);
    }
    file_remove(data_path(file));
    battery_files_enable(false);
}

int main(void)
{
    test_init();
    test_default();
    test_jumpers();
    test_wait_4mhz();
    test_battery();
    return TEST_RESULT();
}
