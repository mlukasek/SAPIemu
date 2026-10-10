// DPS-1 instrument bus card with the measuring bench (plant "IMS-2", docs/desky/DPS-1.md):
// - BASIMS V4.0 (media/dps-1/BASIMS.hex, BASIC EX V4.0 with IMS#) runs the example of its manual: the voltmeter
//   M1T 330 at address 3 measures the output I of the power supply BS 525, BASIC prints its reply
//   (V+1.2564E+1 for 12.564 V);
// - autorange down to the 300 mV range, the reply with a negative exponent;
// - the bus: LAD 3 and TAD 3 with ATN, the reply of the voltmeter ends with EOI;
// - a state saved in the middle of measuring, loaded into a new machine: the replies go on whole;
// - without the bench nobody listens: NDAC is released, so /ACK B (the received NDAC) is low and the 8255 takes
//   the byte at once (/OBF B is reset by the level of /ACK, datasheet 8255A): the shift register never gets to
//   the ERR check, no DAV, the byte is lost. BASIMS checks the listeners by its program and falls: its error path ERRLI -> KONVY -> OUTDAT (UNL) -> ERRLI never ends, the stack
//   runs over the memory and MIKOS starts again (a property of BASIMS, the real one does the same);
// - the cassette of the machine (media/dps-1/BASIMS_IMS2.wav, tools/dps1_kazeta.py): .K_L BASIMS, .J, then BASIC
//   LOAD PRIKLAD (written behind BASIMS by SAVE) runs the example (DPS_PART=kazeta, a minute of tape).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cards/dsm1.h"
#include "core/headless.h"
#include "core/ims2.h"
#include "core/machine.h"
#include "core/util.h"
#include "plant/ims2_bench.h"
#include "test_util.h"

using nlohmann::json;

static const char* MACHINE = "machines/zps/ZPS-2_DPS-1_IMS-2.sapi";

// BASIMS straight into memory (the cassette takes a minute), started by the monitor: G4400
static bool start_basims(Machine& m)
{
    if (!run_until_text(m, "SAPI-1 V4.0", 5.0)) {
        printf("NO MONITOR:\n%s\n", screen_text(m).c_str());
        return false;
    }
    m.run_seconds(0.3);
    std::string hex, err;
    CHECK(file_read_text(data_path("media/dps-1/BASIMS.hex"), hex));
    CHECK(m.load_ihex_text(hex, err));
    m.type_text("G4400\r");
    bool ok = run_until_text(m, "READY", 5.0);
    if (!ok) printf("NO BASIC:\n%s\n", screen_text(m).c_str());
    CHECK(ok);
    m.run_seconds(0.3);
    return ok;
}

// a line of BASIC: typed, waited for its echo (`echo`: a part before any $, shown as a currency sign)
static void line(Machine& m, const char* text, const char* echo)
{
    m.type_text(text);
    m.type_text("\r");
    bool ok = run_until_text(m, echo, 10.0);
    if (!ok) printf("NO ECHO %s:\n%s\n", echo, screen_text(m).c_str());
    CHECK(ok);
    m.run_seconds(0.5);
}

// the last "V..." reply on the screen as a number (NAN when none)
static double last_reply(Machine& m, std::string* raw = nullptr)
{
    std::string s = screen_text(m);
    size_t p = s.rfind("V+");
    size_t q = s.rfind("V-");
    if (q != std::string::npos && (p == std::string::npos || q > p)) p = q;
    if (p == std::string::npos) return NAN;
    size_t e = s.find('\n', p);
    std::string r = s.substr(p, e == std::string::npos ? std::string::npos : e - p);
    while (!r.empty() && r.back() == ' ') r.pop_back();
    if (raw) *raw = r;
    return atof(r.c_str() + 1);
}

static void example(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file(MACHINE, err));
    Ims2Bench* b = dynamic_cast<Ims2Bench*>(m.plant.get());
    CHECK(b != nullptr);
    if (!b || !start_basims(m)) return;
    // the voltmeter measures on its own (START REP) before the program
    printf("M1T 330 display: '%s'\n", b->m1t.display().c_str());
    CHECK(b->m1t.valid && fabs(b->m1t.value - 12.564) < 0.01 && b->m1t.range == 2);
    line(m, "10 IMS# N", "10 IMS");
    line(m, "20 IMS# O3;\"R4D0E\"", "R4D0E");
    line(m, "30 IMS# T3", "30 IMS");
    line(m, "40 IMS# I A$", "40 IMS");
    line(m, "50 PRINT A$", "50 PRINT");
    m.type_text("RUN\r");
    m.run_seconds(3.0);
    std::string r;
    double v = last_reply(m, &r);
    printf("RUN: %s (%g)\n", r.c_str(), v);
    CHECK(fabs(v - 12.564) < 0.005);
    CHECK(r.size() == 11 && r.compare(0, 2, "V+") == 0 && r.compare(8, 3, "E+1") == 0);   // V+1.2564E+1
    CHECK(!b->m1t.remote);   // BASIMS lets REN go at the end (KONVS): local again
    // the bus: the addressing with ATN, the reply with EOI on its last byte
    std::vector<Ims2Event> ev;
    b->ims.log_copy(ev);
    bool lad3 = false, tad3 = false, unl = false, eoi_lf = false;
    for (const Ims2Event& e : ev) {
        if (!e.transfer) continue;
        if ((e.lines & IMS_ATN) && e.byte == 0x23) lad3 = true;
        if ((e.lines & IMS_ATN) && e.byte == 0x43) tad3 = true;
        if ((e.lines & IMS_ATN) && e.byte == 0x3F) unl = true;
        if (!(e.lines & IMS_ATN) && (e.lines & IMS_EOI) && e.byte == '\n' && e.from == "M1T 330") eoi_lf = true;
    }
    CHECK(lad3 && tad3 && unl && eoi_lf);

    // autorange down: 123.45 mV on the 300 mV range
    b->bs.out[0].u_set = 0.12345;
    b->changed();
    m.run_seconds(0.5);
    m.type_text("RUN\r");
    m.run_seconds(3.0);
    v = last_reply(m, &r);
    printf("RUN 0.12345 V: %s (%g)\n", r.c_str(), v);
    CHECK(fabs(v - 0.12345) < 0.0002 && r.find("E-1") != std::string::npos);
    CHECK(b->m1t.range == 0);

    // a load above the current limit: the supply holds 0.1 A, 100 Ohm -> 10 V
    b->bs.out[0].u_set = 20.0;
    b->bs.out[0].i_lim = 0.1;
    b->bs.out[0].load = 3;   // 100 Ohm
    b->changed();
    CHECK(b->bs.out[0].limiting && fabs(b->bs.out[0].u - 10.0) < 1e-9);
    m.type_text("RUN\r");
    m.run_seconds(3.0);
    v = last_reply(m, &r);
    printf("RUN limited: %s (%g)\n", r.c_str(), v);
    CHECK(fabs(v - 10.0) < 0.005);
}

// a state saved while BASIMS measures in a loop, loaded into a new machine: the voltmeter stays remote and no
// byte of its replies is lost (the DPS-1 and the voltmeter attach to the bench bus without resolving the lines,
// before: the bus without the drives of the DPS-1 for a moment dropped REN, the 'V' of the next reply was lost)
static void state_round_trip(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file(MACHINE, err));
    if (!start_basims(m)) return;
    line(m, "10 IMS# N", "10 IMS");
    line(m, "20 IMS# O3;\"R4D0E\"", "R4D0E");
    line(m, "30 IMS# T3", "30 IMS");
    line(m, "40 IMS# I A$", "40 IMS");
    line(m, "50 PRINT A$", "50 PRINT");
    line(m, "60 GOTO 20", "60 GOTO");
    m.type_text("RUN\r");
    m.run_seconds(1.7);
    std::vector<uint8_t> st;
    CHECK(m.save_state(st, "", err));
    Machine m2;
    CHECK(m2.load_file(MACHINE, err));
    m2.run_seconds(0.5);   // the monitor runs, the DPS-1 is not cabled yet
    CHECK(m2.load_state(st, err));
    Ims2Bench* b = dynamic_cast<Ims2Bench*>(m2.plant.get());
    Ims2Bench* b1 = dynamic_cast<Ims2Bench*>(m.plant.get());
    CHECK(b && b1 && b->ims.ctl_lines == b1->ims.ctl_lines && b->ims.nodes.size() == 2);
    m2.run_seconds(2.0);
    std::string s = screen_text(m2);
    int replies = 0, broken = 0;
    for (size_t p = s.find("E+1"); p != std::string::npos; p = s.find("E+1", p + 1)) {
        size_t bol = s.rfind('\n', p);
        bol = bol == std::string::npos ? 0 : bol + 1;
        replies++;
        if (s[bol] != 'V') broken++;
    }
    printf("after the state: %d replies, %d broken\n%s\n", replies, broken, s.c_str());
    CHECK(replies >= 6 && broken == 0);
}

static void nobody_listens(void)
{
    std::string t, err;
    CHECK(file_read_text(data_path(MACHINE), t));
    json cfg = json::parse(t, nullptr, false, true);
    cfg.erase("plant");   // the cable of X2 goes nowhere
    Machine m;
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, "SAPI-1 V4.0", 5.0));
    // the card alone, through its ports: controller, talker, a byte out
    m.bus.io_write(0x33, 0x92);   // D2C mode 0: PA, PB in, PC out (RER = 0: PON cleared, drivers on)
    m.bus.io_write(0x33, 0x0B);   // CEN
    m.bus.io_write(0x33, 0x0E);   // TALK
    m.bus.io_write(0x37, 0x94);   // D1C: port B strobed output
    CHECK((m.bus.io_read(0x30) & 0x60) == 0x00);   // no ERR, PON 0
    m.bus.io_write(0x35, 0x55);
    m.run_seconds(0.001);
    uint8_t pa = m.bus.io_read(0x30), pb = m.bus.io_read(0x31), pc = m.bus.io_read(0x36);
    printf("without the bench: PA %02X PB %02X PC(D1C) %02X\n", pa, pb, pc);
    CHECK(!(pa & 0x20));        // no ERR
    CHECK(!(pb & 0x80));        // no DAV
    CHECK(pc & 0x02);           // /OBF B high: the byte was "taken" by the released NDAC

    // BASIMS falls
    Machine m2;
    CHECK(m2.load_json(cfg, err));
    if (!start_basims(m2)) return;
    line(m2, "IMS# O3;\"E\"", "\"E\"");
    m2.run_seconds(2.0);
    std::string s = screen_text(m2);
    printf("BASIMS without the bench:\n%s\n", s.c_str());
    CHECK(s.find("BASIC") == std::string::npos && s.find("SAPI-1 V4.0") != std::string::npos);
}

// MIKOS reads a key only some time after printing a question
static bool answer(Machine& m, const char* prompt, const char* keys, double timeout_s)
{
    bool ok = run_until_text(m, prompt, timeout_s);
    if (!ok) printf("NO PROMPT %s:\n%s\n", prompt, screen_text(m).c_str());
    m.run_seconds(0.6);
    m.type_text(keys);
    return ok;
}

static void kazeta(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_file(MACHINE, err));
    Dsm1* dsm = nullptr;
    for (auto& c : m.cards)
        if (Dsm1* d = dynamic_cast<Dsm1*>(c.get())) dsm = d;
    CHECK(dsm && dsm->tape()->loaded() && dsm->tape()->write_protect);
    if (!dsm) return;
    CHECK(run_until_text(m, "SAPI-1 V4.0", 5.0));
    m.run_seconds(0.8);
    m.type_text("KL");
    CHECK(answer(m, "NAME", "BASIMS\r", 3.0));
    CHECK(answer(m, "PLAY  DONE?", "", 3.0));
    dsm->tape()->mode = TapeDeck::PLAY;   // PLAY on the recorder, then Y
    m.type_text("Y");
    m.run_seconds(2.0);
    for (int i = 0; i < 200 && dsm->motor_on(); i++) m.run_seconds(0.5);
    m.run_seconds(1.0);
    m.type_text("J");
    CHECK(run_until_text(m, "READY", 5.0));
    m.run_seconds(0.8);
    m.type_text("LOAD\r");
    CHECK(answer(m, "NAME", "PRIKLAD\r", 3.0));
    CHECK(answer(m, "PLAY  DONE?", "Y", 3.0));
    m.run_seconds(2.0);
    for (int i = 0; i < 100 && dsm->motor_on(); i++) m.run_seconds(0.5);
    m.run_seconds(1.0);
    m.type_text("RUN\r");
    m.run_seconds(3.0);
    std::string r;
    double v = last_reply(m, &r);
    printf("cassette: %s (%g)\n%s\n", r.c_str(), v, screen_text(m).c_str());
    CHECK(fabs(v - 12.564) < 0.005);
}

int main(void)
{
    test_init();
    const char* part = getenv("DPS_PART");   // ctest runs the parts in parallel (tests/CMakeLists.txt)
    if (!part || !strcmp(part, "bus")) {
        example();
        state_round_trip();
        nobody_listens();
    }
    if (!part || !strcmp(part, "kazeta")) kazeta();
    return TEST_RESULT();
}
