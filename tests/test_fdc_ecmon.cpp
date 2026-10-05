// FDC-1 and MFC-1A under the CP/M of the ZRMD-1 RAM disk (ECMON, BIOS variants 0 "FDC8" and
// 2 "MFC8", machines/cpm_zrmd_fdc1.sapi and cpm_zrmd_mfc1a.sapi): B: is an 8" disk with
// files (DIR), C: a formatted empty one (SAVE, the file reaches the image). MFC-1A with the
// reset bit as in the schematic (reset_d5 = level) hangs in the BIOS initialization.
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static json machine(const char* file, const char* type, const char* reset_d5)
{
    std::string text;
    CHECK(file_read_text(data_path(file), text));
    json cfg = json::parse(text, nullptr, false, true);
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path("media/rpd-1/CPM22P_48A_Perina.img"), img));
    CHECK(file_write_all(data_path("work/test/ecmon_b.img"), img.data(), img.size()));
    std::vector<uint8_t> blank(256256, 0xE5);
    CHECK(file_write_all(data_path("work/test/ecmon_c.img"), blank.data(), blank.size()));
    for (json& c : cfg["cards"]) {
        std::string t = c.value("type", std::string());
        if (t == "ZRMD-1") {
            c["mode"] = "readonly";
            c["working_copy"] = false;   // the pristine RAM disk image, as test_cpm
        }
        if (t == type) {
            c["drive_a"] = { { "image", "work/test/ecmon_b.img" }, { "write_protect", false } };
            c["drive_b"] = { { "image", "work/test/ecmon_c.img" }, { "write_protect", false } };
            if (reset_d5) c["reset_d5"] = reset_d5;
        }
    }
    return cfg;
}

static void controller(const char* file, const char* type, const char* variant, const char* banner)
{
    printf("--- %s\n", file);
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(file, type, nullptr), err));
    m.type_text(variant);   // ECMON takes the variant right after BOOT (kept in RAM after that)
    CHECK(run_until_text(m, banner, 5.0));
    CHECK(run_until_text(m, "\nA>", 30.0));
    m.run_seconds(0.3);
    m.type_text("DIR B:\r");
    CHECK(run_until_text(m, "RSYS62A .COM", 10.0));
    m.run_seconds(0.5);
    for (int i = 0; i < 0x400; i++) m.bus.mem_write((uint16_t)(0x0100 + i), (uint8_t)(i * 7 + 3));
    m.type_text("SAVE 4 C:X.COM\r");
    m.run_seconds(4.0);
    m.type_text("DIR C:\r");
    CHECK(run_until_text(m, "X       .COM", 10.0));
    m.flush();
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path("work/test/ecmon_c.img"), img));
    bool found = false;
    for (size_t off = 2 * 26 * 128; off + 3 <= img.size() && !found; off += 128)
        if (img[off] == 3 && img[off + 1] == 10 && img[off + 2] == 17) found = true;
    CHECK(found);
    printf("%s\n", screen_text(m).c_str());
}

// reset_d5 = level: FFh written at the initialization holds the 8272 in reset, the BIOS waits
static void mfc_level(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine("machines/cpm_zrmd_mfc1a.sapi", "MFC-1A", "level"), err));
    m.type_text("2");
    CHECK(run_until_text(m, "B,C: MFC1", 5.0));
    CHECK(!run_until_text(m, "\nA>", 10.0));
}

int main(void)
{
    test_init();
    controller("machines/cpm_zrmd_fdc1.sapi", "FDC-1", "0", "B,C: FDC1");
    controller("machines/cpm_zrmd_mfc1a.sapi", "MFC-1A", "2", "B,C: MFC1");
    mfc_level();
    return TEST_RESULT();
}
