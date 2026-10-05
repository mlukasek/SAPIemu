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

int main(void)
{
    test_init();
    screen_system("machines/cpm_boot_rpd1_ss256.sapi", "BIOS-1K V271111JB", "BI2711ZS MAC", "MYLBA    MAC");
    screen_system("machines/cpm_boot_fdc1_ss256.sapi", "BIOS-1K V130711JB", "1307Z2   IDF", "MYLBA    MAC");
    screen_system("machines/cpm_boot_mfc1a_rmd.sapi", "BIOS-1K V281211JB", "MO2812   MAC", nullptr);
    term64k();
    return TEST_RESULT();
}
