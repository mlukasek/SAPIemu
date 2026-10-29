// Card catalog and machine description editing: every emulated card can be
// created from its catalog defaults, machines survive a round trip through the
// .sapi writer, cards not emulated yet stay passive.
#include <filesystem>
#include "cards/catalog.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/machine_cfg.h"
#include "core/machine_map.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static void test_catalog(void)
{
    for (const std::string& t : card_types()) {
        const CardInfo* ci = card_info(t);
        CHECK(ci != nullptr);
        if (ci) CHECK(ci->emulated);
    }
    for (const CardInfo* ci : card_catalog()) {
        Card* c = card_create(ci->type);
        CHECK((c != nullptr) == ci->emulated);
        delete c;
        CHECK(card_info(ci->type) == ci);
        for (const CardParam* p = ci->params; p && p->path; p++) {
            json d = json::parse(p->def, nullptr, false);
            if (d.is_discarded()) fprintf(stderr, "%s.%s: bad default\n", ci->type, p->path);
            CHECK(!d.is_discarded());
            if (p->kind == PK_SECTION) CHECK(d.is_boolean());
            for (int i = 0; p->options && p->options[i].value; i++)
                CHECK(!json::parse(p->options[i].value, nullptr, false).is_discarded());
            if (p->kind == PK_CHOICE) CHECK(param_option_index(p, d) >= 0);
        }
    }
    CHECK(bus_info("ARB-1C")->slots == 14);
    CHECK(bus_info("ARB-1")->slots == 8);
    CHECK(bus_info("ImBUS9")->slots == 9 && !bus_info("ImBUS9")->has_jp3);
    CHECK(bus_info("ImBUS12")->slots == 12);
}

static void test_defaults_load(void)
{
    // every emulated card with its defaults in a machine with JPR-1ZI and MikroMon
    for (const CardInfo* ci : card_catalog()) {
        if (!ci->emulated || ci->cls == CC_CPU) continue;
        json cfg = { { "name", "test" }, { "bus", { { "type", "ARB-1C-8" } } }, { "cards", json::array() } };
        json cpu = card_new_config(card_info("JPR-1ZI"), 1);
        cpu["rom"] = "roms/mikromon/MikroMon1_3_f000.txt";
        cfg["cards"].push_back(cpu);
        cfg["cards"].push_back(card_new_config(ci, 2));
        Machine m;
        std::string err;
        bool ok = m.load_json(cfg, err);
        if (!ok) fprintf(stderr, "%s: %s\n", ci->type, err.c_str());
        CHECK(ok);
        std::vector<CfgProblem> pr = machine_all_problems(cfg);
        for (const CfgProblem& p : pr) fprintf(stderr, "%s: %s\n", ci->type, p.en.c_str());
        CHECK(pr.empty());
    }
}

static void test_round_trip(void)
{
    std::error_code ec;
    std::string dir = path_join(data_root(), "machines");
    int n = 0;
    for (auto& e : std::filesystem::recursive_directory_iterator(std::filesystem::path(std::u8string(dir.begin(), dir.end())), ec)) {
        if (e.path().extension() != ".sapi") continue;
        std::u8string u = e.path().u8string();
        std::string path(u.begin(), u.end());
        std::string text;
        CHECK(file_read_text(path, text));
        json a = json::parse(text, nullptr, false, true);
        CHECK(!a.is_discarded());
        std::string out = machine_cfg_to_jsonc(a);
        json b = json::parse(out, nullptr, false, true);
        if (b.is_discarded() || a != b) fprintf(stderr, "round trip differs: %s\n%s\n", path.c_str(), out.c_str());
        CHECK(!b.is_discarded() && a == b);
        n++;
    }
    CHECK(n >= 3);
}

// "windows" (opened on top at the start, frontend) is written after the description, with its comment
static void test_windows(void)
{
    json cfg = json::parse("{\"cards\": [], \"bus\": {\"type\": \"ARB-1\"}, \"windows\": [\"minigraf\", \"media\"], "
                           "\"name\": \"x\", \"description\": \"y\"}");
    std::string out = machine_cfg_to_jsonc(cfg);
    size_t d = out.find("\"description\""), w = out.find("\"windows\": [ \"minigraf\", \"media\" ],"),
           b = out.find("\"bus\"");
    CHECK(d != std::string::npos && w != std::string::npos && b != std::string::npos && d < w && w < b);
    CHECK(out.find("// Windows opened on top at the start", w) < b);
    CHECK(json::parse(out, nullptr, false, true) == cfg);
    // "main_display" (the display with the big place, frontend/layout.cpp) right after it, with its comment
    cfg["main_display"] = "CGA-1V";
    out = machine_cfg_to_jsonc(cfg);
    size_t md = out.find("\"main_display\": \"CGA-1V\",");
    w = out.find("\"windows\"");
    b = out.find("\"bus\"");
    CHECK(md != std::string::npos && w < md && md < b);
    CHECK(out.find("// Main output:", md) < b);
    CHECK(json::parse(out, nullptr, false, true) == cfg);
}

static void test_edit(void)
{
    const CardInfo* z = card_info("ZRMD-1");
    json card = card_new_config(z, 4);
    cfg_erase(card, "write_enable");
    const CardParam* s3 = nullptr;
    for (const CardParam* p = z->params; p->path; p++)
        if (strcmp(p->path, "write_enable.2") == 0) s3 = p;
    CHECK(s3 != nullptr);
    card_set_param(card, z, s3, false);
    CHECK(card["write_enable"] == json({ true, true, false, true }));

    const CardInfo* d = card_info("DMR-1");
    json dmr = card_new_config(d, 2);
    const CardParam* tcp = nullptr;
    const CardParam* port = nullptr;
    const CardParam* base = nullptr;
    for (const CardParam* p = d->params; p->path; p++) {
        if (strcmp(p->path, "serial.tcp") == 0) tcp = p;
        if (strcmp(p->path, "serial.tcp.port") == 0) port = p;
        if (strcmp(p->path, "uart_base") == 0) base = p;
    }
    CHECK(!card_param_active(dmr, d, port));
    card_set_param(dmr, d, tcp, true);
    CHECK(card_param_active(dmr, d, port));
    CHECK(dmr["serial"]["tcp"]["port"] == 2323);
    card_set_param(dmr, d, tcp, false);
    CHECK(!dmr["serial"].contains("tcp"));
    CHECK(dmr["serial"]["pacing"] == true);
    CHECK(param_option_index(base, json(0x14)) == 1);
    CHECK(cfg_values_equal(json("1Ch"), json(28)));
}

static void test_placeholders(void)
{
    std::string text, err;
    CHECK(file_read_text(data_path("machines/mikromon.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    cfg["cards"].push_back({ { "slot", 4 }, { "type", "DSM-1" } });
    cfg["cards"].push_back({ { "slot", 5 }, { "type", "RPD-1" } });   // two boards: X5 and X6
    CHECK(machine_cfg_card_at(cfg, 6) == 4);
    CHECK(machine_cfg_card_at(cfg, 7) == -1);
    // DSM-1 from the factory sits on 10h like the DMR-1: a port conflict
    CHECK(card_port_label(cfg["cards"][3]) == "#10");
    std::vector<CfgProblem> pr = machine_all_problems(cfg);
    bool clash = false;
    for (const CfgProblem& p : pr) clash = clash || (p.error && p.en.find("ports 10-12h answered by two cards") != std::string::npos);
    if (!clash) for (const CfgProblem& p : pr) fprintf(stderr, "problem: %s\n", p.en.c_str());
    CHECK(clash);
    cfg["cards"][3]["base"] = "14h";
    CHECK(card_display_name(cfg["cards"][3]) == "DSM-1 #14");
    pr = machine_all_problems(cfg);
    for (const CfgProblem& p : pr) fprintf(stderr, "problem: %s\n", p.en.c_str());
    CHECK(pr.empty());   // DSM-1 and RPD-1 are emulated, no clash
    for (const CfgProblem& p : pr) CHECK(!p.error);
    {
        Machine m;
        CHECK(m.load_json(cfg, err));
        CHECK(run_until_text(m, "MikroMon 2kB V1.3", 2.0));
    }
    // RPD-1 does not fit into X8
    cfg["cards"][4]["slot"] = 8;
    CHECK(!machine_cfg_problems(cfg).empty());
    {
        Machine m;
        CHECK(!m.load_json(cfg, err));
    }
    // a machine without a CPU card cannot run (all CPU cards of the catalog are emulated now)
    cfg["cards"][4]["slot"] = 5;
    {
        json nocpu = cfg;
        nocpu["cards"].erase(0);
        Machine m;
        CHECK(!m.load_json(nocpu, err));
        CHECK(err.find("no CPU card") != std::string::npos);
    }
    // the non-CPU slot X1 rule
    json bad = { { "cards", { { { "slot", 1 }, { "type", "DMR-1" } } } } };
    CHECK(machine_cfg_problems(bad).size() == 2);   // X1 reserved + no CPU
}

// DMR-1 JP7: the battery keeps the SRAM across power off / on
static void test_battery(void)
{
    battery_files_enable(true);
    std::string file = "work/test/dmr_sram_test.bin";
    file_remove(data_path(file));
    json cfg = { { "cards", json::array() } };
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    cfg["cards"].push_back({ { "slot", 2 }, { "type", "DMR-1" }, { "ram_config", 0 }, { "sram_file", file } });
    std::string err;
    {
        Machine m;
        CHECK(m.load_json(cfg, err));
        m.poke(0x4000, 0x5A);
    }   // power off: flushed
    {
        Machine m;
        CHECK(m.load_json(cfg, err));
        CHECK_EQ(m.peek(0x4000), 0x5A);
    }
    cfg["cards"][1]["battery"] = false;   // no battery: empty after power on
    {
        Machine m;
        CHECK(m.load_json(cfg, err));
        CHECK_EQ(m.peek(0x4000), 0x00);
    }
    file_remove(data_path(file));
    battery_files_enable(false);
}

// Two DMR-1: the second one only as a serial port on 14h
static void test_two_dmr(void)
{
    std::string text, err;
    CHECK(file_read_text(data_path("machines/mikromon.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    cfg["cards"].push_back({ { "slot", 4 }, { "type", "DMR-1" }, { "uart_base", "14h" } });
    std::vector<CfgProblem> pr = machine_all_problems(cfg);
    bool ram_clash = false;   // two RAMs answer the same addresses
    for (const CfgProblem& p : pr) ram_clash = ram_clash || (p.error && p.en.find("memory 0800-F7FFh") != std::string::npos);
    if (!ram_clash) for (const CfgProblem& p : pr) fprintf(stderr, "problem: %s\n", p.en.c_str());
    CHECK(ram_clash);
    cfg["cards"][3]["ram"] = false;
    CHECK(machine_all_problems(cfg).empty());
    Machine m;
    CHECK(m.load_json(cfg, err));
    CHECK(m.serials.size() == 2);
    if (m.serials.size() == 2) {
        CHECK(m.serials[0]->name == "DMR-1 #10");
        CHECK(m.serials[1]->name == "DMR-1 #14");
    }
    CHECK(m.find_card("DMR-1 X4") != nullptr);
    CHECK(run_until_text(m, "MikroMon 2kB V1.3", 2.0));   // RAM of the first DMR-1 only
}

// Port and memory map from the cards' decoders
static const MapRange* find_range(const std::vector<MapRange>& rs, int addr)
{
    for (const MapRange& r : rs)
        if (addr >= r.lo && addr <= r.hi) return &r;
    return nullptr;
}

static void test_map(void)
{
    std::string text;
    CHECK(file_read_text(data_path("machines/cpm_ide.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    MachineMap m;
    machine_map_build(cfg, m);
    CHECK(machine_map_problems(m).empty());
    CHECK(!m.map_matters);
    // memory: DMR-1 RAM 0000-F7FF (EPROM on the CPU card over 0000-07FF), AND-1Z VRAM F800-FFFF
    const MapRange* r = find_range(m.mem[0], 0x0100);
    CHECK(r && r->lo == 0x0000 && r->hi == 0x07FF && r->users.size() == 2 && !r->conflict);
    r = find_range(m.mem[0], 0x1000);
    CHECK(r && r->lo == 0x0800 && r->hi == 0xF7FF && r->users.size() == 1);
    r = find_range(m.mem[0], 0xF800);
    CHECK(r && r->lo == 0xF800 && r->hi == 0xFFFF && r->users.size() == 1 && r->users[0].what == "VRAM");
    // ports
    r = find_range(m.io, 0x00);
    CHECK(r && r->users.size() == 1 && r->users[0].flags == DEC_WRITE);
    r = find_range(m.io, 0x11);
    CHECK(r && r->lo == 0x10 && r->hi == 0x12 && r->users[0].what == "UART");
    r = find_range(m.io, 0x13);
    CHECK(r && r->users[0].flags == DEC_WRITE);
    r = find_range(m.io, 0x5A);
    CHECK(r && r->lo == 0x58 && r->hi == 0x5B);
    r = find_range(m.io, 0x5D);
    CHECK(r && r->lo == 0x5C && r->hi == 0x5F);
    r = find_range(m.io, 0x75);
    CHECK(r && r->lo == 0x70 && r->hi == 0x7F && r->users[0].what == "RTC");
    CHECK(find_range(m.io, 0x20) == nullptr);   // open bus

    // RAM over the VRAM: DMR-1 JP3 = 0 (whole 64 KB)
    for (json& c : cfg["cards"])
        if (c["type"] == "DMR-1") c["ram_config"] = 0;
    machine_map_build(cfg, m);
    r = find_range(m.mem[0], 0xF900);
    CHECK(r && r->conflict);
    std::vector<CfgProblem> pr = machine_map_problems(m);
    CHECK(pr.size() == 1 && pr[0].error && pr[0].en.find("memory F800-FFFFh") != std::string::npos);

    // AND-1Z disconnected by MAP1: the map depends on MAP1/MAP2
    for (json& c : cfg["cards"])
        if (c["type"] == "AND-1Z-89") c["map_enabled"] = true;
    machine_map_build(cfg, m);
    CHECK(m.map_matters);
    CHECK(find_range(m.mem[1], 0xF900)->users.size() == 1);   // MAP1=1: only the RAM

    // all shipped machines that can run are free of conflicts; probing does not create files
    for (const char* f : { "machines/mikromon.sapi", "machines/cpm_zrmd.sapi" }) {
        CHECK(file_read_text(data_path(f), text));
        json c = json::parse(text, nullptr, false, true);
        std::vector<CfgProblem> p = machine_all_problems(c);
        for (const CfgProblem& q : p) fprintf(stderr, "%s: %s\n", f, q.en.c_str());
        CHECK(p.empty());
    }
}

// A machine written by the configurator has the line ends of a git checkout (CRLF on
// Windows), so saving it unchanged leaves no change in git.
static void test_line_ends(void)
{
    std::string file = data_path("work/test/line_ends.sapi"), back;
    make_dirs(path_dir(file));
    json cfg = json::parse(R"({ "name": "x", "bus": { "type": "ARB-1C-8" }, "cards": [] })");
    CHECK(file_write_text(file, machine_cfg_to_jsonc(cfg)));
    CHECK(file_read_text(file, back));
#ifdef _WIN32
    CHECK(back.find("\r\n") != std::string::npos);
    bool lone_lf = false;
    for (size_t i = 0; i < back.size(); i++)
        if (back[i] == '\n' && (i == 0 || back[i - 1] != '\r')) lone_lf = true;
    CHECK(!lone_lf);
#else
    CHECK(back.find('\r') == std::string::npos);
#endif
    CHECK(json::parse(back, nullptr, false, true) == cfg);
    file_remove(file);
}

static std::vector<MachinePart> parts_of(const char* file, bool czech)
{
    std::string text;
    CHECK(file_read_text(data_path(file), text));
    return machine_cfg_parts(json::parse(text, nullptr, false, true), czech);
}

static bool has_device(const MachinePart& p, const char* d)
{
    for (const std::string& s : p.devices)
        if (s == d) return true;
    return false;
}

// the cards and devices of the window Choose a machine
static void test_parts(void)
{
    std::vector<MachinePart> p = parts_of("machines/sapi1v_hry.sapi", true);
    CHECK(p.size() == 8);
    CHECK(p[0].slot == 1 && p[0].type == "JPR-1V" && has_device(p[0], "Klávesnice Consul 262.3"));
    CHECK(p[4].type == "ZRD-1V" && p[4].devices.empty());
    bool mouse = false;
    for (const MachinePart& m : p)
        if (m.type == "MPH-1V") mouse = has_device(m, "Myš Atari ST");
    CHECK(mouse);
    p = parts_of("machines/aps5.sapi", false);
    CHECK(!p.empty() && p.back().slot == 0 && p.back().type == "APS-5");
    p = parts_of("machines/zps/ZPS-2_MIKOS_V5_XY4130.sapi", true);
    bool plotter = false;
    for (const MachinePart& m : p)
        if (has_device(m, "Zapisovač XY 4130")) plotter = true;
    CHECK(plotter);
}

static bool has_line(const std::vector<std::string>& l, const std::string& s)
{
    for (const std::string& x : l)
        if (x == s) return true;
    fprintf(stderr, "missing change: %s\n", s.c_str());
    return false;
}

// the question about unsaved changes of the configurator lists what really changed
static void test_changes(void)
{
    std::string text;
    CHECK(file_read_text(data_path("machines/sapi1v.sapi"), text));
    const json a = json::parse(text, nullptr, false, true);
    json b = a;
    CHECK(machine_cfg_changes(a, b, true).empty());
    CHECK(machine_cfg_changes(json(), b, true).empty());   // not saved yet: nothing to compare with
    // written differently, the same values: no change
    b["cards"][3]["a"]["base"] = 16;                            // DSM-1V "10h"
    json& ram = b["cards"][1];
    const CardInfo* rci = card_info("RAM-1V");
    for (const CardParam* p = rci->params; p && p->path; p++)   // every default written out
        if (p->kind != PK_SECTION && card_param_active(ram, rci, p)) card_set_param(ram, rci, p, card_param_value(ram, p));
    CHECK(machine_cfg_changes(a, b, true).empty());
    // the media, TURBO and the colour (what the running machine changes), a card moved and one removed, the name
    b["cards"][0]["turbo"] = false;
    b["cards"][2]["color"] = "white";
    b["cards"][7]["slot"] = 6;                                  // MPH-1V from X8 to X6
    b["cards"].erase(5);                                        // IDE-1 out of X6
    b["name"] = "Moje SAPI";
    b["bus"]["jp3"] = "1-2";
    std::vector<std::string> cs = machine_cfg_changes(a, b, true);
    for (const std::string& l : cs) printf("  %s\n", l.c_str());
    CHECK(cs.size() == 6);
    CHECK(!cs.empty() && cs[0] == "Název \xE2\x86\x92 Moje SAPI");
    CHECK(has_line(cs, "Sběrnice, jp3 \xE2\x86\x92 1-2"));
    CHECK(has_line(cs, "X1 JPR-1V: TURBO: CPU 4 MHz, sběrnice vždy 2 MHz \xE2\x86\x92 vypnuto"));
    CHECK(has_line(cs, "X3 AND-1V: S1: barva \xE2\x86\x92 bílá"));
    CHECK(has_line(cs, "X6 IDE-1: odebrána"));
    CHECK(has_line(cs, "X8 MPH-1V: přesunuta do X6"));
    std::vector<std::string> en = machine_cfg_changes(a, b, false);
    CHECK(has_line(en, "X1 JPR-1V: TURBO: CPU 4 MHz, bus always 2 MHz \xE2\x86\x92 off"));
    CHECK(has_line(en, "X8 MPH-1V: moved to X6"));
    b["cards"][0]["x_note"] = "a key the catalog does not know";   // listed besides the parameters of the card
    CHECK(has_line(machine_cfg_changes(a, b, false), "X1 JPR-1V: other settings"));
    // a card added, a cassette put in, a section switched off (one line, not its parameters)
    json c = a;
    c["cards"].erase(7);
    json dsm = card_new_config(card_info("DSM-1"), 8);
    c["cards"].push_back(dsm);
    json d = c;
    d["cards"][7]["tape"] = "work/tapes/hra.wav";
    std::string dsm_ref = "X8 " + card_display_name(dsm);
    std::vector<std::string> l = machine_cfg_changes(c, d, true);
    for (const std::string& x : l) printf("  %s\n", x.c_str());
    CHECK(l.size() == 1 && has_line(l, dsm_ref + ": Kazeta v magnetofonu \xE2\x86\x92 hra.wav"));
    l = machine_cfg_changes(a, c, false);
    CHECK(has_line(l, "X8 MPH-1V: removed") && has_line(l, dsm_ref + ": added"));
    // a section switched on and off: one line, its parameters are not listed
    const CardInfo* dci = card_info("DMR-1");
    const CardParam* tcp = nullptr;
    for (const CardParam* p = dci->params; p && p->path; p++)
        if (!strcmp(p->path, "serial.tcp")) tcp = p;
    CHECK(tcp != nullptr);
    if (!tcp) return;
    json d0 = { { "cards", json::array({ card_new_config(dci, 2) }) } }, d1 = d0;
    card_set_param(d1["cards"][0], dci, tcp, true);
    std::string dmr_ref = "X2 " + card_display_name(d0["cards"][0]) + ": ";
    for (int on = 1; on >= 0; on--) {
        l = on ? machine_cfg_changes(d0, d1, true) : machine_cfg_changes(d1, d0, true);
        for (const std::string& x : l) printf("  %s\n", x.c_str());
        CHECK(l.size() == 1 && l[0].rfind(dmr_ref, 0) == 0 &&
              l[0].find(on ? "TCP" : "") != std::string::npos &&
              l[0].find(on ? "\xE2\x86\x92 zapnuto" : "\xE2\x86\x92 vypnuto") != std::string::npos);
    }
}

int main(int argc, char** argv)
{
    test_init();
    if (argc > 1 && !strcmp(argv[1], "--parts")) {   // print the parts of all machines
        for (const std::string& f : list_machine_files()) {
            printf("%s\n", f.c_str());
            for (const MachinePart& m : parts_of(f.c_str(), argc < 3)) {
                printf("  X%d %s: %s\n", m.slot, m.type.c_str(), m.title.c_str());
                for (const std::string& d : m.devices) printf("      - %s\n", d.c_str());
            }
        }
        return 0;
    }
    if (argc > 1) {   // print a machine as written by the configurator
        std::string text;
        file_read_text(data_path(argv[1]), text);
        printf("%s", machine_cfg_to_jsonc(json::parse(text, nullptr, false, true)).c_str());
        return 0;
    }
    test_catalog();
    test_defaults_load();
    test_round_trip();
    test_windows();
    test_edit();
    test_placeholders();
    test_battery();
    test_two_dmr();
    test_map();
    test_line_ends();
    test_parts();
    test_changes();
    return TEST_RESULT();
}
