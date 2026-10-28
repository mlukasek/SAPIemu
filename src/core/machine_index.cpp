#include "core/machine_index.h"
#include "core/charset.h"
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

static void add_err(std::string& err, const std::string& e)
{
    err += (err.empty() ? "" : "; ") + e;
}

// ---------------------------------------------------------------- the search

// The letter without its diacritics for the Czech and Slovak ones (and ä ö ü), both cases; 0 = not one of them.
static char plain_letter(int32_t cp)
{
    switch (cp) {
    case 0xE1: case 0xC1: case 0xE4: case 0xC4: return 'a';                         // á Á ä Ä
    case 0x10D: case 0x10C: return 'c';                                             // č Č
    case 0x10F: case 0x10E: return 'd';                                             // ď Ď
    case 0xE9: case 0xC9: case 0x11B: case 0x11A: return 'e';                       // é É ě Ě
    case 0xED: case 0xCD: return 'i';                                               // í Í
    case 0x13A: case 0x139: case 0x13E: case 0x13D: return 'l';                     // ĺ Ĺ ľ Ľ
    case 0x148: case 0x147: return 'n';                                             // ň Ň
    case 0xF3: case 0xD3: case 0xF4: case 0xD4: case 0xF6: case 0xD6: return 'o';   // ó Ó ô Ô ö Ö
    case 0x155: case 0x154: case 0x159: case 0x158: return 'r';                     // ŕ Ŕ ř Ř
    case 0x161: case 0x160: return 's';                                             // š Š
    case 0x165: case 0x164: return 't';                                             // ť Ť
    case 0xFA: case 0xDA: case 0x16F: case 0x16E: case 0xFC: case 0xDC: return 'u'; // ú Ú ů Ů ü Ü
    case 0xFD: case 0xDD: return 'y';                                               // ý Ý
    case 0x17E: case 0x17D: return 'z';                                             // ž Ž
    default: return 0;
    }
}

std::string machine_index_fold(const std::string& s)
{
    std::string o;
    o.reserve(s.size());
    size_t pos = 0;
    while (pos < s.size()) {
        size_t at = pos;
        int32_t cp = utf8_next(s, pos);
        if (cp >= 0 && cp < 0x80) {
            o += (char)(cp >= 'A' && cp <= 'Z' ? cp - 'A' + 'a' : cp);
            continue;
        }
        char c = cp > 0 ? plain_letter(cp) : 0;
        if (c) o += c;
        else o.append(s, at, pos - at);   // another character (or a broken sequence) as it is
    }
    return o;
}

// The texts of a machine the search looks in, one per line: a filter does not match across two of them.
static std::string search_text(const MachineEntry& e)
{
    return e.file + "\n" + e.name + "\n" + e.title + "\n" + e.title_en + "\n" + e.summary + "\n" + e.summary_en;
}

bool machine_index_matches(const MachineEntry& e, const char* filter)
{
    if (!filter || !filter[0]) return true;
    std::string f = machine_index_fold(filter);
    if (e.search.empty()) return machine_index_fold(search_text(e)).find(f) != std::string::npos;   // not loaded
    return e.search.find(f) != std::string::npos;
}

// ---------------------------------------------------------------- loading

// index: the text of machines/_vyber.json, nullptr = there is none
static void load(MachineIndex& out, const std::string* index, std::string& err)
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
    json idx = json::object();
    if (index) {
        idx = json::parse(*index, nullptr, false, true);
        if (idx.is_discarded() || !idx.is_object()) {
            err = "machines/_vyber.json: not valid JSON";
            idx = json::object();
        }
    }
    out.intro = jstr(idx, "intro");
    out.intro_en = jstr(idx, "intro_en");
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
    std::vector<std::string> titles(out.machines.size());   // the titles of categories, for the search
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
                        add_err(err, f + ": no such machine");
                        continue;
                    }
                    cat.machines.push_back((int)k);
                    if (!placed[k]) out.machines[k].category = (int)out.categories.size();   // the first one counts
                    placed[k] = true;
                }
            }
            cat.titles.assign(cat.machines.size(), std::string());
            cat.titles_en.assign(cat.machines.size(), std::string());
            if (c.contains("titles")) {
                const json& t = c["titles"];
                if (!t.is_object()) add_err(err, cat.id + ": titles is not an object");
                for (auto it = t.begin(); t.is_object() && it != t.end(); ++it) {
                    std::string f = "machines/" + it.key();
                    size_t i = 0;
                    while (i < cat.machines.size() && out.machines[(size_t)cat.machines[i]].file != f) i++;
                    if (i == cat.machines.size()) {
                        add_err(err, cat.id + ": title of " + f + ", which is not in the category");
                        continue;
                    }
                    cat.titles[i] = jstr(it.value(), "title");
                    cat.titles_en[i] = jstr(it.value(), "title_en");
                    titles[(size_t)cat.machines[i]] += "\n" + cat.titles[i] + "\n" + cat.titles_en[i];
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
    rest.titles.assign(rest.machines.size(), std::string());
    rest.titles_en.assign(rest.machines.size(), std::string());
    if (!rest.machines.empty()) out.categories.push_back(rest);
    for (size_t k = 0; k < out.machines.size(); k++)
        out.machines[k].search = machine_index_fold(search_text(out.machines[k]) + titles[k]);
}

void machine_index_load(MachineIndex& out, std::string& err)
{
    std::string text;
    bool have = file_read_text(data_path("machines/_vyber.json"), text);
    load(out, have ? &text : nullptr, err);
}

void machine_index_load_text(MachineIndex& out, const std::string& index_json, std::string& err)
{
    load(out, &index_json, err);
}

const MachineEntry* machine_index_find(const MachineIndex& idx, const std::string& file)
{
    for (const MachineEntry& e : idx.machines)
        if (e.file == file) return &e;
    return nullptr;
}
