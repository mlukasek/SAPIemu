// FDC-1 and MFC-1A under the CP/M of the ZRMD-1 RAM disk (ECMON, BIOS variants 0 "FDC8" and
// 2 "MFC8", machines/cpm_zrmd_fdc1.sapi and cpm_zrmd_mfc1a.sapi): B: is an 8" disk with
// files (DIR), C: a formatted empty one (SAVE, the file reaches the image). The preset
// MFC-1A is wired as the EC1045 redraw with the reset as in the schematic and the fixed BIOS
// ECBIM8 (RAM disk ..._fixMFC.bin). The original BIOS (RET missing in INIT_MFC) hangs with the
// schematic reset in both wirings and boots only with the reset bit as a pulse (deviation).
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static const char* k_orig_image = "media/zrmd-1/SAPI_CPM_Martin_2019-10-07.bin";

// wiring/reset: override the MFC-1A register; image: another RAM disk image
static json machine(const char* file, const char* type, const char* wiring, const char* reset = "level",
                    const char* image = nullptr)
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
            if (image) c["image"] = image;
        }
        if (t == type) {
            c["drive_a"] = { { "image", "work/test/ecmon_b.img" }, { "write_protect", false } };
            c["drive_b"] = { { "image", "work/test/ecmon_c.img" }, { "write_protect", false } };
            if (wiring) {
                c["wiring"] = wiring;
                c["reset_mode"] = reset;
            }
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

// the original BIOS ECBIM8 with reset_mode = level: FFh written at the initialization holds
// the 8272 in reset, the BIOS waits; with the reset as a pulse it boots
static void mfc_orig(const char* wiring, const char* reset, bool boots)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine("machines/cpm_zrmd_mfc1a.sapi", "MFC-1A", wiring, reset, k_orig_image), err));
    m.type_text("2");
    CHECK(run_until_text(m, "B,C: MFC1", 5.0));
    CHECK(run_until_text(m, "\nA>", 10.0) == boots);
}

int main(void)
{
    test_init();
    controller("machines/cpm_zrmd_fdc1.sapi", "FDC-1", "0", "B,C: FDC1");
    controller("machines/cpm_zrmd_mfc1a.sapi", "MFC-1A", "2", "B,C: MFC1");
    mfc_orig("tesla", "level", false);
    mfc_orig("ec1045", "level", false);
    mfc_orig("ec1045", "pulse", true);
    return TEST_RESULT();
}
