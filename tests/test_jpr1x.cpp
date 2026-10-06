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

// The counter on the diagnostic board (K3): "+" and "-" step it, 99 + 1 = 00, 00 - 1 = 99;
// diag_int.hex by the 3214 interrupts (RST 7 -> 2000h), diag_poll.hex by reading P2-IN.
// ESC ends it (the displays go dark, the vector of MIKROBIOS is back).
static void press(Machine& m, int b)
{
    m.cpu->k3_button(b, true);
    m.run_seconds(0.1);
    m.cpu->k3_button(b, false);
    m.run_seconds(0.1);
}

static void counter(const char* hex)
{
    static const uint8_t font[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
    Machine m;
    std::string err, text;
    CHECK(m.load_file("machines/jpr1x_bgp1s.sapi", err));
    CHECK(m.cpu && m.cpu->k3_diag());
    if (!m.cpu || !m.cpu->k3_diag()) return;
    m.run_seconds(3.0);   // MIKROBIOS and Falcon
    uint8_t vec[3] = { m.peek(0x2000), m.peek(0x2001), m.peek(0x2002) };
    CHECK(file_read_text(data_path(hex), text));
    CHECK(m.load_ihex_text(text, err));
    m.type_text("JUMP 4400\r");
    m.run_seconds(2.0);
    auto shows = [&](int n) { return m.cpu->k3_display(0) == font[n / 10] && m.cpu->k3_display(1) == font[n % 10]; };
    CHECK(shows(0));
    press(m, 0);
    press(m, 0);
    press(m, 0);
    CHECK(shows(3));
    press(m, 1);
    CHECK(shows(2));
    for (int i = 0; i < 3; i++) press(m, 1);
    CHECK(shows(99));   // 00 - 1
    press(m, 0);
    CHECK(shows(0));    // 99 + 1
    printf("%s: %02X %02X\n", hex, m.cpu->k3_display(0), m.cpu->k3_display(1));
    m.type_text("\x1b");   // ESC = the LF key (MIKOS layout)
    m.run_seconds(1.0);
    CHECK_EQ(m.cpu->k3_display(0), 0);
    CHECK_EQ(m.cpu->k3_display(1), 0);
    CHECK(m.peek(0x2000) == vec[0] && m.peek(0x2001) == vec[1] && m.peek(0x2002) == vec[2]);
}

int main(void)
{
    test_init();
    mikrobasic();
    mikrobios();
    interrupt();
    counter("media/jpr-1x/diag_int.hex");
    counter("media/jpr-1x/diag_poll.hex");
    return TEST_RESULT();
}
