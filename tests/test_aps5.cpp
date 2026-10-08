// APS-5: the thermocouples, the cards ADP-1, DMT-1, DRV-1A and the original firmware with the
// front panel HRK-1 and the furnace plant (machines/aps5.sapi): start, calibration, measurement,
// the key switch, power down with the warning, a restart without it, a mains failure, regulation.
#include "cards/hrk1.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "plant/furnaces.h"
#include "plant/thermocouple.h"
#include "test_util.h"

#include <math.h>
#include <string.h>

using nlohmann::json;

static void test_thermocouples(void)
{
    // NIST ITS-90 (K, S, B), DIN 43710 (Fe-Ko), GOST (ch-k)
    CHECK(fabs(tc_emf_mv(TC_CHA, 1000) - 41.276) < 0.002);
    CHECK(fabs(tc_emf_mv(TC_CHA, 100) - 4.096) < 0.002);
    CHECK(fabs(tc_emf_mv(TC_S, 1000) - 9.587) < 0.002);
    CHECK(fabs(tc_emf_mv(TC_B, 1500) - 10.099) < 0.002);
    CHECK(fabs(tc_emf_mv(TC_FEKO, 400) - 22.16) < 0.001);
    CHECK(fabs(tc_emf_mv(TC_CHK, 500) - 40.299) < 0.001);
    for (int t = 0; t < TC_COUNT; t++)
        CHECK(fabs(tc_temp_c(t, tc_emf_mv(t, 600)) - 600) < 0.01);
    CHECK_EQ(tc_parse("PtRh30-PtRh6"), TC_B);
}

// The cards alone: DRV-1A switches the heaters, DMT-1 amplifies the thermocouple of a channel,
// ADP-1 converts it.
static void test_cards(void)
{
    json cfg = { { "cards", json::array() }, { "plant", { { "type", "APS-5" } } } };
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    cfg["cards"].push_back({ { "slot", 2 }, { "type", "ADP-1" }, { "base", "30h" }, { "wiring", "aps5" } });
    cfg["cards"].push_back({ { "slot", 3 }, { "type", "DMT-1" }, { "base", "80h" } });
    cfg["cards"].push_back({ { "slot", 4 }, { "type", "DRV-1A" }, { "load", "plant" } });
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    FurnacePlant* p = dynamic_cast<FurnacePlant*>(m.plant.get());
    CHECK(p != nullptr);
    if (!p) return;
    p->furnaces[1].th = p->furnaces[1].tch = p->furnaces[1].tm = 500;   // Pec 2: ch-a
    m.bus.io_write(0x80, 0x08 | 1);           // K2, gain 199
    m.run_seconds(0.7);                       // settled
    m.bus.io_write(0x31, 0x00);               // S1 = U1, gain 1
    CHECK((m.bus.io_read(0x31) & 0x80) == 0); // busy
    m.run_seconds(0.03);
    uint8_t hi = m.bus.io_read(0x31), lo = m.bus.io_read(0x30);
    CHECK(hi & 0x80);
    int digits = (hi & 0x0F) * 100 + (lo >> 4) * 10 + (lo & 0x0F);
    double expect = (tc_emf_mv(TC_CHA, 500) - tc_emf_mv(TC_CHA, 25)) * 199 / 10.0;   // 10 mV a digit
    CHECK(fabs(digits - expect) <= 1.5);
    m.bus.io_write(0x31, 0x01);               // S2 = U2: -2.50 V for the terminal block at 25 degC
    m.run_seconds(0.03);
    hi = m.bus.io_read(0x31);
    lo = m.bus.io_read(0x30);
    CHECK((hi & 0x10) != 0);                  // minus
    CHECK_EQ((hi & 0x0F) * 100 + (lo >> 4) * 10 + (lo & 0x0F), 250);
    m.bus.io_write(0x31, 0x06);               // S7 = KAL +9.99 V
    m.run_seconds(0.03);
    CHECK_EQ(m.bus.io_read(0x31) & 0x1F, 9);
    CHECK_EQ(m.bus.io_read(0x30), 0x99);
    m.bus.io_write(0x77, 0x05);               // heaters of Pec 1 and Pec 3
    CHECK_EQ(p->heaters, 0x05);
    double t0 = p->furnaces[0].tch;
    m.run_seconds(60);
    CHECK(p->furnaces[0].tch > t0 + 1);
    m.reset();                                // /RES opens all contacts
    CHECK_EQ(p->heaters, 0);
}

static json aps5_config(const std::string& crm_file)
{
    std::string text;
    CHECK(file_read_text(data_path("machines/aps5.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"]) {
        if (c["type"] == "CRM-1") c["file"] = crm_file;
        if (c["type"] == "RTC-1") {
            c["time"] = "emulated";
            c["battery"] = false;
        }
    }
    return cfg;
}

static std::string screen(Machine& m)
{
    std::string s;
    for (const std::string& r : m.display->text_rows()) s += r + "\n";
    return s;
}

// Keys of the panel (0-9, M, R, \b = <-, \r = ->), then run until `expect` is on the screen. The
// firmware is slow to answer: the measurement task waits 600 ms for DMT-1 in each second.
static bool step(Machine& m, const char* k, const char* expect, double timeout = 10.0)
{
    m.type_text(k);
    m.run_seconds(1.0);   // the keys go down one after another
    bool ok = run_until_text(m, expect, timeout);
    if (!ok) fprintf(stderr, "keys \"%s\": no \"%s\" on the screen:\n%s", k, expect, screen(m).c_str());
    m.run_seconds(0.5);
    return ok;
}

// The temperature in the row of a furnace in the table of command 11.
static int shown_temp(Machine& m, int furnace)
{
    for (const std::string& r : m.display->text_rows())
        if (r.size() > 30 && r[0] == '0' + furnace && r[2] == '*') {
            int sg, mn, sc, t;
            if (sscanf(r.c_str() + 3, "%d I %d:%d I %d", &sg, &mn, &sc, &t) == 4) return t;
        }
    return -9999;
}

static void test_firmware(void)
{
    battery_files_enable(true);
    std::string file = "work/test/aps5_crm.bin";
    file_remove(data_path(file));
    std::string err;
    {
        Machine m;
        CHECK(m.load_json(aps5_config(file), err));
        CHECK(run_until_text(m, "ZOBRAZENI TABULKY PORUCH", 10.0));   // cold start, the menu
        m.run_seconds(1.0);
        Hrk1* panel = dynamic_cast<Hrk1*>(m.keyboard());
        CHECK(panel != nullptr);
        if (!panel) return;
        CHECK(panel->relay());                                    // the watchdog runs
        // the key switch in position 1: R does nothing, the command lacks it
        panel->lock = 1;
        CHECK(step(m, "8R\r", "PRIKAZ IGNOROVAN"));
        panel->lock = 2;
        const char* menu = "ZOBRAZENI TABULKY PORUCH";
        CHECK(step(m, "8R\r", "PTRH30 - PTRH6"));                 // thermocouple of Pec 1: ch-a
        CHECK(step(m, "3\r", menu));
        CHECK(step(m, "6R\r", "U [MV]"));                         // calibration: KAL = 9990 mV
        CHECK(step(m, "9990\r", menu));
        FurnacePlant* p = dynamic_cast<FurnacePlant*>(m.plant.get());
        Furnace& f = p->furnaces[0];
        f.th = f.tch = f.tm = 650;
        f.tau_s = 1e6;                                            // it keeps its temperature
        f.heater_tau_s = 1e5;
        CHECK(step(m, "11\r", "VYVOLANI MENU"));
        m.run_seconds(22.0);                                      // measured every 10 s
        int t = shown_temp(m, 1);
        CHECK(abs(t - 650) <= 2);
        if (abs(t - 650) > 2) fprintf(stderr, "Pec 1 shows %d\n%s", t, screen(m).c_str());
        // a program: 20 min to 300 degC; TVZ 30 s, Pec 1 on
        f.th = f.tch = f.tm = 20;
        f.tau_s = 480;
        f.heater_tau_s = 30;
        CHECK(step(m, "M\r", menu));
        CHECK(step(m, "2\r", "(1-32)"));
        CHECK(step(m, "1\r", "REZERVA"));
        CHECK(step(m, "3R\r", "CAS [MIN]"));
        m.type_text("20\r");
        m.run_seconds(3.0);
        CHECK(step(m, "300\r", "300 P"));
        CHECK(step(m, "M\r", menu));
        CHECK(step(m, "9R\r", "(1-32)"));
        CHECK(step(m, "1\r", menu));
        CHECK(step(m, "4R\r", "TVZ [10S] ="));
        CHECK(step(m, "3\r", menu));
        CHECK(step(m, "1\r", "AKTUALNI PEC ZAPNUTA"));
        CHECK(step(m, "1\r", menu));
        CHECK(step(m, "11\r", "VYVOLANI MENU"));
        m.run_seconds(240.0);                                     // four minutes of the program
        CHECK(f.tch > 40);                                        // heated
        CHECK(screen(m).find("1 *  1 I") != std::string::npos);   // segment 1 running
        // the mains fails for 10 s: warning, no power, start again; the outage is reported
        panel->outage(10);
        m.run_seconds(14.0);
        CHECK(panel->powered());
        CHECK(run_until_text(m, "VYPADEK NAPAJENI", 10.0));      // (the allowed outage is 0: Pec 1 off)
        m.power_down();                                           // switched off as the mains goes
        CHECK_EQ(m.peek(0xE239), 0x55);                           // the firmware saved its state
    }
    {   // a warm start: no error, the thermocouple and the program are there
        Machine m;
        CHECK(m.load_json(aps5_config(file), err));
        CHECK(run_until_text(m, "ZOBRAZENI TABULKY PORUCH", 12.0));
        CHECK(screen(m).find("CHYBA") == std::string::npos);
        CHECK(step(m, "10\r", "TERMOCLANEK"));
        CHECK(screen(m).find("TERMOCLANEK     3") != std::string::npos);
        CHECK(screen(m).find("PROGRAM         1") != std::string::npos);
    }   // destroyed without the warning (flush only): the program does not know
    {
        Machine m;
        CHECK(m.load_json(aps5_config(file), err));
        CHECK(run_until_text(m, "INICIALIZACE", 12.0));           // "CHYBA n", waits for M ->
        CHECK(screen(m).find("CHYBA") != std::string::npos);
        CHECK(step(m, "M\r", "ZOBRAZENI TABULKY PORUCH", 15.0));
        CHECK(step(m, "10\r", "TERMOCLANEK"));
        CHECK(screen(m).find("TERMOCLANEK     0") != std::string::npos);   // the cold start cleared it
    }
    file_remove(data_path(file));
    battery_files_enable(false);
}

int main(void)
{
    test_init();
    test_thermocouples();
    test_cards();
    test_firmware();
    return TEST_RESULT();
}
