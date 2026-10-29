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
// ({ "<file>": { "title", "title_en", "summary", "summary_en", "name_en" } }; name_en = the name of the .sapi file in
// English, shown under the title in English).
//
// "My machines" (author 2026-10-29): Save in the configurator of a release build writes a supplied machine (a file of
// machines/ in the data root) as a copy into work/machines/ (work_root(): writable also when the data folder is not; the
// paths inside a machine file are relative to the data root, so the copy works from there as the original did), with
// "based_on": the original file, and its name marked; the supplied file stays as it was (supplied = named by the index,
// machine_supplied; the user's own files are saved as they are). The copies (every
// work/machines/*.sapi) are the category MACHINE_MINE at the top of the index, only when there are some: their title is
// their name, their preview and guides are those of the original.
#include <string>
#include <vector>

#define MACHINE_MINE "mine"                  // the id of the category My machines
#define MACHINE_MINE_DIR "work/machines"     // where the copies are (data relative)

struct MachineEntry {
    std::string file;              // relative to the data root: machines/zps/ZPS-1.sapi
    std::string id;                // zps_ZPS-1
    std::string based_on;          // a copy of My machines: the supplied machine it was saved from (machines/...)
    std::string name, description; // from the .sapi file
    std::string title, title_en, summary, summary_en;   // from the index: a short name, a text for newcomers
    std::string name_en;           // from the index: the name in English (the ECMON machines: the cards in it), or empty
    std::string preview;           // machines/previews/<id>.png if it exists
    std::string guide, guide_en;   // machines/guides/<id>.md, .en.md if they exist
    int category = -1;             // index into MachineIndex::categories
    std::string search;            // what the search looks in (machine_index_fold of the file, the name, all titles
                                   // and summaries), made by the load
    bool listed = false;           // named by machines/_vyber.json (a category or "machines"): supplied with SAPIemu
    int copy = 0;                  // a copy of My machines not renamed: its number (machine_copy_name), else 0
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
    bool listed = false;           // machines/_vyber.json was read (MachineEntry::listed means something)
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

// My machines: the file (data relative, whole or with backslashes) is a supplied machine: under machines/ of the data
// root and named by the index (machines/_vyber.json names every machine supplied with SAPIemu). A file of the user saved
// there (Save as..., a new machine) is the user's own, Save writes into it. Without an index (idx.listed false) every
// file under machines/ counts as supplied.
bool machine_supplied(const MachineIndex& idx, const std::string& file);
// Where Save puts the copy of the supplied `file`: work/machines/<its file name>, or <name>-2, -3... when that is
// taken (an earlier copy of it, or of a machine of the same file name in another folder of machines/). n_out (when
// given): 1 for the first name, 2, 3...
std::string machine_copy_file(const std::string& file, int* n_out = nullptr);
// The name of the copy number n (machine_copy_file): `name` with " (moje)" (czech) or " (mine)", " (moje 2)" for the
// second...; a name the user changed in the configurator (not `orig_name`, the name in the supplied file) stays.
std::string machine_copy_name(const std::string& name, const std::string& orig_name, int n, bool czech);
