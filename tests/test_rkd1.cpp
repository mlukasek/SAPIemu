// RKD-1 cartridge disk controller (docs/desky/RKD-1.md) with the original software of Tesla Elstroj 1987:
// the formatter FORMTRKD, the system generator ES (MOVCPM with the CBIOS), the boot ROM ROMES.
// RKD_MAKE=<file>: also save the system cartridge made here (media/rkd-1/tesla_cpm_system.rkd).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cards/rkd1.h"
#include "core/bus.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static const char* MERCL = "../SAPI_DOKU/SAPIMAN/PMD32-SD_IMAGE8/ZDROJ_STARE/a_mercl.8sd";
static const char* SERVICE = "../SAPI_DOKU/SAPIMAN/PMD32-SD_IMAGE8/SYSPROG/a_sluzebni CPM.8sd";   // PIP, STAT...
static const char* CART = "work/test/rkd_cart.rkd";

static Rkd1* rkd_of(Machine& m)
{
    Rkd1* r = (Rkd1*)m.find_card("RKD-1");
    CHECK(r != nullptr);
    return r;
}

// CP/M of J. Biba from an RPD-1 floppy, B: = the Tesla Elstroj disk, an RKD-1 with a new cartridge
static bool jb_with_rkd(Machine& m)
{
    std::string t, err;
    CHECK(file_read_text(data_path("machines/cpm_boot_rpd1.sapi"), t));
    json cfg = json::parse(t, nullptr, false, true);
    std::vector<uint8_t> img;
    if (!file_read_all(data_path(MERCL), img)) {
        printf("SKIP: %s not found\n", MERCL);
        return false;
    }
    CHECK(file_write_all(data_path("work/test/mercl.img"), img.data(), img.size()));
    CHECK(file_read_all(data_path("media/sapiman/r1_zrmd4m_271111.img"), img));
    CHECK(file_write_all(data_path("work/test/jb_a.img"), img.data(), img.size()));
    for (json& c : cfg["cards"]) {
        std::string ty = c.value("type", std::string());
        if (ty == "ZRMD-1") {
            c["mode"] = "nosave";
            c["working_copy"] = false;
        }
        if (ty == "RPD-1") {
            c["drive_a"] = { { "image", "work/test/jb_a.img" } };
            c["drive_b"] = { { "image", "work/test/mercl.img" } };
            c["working_copy"] = false;
        }
    }
    file_remove(data_path(CART));
    json rkd = { { "slot", 7 }, { "type", "RKD-1" },
                 { "unit0", { { "cartridge", CART }, { "fixed", "work/test/rkd_fixed.rkd" } } } };
    cfg["cards"].push_back(rkd);
    CHECK(m.load_json(cfg, err));
    if (!err.empty()) printf("%s\n", err.c_str());
    CHECK(run_until_text(m, "\nA>", 20.0));
    m.run_seconds(0.3);
    return true;
}

// FORMTRKD: format the logical disks given as keys ("0" = A: ...), the read test of each switched off
// with keys 4-7 unless `test`
static void formtrkd(Machine& m, const char* disks, bool test)
{
    m.type_text("B:FORMTRKD\r");
    CHECK(run_until_text(m, "Test", 20.0));
    m.run_seconds(1.0);
    for (const char* p = disks; *p; p++) {
        char k[2] = { *p, 0 };
        m.type_text(k);
        m.run_seconds(0.2);
        if (!test) {
            k[0] = (char)(*p + 4);
            m.type_text(k);
            m.run_seconds(0.2);
        }
    }
    m.type_text("\r");
    m.run_seconds(0.5);
    m.type_text("a");
    double t0 = m.bus.seconds();
    bool done = run_until_text(m, "dokon", 1200.0);   // "Prikaz dokoncen"
    printf("FORMTRKD %s: %s after %.0f s\n", disks, done ? "done" : "NOT DONE", m.bus.seconds() - t0);
    CHECK(done);
    CHECK(screen_text(m).find("Chyba") == std::string::npos);
    m.run_seconds(0.5);
    m.type_text("\x03");   // Ctrl-C: back to CP/M
    CHECK(run_until_text(m, "\nA>", 10.0));
    m.run_seconds(0.3);
}

// ES 59: MOVCPM of Tesla Elstroj with the CBIOS 87-02-13 builds the 59K system and starts it. Caught at the
// jump into the cold start of its CBIOS (E400H) the memory CE00-F6FF is the system as the boot ROM loads it
// (CCP + BDOS 1600H, CBIOS 5 KB - 256 B). E400H is in the BDOS of the running system too: a hit counts only
// when the CBIOS is there.
static bool es_system(Machine& m, std::vector<uint8_t>& sys)
{
    m.set_breakpoint(0xE400, true);
    m.type_text("B:ES 59\r");
    bool hit = false;
    for (int i = 0; i < 200 && !hit; i++) {
        m.run_seconds(0.5);
        if (!m.stopped_on_breakpoint()) continue;
        sys.resize(0x2900);
        for (int a = 0; a < 0x2900; a++) sys[a] = m.peek((uint16_t)(0xCE00 + a));
        std::string s(sys.begin(), sys.end());
        hit = s.find("Cbios 87-02-13") != std::string::npos && m.peek(0xE400) == 0xC3;
    }
    m.set_breakpoint(0xE400, false);
    printf("ES: %s\n", hit ? "the system caught at E400H" : "NOT caught");
    CHECK(hit);
    return hit;
}

// Our SYSGEN for the RKD-1 (the original one is not known): the system onto the cartridge where ROMES
// loads it from: medium 0 (cartridge, surface 0), track 0 sectors 0-23 = CE00-E5FF, track 1 sectors 0-16.
static void put_system(Rkd1* r, const std::vector<uint8_t>& sys)
{
    for (int i = 0; i < 0x2900 / 256; i++)
        CHECK(r->write_sector(0, Rkd1::CARTRIDGE, 0, i / 24, i % 24, &sys[(size_t)i * 256], 256));
}

// The Tesla Elstroj machine: JPR-1Z with the boot EPROM ROMES, RAM-1Z 62 KB, AND-1Z, Consul 259.11, RPD-1
// (E:, F:), RKD-1 (A:-D:)
static json tesla_machine(const char* cart)
{
    json cfg;
    cfg["name"] = "test RKD-1 Tesla Elstroj";
    cfg["bus"] = { { "type", "ARB-1" } };
    json cards = json::array();
    cards.push_back({ { "slot", 1 }, { "type", "JPR-1Z" }, { "rom", "roms/jpr-1z/ROMES_RKD1_870210.bin" },
                      { "keyboard", { { "type", "Consul 259.11" }, { "code", "koi8cs" } } } });
    cards.push_back({ { "slot", 2 }, { "type", "RAM-1Z" }, { "fitted_kb", 64 }, { "s1_1", false } });
    cards.push_back({ { "slot", 3 }, { "type", "AND-1Z" } });
    cards.push_back({ { "slot", 4 }, { "type", "RPD-1" }, { "working_copy", false },
                      { "drive_a", { { "image", "work/test/mercl.img" } } } });
    cards.push_back({ { "slot", 6 }, { "type", "RKD-1" },
                      { "unit0", { { "cartridge", cart }, { "fixed", "work/test/rkd_fixed.rkd" } } } });
    cfg["cards"] = cards;
    return cfg;
}

// The autosave of the media (EmuHost::autosave -> Machine::save_media): new platters and a written sector make the
// card unsaved, a save writes the cartridge file (through a temporary file); a bad image does not take the cartridge
// out (read first).
static void autosave_insert(void)
{
    const char* cart = "work/test/rkd_as_cart.rkd";
    const char* fixed = "work/test/rkd_as_fixed.rkd";
    const char* bad = "work/test/rkd_as_bad.rkd";
    file_remove(data_path(cart));
    file_remove(data_path(fixed));
    json cfg = tesla_machine(cart);
    cfg["cards"].erase(3);   // no RPD-1
    cfg["cards"][3]["working_copy"] = false;
    cfg["cards"][3]["unit0"]["fixed"] = fixed;
    battery_files_enable(true);   // the media are written
    {
        Machine m;
        std::string err;
        CHECK(m.load_json(cfg, err));
        Rkd1* r = rkd_of(m);
        if (r) {
            CHECK(m.unsaved());   // the new platters
            CHECK_EQ(m.save_media(), 1);
            CHECK(!m.unsaved());
            CHECK(file_exists(data_path(cart)) && file_exists(data_path(fixed)));
            uint8_t s[256];
            for (int i = 0; i < 256; i++) s[i] = (uint8_t)(i * 7 + 3);
            CHECK(r->write_sector(0, Rkd1::CARTRIDGE, 0, 5, 3, s, 256));
            CHECK(m.unsaved());
            CHECK_EQ(m.save_media(), 1);
            CHECK(!m.unsaved());
            RkdPlatter p;
            CHECK(p.load(cart, false, err));
            CHECK(p.data == r->units[0].disk[Rkd1::CARTRIDGE].data);
            uint8_t junk[64] = { 0 };
            CHECK(file_write_all(data_path(bad), junk, sizeof(junk)));
            CHECK(!r->insert(0, bad, false, err));
            CHECK(r->units[0].disk[Rkd1::CARTRIDGE].loaded);
            CHECK(r->units[0].disk[Rkd1::CARTRIDGE].path == data_path(cart));
            file_remove(data_path(bad));
        }
        battery_files_enable(false);
    }
    file_remove(data_path(cart));
    file_remove(data_path(fixed));
}

static void tesla_system(void)
{
    std::vector<uint8_t> sys;
    {
        Machine m;
        if (!jb_with_rkd(m)) return;
        formtrkd(m, "01", false);   // the cartridge: A: and B:
        if (!es_system(m, sys)) return;
        Rkd1* r = rkd_of(m);
        if (!r) return;
        put_system(r, sys);
        std::string err;
        RkdPlatter& p = r->units[0].disk[Rkd1::CARTRIDGE];
        p.path = data_path(CART);
        p.dirty = true;
        CHECK(p.save(err));   // for the next machine (tests do not flush)
    }
    // boot from the cartridge; the files of the Tesla Elstroj disk (E:) onto it with PIP of J. Biba (F:)
    Machine m;
    std::string err;
    json cfg = tesla_machine(CART);
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path(SERVICE), img));
    CHECK(file_write_all(data_path("work/test/sluzebni.img"), img.data(), img.size()));
    cfg["cards"][3]["drive_b"] = { { "image", "work/test/sluzebni.img" } };
    CHECK(m.load_json(cfg, err));
    CHECK(run_until_text(m, "Boot ROM pro RKD-1", 3.0));
    CHECK(run_until_text(m, "Cbios 87-02-13", 10.0));
    CHECK(run_until_text(m, "P0A>", 10.0));   // CCP with the prompt of the CBIOS (user, disk); "SH?": no shell yet
    m.run_seconds(0.3);
    m.type_text("F:PIP A:=E:*.*\r");
    double t0 = m.bus.seconds();
    CHECK(run_until_text(m, "WM.COM\n\nP0A>", 600.0));   // the last file copied
    m.run_seconds(0.3);
    m.type_text("F:PIP A:=F:PIP.COM\r");
    CHECK(run_until_text(m, "PIP.COM\n\nP0A>", 60.0));
    m.run_seconds(0.3);
    m.type_text("DIR A:*.COM\r");
    bool copied = run_until_text(m, "PIP      COM", 30.0);
    printf("PIP: %s after %.0f s\n", copied ? "done" : "NOT DONE", m.bus.seconds() - t0);
    CHECK(copied);
    m.run_seconds(2.0);
    printf("%s\n", screen_text(m).c_str());
    CHECK(screen_text(m).find("FORMTRKD COM") != std::string::npos);
    // a new start runs the shell SH with STARTSYS.SUB from the cartridge
    m.reset();
    CHECK(run_until_text(m, "Cbios 87-02-13", 10.0));
    bool sh = run_until_text(m, "u\xc5\xbeivatele", 20.0);   // "zadej prosim sve cislo uzivatele"
    m.run_seconds(1.0);
    printf("%s\n", screen_text(m).c_str());
    CHECK(sh);
    const char* out = getenv("RKD_MAKE");
    Rkd1* r = rkd_of(m);
    if (out && r) {
        RkdPlatter& p = r->units[0].disk[Rkd1::CARTRIDGE];
        p.path = data_path(out);
        p.dirty = true;
        CHECK(p.save(err));
        printf("saved %s\n", p.path.c_str());
    }
}

// The machine rkd1_tesla_cpm.sapi with the system cartridge from media/ (in memory only): the shell, the
// fixed disk formatted by FORMTRKD, a file on it, a write onto a write protected cartridge.
static void media_system(void)
{
    std::string t, err;
    CHECK(file_read_text(data_path("machines/rkd1_tesla_cpm.sapi"), t));
    json cfg = json::parse(t, nullptr, false, true);
    std::vector<uint8_t> img;
    for (json& c : cfg["cards"]) {
        std::string ty = c.value("type", std::string());
        if (ty == "RPD-1") {
            CHECK(file_read_all(data_path(c["drive_a"]["image"].get<std::string>()), img));
            CHECK(file_write_all(data_path("work/test/rkd_e.img"), img.data(), img.size()));
            c["drive_a"]["image"] = "work/test/rkd_e.img";
            c.erase("drive_b");
            c["working_copy"] = false;
        }
        if (ty == "RKD-1") {
            c["working_copy"] = false;   // the cartridge read from media/, changed only in memory
            c["unit0"]["fixed"] = "work/test/rkd_fixed2.rkd";
        }
    }
    file_remove(data_path("work/test/rkd_fixed2.rkd"));
    Machine m;
    CHECK(m.load_json(cfg, err));
    if (!err.empty()) printf("%s\n", err.c_str());
    CHECK(run_until_text(m, "u\xc5\xbeivatele", 20.0));
    m.run_seconds(0.5);
    m.type_text("1\r");
    CHECK(run_until_text(m, "oblasti", 10.0));   // a new user: the name of the area
    m.run_seconds(0.3);
    m.type_text("TEST\r");
    CHECK(run_until_text(m, "b1$", 30.0));       // STARTSYS wrote USER1.SUB and VOLUME.ID on B:, the SH prompt
    CHECK(screen_text(m).find("B: VOLUME   ID  : USER1    SUB") != std::string::npos);
    m.run_seconds(0.5);
    printf("%s\n", screen_text(m).c_str());
    // the fixed disk: C: (medium 2, DISK active) formatted (without the read test of FORMTRKD, the file below
    // reads it)
    m.type_text("A:FORMTRKD\r");
    CHECK(run_until_text(m, "Test", 20.0));
    m.run_seconds(1.0);
    m.type_text("2");
    m.run_seconds(0.2);
    m.type_text("6");
    m.run_seconds(0.2);
    m.type_text("\r");
    m.run_seconds(0.5);
    m.type_text("a");
    bool done = run_until_text(m, "dokon", 600.0);
    CHECK(done);
    CHECK(screen_text(m).find("Chyba") == std::string::npos);
    m.run_seconds(0.5);
    m.type_text("\x03");
    m.run_seconds(2.0);   // "program ukoncen ^C", the shell again
    m.type_text("SAVE 2 C:PRAZDNY.COM\r");
    m.run_seconds(3.0);
    m.type_text("DIR C:\r");
    CHECK(run_until_text(m, "PRAZDNY  COM", 10.0));
    Rkd1* r = rkd_of(m);
    if (!r) return;
    CHECK(r->units[0].disk[Rkd1::FIXED].loaded && r->units[0].track >= 0);
    // write protect the cartridge (the WPP of the controller or the switch): the drive does not write, nobody
    // notices (the driver does not read WPROTECT); the system keeps running
    RkdPlatter& cart = r->units[0].disk[Rkd1::CARTRIDGE];
    std::vector<uint8_t> before = cart.data;
    cart.write_protect = true;
    m.run_seconds(0.3);
    m.type_text("SAVE 1 A:CHRANENO.COM\r");
    m.run_seconds(5.0);
    m.type_text("E:STAT A:*.COM\r");   // a program: the warm start after it loads the system from the cartridge
    CHECK(run_until_text(m, "A:WM.COM", 30.0));
    m.run_seconds(2.0);
    std::string s = screen_text(m);
    printf("%s\n", s.c_str());
    CHECK(cart.data == before);
    CHECK(!r->units[0].err);
    cart.write_protect = false;
    // another cartridge and RESET (docs/navod-rkd1.md): without the system on it the boot ROM asks for a key;
    // the system cartridge back (kept in memory, as ejected) and a key, the shell again
    const char* fresh = "work/test/rkd_new.rkd";
    file_remove(data_path(fresh));
    RkdPlatter sys;
    std::swap(sys, r->units[0].disk[Rkd1::CARTRIDGE]);
    CHECK(r->insert(0, fresh, false, err));
    m.reset();
    CHECK(run_until_text(m, "opakovat", 30.0));   // "Chyba cteni disku A: pri boot", "Libovolna klavesa -> opakovat"
    printf("%s\n", screen_text(m).c_str());
    r->eject(0);
    std::swap(sys, r->units[0].disk[Rkd1::CARTRIDGE]);
    m.type_text(" ");
    CHECK(run_until_text(m, "u\xc5\xbeivatele", 30.0));
    file_remove(data_path(fresh));
    // a new system cartridge as the guide does it: the shell files to the fixed disk C:, a new cartridge with the
    // help "System..." of the Media window (formatted as FORMTRKD, the same bytes as its sectors on the system
    // cartridge, and the system tracks), RESET, the files back with PIP from C:, RESET: the shell runs from it
    // (as user 0: Enter without a number; the plain CCP after the RESET runs as user 0 too)
    m.run_seconds(0.5);
    m.type_text("\r");
    CHECK(run_until_text(m, "b0$", 30.0));
    m.run_seconds(0.5);
    m.type_text("PIP C:=A:SH.*\r");
    m.run_seconds(15.0);
    m.type_text("PIP C:=A:STARTSYS.SUB\r");
    m.run_seconds(15.0);
    m.type_text("PIP C:=A:PIP.COM\r");
    m.run_seconds(15.0);
    m.type_text("DIR C:\r");
    CHECK(run_until_text(m, "PIP      COM", 10.0));
    m.run_seconds(1.0);
    printf("%s\n", screen_text(m).c_str());
    RkdPlatter& sp = r->units[0].disk[Rkd1::CARTRIDGE];
    std::vector<uint8_t> e5_ref(&sp.data[sp.record(1, 203, 23)], &sp.data[sp.record(1, 203, 23)] + 2 + sp.cap());
    std::vector<uint8_t> sys_ref(&sp.data[sp.record(0, 0, 0)], &sp.data[sp.record(0, 2, 0)]);
    const char* fresh2 = "work/test/rkd_sysgen.rkd";
    file_remove(data_path(fresh2));
    r->eject(0);
    CHECK(r->insert(0, fresh2, false, err));
    CHECK(!r->platter_formatted(0, Rkd1::CARTRIDGE));
    CHECK(r->format_platter(0, Rkd1::CARTRIDGE, err));
    CHECK(r->write_system(0, Rkd1::SYSTEM_SOURCE, err));
    CHECK(r->platter_formatted(0, Rkd1::CARTRIDGE));
    RkdPlatter& np = r->units[0].disk[Rkd1::CARTRIDGE];
    CHECK(memcmp(&np.data[np.record(1, 203, 23)], e5_ref.data(), e5_ref.size()) == 0);
    CHECK(memcmp(&np.data[np.record(0, 0, 0)], sys_ref.data(), sys_ref.size()) == 0);
    m.reset();
    CHECK(run_until_text(m, "Cbios 87-02-13", 10.0));
    CHECK(run_until_text(m, "P0A>", 20.0));   // no SH on the new cartridge: the plain CCP
    m.run_seconds(0.5);
    m.type_text("DIR A:\r");
    CHECK(run_until_text(m, "NO FILE", 10.0));
    m.type_text("C:PIP A:=C:*.*\r");
    CHECK(run_until_text(m, "PIP.COM\n\nP0A>", 120.0));
    m.run_seconds(0.5);
    printf("%s\n", screen_text(m).c_str());
    m.reset();
    CHECK(run_until_text(m, "u\xc5\xbeivatele", 30.0));   // SH and STARTSYS.SUB from the new cartridge
    file_remove(data_path(fresh2));
}

// The Bulgarian drive CM 5400 on the cable SM 5400 with FORMRKD v1.1 of J. Biba (12 x 512 B) under his
// CP/M from an RPD-1 floppy: the cartridge F: formatted and read back (O), the fixed disk D: formatted (F).
static void cm5400_formrkd(void)
{
    std::string t, err;
    CHECK(file_read_text(data_path("machines/rkd1_jb_cm5400.sapi"), t));
    json cfg = json::parse(t, nullptr, false, true);
    std::vector<uint8_t> img;
    for (json& c : cfg["cards"]) {
        std::string ty = c.value("type", std::string());
        if (ty == "RPD-1") {
            const char* src[2] = { "drive_a", "drive_b" };
            const char* dst[2] = { "work/test/jb5400_a.img", "work/test/jb5400_b.img" };
            for (int i = 0; i < 2; i++) {
                CHECK(file_read_all(data_path(c[src[i]]["image"].get<std::string>()), img));
                CHECK(file_write_all(data_path(dst[i]), img.data(), img.size()));
                c[src[i]]["image"] = dst[i];
            }
            c["working_copy"] = false;
        }
        if (ty == "RKD-1") {
            c["working_copy"] = false;
            c["unit0"]["cartridge"] = "work/test/rkd5400_cart.rkd";
        }
        if (ty == "ZRMD-1") {
            c["mode"] = "nosave";
            c["working_copy"] = false;
            c["unit0"]["fixed"] = "work/test/rkd5400_fixed.rkd";
        }
    }
    file_remove(data_path("work/test/rkd5400_cart.rkd"));
    file_remove(data_path("work/test/rkd5400_fixed.rkd"));
    Machine m;
    CHECK(m.load_json(cfg, err));
    if (!err.empty()) printf("%s\n", err.c_str());
    CHECK(run_until_text(m, "\nA>", 20.0));
    m.run_seconds(0.3);
    Rkd1* r = rkd_of(m);
    if (!r) return;
    CHECK(r->units[0].drive == Rkd1::CM5400 && r->units[0].disk[Rkd1::CARTRIDGE].sectors == 12 &&
          r->units[0].disk[Rkd1::FIXED].sectors == 12);
    m.type_text("B:FORMRKD\r");
    CHECK(run_until_text(m, "SM 5400", 20.0));
    m.run_seconds(1.0);
    const char* jobs[2][2] = { { "F", "O" }, { "D", "F" } };   // cartridge lower: format and read; fixed lower: format
    for (int j = 0; j < 2; j++) {
        m.type_text(jobs[j][0]);
        m.run_seconds(0.5);
        m.type_text(jobs[j][1]);
        m.run_seconds(0.5);
        m.type_text("A");
        double t0 = m.bus.seconds();
        // "Cteni skoncilo" after O, "Formatovani skoncilo" after F
        const char* end = j == 0 ? "\xc4\x8cten\xc3\xad skon" : "Form\xc3\xa1tov\xc3\xa1n\xc3\xad skon";
        bool done = run_until_text(m, end, 400.0);
        printf("FORMRKD %s %s: %s after %.0f s\n", jobs[j][0], jobs[j][1], done ? "done" : "NOT DONE", m.bus.seconds() - t0);
        CHECK(done);
        std::string s = screen_text(m);
        CHECK(s.find("Chyb") == std::string::npos && s.find("HARD") == std::string::npos);
        if (!done) printf("%s\n", s.c_str());
        m.run_seconds(3.0);   // the delay of FORMRKD, back to the question for the drive
    }
    // the sectors as the driver in FORMRKD.COM writes them: an older PREP_MARK than RKD.MAC 87-02-13, the
    // header is only the track (track x 32, low byte first), no sector, disk or 512 B bits
    for (int d = 0; d < 2; d++) {
        RkdPlatter& q = r->units[0].disk[d == 0 ? Rkd1::CARTRIDGE : Rkd1::FIXED];
        for (int tt : { 0, 101, 203 }) {
            const uint8_t* x = &q.data[q.record(0, tt, 11)] + 2;
            int len = x[-2] | x[-1] << 8;
            bool zeros = true;
            for (int i = 0; i < 26; i++) zeros = zeros && x[i] == 0;
            CHECK(len == 512 + 31 && zeros && x[26] == 0x80 && x[27] == (uint8_t)(tt << 5) && x[28] == (uint8_t)(tt >> 3) &&
                  x[29] == 0xE5 && x[29 + 511] == 0xE5 && x[29 + 512] == 0x00 && x[29 + 513] == 0xCA);
        }
        CHECK((q.data[q.record(1, 0, 0)] | q.data[q.record(1, 0, 0) + 1]) == 0);   // G:, E: not formatted
    }
    CHECK(r->units[0].cyl == 1);   // RK_HOME at the end: T00 alone on the cable = cylinder 1
    // the CP/M of the boot disk has D: to G: on the RKD-1 (BIOS made in SAPIemu, media/rkd-1/cm5400): files on
    // the disks FORMRKD formatted, a copy between them with verify, a program run from D:
    m.type_text("D");   // the end of FORMRKD is a kind of work: a drive first, then K
    m.run_seconds(0.5);
    m.type_text("K");
    CHECK(run_until_text(m, "A>", 20.0));
    m.run_seconds(0.5);
    m.type_text("DIR F:\r");
    CHECK(run_until_text(m, "NO FILE", 30.0));
    m.run_seconds(0.3);
    m.type_text("B:PIP F:=B:STAT.COM[V]\r");
    m.run_seconds(30.0);
    m.type_text("B:PIP D:=F:STAT.COM[V]\r");
    m.run_seconds(30.0);
    m.type_text("D:STAT D:*.*\r");
    bool stat = run_until_text(m, "REMAINING ON D:", 60.0);
    m.run_seconds(3.0);
    printf("%s\n", screen_text(m).c_str());
    CHECK(stat);
    std::string s = screen_text(m);
    CHECK(s.find("ON D: 1210K") != std::string::npos);   // 1224 KB - 8 KB directory - 6 KB of STAT.COM
    CHECK(s.find("D:STAT.COM") != std::string::npos && s.find("Bdos") == std::string::npos &&
          s.find("VERIFY") == std::string::npos);
    m.type_text("DIR F:\r");
    CHECK(run_until_text(m, "STAT     COM", 20.0));
}

static void trace_boot(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(tesla_machine(CART), err));
    for (int i = 0; i < 40; i++) {
        CpuRegs r;
        m.cpu->get_regs(r);
        printf("%04X: %02X %02X %02X  SP=%04X A=%02X\n", r.pc, m.peek(r.pc), m.peek((uint16_t)(r.pc + 1)),
               m.peek((uint16_t)(r.pc + 2)), r.sp, r.af >> 8);
        m.step();
    }
}

int main(void)
{
    test_init();
    if (getenv("RKD_TRACE")) {
        trace_boot();
        return 0;
    }
    const char* part = getenv("RKD_PART");   // ctest runs the parts in parallel (tests/CMakeLists.txt)
    if (!part || !strcmp(part, "media")) {
        media_system();
        autosave_insert();
    }
    if (!part || !strcmp(part, "cm5400")) cm5400_formrkd();
    if (!part || !strcmp(part, "tesla")) tesla_system();
    return TEST_RESULT();
}
