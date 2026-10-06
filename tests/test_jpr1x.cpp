// JPR-1X (80C85, Libor Lasota): MIKROBASIC 2.4 from the upper ROM bank (ZPS-1 equivalent),
// MIKROBIOS 1.02 from the lower bank with BGP-1S (the same picture as on the JPR-1S), the
// TURBO jumper doubles the speed of the CPU while running, the 3214 interrupt (RST 7).
#include <string.h>
#include <vector>
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

static void mikrobasic(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-1_JPR-1X.sapi", err));
    CHECK(run_until_text(m, "READY", 3.0));
    m.run_seconds(0.3);
    m.type_text("PRINT 12*34+5\r");
    CHECK(run_until_text(m, " 413", 3.0));

    // a loop timed at 2 MHz and with TURBO (4 MHz): about half the time
    double t[2];
    for (int turbo = 0; turbo < 2; turbo++) {
        CHECK(m.set_live_param(m.cpu, "turbo", turbo == 1, err));
        m.type_text("NEW\r10 FOR I=1 TO 5000\r20 NEXT I\r");
        m.type_text(turbo ? "30 PRINT \"DONE2\"\r" : "30 PRINT \"DONE1\"\r");
        for (int i = 0; i < 200 && m.keyboard()->pending(); i++) m.run_seconds(0.1);   // typed
        m.run_seconds(0.3);
        uint64_t t0 = m.bus.now;   // typing RUN takes the same time in both
        m.type_text("RUN\r");
        CHECK(run_until_text(m, turbo ? "\nDONE2" : "\nDONE1", 20.0));
        t[turbo] = (double)(m.bus.now - t0) / m.bus.cpu_hz;
    }
    printf("FOR loop: %.3f s at 2 MHz, %.3f s with TURBO\n", t[0], t[1]);
    CHECK(t[1] < t[0] * 0.65 && t[1] > t[0] * 0.4);
    printf("%s", screen_text(m).c_str());
}

// MIKROBIOS on JPR-1X with RAM-1 gives the same picture as on JPR-1S with its RAM banks
// (above the cursor line, whose blinking depends on the time).
static bool bios_picture(const char* file, std::vector<uint32_t>& px, int& w)
{
    Machine m;
    std::string err;
    if (!m.load_file(file, err)) return false;
    m.run_seconds(3.0);
    m.type_text("?");
    m.run_seconds(1.5);
    DisplayDevice* d = m.display;
    if (!d) return false;
    w = d->screen_width();
    px.assign((size_t)w * d->screen_height(), 0);
    d->render(px.data(), 0xFF000000u);
    px.resize((size_t)w * 280);   // the rows above the prompt
    return true;
}

static void mikrobios(void)
{
    std::vector<uint32_t> a, b;
    int wa = 0, wb = 0;
    CHECK(bios_picture("machines/jpr1x_bgp1s.sapi", a, wa));
    CHECK(bios_picture("machines/jpr1s_bgp1s.sapi", b, wb));
    CHECK_EQ(wa, wb);
    CHECK(a == b);
    int lit = 0;
    for (uint32_t p : a) lit += (p & 0xFFFFFF) != 0;
    CHECK(lit > 1000);   // the banner and the memory map are there
}

// An interrupt from AND-1 (INT1 at the end of the frame, request R1) with the 3214 enabled
// by a status write: RST 7 (RSTN/7 open) after EI.
static void interrupt(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-1_JPR-1X.sapi", err));
    CHECK(run_until_text(m, "READY", 3.0));
    static const uint8_t prog[] = {
        0xF3,               // DI
        0x3E, 0x00,         // MVI A,0
        0x32, 0x00, 0x30,   // STA 3000h: 3214 status 0, enabled
        0x01, 0x00, 0x40,   // LXI B,4000h
        0x0B,               // loop: DCX B
        0x78, 0xB1,         // MOV A,B; ORA C
        0xC2, 0x09, 0x41,   // JNZ loop (about 60 ms at 2 MHz: an INT1 comes, latched under DI)
        0xFB,               // EI
        0x00,               // NOP
        0xC3, 0x10, 0x41,   // JMP $-1
    };
    for (size_t i = 0; i < sizeof(prog); i++) m.poke((uint16_t)(0x4100 + i), prog[i]);
    CpuRegs r;
    m.cpu->get_regs(r);
    r.pc = 0x4100;
    r.sp = 0x4600;
    m.cpu->set_regs(r);
    m.set_breakpoint(0x0038, true);
    m.run_seconds(0.5);
    CHECK(m.stopped_on_breakpoint());
    m.cpu->get_regs(r);
    CHECK_EQ(r.pc, 0x0038);
    CHECK(!r.iff1);
}

int main(void)
{
    test_init();
    mikrobasic();
    mikrobios();
    interrupt();
    return TEST_RESULT();
}
