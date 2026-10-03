// Integration test: ZPS-1 (JPR-1 8080, REM-1, AND-1, ANK-1) boots MIKROBASIC 2.4,
// takes a program typed on the ANK-1 matrix and runs it; the AR/B 1983 version
// boots from the EPROMs and RAM on the JPR-1 itself.
#include "core/headless.h"
#include "core/machine.h"
#include "core/machine_map.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static void zps1(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-1.sapi", err));
    if (!err.empty()) fprintf(stderr, "%s\n", err.c_str());
    CHECK(run_until_text(m, "READY", 2.0));
    CHECK(screen_contains(m, "MIKRO BASIC"));

    m.type_text("PRINT 12*34+5\r");
    CHECK(run_until_text(m, "413", 5.0));
    m.type_text("10 FOR I=1 TO 3\r20 PRINT I*I\r30 NEXT I\rRUN\r");
    CHECK(run_until_text(m, "       9", 20.0));
    // MIKROBASIC keeps its variables in the REM-1 RAM at 4000h
    CHECK(m.peek(0x3800) == 'M' - 0x40);   // "MIKRO BASIC" on the AND-1, code 0Dh = M

    // the shifted legends of the ANK-1 (SHIFT + 2 = ?, SHIFT + L = =)
    m.type_text("PRINT \"?=<>\"\r");
    CHECK(run_until_text(m, "?=<>\n", 5.0) || screen_contains(m, "?=<>"));
    printf("%s", screen_text(m).c_str());

    std::string text;
    CHECK(file_read_text(data_path("machines/zps/ZPS-1.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    std::vector<CfgProblem> pr = machine_all_problems(cfg);
    for (const CfgProblem& p : pr) {
        if (p.error) fprintf(stderr, "ZPS-1: %s\n", p.en.c_str());
        CHECK(!p.error);   // DSM-1, BDK-1 not emulated yet: warnings only
    }
}

static void ar1983(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/MIKROBASIC_AR1983.sapi", err));
    if (!err.empty()) fprintf(stderr, "%s\n", err.c_str());
    CHECK(run_until_text(m, "READY", 2.0));
    m.type_text("PRINT 7*6\r");
    CHECK(run_until_text(m, "42", 5.0));
    CHECK(m.peek(0x2000) != 0xFF || m.peek(0x2001) != 0xFF);   // RAM on the JPR-1
}

int main(void)
{
    test_init();
    zps1();
    ar1983();
    return TEST_RESULT();
}
