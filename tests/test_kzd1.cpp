// KZD-1 cassette data recorder on DPP-1 X2: the mechanism driven through the 8255 like MIKOS/K
// drives it (recording bits, rewind to the motor overload, reading the pulses back, the write tab),
// and MIKOS/K V5.0 itself: K_I, K_S, K_D, K_Z, K_L and a write without the tab.
#include "cards/dpp1.h"
#include "chips/kzd1.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

#include <math.h>
#include <string.h>

using nlohmann::json;

static void wait_us(Machine& m, double us)   // bus time goes on (no program runs here)
{
    m.bus.now += (uint64_t)(us * m.bus.cpu_hz / 1e6);
    m.bus.advance_all();
}

static void test_mechanism(void)
{
    std::string file = data_path("work/test/kzd_unit.kzd");
    file_remove(file);
    json cfg = { { "cards", json::array() } };
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    cfg["cards"].push_back({ { "slot", 2 }, { "type", "DPP-1" }, { "base", "30h" }, { "kzd1", { { "cassette", file } } } });
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    Kzd1* k = m.cards[1]->kzd1();
    CHECK(k && k->loaded());
    if (!k) return;
    m.bus.io_write(0x33, 0x8B);              // PA out, PB in, PC in
    m.bus.io_write(0x30, 0xFF);              // all off
    CHECK_EQ(m.bus.io_read(0x31) & 0xF8, 0x08);   // no pulses, no overload, ZAZN. U.V.* high in read mode
    // record forward with the tab: ZAZN. U.V.* follows ZAZNAM* low = allowed
    m.bus.io_write(0x30, 0xAE);
    CHECK_EQ(m.bus.io_read(0x31) & 0x08, 0);
    wait_us(m, 200000);                      // up to speed, erase a gap
    static const uint8_t data[] = { 0xFB, 0x01, 0x41, 0x00, 0xBE, 0x55, 0x00, 0xFF, 0x3C, 0xA5 };
    for (uint8_t b : data)
        for (int i = 7; i >= 0; i--) {       // PISBYT: data on PA7 with D.P.* high, then D.P.* low
            uint8_t v = (uint8_t)((((b >> i) & 1) << 7) | 0x2E);
            m.bus.io_write(0x30, v);
            m.bus.io_write(0x30, v ^ 0x20);
            wait_us(m, 50);
            m.bus.io_write(0x30, v);
            wait_us(m, 70);
        }
    wait_us(m, 100000);                      // a gap after
    m.bus.io_write(0x30, 0xFF);              // stop
    wait_us(m, 100000);
    CHECK(k->transitions() >= 80);
    // rewind (B3h) until motor 1 reports the overload at the start of the tape
    m.bus.io_write(0x30, 0xB3);
    int t = 0;
    while (!(m.bus.io_read(0x31) & 0x20) && t < 600) {
        wait_us(m, 10000);
        t++;
    }
    CHECK(t < 600);
    CHECK(k->position_m() < 0.001);
    m.bus.io_write(0x30, 0xFF);
    wait_us(m, 100000);
    CHECK_EQ(m.bus.io_read(0x31) & 0x30, 0);  // released
    // read forward (FEh): every pulse is a bit, PB7 tells which track
    m.bus.io_write(0x30, 0xFE);
    std::vector<int> bits;
    bool was = false;
    for (int i = 0; i < 200000 && bits.size() < 80; i++) {
        uint8_t v = m.bus.io_read(0x31);
        bool p = (v & 0xC0) != 0;
        if (p && !was) bits.push_back((v & 0x80) ? 1 : 0);
        was = p;
        wait_us(m, 12);
    }
    CHECK_EQ(bits.size(), 80u);
    for (size_t i = 0; i < sizeof(data) && i * 8 + 7 < bits.size(); i++) {
        int b = 0;
        for (int j = 0; j < 8; j++) b = b << 1 | bits[i * 8 + j];
        CHECK_EQ(b, data[i]);
    }
    m.bus.io_write(0x30, 0xFF);
    // without the tab ZAZN. U.V.* stays high in record mode
    k->set_tab(0, false);
    m.bus.io_write(0x30, 0xAF);
    CHECK((m.bus.io_read(0x31) & 0x08) != 0);
    m.bus.io_write(0x30, 0xFF);
    // the cassette file keeps it
    battery_files_enable(true);
    k->flush();
    battery_files_enable(false);
    KzdCassette c;
    CHECK(c.load(file, err));
    CHECK(c.tr[0].size() >= 80 && !c.tab[0] && c.tab[1]);
    // the state refers to the cassette: one inserted since is replaced on loading, the tape stands where it stood
    m.bus.io_write(0x30, 0xFE);
    wait_us(m, 300000);
    m.bus.io_write(0x30, 0xFF);
    wait_us(m, 100000);
    double pos = k->position_m();
    CHECK(pos > 0.1);
    std::vector<uint8_t> st;
    CHECK(m.save_state(st, "", err));
    std::string other = data_path("work/test/kzd_other.kzd");
    file_remove(other);
    CHECK(k->insert(other, err));
    CHECK(m.load_state(st, err));
    CHECK(k->loaded() && k->path() == file);
    CHECK(fabs(k->position_m() - pos) < 1e-6);
    file_remove(other);
    file_remove(file);
    // an empty cassette (the header only) loads back
    KzdCassette e;
    std::string ef = data_path("work/test/kzd_empty.kzd");
    CHECK(e.save(ef, err) && e.load(ef, err) && e.tr[0].empty() && e.tr[1].empty());
    file_remove(ef);
}

static std::string screen(Machine& m)
{
    std::string s;
    for (const std::string& r : m.display->text_rows()) s += r + "\n";
    return s;
}

// the monitor waits for a command: its last line is the prompt "."
static bool prompt(Machine& m)
{
    std::vector<std::string> rows = m.display->text_rows();
    for (int i = (int)rows.size() - 1; i >= 0; i--) {
        std::string r = rows[(size_t)i];
        while (!r.empty() && r.back() == ' ') r.pop_back();
        if (r.empty()) continue;
        return r == ".";
    }
    return false;
}

static bool command(Machine& m, const char* keys, double timeout_s)
{
    m.type_text(keys);
    m.run_seconds(0.5);
    for (double t = 0; t < timeout_s; t += 0.25) {
        if (prompt(m)) return true;
        m.run_seconds(0.25);
    }
    fprintf(stderr, "\"%s\" did not end:\n%s", keys, screen(m).c_str());
    return false;
}

static void test_mikos_k(void)
{
    std::string file = "work/test/kzd_mikosk.kzd";
    file_remove(data_path(file));
    std::string text, err;
    CHECK(file_read_text(data_path("machines/zps/ZPS-2_MIKOS_K.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"])
        if (c["type"] == "DPP-1") c["kzd1"]["cassette"] = file;
    Machine m;
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, "MIKOS/K V5.0", 5.0));
    CHECK(run_until_text(m, "MONITOR V5.0", 5.0));
    m.run_seconds(0.5);
    // K_I: prepare the tape and name it
    m.type_text("KI");
    CHECK(run_until_text(m, "NAZEV:", 60.0));
    CHECK(command(m, "PASKA1\r", 30.0));
    // K_S: 1 KB from 5000h
    for (int i = 0; i < 1024; i++) m.poke((uint16_t)(0x5000 + i), (uint8_t)(i * 7 + 3));
    m.type_text("KS");
    CHECK(run_until_text(m, "NAZEV:", 30.0));
    m.type_text("VZOREK\r");
    m.run_seconds(1.0);
    CHECK(run_until_text(m, "=", 30.0));
    CHECK(command(m, "5000 53FF\r", 60.0));
    CHECK(screen(m).find("CHYBA") == std::string::npos);
    // K_D: the directory
    CHECK(command(m, "KD", 60.0));
    std::string s = screen(m);
    CHECK(s.find("00 PASKA1") != std::string::npos);
    CHECK(s.find("01 VZOREK") != std::string::npos);
    // K_Z, K_L: back into the cleared memory. K_Z, not K_R: K_R stops at the very start of the tape and
    // the search of K_L then reads the 55h lead-in of K_I as a bad block, which MIKOS/K (6.4.88 and the
    // 18.5.88 source alike) takes for a header and compares with the stale buffer; after K_D it still
    // holds the header of VZOREK, so the load starts at the wrong place and ends in NENI KONEC SOUBORU.
    for (int i = 0; i < 1024; i++) m.poke((uint16_t)(0x5000 + i), 0);
    CHECK(command(m, "KZ", 30.0));
    m.type_text("KL");
    CHECK(run_until_text(m, "NAZEV:", 30.0));
    CHECK(command(m, "VZOREK\r", 60.0));
    CHECK(screen(m).find("CHYBA") == std::string::npos);
    if (screen(m).find("CHYBA") != std::string::npos) fprintf(stderr, "%s", screen(m).c_str());
    int bad = 0;
    for (int i = 0; i < 1024; i++)
        if (m.peek((uint16_t)(0x5000 + i)) != (uint8_t)(i * 7 + 3)) bad++;
    CHECK_EQ(bad, 0);
    // the tab of side A broken out: MIKOS/K refuses to record
    for (auto& c : m.cards)
        if (c->kzd1()) c->kzd1()->set_tab(0, false);
    m.type_text("KS");
    CHECK(run_until_text(m, "NAZEV:", 30.0));
    CHECK(command(m, "DRUHY\r", 60.0));
    CHECK(screen(m).find("ZAZNAM NENI POVOLEN") != std::string::npos);
    file_remove(data_path(file));
}

int main(void)
{
    test_init();
    test_mechanism();
    test_mikos_k();
    return TEST_RESULT();
}
