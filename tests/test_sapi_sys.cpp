// Machines from the system disks of J. Biba (SAPI_DOKU_FTP ...\PETR\SAPI_SYS, media/sapiman/Z*):
// RPD-1 and FDC-1 with the ZRMD-1 RAM disk in the ECMON layout (SS256): C: lists the files of the
// RAM disk image; MFC-1A with RMD-1; the 64K CP/M with only a terminal on the serial line.
// Drive motors: the long CP/M turns the motors of its PC drives on (motor_line), the 8" system
// on FDC-1 does not, so with motor_line its boot disk never turns. The boot disks with RMD-1
// (C: initialised by B:IRMD40 from Z6) and FDC-1 with PMD32-SD on DPP-1. RPD-1 systems with a second
// controller for D:, E:: MFC-1A with 8" FM disks of 14 x 256 and 8 x 512 B, FDC-1 with 8" SS SD. The BIOS
// BIRMD sets the random RMD-1 up by itself and finds its size (256 KB here).
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

// RMD-1 after power on is random: IRMD40 from Z6 in B: initialises it, then C: takes a file
static void rmd_boot(const char* file, const char* banner)
{
    printf("--- %s\n", file);
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(file), err));
    CHECK(run_until_text(m, banner, 10.0));
    CHECK(run_until_text(m, "C: RMD1/1MB", 10.0));
    CHECK(run_until_text(m, "A>", 20.0));
    m.run_seconds(0.3);
    m.type_text("B:IRMD40\r");
    CHECK(run_until_text(m, "(A/N)", 15.0));
    m.type_text("A");
    m.run_seconds(15.0);
    m.type_text("SAVE 2 C:X.COM\r");
    m.run_seconds(3.0);
    m.type_text("DIR C:\r");
    CHECK(run_until_text(m, "X        COM", 10.0));
    printf("%s\n", screen_text(m).c_str());
}

// boot from RPD-1, RMD-1 initialised as C:, a file saved to D: on the second controller (a copy of the
// empty disk of the machine in work/test)
static void rpd_second(const char* file, const char* type, const char* banner)
{
    printf("--- %s\n", file);
    json cfg = machine(file);
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == type) {
            std::string img = c["drive_a"]["image"].get<std::string>();
            std::string ext = img.substr(img.find_last_of('.'));
            std::vector<uint8_t> data;
            CHECK(file_read_all(data_path(img), data));
            std::string copy = std::string("work/test/rpd_second") + ext;
            CHECK(file_write_all(data_path(copy), data.data(), data.size()));
            c["drive_a"]["image"] = copy;
            c["drive_a"]["write_protect"] = false;
        }
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, banner, 10.0));
    CHECK(run_until_text(m, "A>", 20.0));
    m.run_seconds(0.3);
    m.type_text("B:IRMD40\r");
    CHECK(run_until_text(m, "(A/N)", 15.0));
    m.type_text("A");
    m.run_seconds(15.0);
    m.type_text("SAVE 2 C:X.COM\r");
    m.run_seconds(3.0);
    m.type_text("SAVE 4 D:Y.COM\r");
    m.run_seconds(5.0);
    m.type_text("DIR C:\r");
    CHECK(run_until_text(m, "X        COM", 10.0));
    m.run_seconds(1.0);
    m.type_text("DIR D:\r");
    CHECK(run_until_text(m, "Y        COM", 10.0));
    printf("%s\n", screen_text(m).c_str());
}

static void irmd(void)
{
    printf("--- machines/cpm_boot_fdc1_irmd.sapi\n");
    Machine m;
    std::string err;
    CHECK(m.load_json(machine("machines/cpm_boot_fdc1_irmd.sapi"), err));
    CHECK(run_until_text(m, "RMD1/1MB/256KB", 10.0));
    CHECK(run_until_text(m, "A>", 20.0));
    m.run_seconds(0.3);
    m.type_text("SAVE 2 C:X.COM\r");
    m.run_seconds(3.0);
    m.type_text("B:STAT C:\r");
    CHECK(run_until_text(m, "ON C: 253K", 15.0));   // 256 KB found, 2 KB used by X.COM, 1 KB by the directory
    printf("%s\n", screen_text(m).c_str());
}

static void pmd_fdc1(void)
{
    printf("--- machines/cpm_pmd_fdc1.sapi\n");
    Machine m;
    std::string err;
    CHECK(m.load_json(machine("machines/cpm_pmd_fdc1.sapi"), err));
    CHECK(run_until_text(m, "PMD32-SD OK", 30.0));
    CHECK(run_until_text(m, "A>", 10.0));
    m.run_seconds(0.3);
    m.type_text("DIR C:\r");
    CHECK(run_until_text(m, "TURBO    COM", 20.0));
    m.run_seconds(4.0);
    m.type_text("DIR D:\r");
    CHECK(run_until_text(m, "WSOVLY1  OVR", 20.0));
    printf("%s\n", screen_text(m).c_str());
}

// with motor_line the drives turn only when register 0FH switches their motors on
static void motors(void)
{
    printf("--- motors\n");
    json cfg = machine("machines/cpm_boot_fdc1.sapi");
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "FDC-1") c["motor_line"] = true;
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, "130711JB", 10.0));
    CHECK(!run_until_text(m, "A>", 8.0));   // the monitor does not switch the motors on
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "FDC-1") c["motor_line"] = false;
    Machine n;
    CHECK(n.load_json(cfg, err));
    CHECK(run_until_text(n, "A>", 20.0));
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
    motors();
    rmd_boot("machines/cpm_boot_fdc1_rmd.sapi", "BIOS-1K V130711JB");
    rmd_boot("machines/cpm_boot_rpd1_rmd.sapi", "BIOS-1K V271111JB");
    pmd_fdc1();
    irmd();
    rpd_second("machines/cpm_boot_rpd1_mfc256.sapi", "MFC-1A", "D,E: MFC1; 8\" 14x256");
    rpd_second("machines/cpm_boot_rpd1_mfc512.sapi", "MFC-1A", "D,E: MFC1; 8\" 8x512");
    rpd_second("machines/cpm_boot_rpd1_fdc1_rmd.sapi", "FDC-1", "D,E: FDC1; 8\" SS,SD");
    return TEST_RESULT();
}
