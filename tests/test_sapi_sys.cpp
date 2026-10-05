// Machines from the system disks of J. Biba (SAPI_DOKU_FTP ...\PETR\SAPI_SYS, media/sapiman/Z*):
// RPD-1 and FDC-1 with the ZRMD-1 RAM disk in the ECMON layout (SS256): C: lists the files of the
// RAM disk image; MFC-1A with RMD-1; the 64K CP/M with only a terminal on the serial line.
#include "core/devices.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static json machine(const char* file)
{
    std::string t;
    CHECK(file_read_text(data_path(file), t));
    json cfg = json::parse(t, nullptr, false, true);
    for (json& c : cfg["cards"]) {
        std::string ty = c.value("type", std::string());
        if (ty == "ZRMD-1") {
            c["mode"] = "readonly";
            c["working_copy"] = false;
        }
        if (ty == "RPD-1" || ty == "FDC-1" || ty == "MFC-1A") {
            c["working_copy"] = false;
            for (const char* k : { "drive_a", "drive_b" })
                if (c.contains(k)) c[k]["write_protect"] = true;
        }
        if (ty == "DMR-1" && c.contains("serial")) c["serial"].erase("tcp");
        if (c.contains("pmd32sd")) {
            c["pmd32sd"]["working_copy"] = false;
            c["pmd32sd"]["card_locked"] = true;
        }
    }
    return cfg;
}

static void screen_system(const char* file, const char* banner, const char* on_b, const char* on_c)
{
    printf("--- %s\n", file);
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(file), err));
    CHECK(run_until_text(m, banner, 10.0));
    CHECK(run_until_text(m, "A>", 20.0));   // "U0A>" with the user number on some systems
    m.run_seconds(0.3);
    m.type_text("DIR B:\r");
    CHECK(run_until_text(m, on_b, 15.0));
    if (on_c) {   // RMD-1 has random content after power on: not listed
        m.run_seconds(5.0);
        m.type_text("DIR C:\r");
        CHECK(run_until_text(m, on_c, 10.0));
    }
    printf("%s\n", screen_text(m).c_str());
}

// the console is the serial line only
static bool serial_until(Machine& m, std::string& got, const char* text, double seconds)
{
    for (int i = 0; i < (int)(seconds * 20); i++) {
        m.run_seconds(0.05);
        std::string s;
        m.serials[0]->host_receive(s, 1 << 16);
        got += s;
        if (got.find(text) != std::string::npos) return true;
    }
    return false;
}

static void term64k(void)
{
    printf("--- machines/cpm_term64k_fdc1.sapi\n");
    Machine m;
    std::string err;
    CHECK(m.load_json(machine("machines/cpm_term64k_fdc1.sapi"), err));
    CHECK(!m.serials.empty());
    if (m.serials.empty()) return;
    std::string got;
    CHECK(serial_until(m, got, "JPR-1Z 64K CP/M V2.2", 10.0));
    CHECK(serial_until(m, got, "CON: TERMINAL PRES DSM1.1", 5.0));
    CHECK(serial_until(m, got, "A>", 10.0));
    const char* cmd = "DIR\r";
    m.serials[0]->host_send((const uint8_t*)cmd, 4);
    CHECK(serial_until(m, got, "SYS      STO", 15.0));   // the last file: a key during DIR would be lost
    m.run_seconds(1.0);
    const char* init = "B:IRMD40\r";
    m.serials[0]->host_send((const uint8_t*)init, 9);
    CHECK(serial_until(m, got, "Pokracovat? (A/N)", 15.0));
    m.serials[0]->host_send((const uint8_t*)"A", 1);
    CHECK(serial_until(m, got, "\nA>", 30.0));
    const char* save = "SAVE 2 C:X.COM\rDIR C:\r";
    m.serials[0]->host_send((const uint8_t*)save, 22);
    CHECK(serial_until(m, got, "X        COM", 10.0));
    printf("%s\n", got.c_str());
}

// the long CP/M (RPD-1 + FDC-1 5.25" HD): the boot disk composed from the sources, D: on FDC-1
static void long_cpm(const char* file, const char* blank, const char* banner, const char* free_text)
{
    printf("--- %s\n", file);
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path(blank), img));
    CHECK(file_write_all(data_path("work/test/long_d.img"), img.data(), img.size()));
    json cfg = machine(file);
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "FDC-1")
            c["drive_a"] = { { "image", "work/test/long_d.img" }, { "write_protect", false } };
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, banner, 15.0));
    CHECK(run_until_text(m, "A>", 10.0));
    m.run_seconds(0.3);
    m.type_text("DIR B:\r");
    CHECK(run_until_text(m, "BI142ZD2 MAC", 15.0));
    m.run_seconds(4.0);
    m.type_text("SAVE 4 D:TEST.COM\r");
    m.run_seconds(4.0);
    m.type_text("STAT D:\r");
    CHECK(run_until_text(m, free_text, 10.0));
    printf("%s\n", screen_text(m).c_str());
}

// DPP-1 with PMD32-SD on channel 2: the long CP/M 170212 (C:, D:) and ECMON (D: 2 MB, E: 8")
static void pmd_systems(void)
{
    printf("--- machines/cpm_pmd_rpd1.sapi\n");
    {
        Machine m;
        std::string err;
        CHECK(m.load_json(machine("machines/cpm_pmd_rpd1.sapi"), err));
        CHECK(run_until_text(m, "BIOS-2K V170212JB", 10.0));
        CHECK(run_until_text(m, "A>", 20.0));
        m.run_seconds(0.3);
        m.type_text("DIR C:\r");
        CHECK(run_until_text(m, "TURBO    COM", 20.0));
        m.run_seconds(4.0);
        m.type_text("DIR D:\r");
        CHECK(run_until_text(m, "WSOVLY1  OVR", 20.0));
        printf("%s\n", screen_text(m).c_str());
    }
    printf("--- machines/cpm_zrmd_dpp1.sapi\n");
    {
        Machine m;
        std::string err;
        CHECK(m.load_json(machine("machines/cpm_zrmd_dpp1.sapi"), err));
        m.type_text("1");   // ECMON variant 1 (RPD8: no floppy controller needed)
        CHECK(run_until_text(m, "PMD32-SD OK", 30.0));
        CHECK(run_until_text(m, "\nA>", 10.0));
        m.run_seconds(0.3);
        m.type_text("DIR D:\r");
        CHECK(run_until_text(m, "PARABOLA.BAS", 20.0));
        m.run_seconds(4.0);
        m.type_text("DIR E:\r");
        CHECK(run_until_text(m, "FORMFDC .MAC", 20.0));
        printf("%s\n", screen_text(m).c_str());
    }
}

int main(void)
{
    test_init();
    screen_system("machines/cpm_boot_rpd1_ss256.sapi", "BIOS-1K V271111JB", "BI2711ZS MAC", "MYLBA    MAC");
    screen_system("machines/cpm_boot_fdc1_ss256.sapi", "BIOS-1K V130711JB", "1307Z2   IDF", "MYLBA    MAC");
    screen_system("machines/cpm_boot_mfc1a_rmd.sapi", "BIOS-1K V281211JB", "MO2812   MAC", nullptr);
    term64k();
    pmd_systems();
    long_cpm("machines/cpm_boot_dlouhe_rpd1_fdc1.sapi", "media/fdc-1/prazdna_5.25HD_CPM.img",
             "D,E:FDC1;5,25\"DS,HD512x30PC", "ON D: 1174K");
    long_cpm("machines/cpm_boot_dlouhe_rpd1_fdc1_35.sapi", "media/fdc-1/prazdna_3.5HD_CPM.img",
             "D,E:FDC1;3,5\"DS,HD512x36PC", "ON D: 1412K");
    return TEST_RESULT();
}
