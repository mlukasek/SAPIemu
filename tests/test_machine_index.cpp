// The machines offered in the window "Choose a machine" (core/machine_index.h, machines/_vyber.json): the index
// names only existing machines, every machine of a category with guides has its guide, the ids, files starting
// with "_" are not machines, own machines fall into the last category.
#include "core/machine_index.h"
#include "core/util.h"
#include "test_util.h"

#include <stdio.h>

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
    }
    // the first category is the basics, each of its machines has a short text and a guide
    CHECK(idx.categories[0].id == "zaklad");
    for (const MachineCategory& c : idx.categories) {
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
    const MachineEntry* hry = machine_index_find(idx, "machines/sapi1v_hry.sapi");
    CHECK(hry != nullptr);
    if (hry) CHECK(idx.categories[(size_t)hry->category].id == "hry");
    CHECK(machine_id("machines/zps/ZPS-1.sapi") == "zps_ZPS-1");
    CHECK(machine_id("machines/mikromon.sapi") == "mikromon");
}

int main(void)
{
    test_init();
    test_index();
    return TEST_RESULT();
}
