// ZPS-4 (JPR-1Z, RAM-1Z, AND-1Z, DSM-1, RPD-1, RMD-1): Perina's CP/M 2.2/P
// (MOR62Z) boots from the 8" disk, RTEST initialises the RAM disk E: and a
// file saved there is listed; ELIS 61k CP/M boots with both ELIS ROMs and
// PIP copies a file to the RAM disk C:.
#include <string.h>
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static json machine_cfg(const char* file, const char* disk, const char* test_disk, const char* rom, const char* fill,
                        bool latch_mod = true)
{
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path(disk), img));
    CHECK(file_write_all(data_path(test_disk), img.data(), img.size()));
    std::string text;
    CHECK(file_read_text(data_path(file), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"]) {
        std::string t = c.value("type", std::string());
        if (t == "RPD-1") c["drive_a"] = { { "image", test_disk }, { "write_protect", false } };
        if (t == "JPR-1Z" && rom) c["rom"] = rom;
        if (t == "JPR-1Z") c["keyboard"] = { { "type", "Consul 262.3" }, { "latch_mod", latch_mod } };
        if (t == "RMD-1" && fill) c["power_on"] = fill;
    }
    return cfg;
}

static void perina(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg("machines/zps/ZPS-4.sapi", "media/rpd-1/CPM22P_62Z_Perina.img",
                                  "work/test/zps4_cpm62z.img", nullptr, nullptr), err));
    CHECK(run_until_text(m, "SAPI-1 ZPS4-62K", 2.0));
    CHECK(run_until_text(m, "CPM V2.2 V/P1", 5.0));
    CHECK(run_until_text(m, "\nA>", 2.0));
    m.run_seconds(0.3);
    m.type_text("RTEST\r");
    CHECK(run_until_text(m, "RAM DISK READY", 90.0));
    m.run_seconds(1.0);
    m.type_text("E:\r");
    m.run_seconds(1.0);
    m.type_text("SAVE 8 PROG.COM\r");
    m.run_seconds(2.0);
    m.type_text("A:\r");
    m.run_seconds(1.0);
    m.type_text("DIR E:\r");
    CHECK(run_until_text(m, "E> PROG     COM", 5.0));
    printf("%s\n", screen_text(m).c_str());
}

static void elis(const char* rom, const char* banner, bool latch_mod, bool use_keyboard)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg("machines/zps/ZPS-4_ELIS.sapi", "media/rpd-1/ELIS_61K.img", "work/test/zps4_elis.img",
                                  rom, "e5", latch_mod), err));
    CHECK(run_until_text(m, banner, 3.0));
    CHECK(run_until_text(m, "C: RAM-DISK Rmd-1", 5.0));
    CHECK(run_until_text(m, "\nA>", 2.0));
    if (!use_keyboard) return;
    m.run_seconds(0.3);
    m.type_text("DIR\r");
    CHECK(run_until_text(m, "INIRMD5  COM", 5.0));
    m.run_seconds(3.0);   // the rest of the listing
    m.type_text("PIP C:=A:STAT.COM\r");
    m.run_seconds(8.0);
    m.type_text("C:STAT\r");
    CHECK(run_until_text(m, "C: R/W, SPACE:", 10.0));
    printf("%s\n", screen_text(m).c_str());
}

int main(void)
{
    test_init();
    perina();
    elis("roms/jpr-1z/ELIS_V7.01.bin", "ELIS 62k MON-", true, true);
    // v3.00 boots, but it reads every key twice (pulse strobe) or for ever (latched):
    // its keyboard interface is not understood yet (docs/software/CPM_ZPS-4.md)
    elis("roms/jpr-1z/ELIS_V3.00.bin", "V3.00 *JME", false, false);
    return TEST_RESULT();
}
