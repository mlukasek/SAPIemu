#pragma once
// The machines offered to the user (the window "Choose a machine", MCP list_machines): the categories and short
// texts from machines/_vyber.json, the name and the description from each .sapi file, a preview picture
// machines/previews/<id>.png and a guide machines/guides/<id>.md (<id>.en.md in English), where <id> is the file
// relative to machines/ without .sapi, "/" replaced by "_" (zps/ZPS-1.sapi -> zps_ZPS-1). Machines the index does
// not name (own ones) fall into the category "other" at the end. docs/vydani/sestavy.md.
//
// machines/_vyber.json: "intro", "intro_en" (the text over the list), "categories" (in the order shown: "id", "name",
// "name_en", "text", "text_en", "group", "group_en", "guides", "machines": files relative to machines/, "titles":
// { "<file>": { "title", "title_en" } } = the title of a machine in this category only) and "machines"
// ({ "<file>": { "title", "title_en", "summary", "summary_en" } }).
#include <string>
#include <vector>

struct MachineEntry {
    std::string file;              // relative to the data root: machines/zps/ZPS-1.sapi
    std::string id;                // zps_ZPS-1
    std::string name, description; // from the .sapi file
    std::string title, title_en, summary, summary_en;   // from the index: a short name, a text for newcomers
    std::string preview;           // machines/previews/<id>.png if it exists
    std::string guide, guide_en;   // machines/guides/<id>.md, .en.md if they exist
    int category = -1;             // index into MachineIndex::categories
    std::string search;            // what the search looks in (machine_index_fold of the file, the name, all titles
                                   // and summaries), made by the load
};

struct MachineCategory {
    std::string id, name, name_en, text, text_en;
    std::string group, group_en;   // categories of one group are shown under one header ("Ostatní")
    bool guides = false;           // the guide opens itself when a machine of this category starts
    std::vector<int> machines;     // indexes into MachineIndex::machines, in the order of the index
    // the title of each of the machines in this category only ("titles" of the index: jpr1s_bgp1s as Manic Miner
    // among the games), empty = its own title; as long as machines
    std::vector<std::string> titles, titles_en;
};

struct MachineIndex {
    std::vector<MachineCategory> categories;
    std::vector<MachineEntry> machines;
    std::string intro, intro_en;   // the text over the list: what SAPI-1 was, where to start
};

// Reads the index and all machine files; err gets problems of the index (the list is built anyway).
void machine_index_load(MachineIndex& out, std::string& err);
// The same with the text of the index given instead of machines/_vyber.json (the machine files are read; tests).
void machine_index_load_text(MachineIndex& out, const std::string& index_json, std::string& err);
const MachineEntry* machine_index_find(const MachineIndex& idx, const std::string& file);
// The id of a machine file (machines/zps/ZPS-1.sapi -> zps_ZPS-1).
std::string machine_id(const std::string& file);
// For the search: lower case and the Czech (and Slovak) letters without diacritics ("Záznamník" -> "zaznamnik");
// other characters stay as they are.
std::string machine_index_fold(const std::string& s);
// The search of the window "Choose a machine": the filter is a part of the file, the name, a title (also one of
// a category) or a summary; case and diacritics do not matter on either side ("zaznamnik" finds "Záznamník").
// An empty filter matches every machine.
bool machine_index_matches(const MachineEntry& e, const char* filter);
