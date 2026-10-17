#pragma once
// The machines offered to the user (the window "Choose a machine", MCP list_machines): the categories and short
// texts from machines/_vyber.json, the name and the description from each .sapi file, a preview picture
// machines/previews/<id>.png and a guide machines/guides/<id>.md (<id>.en.md in English), where <id> is the file
// relative to machines/ without .sapi, "/" replaced by "_" (zps/ZPS-1.sapi -> zps_ZPS-1). Machines the index does
// not name (own ones) fall into the category "other" at the end. docs/vydani/sestavy.md.
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
};

struct MachineCategory {
    std::string id, name, name_en, text, text_en;
    std::string group, group_en;   // categories of one group are shown under one header ("Ostatní")
    bool guides = false;           // the guide opens itself when a machine of this category starts
    std::vector<int> machines;     // indexes into MachineIndex::machines, in the order of the index
};

struct MachineIndex {
    std::vector<MachineCategory> categories;
    std::vector<MachineEntry> machines;
};

// Reads the index and all machine files; err gets problems of the index (the list is built anyway).
void machine_index_load(MachineIndex& out, std::string& err);
const MachineEntry* machine_index_find(const MachineIndex& idx, const std::string& file);
// The id of a machine file (machines/zps/ZPS-1.sapi -> zps_ZPS-1).
std::string machine_id(const std::string& file);
