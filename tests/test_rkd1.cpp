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
    // write protect the cartridge (the WPP of the controller or the switch): writes fail
    r->units[0].disk[Rkd1::CARTRIDGE].write_protect = true;
    m.run_seconds(0.3);
    m.type_text("SAVE 1 A:CHRANENO.COM\r");
    m.run_seconds(15.0);
    std::string s = screen_text(m);
    printf("%s\n", s.c_str());
    CHECK(s.find("Err") != std::string::npos || s.find("Chyba") != std::string::npos);
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
    media_system();
    tesla_system();
    return TEST_RESULT();
}
