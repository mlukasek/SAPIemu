// Card catalog and machine description editing: every emulated card can be
// created from its catalog defaults, machines survive a round trip through the
// .sapi writer, cards not emulated yet stay passive.
#include <filesystem>
#include "cards/catalog.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/machine_cfg.h"
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
        CHECK(machine_cfg_problems(cfg).empty());
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
    std::vector<CfgProblem> pr = machine_cfg_problems(cfg);
    CHECK(pr.size() == 2);
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
    // CPU cards that are not emulated cannot run
    cfg["cards"][4]["slot"] = 5;
    cfg["cards"][0]["type"] = "JPR-1A";
    {
        Machine m;
        CHECK(!m.load_json(cfg, err));
        CHECK(err.find("not emulated") != std::string::npos);
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

int main(int argc, char** argv)
{
    test_init();
    if (argc > 1) {   // print a machine as written by the configurator
        std::string text;
        file_read_text(data_path(argv[1]), text);
        printf("%s", machine_cfg_to_jsonc(json::parse(text, nullptr, false, true)).c_str());
        return 0;
    }
    test_catalog();
    test_defaults_load();
    test_round_trip();
    test_edit();
    test_placeholders();
    test_battery();
    return TEST_RESULT();
}
