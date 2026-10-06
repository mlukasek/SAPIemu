// ZPS-3 (JPR-1A, RAM-1A, AND-1A, DSM-1, RPD-1, Consul 259.11): the original
// boot ROM MON V1.0-C1 and Perina's MOR48A boot CP/M 2.2/P from the 8" disk,
// DIR, SAVE and ERA work; with a terminal on DSM-1 the console goes there.
// The machines of machines/zps/ZPS-3_CPM*.sapi: CP/M 48K and 62K with RPD-1 and with
// FDC-1 (8272 + DMA 8257); a RESET with the head on track 0 boots again (the 8272
// reports "ready changed" for all drives after its reset). The original CP/M 64K of Tesla IMA:
// MON V1.0-C3 copies itself to the RAM at F800 and switches the EPROM off, the video at F000.
// The original CP/M 48K (46K) of Tesla IMA with MON V1.0-C1: its system tracks rebuilt by
// MOVCPM48 46 * and SYSGEN come out the same again.
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

// A machine file of machines/zps with drive A on a fresh copy of its disk: boot, DIR, SAVE,
// DIR, ERA; the saved file reaches the image; RESET boots again.
static void cpm_machine(const char* file, const char* banner, const char* disk, const char* test_disk)
{
    printf("--- %s\n", file);
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path(disk), img));
    CHECK(file_write_all(data_path(test_disk), img.data(), img.size()));
    std::string text;
    CHECK(file_read_text(data_path(file), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"]) {
        std::string t = c.value("type", std::string());
        if (t == "RPD-1" || t == "FDC-1") c["drive_a"] = { { "image", test_disk }, { "write_protect", false } };
    }
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, banner, 2.0));
    CHECK(run_until_text(m, "CPM V2.2 V/P1", 8.0));
    CHECK(run_until_text(m, "\nA>", 2.0));
    m.run_seconds(0.3);
    m.type_text("DIR S*.*\r");
    CHECK(run_until_text(m, "A> SYSGEN   COM", 5.0));
    m.run_seconds(0.3);
    for (int i = 0; i < 0x400; i++) m.bus.mem_write((uint16_t)(0x0100 + i), (uint8_t)(i * 7 + 3));
    m.type_text("SAVE 4 TEST.COM\r");
    m.run_seconds(3.0);
    m.type_text("DIR T*.*\r");
    CHECK(run_until_text(m, "A> TEST     COM", 5.0));
    m.flush();
    CHECK(file_read_all(data_path(test_disk), img));
    bool found = false;
    for (size_t off = 2 * 26 * 128; off + 0x400 <= img.size() && !found; off += 128)
        if (img[off] == 3 && img[off + 1] == 10 && img[off + 2] == 17) found = true;   // the data, sector 1 of a block
    CHECK(found);
    m.run_seconds(0.3);
    m.type_text("ERA TEST.COM\r");
    m.run_seconds(3.0);
    m.type_text("DIR T*.*\r");
    CHECK(run_until_text(m, "NO FILE", 5.0));
    // RESET with the head on track 0 or wherever CP/M left it: boots again
    m.reset();
    CHECK(run_until_text(m, "CPM V2.2 V/P1", 8.0));
    CHECK(run_until_text(m, "\nA>", 2.0));
    printf("%s\n", screen_text(m).c_str());
}

// ZPS-3_CPM48K_IMA.sapi: boot, DIR, SAVE, ERA; MOVCPM48 46 * and SYSGEN to B: write the same
// system tracks as those of A:
static void ima48k(void)
{
    printf("--- machines/zps/ZPS-3_CPM48K_IMA.sapi\n");
    const char* disk_a = "work/test/zps3_ima48_a.img";
    const char* disk_b = "work/test/zps3_ima48_b.img";
    std::vector<uint8_t> a, b;
    CHECK(file_read_all(data_path("media/rpd-1/CPM48K_IMA_004.img"), a));
    CHECK(file_write_all(data_path(disk_a), a.data(), a.size()));
    CHECK(file_read_all(data_path("media/rpd-1/CPM48K_IMA_003.img"), b));
    CHECK(file_write_all(data_path(disk_b), b.data(), b.size()));
    CHECK_EQ((int)b[0], 0x40);   // disk 003 has no system
    std::string text;
    CHECK(file_read_text(data_path("machines/zps/ZPS-3_CPM48K_IMA.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "RPD-1") {
            c["drive_a"] = { { "image", disk_a }, { "write_protect", false } };
            c["drive_b"] = { { "image", disk_b }, { "write_protect", false } };
        }
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, "JPR-1A 48K MON V1.0-C1", 2.0));
    CHECK(run_until_text(m, "JPR-1A 46K CPM V2.2", 8.0));
    CHECK(run_until_text(m, "\nA>", 2.0));
    m.run_seconds(0.3);
    m.type_text("SAVE 4 TEST.COM\r");
    m.run_seconds(3.0);
    m.type_text("DIR TEST.COM\r");
    CHECK(run_until_text(m, "A: TEST     COM", 5.0));
    m.run_seconds(0.3);
    m.type_text("ERA TEST.COM\r");
    m.run_seconds(3.0);
    m.type_text("MOVCPM48 46 *\r");
    CHECK(run_until_text(m, "READY FOR \"SYSGEN\"", 15.0));
    m.run_seconds(1.0);
    m.type_text("SYSGEN\r");
    CHECK(run_until_text(m, "SOURCE DRIVE NAME", 5.0));
    m.type_text("\r");
    CHECK(run_until_text(m, "DESTINATION DRIVE NAME", 5.0));
    m.type_text("B");
    CHECK(run_until_text(m, "DESTINATION ON B, THEN TYPE RETURN", 5.0));
    m.type_text("\r");
    CHECK(run_until_text(m, "FUNCTION COMPLETE", 15.0));
    m.flush();
    CHECK(file_read_all(data_path(disk_b), b));
    CHECK(file_read_all(data_path(disk_a), a));
    CHECK(a.size() == b.size() && memcmp(a.data(), b.data(), 2 * 26 * 128) == 0);
    printf("%s\n", screen_text(m).c_str());
}

// ZPS-3_CPM64K_IMA.sapi: boot, the monitor in the RAM, DIR, SAVE, ERA, RESET
static void ima64k(void)
{
    printf("--- machines/zps/ZPS-3_CPM64K_IMA.sapi\n");
    const char* test_disk = "work/test/zps3_ima64.img";
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path("media/rpd-1/CPM64K_IMA_01.img"), img));
    CHECK(file_write_all(data_path(test_disk), img.data(), img.size()));
    std::string text;
    CHECK(file_read_text(data_path("machines/zps/ZPS-3_CPM64K_IMA.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "RPD-1") {
            c["drive_a"] = { { "image", test_disk }, { "write_protect", false } };
            c.erase("drive_b");
        }
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, "JPR-1A 64K V C3", 2.0));
    CHECK(run_until_text(m, "JPR-1A 60K CPM V2.2", 8.0));
    CHECK(run_until_text(m, "\nA>", 2.0));
    CHECK_EQ(m.bus.mem_peek(0x0000), 0xC3);   // BOOT off: page zero in RAM
    CHECK_EQ(m.bus.mem_peek(0xF800), 0xC3);   // the monitor copied to the RAM (JMP COLD)
    m.run_seconds(0.3);
    m.type_text("DIR SYSGEN.COM\r");
    CHECK(run_until_text(m, "A: SYSGEN   COM", 5.0));
    m.run_seconds(0.3);
    m.type_text("SAVE 4 TEST.COM\r");
    m.run_seconds(3.0);
    m.type_text("DIR TEST.COM\r");
    CHECK(run_until_text(m, "A: TEST     COM", 5.0));
    m.run_seconds(0.3);
    m.type_text("ERA TEST.COM\r");
    m.run_seconds(3.0);
    m.type_text("DIR TEST.COM\r");
    CHECK(run_until_text(m, "NO FILE", 5.0));
    m.reset();
    m.run_seconds(5.0);   // the old text stays in the video RAM until the monitor clears it
    CHECK(!screen_contains(m, "NO FILE"));
    CHECK(screen_contains(m, "JPR-1A 60K CPM V2.2"));
    CHECK(screen_contains(m, "\nA>"));
    printf("%s\n", screen_text(m).c_str());
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
    cpm_machine("machines/zps/ZPS-3_CPM48K.sapi", "SAPI-1 ZPS3-48K", "media/rpd-1/CPM22P_48A_Perina.img",
                "work/test/zps3_m48a.img");
    cpm_machine("machines/zps/ZPS-3_CPM62K.sapi", "SAPI-1 ZPS3-62K", "media/rpd-1/CPM22P_62A_Perina.img",
                "work/test/zps3_m62a.img");
    cpm_machine("machines/zps/ZPS-3_CPM48K_FDC-1.sapi", "SAPI-1 ZPS3-48K/FDC-1", "media/fdc-1/CPM22P_48AF_Perina.img",
                "work/test/zps3_m48af.img");
    cpm_machine("machines/zps/ZPS-3_CPM62K_FDC-1.sapi", "SAPI-1 ZPS3-62K/FDC-1", "media/fdc-1/CPM22P_62AF_Perina.img",
                "work/test/zps3_m62af.img");
    ima48k();
    ima64k();
    return TEST_RESULT();
}
