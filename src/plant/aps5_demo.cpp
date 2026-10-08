#include "plant/aps5_demo.h"
#include "cards/hrk1.h"
#include "core/machine.h"

#include <stdio.h>

const double Aps5Demo::HEAT_S = 25 * 60;

static const char* const MENU = "ZOBRAZENI TABULKY PORUCH";   // the last line of the main menu
static const double TIMEOUT_S = 20;     // no answer: something else is on the screen
static const double SETTLE_S = 0.4;     // the expected text stays this long (the screen is drawn)
static const double STALE_S = 2.5;      // the screen did not change: taken as answered after this

static std::string screen(Machine& m)
{
    std::string s;
    if (m.display)
        for (const std::string& r : m.display->text_rows()) s += r + "\n";
    return s;
}

// The line of the program editor after a segment was added: PROG, SEG, CAS, TEPLOTA, "P" (last).
static std::string segment_line(int seg, int minutes, int temp)
{
    char b[48];
    snprintf(b, sizeof(b), "%6d%5d%9d P", seg, minutes, temp);
    return b;
}

bool Aps5Demo::start(Machine& m, std::string& err)
{
    Hrk1* panel = dynamic_cast<Hrk1*>(m.keyboard());
    if (!panel || !m.display) {
        err = "no APS-5 panel (keyboard HRK-1)";
        return false;
    }
    panel->lock = 2;   // the reserved commands need the key in position 2
    panel->clear();
    list.clear();
    std::string s = screen(m);
    if (s.find(MENU) == std::string::npos) list.push_back({ "M\r", MENU, "Back to the menu" });
    list.push_back({ "1\r", "VSECHNY PECE VYPNUTY", "Switching all furnaces off" });
    list.push_back({ "2\r", MENU, "Switching all furnaces off" });
    list.push_back({ "6R\r", "U [MV]", "Calibration" });
    list.push_back({ "9990\r", MENU, "Calibration" });
    list.push_back({ "2\r", "(1-32)", "Program 1" });
    list.push_back({ "1\r", "REZERVA", "Program 1" });
    // delete it: the reserve is all 512 segments; an empty program 1 gives CHYBA PARAMETRU. A program
    // assigned to a furnace cannot be deleted (CHYBA PRISTUPU): when it starts as the demo program
    // (a demo before) it stays, another one is not overwritten.
    size_t del = list.size();
    std::string first = segment_line(1, 20, 700);
    list.push_back({ "7R\r", "512", "Program 1", "CHYBA PARAMETRU", "CHYBA PRISTUPU", -1,
                     first.substr(0, first.size() - 2) });
    static const int seg[3][2] = { { 20, 700 }, { 20, 700 }, { 20, 300 } };
    for (int i = 0; i < 3; i++) {
        char t[8], c[8];
        snprintf(t, sizeof(t), "%d\r", seg[i][0]);
        snprintf(c, sizeof(c), "%d\r", seg[i][1]);
        list.push_back({ "3R\r", "CAS [MIN]", "Program 1" });
        list.push_back({ t, "TEPLOTA:", "Program 1" });
        list.push_back({ c, segment_line(i + 1, seg[i][0], seg[i][1]), "Program 1" });
    }
    list[del].skip_to = (int)list.size();
    list.push_back({ "M\r", MENU, "Program 1" });
    static const struct { int n, tvz; const char* label; } reg[3] = {
        { 1, 6, "Furnace 1: program 1" }, { 2, 9, "Furnace 2: program 1" }, { 5, 8, "Furnace 5: program 1" } };
    for (const auto& f : reg) {
        char n[8], t[8];
        snprintf(n, sizeof(n), "%d\r", f.n);
        snprintf(t, sizeof(t), "%d\r", f.tvz);
        list.push_back({ "0\r", "(1-8)", f.label });
        list.push_back({ n, MENU, f.label });
        list.push_back({ "8R\r", "PTRH30", f.label });
        list.push_back({ "3\r", MENU, f.label });            // ch-a
        list.push_back({ "4R\r", "TVZ [10S] =", f.label });
        list.push_back({ t, MENU, f.label });
        list.push_back({ "7R\r", "TVYP [MIN] =", f.label });
        list.push_back({ "60\r", MENU, f.label });
        list.push_back({ "9R\r", "(1-32)", f.label });
        list.push_back({ "1\r", MENU, f.label });
        list.push_back({ "1\r", "AKTUALNI PEC ZAPNUTA", f.label });
        list.push_back({ "1\r", MENU, f.label });
    }
    static const struct { int n, tc, power; const char* label; } man[2] = {
        { 4, 4, 70, "Furnace 4: 70 % by hand" }, { 6, 2, 50, "Furnace 6: 50 % by hand" } };
    for (const auto& f : man) {
        char n[8], tc[8], p[8];
        snprintf(n, sizeof(n), "%d\r", f.n);
        snprintf(tc, sizeof(tc), "%d\r", f.tc);
        snprintf(p, sizeof(p), "%d\r", f.power);
        list.push_back({ "0\r", "(1-8)", f.label });
        list.push_back({ n, MENU, f.label });
        list.push_back({ "8R\r", "PTRH30", f.label });
        list.push_back({ tc, MENU, f.label });
        list.push_back({ "3\r", "P[%] =", f.label });
        list.push_back({ p, MENU, f.label });
    }
    list.push_back({ "11\r", "VYVOLANI MENU", "State of the furnaces" });
    steps = (int)list.size();
    step = 0;
    typed = false;
    error.clear();
    label = list[0].label;
    phase = TYPING;
    return true;
}

void Aps5Demo::poll(Machine& m)
{
    double now = m.bus.seconds();
    if (phase == HEATING) {
        heat_left = heat_end - now;
        if (heat_left <= 0) {
            heat_left = 0;
            phase = DONE;
        }
        return;
    }
    if (phase != TYPING) return;
    Hrk1* panel = dynamic_cast<Hrk1*>(m.keyboard());
    if (!panel || !m.display) {
        error = "the panel is gone";
        phase = FAILED;
        return;
    }
    const Step& st = list[(size_t)step];
    label = st.label;
    if (!typed) {
        before = screen(m);
        m.type_text(st.keys);
        typed = true;
        keys_at = now;
        seen_at = -1;
        return;
    }
    if (panel->pending()) {   // the keys go down one after another
        keys_at = now;
        return;
    }
    std::string s = screen(m);
    if (!st.skip.empty() && s.find(st.skip) != std::string::npos && before.find(st.skip) == std::string::npos) {
        if (before.find(st.need) == std::string::npos) {
            error = "program 1 is assigned to a furnace and is not the demo program";
            phase = FAILED;
            return;
        }
        typed = false;   // the program is there already
        step = st.skip_to;
        return;
    }
    for (const char* bad : { "CHYBA PRISTUPU", "CHYBA PARAMETRU" })
        if (st.alt != bad && s.find(bad) != std::string::npos && before.find(bad) == std::string::npos) {
            error = bad;
            phase = FAILED;
            return;
        }
    bool found = s.find(st.expect) != std::string::npos ||
                 (!st.alt.empty() && s.find(st.alt) != std::string::npos && before.find(st.alt) == std::string::npos);
    bool answered = found && (s != before || now - keys_at > STALE_S);
    if (!answered) {
        seen_at = -1;
        if (now - keys_at > TIMEOUT_S) {
            error = "no \"" + st.expect + "\" on the screen";
            phase = FAILED;
        }
        return;
    }
    if (seen_at < 0) seen_at = now;
    if (now - seen_at < SETTLE_S) return;
    typed = false;
    if (++step >= steps) {
        phase = HEATING;
        heat_end = now + HEAT_S;
        heat_left = HEAT_S;
    }
}
