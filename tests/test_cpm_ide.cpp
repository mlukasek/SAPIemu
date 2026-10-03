// Integration test: ECMON (IDE-1 version), option 3 = HDD. The BIOS finds the
// PMD32-SD and the IDE disk; DIR D: lists the 2MB image, PIP copies a file to
// the IDE disk B: and STAT reads it back. Originals are not modified: the RAM
// disk is read only, the SD card locked, the IDE disk a fresh test image.
#include <stdio.h>
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

int main(void)
{
    data_root_init();
    std::string text;
    CHECK(file_read_text(data_path("machines/cpm_ide.jsonc"), text));
    json cfg = json::parse(text, nullptr, false, true);
    std::string img = path_join(data_root(), "work/tests/cpm_ide.img");
    make_dirs(path_dir(img));
    remove(img.c_str());
    for (json& c : cfg["cards"]) {
        if (c["type"] == "ZRMD-1") { c["mode"] = "readonly"; c["working_copy"] = false; }
        if (c["type"] == "IDE-1") {
            c["ide"]["image"] = img;
            c["ide"]["create_mb"] = 16;   // slots B: (LBA 0) and C: (LBA 16384)
            c["pmd32sd"]["working_copy"] = false;
            c["pmd32sd"]["card_locked"] = true;
        }
    }
    Machine* m = new Machine();
    std::string err;
    CHECK(m->load_json(cfg, err));
    if (!err.empty()) { fprintf(stderr, "%s\n", err.c_str()); return 1; }

    CHECK(run_until_text(*m, "Zadej 0-3", 3.0));
    m->type_text("3");
    CHECK(run_until_text(*m, "PMD32-SD OK", 10.0));
    CHECK(run_until_text(*m, "INIT HDD SAPIemu IDE disk", 10.0));
    CHECK(run_until_text(*m, "A>", 10.0));

    m->type_text("DIR D:\r");
    CHECK(run_until_text(*m, "USER0   .LBR", 30.0));
    CHECK(run_until_text(*m, "\nA>", 5.0));

    m->type_text("PIP B:=D:STAT.COM\r");
    m->run_seconds(15.0);
    m->type_text("B:STAT B:\r");
    CHECK(run_until_text(*m, "BYTES REMAINING ON B: 8160K", 15.0));

    printf("%s", screen_text(*m).c_str());
    delete m;
    remove(img.c_str());
    return TEST_RESULT();
}
