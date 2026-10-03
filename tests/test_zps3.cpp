// ZPS-3 (JPR-1A, RAM-1A, AND-1A, DSM-1, RPD-1, Consul 259.11): the original
// boot ROM MON V1.0-C1 and Perina's MOR48A boot CP/M 2.2/P from the 8" disk,
// DIR, SAVE and ERA work; with a terminal on DSM-1 the console goes there.
#include <string.h>
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static const char* TEST_DISK = "work/test/zps3_cpm48a.img";

static json machine_cfg(const char* rom, bool terminal)
{
    std::string text;
    CHECK(file_read_text(data_path("machines/zps/ZPS-3.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"]) {
        std::string t = c.value("type", std::string());
        if (t == "RPD-1") c["drive_a"] = { { "image", TEST_DISK }, { "write_protect", false } };
        if (t == "JPR-1A" && rom) c["rom"] = rom;
        if (t == "DSM-1") c["connected"] = terminal;
    }
    return cfg;
}

static void fresh_disk(void)
{
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path("media/rpd-1/CPM22P_48A_Perina.img"), img));
    CHECK_EQ((int)img.size(), 256256);
    CHECK(file_write_all(data_path(TEST_DISK), img.data(), img.size()));
}

static void mon_c1(void)
{
    fresh_disk();
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(nullptr, false), err));
    CHECK(run_until_text(m, "JPR-1A 48K MON V1.0-C1", 2.0));
    CHECK(run_until_text(m, "CPM V2.2 V/P1", 5.0));
    CHECK(run_until_text(m, "\nA>", 2.0));
    CHECK_EQ(m.bus.mem_peek(0x0000), 0xC3);   // BOOT off: CP/M page zero in RAM (JMP WBOOT)
    m.run_seconds(0.3);
    m.type_text("DIR S*.*\r");
    CHECK(run_until_text(m, "A> SYSGEN   COM", 5.0));
    m.run_seconds(0.3);
    m.type_text("SAVE 4 TEST.COM\r");
    m.run_seconds(3.0);
    m.type_text("DIR T*.*\r");
    CHECK(run_until_text(m, "A> TEST     COM", 5.0));
    m.run_seconds(0.3);
    m.type_text("ERA TEST.COM\r");
    m.run_seconds(3.0);
    m.type_text("DIR T*.*\r");
    CHECK(run_until_text(m, "NO FILE", 5.0));
    printf("%s\n", screen_text(m).c_str());
}

static void mor48a(void)
{
    fresh_disk();
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg("roms/jpr-1a/MOR48A.bin", false), err));
    CHECK(run_until_text(m, "SAPI-1 ZPS3-48K", 2.0));
    CHECK(run_until_text(m, "CPM V2.2 V/P1", 5.0));
    CHECK(run_until_text(m, "\nA>", 2.0));
}

// DCD on DSM-1 (terminal connected): MON V1.0-C1 uses the serial console
static void terminal(void)
{
    fresh_disk();
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(nullptr, true), err));
    m.run_seconds(3.0);
    std::string out;
    CHECK(!m.serials.empty());
    if (!m.serials.empty()) m.serials[0]->host_receive(out, 1 << 16);
    printf("serial: %s\n", out.c_str());
    CHECK(out.find("JPR-1A 48K MON V1.0-C1") != std::string::npos);
    CHECK(!screen_contains(m, "JPR-1A 48K MON"));
}

int main(void)
{
    test_init();
    mon_c1();
    mor48a();
    terminal();
    return TEST_RESULT();
}
