// The machines offered in the window "Choose a machine" (core/machine_index.h, machines/_vyber.json). The real
// index: only existing machines, every machine of a category with guides has its guide, the ids, files starting
// with "_" are not machines, own machines fall into the last category, the categories by the history of SAPI-1
// (the author, 2026-10-28: basics, instruments, the boards of Libor Lasota, games and demos, then Ostatní), the
// intro, Manic Miner among the games. A test index of its own: the intro, the titles of a machine in one category
// and the search without diacritics.
#include "core/machine_index.h"
#include "core/util.h"
#include "test_util.h"

#include <stdio.h>

static const MachineCategory* find_category(const MachineIndex& idx, const char* id)
{
    for (const MachineCategory& c : idx.categories)
        if (c.id == id) return &c;
    return nullptr;
}

// The first category of a machine (the one MCP list_machines gives), "" = no such machine.
static std::string first_category(const MachineIndex& idx, const char* file)
{
    const MachineEntry* e = machine_index_find(idx, file);
    if (!e || e->category < 0 || e->category >= (int)idx.categories.size()) return std::string();
    return idx.categories[(size_t)e->category].id;
}

static void test_index(void)
{
    MachineIndex idx;
    std::string err;
    machine_index_load(idx, err);
    if (!err.empty()) fprintf(stderr, "index: %s\n", err.c_str());
    CHECK(err.empty());
    CHECK(idx.machines.size() >= 50);
    CHECK(idx.categories.size() >= 4);
    for (const MachineEntry& e : idx.machines) {
        CHECK(e.file.find("/_") == std::string::npos);   // _vyber.json is not a machine
        CHECK(!e.name.empty());
        CHECK(e.category >= 0 && e.category < (int)idx.categories.size());
        // the search finds every machine by its title, typed with or without the diacritics
        const std::string& t = e.title.empty() ? e.name : e.title;
        CHECK(machine_index_matches(e, t.c_str()));
        CHECK(machine_index_matches(e, machine_index_fold(t).c_str()));
    }
    // the categories in the order of the author (2026-10-28), the first four on their own (not in the group
    // Ostatní) and with guides; every machine of a category with guides has a short text and a guide. My machines
    // before them when work/machines of this data root has copies (a release build ran here).
    static const char* const order[] = { "zaklad", "pristroje", "libor", "hry" };
    size_t first = !idx.categories.empty() && idx.categories[0].id == MACHINE_MINE ? 1 : 0;
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        size_t c = first + i;
        bool ok = c < idx.categories.size() && idx.categories[c].id == order[i] && idx.categories[c].group.empty() &&
                  idx.categories[c].guides;
        if (!ok)
            fprintf(stderr, "category %zu: '%s', expected '%s' on its own with guides\n", c,
                    c < idx.categories.size() ? idx.categories[c].id.c_str() : "", order[i]);
        CHECK(ok);
    }
    for (size_t i = 0; i < idx.categories.size(); i++)   // the ids of the headers and the items (browser.cpp)
        for (size_t j = i + 1; j < idx.categories.size(); j++)
            CHECK(idx.categories[i].id != idx.categories[j].id);
    for (const MachineCategory& c : idx.categories) {
        CHECK(c.titles.size() == c.machines.size() && c.titles_en.size() == c.machines.size());
        for (size_t i = 0; i < c.titles.size() && i < c.titles_en.size(); i++)
            CHECK(c.titles[i].empty() == c.titles_en[i].empty());   // a title of a category in both languages
        if (!c.guides) continue;
        for (int k : c.machines) {
            const MachineEntry& e = idx.machines[(size_t)k];
            if (e.guide.empty() || e.summary.empty() || e.title.empty())
                fprintf(stderr, "%s: guide '%s', summary %zu, title '%s'\n", e.file.c_str(), e.guide.c_str(),
                        e.summary.size(), e.title.c_str());
            CHECK(!e.guide.empty());
            CHECK(!e.summary.empty());
            CHECK(!e.title.empty());
        }
    }
    CHECK(!idx.intro.empty() && !idx.intro_en.empty());   // the intro in both languages
    CHECK(first_category(idx, "machines/aps5.sapi") == "pristroje");   // also the category of MCP list_machines
    CHECK(first_category(idx, "machines/sapi1v_hry.sapi") == "hry");
    // Manic Miner: jpr1s_bgp1s with its own title in the boards of Libor Lasota (its first category), among the
    // games with the title of the category
    CHECK(first_category(idx, "machines/jpr1s_bgp1s.sapi") == "libor");
    const MachineCategory* games = find_category(idx, "hry");
    CHECK(games != nullptr);
    bool manic = false;
    for (size_t i = 0; games && i < games->machines.size(); i++) {
        if (idx.machines[(size_t)games->machines[i]].file != "machines/jpr1s_bgp1s.sapi") continue;
        manic = i < games->titles.size() && games->titles[i] == "Manic Miner (JPR-1S + BGP-1S)" &&
                i < games->titles_en.size() && games->titles_en[i] == "Manic Miner (JPR-1S + BGP-1S)";
    }
    CHECK(manic);
    CHECK(machine_id("machines/zps/ZPS-1.sapi") == "zps_ZPS-1");
    CHECK(machine_id("machines/mikromon.sapi") == "mikromon");
    // every machine of machines/ is named by the index: supplied, so Save of a release build never overwrites it (a
    // machine missing there would count as the user's own, machine_supplied)
    CHECK(idx.listed);
    for (const MachineEntry& e : idx.machines) {
        if (e.file.rfind("machines/", 0) != 0) continue;
        if (!machine_supplied(idx, e.file)) fprintf(stderr, "%s: not in machines/_vyber.json\n", e.file.c_str());
        CHECK(machine_supplied(idx, e.file));
    }
}

static void test_fold(void)
{
    CHECK(machine_index_fold("áčďéěíňóřšťúůýž") == "acdeeinorstuuyz");
    CHECK(machine_index_fold("ÁČĎÉĚÍŇÓŘŠŤÚŮÝŽ") == "acdeeinorstuuyz");
    CHECK(machine_index_fold("Ľ ĺ Ŕ ô Ä ö ü") == "l l r o a o u");
    CHECK(machine_index_fold("ZPS-1 (MIKROBASIC), 40 × 24") == "zps-1 (mikrobasic), 40 × 24");   // others stay
    CHECK(machine_index_fold("") == "");
    MachineEntry e;   // not from the load: the search text is made on the spot
    e.file = "machines/x.sapi";
    e.title = "Záznamník dat KZD-1";
    CHECK(machine_index_matches(e, "zaznamnik"));
    CHECK(machine_index_matches(e, "ZÁZNAMNÍK DAT"));
    CHECK(machine_index_matches(e, ""));
    CHECK(!machine_index_matches(e, "zaznamnik kzd"));
}

// The intro, the titles of a machine in one category, problems of the index and the search on an index of the test.
static const char* k_index = R"json(
// a test index
{
    "intro": "SAPI-1 byl průmyslový řídicí počítač.",
    "intro_en": "SAPI-1 was an industrial control computer.",
    "categories": [
        {
            "id": "zaklad", "name": "Základ", "name_en": "Basics", "text": "Původní sestavy", "guides": true,
            "machines": [ "zps/ZPS-1.sapi", "jpr1s_bgp1s.sapi" ]
        },
        {
            "id": "hry", "name": "Hry", "name_en": "Games",
            "machines": [ "mikromon.sapi", "jpr1s_bgp1s.sapi" ],
            "titles": {
                "jpr1s_bgp1s.sapi": { "title": "Manic Miner (JPR-1S + BGP-1S)", "title_en": "Manic Miner (en)" },
                "zps/ZPS-1.sapi": { "title": "Not in the category" }
            }
        }
    ],
    "machines": {
        "zps/ZPS-1.sapi": { "title": "Záznamník ŽLUŤOUČKÝ kůň", "summary": "Výpočet" },
        "mikromon.sapi": { "title": "Zaznamnik bez hacku" }
    }
}
)json";

static void test_titles_and_search(void)
{
    MachineIndex idx;
    std::string err;
    machine_index_load_text(idx, k_index, err);
    CHECK(err.find("hry: title of machines/zps/ZPS-1.sapi") != std::string::npos);
    CHECK(idx.intro == "SAPI-1 byl průmyslový řídicí počítač.");
    CHECK(idx.intro_en == "SAPI-1 was an industrial control computer.");
    CHECK(idx.categories.size() == 3);   // and the machines the index does not name
    if (idx.categories.size() != 3) return;
    const MachineCategory& zaklad = idx.categories[0];
    const MachineCategory& hry = idx.categories[1];
    CHECK(idx.categories[2].id == "unsorted");
    CHECK(idx.categories[2].titles.size() == idx.categories[2].machines.size());
    CHECK(zaklad.titles.size() == 2 && zaklad.titles[0].empty() && zaklad.titles[1].empty());
    CHECK(hry.machines.size() == 2 && hry.titles.size() == 2 && hry.titles_en.size() == 2);
    if (hry.titles.size() == 2 && hry.titles_en.size() == 2) {
        CHECK(hry.titles[0].empty() && hry.titles_en[0].empty());
        CHECK(hry.titles[1] == "Manic Miner (JPR-1S + BGP-1S)");
        CHECK(hry.titles_en[1] == "Manic Miner (en)");
    }
    const MachineEntry* zps1 = machine_index_find(idx, "machines/zps/ZPS-1.sapi");
    const MachineEntry* mm = machine_index_find(idx, "machines/mikromon.sapi");
    const MachineEntry* bgp = machine_index_find(idx, "machines/jpr1s_bgp1s.sapi");
    CHECK(zps1 && mm && bgp);
    if (!zps1 || !mm || !bgp) return;
    CHECK(bgp->category == 0);   // the first category counts
    CHECK(bgp->title.empty());   // the title of the category is not its own
    // both sides folded: "zaznamnik" finds "Záznamník", "záznamník" finds "Zaznamnik"
    CHECK(machine_index_matches(*zps1, "zaznamnik"));
    CHECK(machine_index_matches(*zps1, "ZAZNAMNIK"));
    CHECK(machine_index_matches(*zps1, "Záznamník"));
    CHECK(machine_index_matches(*zps1, "zlutoucky kun"));
    CHECK(machine_index_matches(*zps1, "vypocet"));
    CHECK(machine_index_matches(*zps1, "VÝPOČET"));
    CHECK(machine_index_matches(*zps1, "zps/zps-1"));   // the file
    CHECK(!machine_index_matches(*zps1, "kun vypocet"));   // not across two texts
    CHECK(!machine_index_matches(*zps1, "xyzzy"));
    CHECK(machine_index_matches(*mm, "záznamník"));
    CHECK(!machine_index_matches(*mm, "vypocet"));
    CHECK(machine_index_matches(*bgp, "manic miner"));   // by its title in a category
    CHECK(machine_index_matches(*bgp, "Manic Miner (en)"));
    // an empty text is not an index; an empty index puts every machine among the unsorted ones
    machine_index_load_text(idx, "", err);
    CHECK(!err.empty());
    machine_index_load_text(idx, "{}", err);
    CHECK(err.empty());
    CHECK(idx.categories.size() == 1 && idx.categories[0].id == "unsorted" && idx.intro.empty());
}

// My machines (author 2026-10-29) in a data root of the test (work/test/mine_root): which files are supplied, the file
// and the name of a copy, the category at the top with the preview, the guide and the titles of the original.
static const char* k_mine_index = R"json(
{
    "categories": [ { "id": "zaklad", "name": "Základ", "name_en": "Basics", "guides": true, "machines": [ "zps/A.sapi" ] } ],
    "machines": { "zps/A.sapi": { "title": "Stroj A s BASICem", "title_en": "Machine A with BASIC" } }
}
)json";

static void test_mine(void)
{
    std::string repo = data_root();
    std::string root = path_join(repo, "work/test/mine_root");
    CHECK(make_dirs(path_join(root, "roms")));
    CHECK(make_dirs(path_join(root, "machines/zps")));
    CHECK(make_dirs(path_join(root, "machines/previews")));
    CHECK(make_dirs(path_join(root, "machines/guides")));
    CHECK(file_write_text(path_join(root, "machines/zps/A.sapi"), "{ \"name\": \"Stroj A\" }\n"));
    CHECK(file_write_text(path_join(root, "machines/previews/zps_A.png"), "not a picture"));
    CHECK(file_write_text(path_join(root, "machines/guides/zps_A.md"), "# A\n"));
    data_root_set(root);
    CHECK(work_root() == path_join(root, "work"));
    std::string mine = data_path(MACHINE_MINE_DIR);
    CHECK(make_dirs(mine));
    static const char* const copies[] = { "A.sapi", "A-2.sapi", "_skryta.sapi", nullptr };
    for (int i = 0; copies[i]; i++) file_remove(path_join(mine, copies[i]));   // of a run before

    // supplied: under machines/ of the data root and named by the index, however the path is written; a file of the
    // user saved there is the user's own; without an index every file under machines/
    CHECK(file_write_text(path_join(root, "machines/zps/Vlastni.sapi"), "{ \"name\": \"Můj vlastní\" }\n"));
    MachineIndex idx;
    std::string err;
    machine_index_load_text(idx, k_mine_index, err);
    CHECK(err.empty());
    CHECK(idx.listed);
    CHECK(machine_supplied(idx, "machines/zps/A.sapi"));
    CHECK(machine_supplied(idx, "machines\\zps\\A.sapi"));
    CHECK(machine_supplied(idx, data_path("machines/zps/A.sapi")));
    CHECK(!machine_supplied(idx, "machines/zps/Vlastni.sapi"));
    CHECK(!machine_supplied(idx, "work/machines/A.sapi"));
    CHECK(!machine_supplied(idx, data_path("work/machines/A.sapi")));
    CHECK(!machine_supplied(idx, "roms/x.bin"));
    CHECK(!machine_supplied(idx, ""));
    MachineIndex none;   // no index read
    CHECK(!none.listed);
    CHECK(machine_supplied(none, "machines/zps/Vlastni.sapi"));
    CHECK(!machine_supplied(none, "work/machines/A.sapi"));
    file_remove(path_join(root, "machines/zps/Vlastni.sapi"));

    // no copies: no category My machines
    machine_index_load_text(idx, k_mine_index, err);
    CHECK(err.empty());
    CHECK(!idx.categories.empty() && idx.categories[0].id == "zaklad");

    // the file of a copy: named after the original, the next one numbered; an older suffix becomes .sapi
    int n = 0;
    CHECK(machine_copy_file("machines/zps/A.sapi", &n) == "work/machines/A.sapi");
    CHECK_EQ(n, 1);
    CHECK(file_write_text(path_join(mine, "A.sapi"),
                          "{ \"name\": \"Stroj A (moje)\", \"based_on\": \"machines/zps/A.sapi\" }\n"));
    CHECK(machine_copy_file("machines/zps/A.sapi", &n) == "work/machines/A-2.sapi");
    CHECK_EQ(n, 2);
    CHECK(machine_copy_file("machines/A.jsonc") == "work/machines/A-2.sapi");
    // the name: marked in the language of the UI, numbered from the second; a name the user changed stays
    CHECK(machine_copy_name("Stroj A", "Stroj A", 1, true) == "Stroj A (moje)");
    CHECK(machine_copy_name("Stroj A", "Stroj A", 2, false) == "Stroj A (mine 2)");
    CHECK(machine_copy_name("Můj stroj", "Stroj A", 1, true) == "Můj stroj");

    // My machines at the top: the copy with the preview and the guide of its original, found by the original's title,
    // its short title with the mark in each language (not renamed; a renamed one keeps its name); a file starting with
    // "_" is no machine
    CHECK(file_write_text(path_join(mine, "A-2.sapi"),
                          "{ \"name\": \"Můj stroj\", \"based_on\": \"machines/zps/A.sapi\" }\n"));
    CHECK(file_write_text(path_join(mine, "_skryta.sapi"), "{}"));
    machine_index_load_text(idx, k_mine_index, err);
    CHECK(err.empty());
    CHECK(idx.categories.size() == 2);
    if (idx.categories.size() == 2) {
        const MachineCategory& m = idx.categories[0];
        CHECK(m.id == MACHINE_MINE && !m.name.empty() && !m.name_en.empty() && !m.text.empty() && !m.text_en.empty());
        CHECK(m.group.empty() && !m.guides);   // on its own; a guide does not open itself for the user's machines
        CHECK(m.machines.size() == 2 && m.titles.size() == 2 && m.titles_en.size() == 2);
        CHECK(idx.categories[1].id == "zaklad" && idx.categories[1].machines.size() == 1);
    }
    const MachineEntry* c = machine_index_find(idx, "work/machines/A.sapi");
    const MachineEntry* r = machine_index_find(idx, "work/machines/A-2.sapi");
    const MachineEntry* o = machine_index_find(idx, "machines/zps/A.sapi");
    CHECK(c && r && o);
    if (c && r && o) {
        CHECK(c->category == 0 && r->category == 0 && o->category == 1);
        CHECK(c->based_on == "machines/zps/A.sapi" && o->based_on.empty());
        CHECK(c->name == "Stroj A (moje)");
        CHECK(c->copy == 1 && r->copy == 0 && o->copy == 0);   // r renamed by the user
        CHECK(o->listed && !c->listed && !r->listed);
        CHECK(c->title == "Stroj A s BASICem (moje)" && c->title_en == "Machine A with BASIC (mine)");
        CHECK(r->name == "Můj stroj" && r->title.empty() && r->title_en.empty());
        CHECK(c->preview == "machines/previews/zps_A.png" && c->preview == o->preview && r->preview == o->preview);
        CHECK(c->guide == "machines/guides/zps_A.md" && c->guide_en.empty());
        CHECK(machine_index_matches(*c, "s basicem"));   // the title of the original
        CHECK(machine_index_matches(*r, "machine a with"));
        CHECK(machine_index_matches(*c, "moje"));
        CHECK(!machine_index_matches(*o, "moje"));
    }
    CHECK(!machine_index_find(idx, "work/machines/_skryta.sapi"));

    for (int i = 0; copies[i]; i++) file_remove(path_join(mine, copies[i]));
    data_root_set(repo);
}

int main(void)
{
    test_init();
    test_index();
    test_fold();
    test_titles_and_search();
    test_mine();
    return TEST_RESULT();
}
