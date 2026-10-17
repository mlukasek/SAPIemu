#include "core/machine_index.h"
#include "core/util.h"
#include "json.hpp"

using nlohmann::json;

std::string machine_id(const std::string& file)
{
    std::string f = file;
    if (f.rfind("machines/", 0) == 0) f = f.substr(9);
    size_t dot = f.rfind('.');
    if (dot != std::string::npos && f.find('/', dot) == std::string::npos) f = f.substr(0, dot);
    for (char& c : f)
        if (c == '/' || c == '\\') c = '_';
    return f;
}

static std::string jstr(const json& j, const char* key)
{
    if (j.is_object() && j.contains(key) && j[key].is_string()) return j[key].get<std::string>();
    return std::string();
}

void machine_index_load(MachineIndex& out, std::string& err)
{
    out = MachineIndex();
    err.clear();
    for (const std::string& f : list_machine_files()) {
        MachineEntry e;
        e.file = f;
        e.id = machine_id(f);
        std::string text;
        if (file_read_text(data_path(f), text)) {
            json j = json::parse(text, nullptr, false, true /* comments */);
            if (!j.is_discarded()) {
                e.name = jstr(j, "name");
                e.description = jstr(j, "description");
            }
        }
        if (e.name.empty()) e.name = e.id;
        std::string p = "machines/previews/" + e.id + ".png";
        if (file_exists(data_path(p))) e.preview = p;
        p = "machines/guides/" + e.id + ".md";
        if (file_exists(data_path(p))) e.guide = p;
        p = "machines/guides/" + e.id + ".en.md";
        if (file_exists(data_path(p))) e.guide_en = p;
        out.machines.push_back(e);
    }
    std::string text;
    json idx;
    if (file_read_text(data_path("machines/_vyber.json"), text)) {
        idx = json::parse(text, nullptr, false, true);
        if (idx.is_discarded()) {
            err = "machines/_vyber.json: not valid JSON";
            idx = json::object();
        }
    }
    const json& texts = idx.contains("machines") ? idx["machines"] : json::object();
    for (MachineEntry& e : out.machines) {
        std::string rel = e.file.rfind("machines/", 0) == 0 ? e.file.substr(9) : e.file;
        if (!texts.is_object() || !texts.contains(rel)) continue;
        const json& t = texts[rel];
        e.title = jstr(t, "title");
        e.title_en = jstr(t, "title_en");
        e.summary = jstr(t, "summary");
        e.summary_en = jstr(t, "summary_en");
    }
    std::vector<bool> placed(out.machines.size(), false);
    if (idx.contains("categories") && idx["categories"].is_array()) {
        for (const json& c : idx["categories"]) {
            MachineCategory cat;
            cat.id = jstr(c, "id");
            cat.name = jstr(c, "name");
            cat.name_en = jstr(c, "name_en");
            cat.text = jstr(c, "text");
            cat.text_en = jstr(c, "text_en");
            cat.group = jstr(c, "group");
            cat.group_en = jstr(c, "group_en");
            cat.guides = c.value("guides", false);
            if (c.contains("machines") && c["machines"].is_array()) {
                for (const json& m : c["machines"]) {
                    if (!m.is_string()) continue;
                    std::string f = "machines/" + m.get<std::string>();
                    size_t k = 0;
                    while (k < out.machines.size() && out.machines[k].file != f) k++;
                    if (k == out.machines.size()) {
                        err += (err.empty() ? "" : "; ") + f + ": no such machine";
                        continue;
                    }
                    cat.machines.push_back((int)k);
                    if (!placed[k]) out.machines[k].category = (int)out.categories.size();   // the first one counts
                    placed[k] = true;
                }
            }
            out.categories.push_back(cat);
        }
    }
    // machines the index does not name: own ones, new ones
    MachineCategory rest;
    rest.id = "unsorted";
    rest.name = "Nezařazené a vlastní sestavy";
    rest.name_en = "Unsorted and own machines";
    rest.group = "Ostatní";
    rest.group_en = "Other";
    for (size_t k = 0; k < out.machines.size(); k++)
        if (!placed[k]) {
            out.machines[k].category = (int)out.categories.size();
            rest.machines.push_back((int)k);
        }
    if (!rest.machines.empty()) out.categories.push_back(rest);
}

const MachineEntry* machine_index_find(const MachineIndex& idx, const std::string& file)
{
    for (const MachineEntry& e : idx.machines)
        if (e.file == file) return &e;
    return nullptr;
}
