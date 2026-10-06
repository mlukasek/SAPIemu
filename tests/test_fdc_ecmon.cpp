// FDC-1 and MFC-1A under the CP/M of the ZRMD-1 RAM disk (ECMON, BIOS variants 0 "FDC8" and
// 2 "MFC8", machines/cpm_zrmd_fdc1.sapi and cpm_zrmd_mfc1a.sapi): B: is an 8" disk with
// files (DIR), C: a formatted empty one (SAVE, the file reaches the image). The preset
// MFC-1A is wired as the EC1045 redraw with the reset as in the schematic and the fixed BIOS
// ECBIM8 (RAM disk ..._fixMFC.bin). The original BIOS (RET missing in INIT_MFC) hangs with the
// schematic reset in both wirings and boots only with the reset bit as a pulse (deviation).
// CP/M of J. Biba (2011) booted from an 8" disk through MFC-1A, FDC-1 and RPD-1
// (machines/cpm_boot_*.sapi): DIR A: lists the system sources on the disk, SAVE to the
// RAM disk C: (CP/M over all 4 MB).
// The EC1045 system (machines/cpm_ec1045.sapi) with PC disks on FDC-1 in MFM: variant 1
// 3.5" HD 1.44 MB, variant 2 5.25" HD 1.2 MB; SAVE to B: puts the directory entry on cylinder 1
// head 0 sector 1 of the image (OFF 1, two sides).
// DUP8 of J. Biba (physical copy through the BIOS, on the RAM disk of cpm_zrmd) copies B: to
// C: on FDC-1 and MFC-1A sector by sector: the images are the same.
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "chips/floppy.h"

#include <string.h>
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

static void dup8(const char* file, const char* type, const char* variant)
{
    printf("--- DUP8 %s\n", file);
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(file, type, nullptr), err));
    m.type_text(variant);
    CHECK(run_until_text(m, "\nA>", 30.0));
    m.run_seconds(0.3);
    m.type_text("DUP8\r");
    CHECK(run_until_text(m, "Kopie disku", 10.0));
    m.type_text("2");
    CHECK(run_until_text(m, "cilovy", 10.0) || run_until_text(m, "zdrojovy", 1.0));
    m.type_text("B");
    CHECK(run_until_text(m, "cilovy", 10.0));
    m.type_text("C");
    CHECK(run_until_text(m, "[A/N]", 10.0));
    m.type_text("A");
    CHECK(run_until_text(m, "<CR>", 10.0));
    m.type_text("\r");
    bool done = false;
    for (int i = 0; i < 60 && !done; i++) {   // 77 tracks, read and written sector by sector
        m.run_seconds(5.0);
        std::string t = screen_text(m);
        size_t at = t.rfind("<CR>");
        done = at != std::string::npos && t.find("Exit", at) != std::string::npos;   // the menu again
    }
    CHECK(done);
    m.flush();
    std::vector<uint8_t> b, c;
    CHECK(file_read_all(data_path("work/test/ecmon_b.img"), b));
    CHECK(file_read_all(data_path("work/test/ecmon_c.img"), c));
    CHECK(b.size() == 256256 && b == c);
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

// boot disk in A: (a copy in work/test), empty RAM disk C:
static void boot_jb(const char* file, const char* type, const char* disk, const char* banner, const char* file_on_a)
{
    printf("--- %s\n", file);
    std::string t;
    CHECK(file_read_text(data_path(file), t));
    json cfg = json::parse(t, nullptr, false, true);
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path(disk), img));
    CHECK(file_write_all(data_path("work/test/jb_a.img"), img.data(), img.size()));
    for (json& c : cfg["cards"]) {
        std::string ty = c.value("type", std::string());
        if (ty == "ZRMD-1") {
            c["mode"] = "nosave";
            c["working_copy"] = false;
        }
        if (ty == type) {
            c["drive_a"] = { { "image", "work/test/jb_a.img" }, { "write_protect", true } };
            c.erase("drive_b");
            c["working_copy"] = false;
        }
    }
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, banner, 5.0));
    CHECK(run_until_text(m, "\nA>", 20.0));
    m.run_seconds(0.3);
    m.type_text("DIR A:\r");
    CHECK(run_until_text(m, file_on_a, 10.0));
    m.run_seconds(4.0);   // the rest of the listing (a key typed during DIR is taken by its break check)
    m.type_text("SAVE 4 C:TEST.COM\r");
    m.run_seconds(2.0);
    m.type_text("DIR C:\r");
    CHECK(run_until_text(m, "TEST     COM", 10.0));
    printf("%s\n", screen_text(m).c_str());
}

static void ec1045_hd(const char* variant, const char* blank, const char* banner, const char* free_text)
{
    printf("--- EC1045 variant %s, %s\n", variant, blank);
    std::string t;
    CHECK(file_read_text(data_path("machines/cpm_ec1045.sapi"), t));
    json cfg = json::parse(t, nullptr, false, true);
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path(blank), img));
    CHECK(file_write_all(data_path("work/test/ec_b.img"), img.data(), img.size()));
    for (json& c : cfg["cards"]) {
        std::string ty = c.value("type", std::string());
        if (ty == "ZRMD-1") {
            c["mode"] = "readonly";
            c["working_copy"] = false;
        }
        if (ty == "FDC-1") {
            c["drive_a"] = { { "image", "work/test/ec_b.img" }, { "write_protect", false } };
            c["working_copy"] = false;
        }
    }
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    m.type_text(variant);
    CHECK(run_until_text(m, banner, 5.0));
    CHECK(run_until_text(m, "\nA>", 30.0));
    m.run_seconds(0.3);
    for (int i = 0; i < 0x800; i++) m.bus.mem_write((uint16_t)(0x0100 + i), (uint8_t)(i * 7 + 3));
    m.type_text("SAVE 8 B:TEST.COM\r");
    m.run_seconds(4.0);
    m.type_text("STAT B:*.*\r");
    CHECK(run_until_text(m, free_text, 10.0));
    m.flush();
    CHECK(file_read_all(data_path("work/test/ec_b.img"), img));
    const FloppyGeom* g = floppy_geom_for_size(img.size());
    CHECK(g != nullptr);
    if (g) {
        size_t cyl1 = (size_t)g->heads * g->sectors * 512;   // directory: cylinder 1, head 0, sector 1
        CHECK(img.size() > cyl1 + 32 && memcmp(&img[cyl1 + 1], "TEST    COM", 11) == 0);
        bool found = false;   // the data somewhere after the directory
        for (size_t off = cyl1; off + 3 <= img.size() && !found; off += 128)
            if (img[off] == 3 && img[off + 1] == 10 && img[off + 2] == 17) found = true;
        CHECK(found);
    }
    printf("%s\n", screen_text(m).c_str());
}

int main(void)
{
    test_init();
    controller("machines/cpm_zrmd_fdc1.sapi", "FDC-1", "0", "B,C: FDC1");
    controller("machines/cpm_zrmd_mfc1a.sapi", "MFC-1A", "2", "B,C: MFC1");
    boot_jb("machines/cpm_boot_mfc1a.sapi", "MFC-1A", "media/sapiman/m1_281211_zrmd.img", "MON CZ 281211JB",
            "BI2812ZS MAC");
    boot_jb("machines/cpm_boot_fdc1.sapi", "FDC-1", "media/sapiman/f1_zrmd4_130711.img", "MON CZ 130711JB",
            "BI1307ZS MAC");
    boot_jb("machines/cpm_boot_rpd1.sapi", "RPD-1", "media/sapiman/r1_zrmd4m_271111.img", "MON CS 271111JB",
            "B2711Z4  MAC");
    ec1045_hd("1", "media/fdc-1/prazdna_3.5HD_CPM.img", "B,C: FDC1; 3,5\" DS,HD", "ON B: 1412K");
    ec1045_hd("2", "media/fdc-1/prazdna_5.25HD_CPM.img", "B,C: FDC1; 5,25\" DS,HD", "ON B: 1174K");
    dup8("machines/cpm_zrmd_fdc1.sapi", "FDC-1", "0");
    dup8("machines/cpm_zrmd_mfc1a.sapi", "MFC-1A", "2");
    mfc_orig("tesla", "level", false);
    mfc_orig("ec1045", "level", false);
    mfc_orig("ec1045", "pulse", true);
    return TEST_RESULT();
}
