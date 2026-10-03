#include "core/machine_cfg.h"
#include "core/util.h"

#include <ctype.h>
#include <string.h>
#include <algorithm>

using nlohmann::json;

// ---------------------------------------------------------------- paths

static std::vector<std::string> split_path(const char* path)
{
    std::vector<std::string> segs;
    std::string cur;
    for (const char* s = path; *s; s++) {
        if (*s == '.') { segs.push_back(cur); cur.clear(); }
        else cur += *s;
    }
    segs.push_back(cur);
    return segs;
}

static bool is_index(const std::string& s)
{
    if (s.empty()) return false;
    for (char c : s) if (!isdigit((unsigned char)c)) return false;
    return true;
}

const json* cfg_find(const json& obj, const char* path)
{
    const json* cur = &obj;
    for (const std::string& s : split_path(path)) {
        if (cur->is_object()) {
            auto it = cur->find(s);
            if (it == cur->end()) return nullptr;
            cur = &*it;
        } else if (cur->is_array() && is_index(s)) {
            size_t i = (size_t)atoi(s.c_str());
            if (i >= cur->size()) return nullptr;
            cur = &(*cur)[i];
        } else {
            return nullptr;
        }
    }
    return cur;
}

void cfg_erase(json& obj, const char* path)
{
    std::vector<std::string> segs = split_path(path);
    json* cur = &obj;
    for (size_t k = 0; k + 1 < segs.size(); k++) {
        const std::string& s = segs[k];
        if (cur->is_object() && cur->contains(s)) cur = &(*cur)[s];
        else if (cur->is_array() && is_index(s) && (size_t)atoi(s.c_str()) < cur->size()) cur = &(*cur)[(size_t)atoi(s.c_str())];
        else return;
    }
    const std::string& last = segs.back();
    if (cur->is_object()) cur->erase(last);
    else if (cur->is_array() && is_index(last) && (size_t)atoi(last.c_str()) < cur->size()) (*cur)[(size_t)atoi(last.c_str())] = nullptr;
}

// Reference to the value at a path, creating objects / arrays on the way.
static json& cfg_make(json& obj, const std::vector<std::string>& segs)
{
    json* cur = &obj;
    for (size_t k = 0; k < segs.size(); k++) {
        const std::string& s = segs[k];
        if (is_index(s) && (cur->is_array() || cur->is_null())) {
            if (cur->is_null()) *cur = json::array();
            size_t i = (size_t)atoi(s.c_str());
            while (cur->size() <= i) cur->push_back(nullptr);
            cur = &(*cur)[i];
        } else {
            if (!cur->is_object()) *cur = json::object();
            cur = &(*cur)[s];
        }
    }
    return *cur;
}

// ---------------------------------------------------------------- values

static bool as_number(const json& v, long long& out)
{
    if (v.is_number_integer()) { out = v.get<long long>(); return true; }
    if (v.is_number_unsigned()) { out = (long long)v.get<unsigned long long>(); return true; }
    if (v.is_string()) return parse_number(v.get<std::string>(), out);
    return false;
}

bool cfg_values_equal(const json& a, const json& b)
{
    if (a == b) return true;
    long long x, y;
    if ((a.is_number() || b.is_number() || (a.is_string() && b.is_string())) && as_number(a, x) && as_number(b, y))
        return x == y;
    return false;
}

json param_default(const CardParam* p)
{
    json v = json::parse(p->def ? p->def : "null", nullptr, false);
    return v.is_discarded() ? json() : v;
}

json card_param_value(const json& card, const CardParam* p)
{
    const json* v = cfg_find(card, p->path);
    if (p->kind == PK_SECTION) {
        if (!v) return param_default(p);
        return json(!v->is_null());
    }
    if (!v) return param_default(p);
    return *v;
}

int param_option_index(const CardParam* p, const json& v)
{
    if (!p->options) return -1;
    for (int i = 0; p->options[i].value; i++) {
        json o = json::parse(p->options[i].value, nullptr, false);
        if (!o.is_discarded() && cfg_values_equal(o, v)) return i;
    }
    return -1;
}

static bool has_prefix(const char* path, const char* prefix)
{
    size_t n = strlen(prefix);
    return strncmp(path, prefix, n) == 0 && path[n] == '.';
}

const CardParam* param_section(const CardInfo* ci, const CardParam* p)
{
    if (!ci || !ci->params) return nullptr;
    for (const CardParam* s = ci->params; s->path; s++)
        if (s->kind == PK_SECTION && s != p && has_prefix(p->path, s->path)) return s;
    return nullptr;
}

bool card_param_active(const json& card, const CardInfo* ci, const CardParam* p)
{
    const CardParam* s = param_section(ci, p);
    if (!s) return true;
    return card_param_value(card, s).get<bool>() && card_param_active(card, ci, s);
}

// Path of the array a parameter is an element of ("write_enable" for "write_enable.2"), empty if none.
static std::string array_of(const char* path)
{
    std::vector<std::string> segs = split_path(path);
    if (segs.size() < 2 || !is_index(segs.back())) return std::string();
    std::string a = segs[0];
    for (size_t i = 1; i + 1 < segs.size(); i++) a += "." + segs[i];
    return a;
}

void card_set_param(json& card, const CardInfo* ci, const CardParam* p, const json& v)
{
    if (p->kind == PK_SECTION) {
        bool on = v.is_boolean() ? v.get<bool>() : !v.is_null();
        const json* cur = cfg_find(card, p->path);
        if (!on) {
            cfg_erase(card, p->path);
            return;
        }
        if (cur && cur->is_object()) return;
        cfg_make(card, split_path(p->path)) = json::object();
        for (const CardParam* c = ci->params; c && c->path; c++) {
            if (!has_prefix(c->path, p->path) || c->kind == PK_SECTION) continue;
            json d = param_default(c);
            if (!d.is_null()) card_set_param(card, ci, c, d);
        }
        return;
    }
    std::string arr = array_of(p->path);
    if (!arr.empty() && !cfg_find(card, arr.c_str()) && ci && ci->params) {
        // create the whole array with the defaults of its elements
        for (const CardParam* c = ci->params; c->path; c++)
            if (array_of(c->path) == arr) cfg_make(card, split_path(c->path)) = param_default(c);
    }
    if (v.is_null() && arr.empty()) {
        cfg_erase(card, p->path);
        return;
    }
    cfg_make(card, split_path(p->path)) = v;
}

json card_new_config(const CardInfo* ci, int slot)
{
    json card = json::object();
    card["slot"] = slot;
    card["type"] = ci->type;
    for (const CardParam* p = ci->params; p && p->path; p++) {
        if (p->kind == PK_SECTION) {
            if (param_default(p).get<bool>() && card_param_active(card, ci, p)) card_set_param(card, ci, p, true);
            continue;
        }
        if (!card_param_active(card, ci, p)) continue;
        json d = param_default(p);
        if (p->kind == PK_KEYBOARD) d = json{ { "type", "EKL-1" } };   // a new CPU card comes with a keyboard
        if (!d.is_null()) card_set_param(card, ci, p, d);
    }
    return card;
}

// ---------------------------------------------------------------- layout

static const BusInfo* cfg_bus(const json& cfg, std::string& type)
{
    type = "ARB-1C-8";
    if (cfg.contains("bus") && cfg["bus"].is_object()) type = cfg["bus"].value("type", type);
    return bus_info(type);
}

int machine_cfg_slots(const json& cfg)
{
    std::string type;
    const BusInfo* b = cfg_bus(cfg, type);
    int n = b ? b->slots : 8;
    if (cfg.contains("bus") && cfg["bus"].is_object() && cfg["bus"].contains("slots")) {
        long long v;
        if (as_number(cfg["bus"]["slots"], v) && v >= 1 && v <= 32) n = (int)v;
    }
    return n;
}

bool machine_cfg_cpu_in_x1(const json& cfg)
{
    std::string type;
    const BusInfo* b = cfg_bus(cfg, type);
    std::string jp3 = "2-3";
    if (cfg.contains("bus") && cfg["bus"].is_object()) jp3 = cfg["bus"].value("jp3", jp3);
    return !(b && b->has_jp3 && jp3 == "1-2");
}

int card_cfg_width(const json& card)
{
    const CardInfo* ci = card_info(card.value("type", std::string()));
    return ci ? ci->width : 1;
}

static int card_slot(const json& card)
{
    long long v = 0;
    if (card.contains("slot")) as_number(card["slot"], v);
    return (int)v;
}

int machine_cfg_card_at(const json& cfg, int slot)
{
    if (!cfg.contains("cards") || !cfg["cards"].is_array()) return -1;
    const json& cards = cfg["cards"];
    for (size_t i = 0; i < cards.size(); i++) {
        int s = card_slot(cards[i]);
        if (slot >= s && slot < s + card_cfg_width(cards[i])) return (int)i;
    }
    return -1;
}

std::vector<CfgProblem> machine_cfg_problems(const json& cfg)
{
    std::vector<CfgProblem> out;
    std::string btype;
    if (!cfg_bus(cfg, btype))
        out.push_back({ 0, true, "unknown bus " + btype, "neznámá sběrnice " + btype });
    int slots = machine_cfg_slots(cfg);
    bool x1 = machine_cfg_cpu_in_x1(cfg);
    std::vector<int> used(slots + 2, 0);
    int cpus = 0;
    if (cfg.contains("cards") && cfg["cards"].is_array()) {
        for (const json& c : cfg["cards"]) {
            std::string type = c.value("type", std::string());
            const CardInfo* ci = card_info(type);
            int s = card_slot(c);
            int w = ci ? ci->width : 1;
            if (!ci) {
                out.push_back({ s, true, "unknown card " + type, "neznámá karta " + type });
                continue;
            }
            if (s < 1 || s + w - 1 > slots) {
                out.push_back({ s, true, type + ": slot out of range", type + ": slot mimo rozsah sběrnice" });
                continue;
            }
            for (int i = 0; i < w; i++) {
                if (used[s + i])
                    out.push_back({ s + i, true, "X" + std::to_string(s + i) + ": two cards in one slot",
                                    "X" + std::to_string(s + i) + ": dvě karty v jednom slotu" });
                used[s + i] = 1;
            }
            if (ci->cls == CC_CPU) {
                cpus++;
                if (x1 && s != 1)
                    out.push_back({ s, true, type + ": the CPU card belongs to X1",
                                    type + ": procesorová karta patří do X1" });
                if (!ci->emulated)
                    out.push_back({ s, true, type + ": CPU card not emulated yet, the machine cannot run",
                                    type + ": procesorová karta zatím není emulovaná, sestava nepoběží" });
            } else {
                if (x1 && s == 1)
                    out.push_back({ s, true, "X1 is reserved for the CPU card", "X1 je vyhrazený pro procesorovou kartu" });
                if (!ci->emulated)
                    out.push_back({ s, false, type + ": not emulated yet (passive)",
                                    type + ": zatím neemulovaná (neodpovídá)" });
            }
        }
    }
    if (cpus == 0) out.push_back({ 0, true, "no CPU card", "chybí procesorová karta" });
    if (cpus > 1) out.push_back({ 0, true, "more than one CPU card", "víc procesorových karet" });
    return out;
}

// ---------------------------------------------------------------- .sapi writer (JSON with comments)

namespace {

struct Line {
    std::string text;
    std::string comment;
};

struct Writer {
    const CardInfo* ci = nullptr;   // card being written (nullptr = root / bus)
    bool bus = false;
};

std::string scalar(const json& v)
{
    return v.dump(-1, ' ', false, json::error_handler_t::replace);
}

const CardParam* find_param(const CardInfo* ci, const std::string& path)
{
    if (!ci || !ci->params) return nullptr;
    for (const CardParam* p = ci->params; p->path; p++)
        if (path == p->path) return p;
    return nullptr;
}

std::string comment_for(const Writer& w, const std::string& path, const json& v)
{
    if (w.bus) {
        if (path == "type") {
            const BusInfo* b = v.is_string() ? bus_info(v.get<std::string>()) : nullptr;
            return b ? b->en : std::string();
        }
        if (path == "jp3") return "JP3: 2-3 = CPU card in X1, 1-2 = address buffers off (PKT-1)";
        return std::string();
    }
    if (!w.ci) return std::string();
    if (path == "type") return w.ci->en;
    if (path == "slot") return "position X" + scalar(v);
    const CardParam* p = find_param(w.ci, path);
    if (!p) return std::string();
    std::string c = p->en;
    if (p->kind == PK_CHOICE || p->kind == PK_FILE) {
        int i = param_option_index(p, v);
        if (i >= 0 && scalar(v) != std::string("\"") + p->options[i].en + "\"" && scalar(v) != p->options[i].en)
            c += std::string(" - ") + p->options[i].en;
    }
    return c;
}

bool has_comments(const Writer& w, const std::string& path, const json& v)
{
    if (v.is_object()) {
        for (auto it = v.begin(); it != v.end(); ++it) {
            std::string p = path.empty() ? it.key() : path + "." + it.key();
            if (!comment_for(w, p, it.value()).empty() || has_comments(w, p, it.value())) return true;
        }
    } else if (v.is_array()) {
        for (size_t i = 0; i < v.size(); i++) {
            std::string p = path + "." + std::to_string(i);
            if (!comment_for(w, p, v[i]).empty() || has_comments(w, p, v[i])) return true;
        }
    }
    return false;
}

bool only_scalars(const json& v)
{
    for (const json& e : v)
        if (e.is_object() || e.is_array()) return false;
    return true;
}

// Keys of an object in the order of the catalog parameters.
std::vector<std::string> key_order(const Writer& w, const std::string& path, const json& obj)
{
    std::vector<std::string> keys;
    auto add = [&](const std::string& k) {
        if (obj.contains(k) && std::find(keys.begin(), keys.end(), k) == keys.end()) keys.push_back(k);
    };
    if (path.empty() && !w.ci && !w.bus) {
        add("name"); add("description"); add("bus"); add("cards");
    } else if (w.bus) {
        add("type"); add("slots"); add("jp3");
    } else if (path.empty()) {
        add("slot"); add("type"); add("name");
    } else {
        add("type");
    }
    if (w.ci && w.ci->params) {
        std::string prefix = path.empty() ? std::string() : path + ".";
        for (const CardParam* p = w.ci->params; p->path; p++) {
            std::string pp = p->path;
            if (pp.compare(0, prefix.size(), prefix) != 0) continue;
            std::string rest = pp.substr(prefix.size());
            add(rest.substr(0, rest.find('.')));
        }
    }
    for (auto it = obj.begin(); it != obj.end(); ++it) add(it.key());
    return keys;
}

void value_lines(const Writer& w, const json& v, const std::string& path, std::vector<Line>& out, int ind);

void member_lines(const Writer& w, const std::string& prefix, const json& v, const std::string& path,
                  std::vector<Line>& out, int ind, bool last)
{
    size_t first = out.size();
    value_lines(w, v, path, out, ind);
    out[first].text = std::string((size_t)ind, ' ') + prefix + out[first].text.substr((size_t)ind);
    std::string c = comment_for(w, path, v);
    if (!c.empty()) out[first].comment = c;
    if (!last) out.back().text += ",";
}

void value_lines(const Writer& w, const json& v, const std::string& path, std::vector<Line>& out, int ind)
{
    std::string pad((size_t)ind, ' ');
    bool container = v.is_object() || v.is_array();
    bool root = path.empty();
    if (container && !root && only_scalars(v) && !has_comments(w, path, v)) {
        std::string s = scalar(v);
        if (v.is_object() && !v.empty()) {   // "{ "a": 1 }" with spaces, keys in catalog order
            s = "{ ";
            std::vector<std::string> keys = key_order(w, path, v);
            for (size_t i = 0; i < keys.size(); i++)
                s += scalar(json(keys[i])) + ": " + scalar(v[keys[i]]) + (i + 1 < keys.size() ? ", " : " }");
        } else if (v.is_array() && !v.empty()) {
            s = "[ ";
            for (size_t i = 0; i < v.size(); i++) s += scalar(v[i]) + (i + 1 < v.size() ? ", " : " ]");
        }
        if (s.size() <= 80) {
            out.push_back({ pad + s, std::string() });
            return;
        }
    }
    if (v.is_object()) {
        out.push_back({ pad + "{", std::string() });
        std::vector<std::string> keys = key_order(w, path, v);
        for (size_t i = 0; i < keys.size(); i++) {
            const std::string& k = keys[i];
            std::string p = root ? k : path + "." + k;
            Writer cw = w;
            if (!w.ci && !w.bus && root && k == "bus") {
                cw.bus = true;
                std::vector<Line> tmp;
                member_lines(cw, scalar(json(k)) + ": ", v[k], std::string(), tmp, ind + 4, i + 1 == keys.size());
                out.insert(out.end(), tmp.begin(), tmp.end());
                continue;
            }
            member_lines(cw, scalar(json(k)) + ": ", v[k], p, out, ind + 4, i + 1 == keys.size());
        }
        out.push_back({ pad + "}", std::string() });
    } else if (v.is_array()) {
        out.push_back({ pad + "[", std::string() });
        for (size_t i = 0; i < v.size(); i++) {
            if (!w.ci && !w.bus && path == "cards" && v[i].is_object()) {
                // a card: comments and key order from its catalog entry
                Writer cw;
                cw.ci = card_info(v[i].value("type", std::string()));
                member_lines(cw, "", v[i], std::string(), out, ind + 4, i + 1 == v.size());
                continue;
            }
            member_lines(w, "", v[i], path + "." + std::to_string(i), out, ind + 4, i + 1 == v.size());
        }
        out.push_back({ pad + "]", std::string() });
    } else {
        out.push_back({ pad + scalar(v), std::string() });
    }
}

} // namespace

std::string machine_cfg_to_jsonc(const json& cfg)
{
    std::vector<Line> lines;
    Writer w;
    value_lines(w, cfg, std::string(), lines, 0);
    std::string out =
        "// SAPI-1 machine description, written by the SAPIemu configurator.\n"
        "// Paths are relative to the emulator data folder (roms/, media/, work/).\n";
    for (const Line& l : lines) {
        std::string t = l.text;
        if (!l.comment.empty()) {
            size_t col = t.size() + 2 > 44 ? t.size() + 2 : 44;
            t.resize(col, ' ');
            t += "// " + l.comment;
        }
        out += t + "\n";
    }
    return out;
}
