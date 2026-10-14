// Integration test: ECMON boots CP/M 2.2 from the ZRMD-1 RAM disk image and DIR
// lists the files. The RAM disk is opened read only so the image is not modified.
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

int main(void)
{
    test_init();
    std::string text;
    CHECK(file_read_text(data_path("machines/cpm_zrmd.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    CHECK(!cfg.is_discarded());
    for (json& c : cfg["cards"])
        if (c["type"] == "ZRMD-1") {
            c["mode"] = "readonly";
            c["working_copy"] = false;   // read the pristine image directly
        }

    Machine* m = new Machine();
    std::string err;
    CHECK(m->load_json(cfg, err));
    if (!err.empty()) fprintf(stderr, "%s\n", err.c_str());

    CHECK(run_until_text(*m, "Zadej 0-3", 3.0));
    m->type_text("3");   // A: ZRMD-1, B,C: HDD: without IDE-1 the disks read XYXY (open bus), 1 (RPD8) waits for the RPD-1 for ever
    CHECK(run_until_text(*m, "JPR-1Z 62K CP/M V2.2Z", 3.0));
    CHECK(run_until_text(*m, "A: ZRMD1/4M BOOT SS256", 1.0));   // the BIOS of variant 3 (BI-3K)
    CHECK(run_until_text(*m, "INIT HDD YXYX", 5.0));               // no IDE-1: the last bus byte, as on the board
    CHECK(run_until_text(*m, "A>", 10.0));

    m->type_text("DIR\r");
    CHECK(run_until_text(*m, "WM1Z    .HLP", 30.0));
    CHECK(run_until_text(*m, "\nA>", 5.0));

    printf("%s", screen_text(*m).c_str());
    delete m;
    return TEST_RESULT();
}
