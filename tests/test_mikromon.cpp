// Integration test: MikroMon 1.3 boots on the basic machine and answers commands
// typed on the EKL-1 keyboard.
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

int main(void)
{
    data_root_init();
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/mikromon.sapi", err));
    if (!err.empty()) fprintf(stderr, "%s\n", err.c_str());

    CHECK(run_until_text(m, "MikroMon 2kB V1.3", 2.0));
    CHECK(screen_contains(m, "ZPS-4"));

    m.type_text("M F000\r");
    CHECK(run_until_text(m, ">F000: F301 0008", 2.0));
    m.type_text("\x1b");   // M waits for SPACE/CR/ESC after a page
    m.run_seconds(0.2);

    // Czech text through the keyboard into memory
    m.type_text("> 9000 \"žluťoučký\"\r");
    CHECK(run_until_text(m, ">9009", 2.0));
    // EKL-1 sends Czech letters with swapped case (KOI-8 cs XOR 20h, like its
    // firmware); MikroMon stores the codes as they come
    CHECK_EQ(m.peek(0x9000), 0xFA);   // ž -> code of Ž
    CHECK_EQ(m.peek(0x9003), 0xF4);   // ť -> code of Ť

    // execution breakpoint inside the keyboard wait loop is hit
    CpuRegs r;
    m.cpu->get_regs(r);
    m.set_breakpoint(r.pc, true);
    m.run_seconds(0.1);
    CHECK(m.stopped_on_breakpoint());

    printf("%s", screen_text(m).c_str());
    return TEST_RESULT();
}
