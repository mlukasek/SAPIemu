// CP/M letters of the drives (Media window): the CPU card notes the BIOS SELDSK calls, the disk
// controllers (RPD-1, FDC-1, MFC-1A, PMD32-SD) give the drive of the last SELDSK to the unit of
// their next sector read. DIR of each letter teaches the letters of ECMON, the monitors of
// J. Biba, the long CP/M, EC1045 (Z80) and CP/M 2.2/P of ZPS-3 (8080).
#include "chips/floppy.h"
#include "chips/pmd32sd.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

#include <stdio.h>
#include <string.h>

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
            for (const char* k : { "drive_a", "drive_b", "drive_c", "drive_d" })
                if (c.contains(k)) {
                    c[k]["write_protect"] = true;
                    c[k].erase("cpm");   // only what is seen while running
                }
        }
        if (ty == "DMR-1" && c.contains("serial")) c["serial"].erase("tcp");
        if (c.contains("pmd32sd")) {
            c["pmd32sd"]["working_copy"] = false;
            c["pmd32sd"]["card_locked"] = true;
            c["pmd32sd"].erase("cpm");
        }
    }
    return cfg;
}

// letters: per card type, the expected letter of units 0-3 ('.' = not checked)
struct Expect {
    const char* type;
    const char* letters;
};

static void check(const char* file, const char* key, const char* prompt, const char* dirs, const Expect* ex)
{
    printf("--- %s\n", file);
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(file), err));
    if (key) m.type_text(key);
    CHECK(run_until_text(m, prompt, 40.0));
    for (const char* d = dirs; *d; d++) {
        m.run_seconds(1.0);
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "DIR %c:\r", *d);
        m.type_text(cmd);
        m.run_seconds(6.0);
    }
    for (; ex->type; ex++)
        for (auto& c : m.cards) {
            if (c->type != ex->type) continue;
            for (int u = 0; ex->letters[u]; u++) {
                if (ex->letters[u] == '.') continue;
                char seen = 0;
                if (Pmd32Sd* p = c->pmd32sd()) seen = p->cpm_seen[u];
                else if (FloppyDrive* f = c->floppy_drive(u)) seen = f->cpm_seen;
                printf("  %s unit %d: %c (expected %c)\n", ex->type, u, seen ? seen : '-', ex->letters[u]);
                CHECK(seen == ex->letters[u]);
            }
        }
}

int main(void)
{
    test_init();
    static const Expect zrmd_fdc[] = { { "FDC-1", "BC" }, { nullptr, nullptr } };
    check("machines/cpm_zrmd_fdc1.sapi", "0", "\nA>", "BC", zrmd_fdc);
    static const Expect zrmd_mfc[] = { { "MFC-1A", "BC" }, { nullptr, nullptr } };
    check("machines/cpm_zrmd_mfc1a.sapi", "2", "\nA>", "BC", zrmd_mfc);
    static const Expect boot_rpd[] = { { "RPD-1", "AB" }, { nullptr, nullptr } };
    check("machines/cpm_boot_rpd1.sapi", nullptr, "A>", "AB", boot_rpd);
    static const Expect boot_fdc[] = { { "FDC-1", "AB" }, { nullptr, nullptr } };
    check("machines/cpm_boot_fdc1.sapi", nullptr, "A>", "AB", boot_fdc);
    static const Expect boot_mfc[] = { { "MFC-1A", "AB" }, { nullptr, nullptr } };
    check("machines/cpm_boot_mfc1a.sapi", nullptr, "A>", "AB", boot_mfc);
    static const Expect dlouhe[] = { { "RPD-1", "AB" }, { "FDC-1", "D" }, { nullptr, nullptr } };
    check("machines/cpm_boot_dlouhe_rpd1_fdc1.sapi", nullptr, "A>", "ABD", dlouhe);
    static const Expect ec1045[] = { { "FDC-1", "B" }, { nullptr, nullptr } };
    check("machines/cpm_ec1045.sapi", "1", "\nA>", "B", ec1045);
    static const Expect pmd_rpd[] = { { "RPD-1", "A" }, { "DPP-1", "CD" }, { nullptr, nullptr } };
    check("machines/cpm_pmd_rpd1.sapi", nullptr, "A>", "ACD", pmd_rpd);
    static const Expect zps3[] = { { "RPD-1", "A" }, { nullptr, nullptr } };   // B: has no disk
    check("machines/zps/ZPS-3_CPM48K.sapi", nullptr, "A>", "A", zps3);
    return TEST_RESULT();
}
