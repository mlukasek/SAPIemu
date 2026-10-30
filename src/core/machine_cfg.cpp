#include "core/machine_cfg.h"
#include "core/util.h"
#include "input/keyboards.h"

#include <ctype.h>
#include <string.h>
#include <algorithm>

using nlohmann::json;

// A string of an object: def when the object or the key is missing or the value is not a string (a machine file
// edited by hand may hold anything; value() would throw and end the program).
static std::string jstr(const json& j, const char* key, const std::string& def = std::string())
{
    if (!j.is_object()) return def;
    json::const_iterator it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : def;
}

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
    if (cfg.contains("bus") && cfg["bus"].is_object()) type = jstr(cfg["bus"], "type", type);
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
    const CardInfo* ci = card_info(jstr(card, "type"));
    return ci ? ci->width : 1;
}

// "#10" from the port parameter `path` of a card.
static std::string port_of(const json& card, const CardInfo* ci, const char* path)
{
    for (const CardParam* p = ci->params; p && p->path; p++) {
        if (strcmp(p->path, path) != 0) continue;
        long long v;
        if (!as_number(card_param_value(card, p), v)) return std::string();
        char buf[8];
        snprintf(buf, sizeof(buf), "#%02llX", v & 0xFF);
        return buf;
    }
    return std::string();
}

std::string card_port_label(const json& card)
{
    const CardInfo* ci = card_info(jstr(card, "type"));
    const char* pp = card_port_param(ci);
    if (!pp) return std::string();
    if (!strcmp(ci->type, "DMR-1")) {   // UART switched off: no port
        const json* u = cfg_find(card, "uart");
        if (u && u->is_boolean() && !u->get<bool>()) return std::string();
    }
    return port_of(card, ci, pp);
}

std::vector<std::string> card_connector_ports(const json& card)
{
    std::vector<std::string> out;
    const CardInfo* ci = card_info(jstr(card, "type"));
    if (!ci) return out;
    if (!strcmp(ci->type, "DSM-1V")) {
        for (const char* ch : { "a", "b" }) {
            const json* sec = cfg_find(card, ch);
            const json* en = cfg_find(card, (std::string(ch) + ".enabled").c_str());
            bool on = sec && sec->is_object() && !(en && en->is_boolean() && !en->get<bool>());
            out.push_back(on ? port_of(card, ci, (std::string(ch) + ".base").c_str()) : std::string());
        }
    } else if (card_port_param(ci)) {
        out.push_back(card_port_label(card));
    }
    return out;
}

std::string card_display_name(const json& card)
{
    std::string n = jstr(card, "type", "?");
    std::string port = card_port_label(card);
    return port.empty() ? n : n + " " + port;
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

// ---------------------------------------------------------------- parts of a machine

static std::string base_name(const std::string& path)
{
    size_t s = path.find_last_of("/\\");
    return s == std::string::npos ? path : path.substr(s + 1);
}

// a label up to its details in parentheses, with a capital letter ("myš Atari ST" -> "Myš Atari ST")
static std::string short_label(const char* label)
{
    std::string s = label;
    size_t p = s.find(" (");
    if (p != std::string::npos) s = s.substr(0, p);
    if (!s.empty()) s[0] = (char)toupper((unsigned char)s[0]);
    return s;
}

// the first file (disk image, cartridge, cassette) set in a section
static std::string section_file(const json& card, const CardInfo* ci, const CardParam* sec)
{
    for (const CardParam* p = ci->params; p->path; p++) {
        if (p->kind != PK_FILE || !has_prefix(p->path, sec->path)) continue;
        json v = card_param_value(card, p);
        if (v.is_string() && !v.get<std::string>().empty()) return base_name(v.get<std::string>());
    }
    return std::string();
}

// the sections of cards that are devices, with short names; drives and units ("drive_a", "unit0") take the label
// of the catalog, other sections (channels of serial cards, the PC side) are not devices
static const struct {
    const char* path;
    const char* en;
    const char* cs;
} k_device_sections[] = {
    { "printer", "Printer", "Tiskárna" },
    { "xy4130", "Plotter XY 4130", "Zapisovač XY 4130" },
    { "minigraf", "Plotter ARITMA Minigraf 0507", "Zapisovač ARITMA Minigraf 0507" },
    { "kzd1", "Cassette data recorder KZD-1", "Kazetopáskový záznamník dat KZD-1" },
    { "pmd32sd", "Floppy emulator PMD32-SD", "Emulátor disket PMD32-SD" },
    { "ide", "IDE disk", "IDE disk" },
};

static std::vector<std::string> card_devices(const json& card, const CardInfo* ci, bool czech)
{
    std::vector<std::string> out;
    if (!ci->params) return out;
    for (const CardParam* p = ci->params; p->path; p++) {
        if (strchr(p->path, '.') && strcmp(p->path, "keyboard.joyport") != 0) continue;
        json v = card_param_value(card, p);
        if (p->kind == PK_KEYBOARD) {
            std::string type = v.is_object() ? jstr(v, "type") : v.is_string() ? v.get<std::string>() : "";
            if (type.empty()) continue;
            std::string name = (czech ? "Klávesnice " : "Keyboard ") + type;
            for (const KeyboardPreset& k : keyboard_presets())   // another device on the keyboard connector
                if (!k.keyboard && jstr(json::parse(k.config, nullptr, false), "type") == type) name = k.name;
            out.push_back(name);
        } else if (!strcmp(p->path, "keyboard.joyport")) {   // the joystick connector of ANK-1M2
            const json* kb = cfg_find(card, "keyboard");
            std::string type = kb ? jstr(*kb, "type") : "";
            int o = param_option_index(p, v);
            if (type.rfind("ANK-1M", 0) == 0 && o > 0) out.push_back(short_label(czech ? p->options[o].cs : p->options[o].en));
        } else if (p->kind == PK_SECTION && v == json(true)) {
            std::string name;
            for (const auto& d : k_device_sections)
                if (!strcmp(d.path, p->path)) name = czech ? d.cs : d.en;
            if (!strncmp(p->path, "drive_", 6) || !strncmp(p->path, "unit", 4)) name = czech ? p->cs : p->en;
            if (name.empty()) continue;
            std::string f = strcmp(p->path, "printer") ? section_file(card, ci, p) : "";   // not the output on the PC
            out.push_back(f.empty() ? name : name + ": " + f);
        } else if (p->kind == PK_FILE && !strcmp(p->path, "tape")) {   // the cassette recorder of DSM-1
            std::string f = v.is_string() ? base_name(v.get<std::string>()) : "";
            std::string name = czech ? "Magnetofon" : "Tape recorder";
            out.push_back(f.empty() ? name : name + ": " + f);
        } else if (p->kind == PK_FILE && !strcmp(p->path, "eprom")) {   // the socket of the DSE-1 programmer (cable X2)
            std::string f = v.is_string() ? base_name(v.get<std::string>()) : "";
            std::string name = czech ? "Patice programátoru EPROM" : "EPROM programmer socket";
            out.push_back(f.empty() ? name : name + ": " + f);
        } else if (p->kind == PK_CHOICE && (!strcmp(p->path, "k2") || !strcmp(p->path, "k3"))) {   // connectors
            int o = param_option_index(p, v);
            if (o >= 0 && v != json("none")) out.push_back(short_label(czech ? p->options[o].cs : p->options[o].en));
        }
    }
    return out;
}

std::vector<MachinePart> machine_cfg_parts(const json& cfg, bool czech)
{
    std::vector<MachinePart> out;
    if (cfg.contains("cards") && cfg["cards"].is_array()) {
        for (const json& card : cfg["cards"]) {
            if (!card.is_object()) continue;
            MachinePart m;
            m.slot = card_slot(card);
            m.type = card_display_name(card);
            if (const CardInfo* ci = card_info(jstr(card, "type"))) {
                m.title = czech ? ci->cs : ci->en;
                m.devices = card_devices(card, ci, czech);
            }
            out.push_back(m);
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const MachinePart& a, const MachinePart& b) { return a.slot < b.slot; });
    if (cfg.contains("plant") && cfg["plant"].is_object()) {   // the technology outside the computer
        MachinePart m;
        m.slot = 0;
        m.type = jstr(cfg["plant"], "type");
        if (m.type == "APS-5") m.title = czech ? "Osm pecí s termočlánky" : "Eight furnaces with thermocouples";
        if (m.type == "IMS-2")
            m.title = czech ? "Měřicí pracoviště: voltmetr M1T 330, zdroj BS 525" : "Measuring bench: voltmeter M1T 330, power supply BS 525";
        out.push_back(m);
    }
    return out;
}

// ---------------------------------------------------------------- changes between two descriptions

// a text of at most about n bytes, cut on a character boundary (UTF-8)
static std::string text_cut(std::string s, size_t n)
{
    if (s.size() <= n) return s;
    size_t k = n;
    while (k > 0 && ((unsigned char)s[k] & 0xC0) == 0x80) k--;
    s.resize(k);
    return s + "\xE2\x80\xA6";
}

// a label without its details in parentheses ("RDY wait states (on VRAM access)" -> "RDY wait states", "Wait (RDY) on
// VRAM access" -> "Wait on VRAM access")
static std::string label_short(const char* label)
{
    std::string s;
    int depth = 0;
    for (const char* c = label ? label : ""; *c; c++) {
        if (*c == '(') {
            depth++;
            while (!s.empty() && s.back() == ' ') s.pop_back();
        } else if (*c == ')' && depth > 0) {
            depth--;
        } else if (depth == 0) {
            s += *c;
        }
    }
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return text_cut(s, 64);
}

static std::string json_text(const json& v)
{
    if (v.is_null()) return "-";
    if (v.is_string()) return v.get<std::string>();
    return v.dump(-1, ' ', false, json::error_handler_t::replace);
}

static const char* on_off(bool on, bool czech)
{
    return on ? (czech ? "zapnuto" : "on") : (czech ? "vypnuto" : "off");
}

// the value of a parameter as the configurator shows it: the option, the file name, on / off
static std::string param_text(const CardParam* p, const json& v, bool czech)
{
    switch (p->kind) {
    case PK_BOOL:
    case PK_SECTION:
        return on_off(v.is_boolean() ? v.get<bool>() : !v.is_null(), czech);
    case PK_CHOICE: {
        int o = param_option_index(p, v);
        if (o >= 0) return text_cut(label_short(czech ? p->options[o].cs : p->options[o].en), 48);
        break;
    }
    case PK_FILE:
    case PK_DIR: {
        std::string f = v.is_string() ? v.get<std::string>() : "";
        return f.empty() ? (czech ? "(nic)" : "(none)") : text_cut(base_name(f), 48);
    }
    case PK_KEYBOARD: {
        std::string type = v.is_object() ? jstr(v, "type") : v.is_string() ? v.get<std::string>() : "";
        return type.empty() ? (czech ? "(žádná)" : "(none)") : type;
    }
    default:
        break;
    }
    return text_cut(json_text(v), 48);
}

// the name of a parameter, after its section ("Mechanika 0, obraz diskety")
static std::string param_name(const CardInfo* ci, const CardParam* p, bool czech)
{
    std::string l = label_short(czech ? p->cs : p->en);
    const CardParam* s = param_section(ci, p);
    if (!s) return l;
    if (l.size() > 1 && isupper((unsigned char)l[0]) && islower((unsigned char)l[1])) l[0] = (char)tolower((unsigned char)l[0]);
    return param_name(ci, s, czech) + ", " + l;
}

struct CfgCardRef {
    const json* card;
    int slot;
    std::string type;
    int peer;   // the index of the same card in the other description, -1 = none
};

static std::vector<CfgCardRef> cfg_card_refs(const json& cfg)
{
    std::vector<CfgCardRef> out;
    if (cfg.contains("cards") && cfg["cards"].is_array())
        for (const json& c : cfg["cards"])
            if (c.is_object()) out.push_back({ &c, card_slot(c), jstr(c, "type"), -1 });
    return out;
}

// keys of a card that no parameter of the catalog covers, different in the two entries
static bool other_keys_differ(const json& a, const json& b, const CardInfo* ci)
{
    for (const json* j : { &a, &b })
        for (auto it = j->begin(); it != j->end(); ++it) {
            const std::string& k = it.key();
            if (k == "slot" || k == "type") continue;
            bool known = false;
            for (const CardParam* p = ci ? ci->params : nullptr; p && p->path && !known; p++)
                known = split_path(p->path)[0] == k;
            if (known) continue;
            json va = a.contains(k) ? a[k] : json(), vb = b.contains(k) ? b[k] : json();
            if (va != vb) return true;
        }
    return false;
}

// the same settings, the slot aside: every parameter of the catalog equal (the configurator writes a value also when
// it is the default) and the other keys too
static bool card_same_settings(const json& a, const json& b, const CardInfo* ci)
{
    for (const CardParam* p = ci ? ci->params : nullptr; p && p->path; p++)
        if (!cfg_values_equal(card_param_value(a, p), card_param_value(b, p))) return false;
    return !other_keys_differ(a, b, ci);
}

std::vector<std::string> machine_cfg_changes(const json& before, const json& after, bool czech)
{
    std::vector<std::pair<int, std::string>> out;   // slot (0 = the machine) and the line
    if (!before.is_object() || !after.is_object()) return std::vector<std::string>();
    auto get = [](const json& j, const std::string& k) { return j.contains(k) ? j[k] : json(); };

    // the machine and the bus: the known keys in the order of the configurator, then any other
    std::vector<std::string> keys = { "name", "description", "bus", "power_on_memory", "windows" };
    for (const json* j : { &before, &after })
        for (auto it = j->begin(); it != j->end(); ++it)
            if (it.key() != "cards" && std::find(keys.begin(), keys.end(), it.key()) == keys.end()) keys.push_back(it.key());
    for (const std::string& k : keys) {
        json va = get(before, k), vb = get(after, k);
        if (va == vb) continue;
        std::string t;
        if (k == "name") {
            t = std::string(czech ? "Název" : "Name") + " \xE2\x86\x92 " + text_cut(json_text(vb), 48);
        } else if (k == "description") {
            t = czech ? "Popis změněn" : "Description changed";
        } else if (k == "power_on_memory") {
            t = std::string(czech ? "Náhodný obsah pamětí po zapnutí" : "Random memory content after power on") + " \xE2\x86\x92 " +
                on_off(vb == json("random"), czech);
        } else if (k == "windows") {
            t = czech ? "Okna nahoře po spuštění změněna" : "Windows on top at the start changed";
        } else if (k == "bus") {
            json ba = va.is_object() ? va : json::object(), bb = vb.is_object() ? vb : json::object();
            std::vector<std::string> bk;
            for (const json* j : { &ba, &bb })
                for (auto it = j->begin(); it != j->end(); ++it)
                    if (std::find(bk.begin(), bk.end(), it.key()) == bk.end()) bk.push_back(it.key());
            for (const std::string& b : bk) {
                json x = get(ba, b), y = get(bb, b);
                if (x == y) continue;
                std::string v = json_text(y);
                if (b == "type")
                    if (const BusInfo* bi = bus_info(v)) v = czech ? bi->cs : bi->en;
                out.push_back({ 0, std::string(czech ? "Sběrnice" : "Bus") + (b == "type" ? std::string() : ", " + b) +
                                       " \xE2\x86\x92 " + text_cut(v, 48) });
            }
            continue;
        } else if (vb.is_null()) {
            t = k + (czech ? " odebráno" : " removed");
        } else if (vb.is_object() || vb.is_array()) {
            t = k + (czech ? " změněno" : " changed");
        } else {
            t = k + " \xE2\x86\x92 " + text_cut(json_text(vb), 48);
        }
        out.push_back({ 0, t });
    }

    // the cards: the same card in the same slot, then moved ones (the same settings first, then any of the type)
    std::vector<CfgCardRef> A = cfg_card_refs(before), B = cfg_card_refs(after);
    for (size_t i = 0; i < A.size(); i++)
        for (size_t j = 0; j < B.size() && A[i].peer < 0; j++)
            if (B[j].peer < 0 && A[i].slot == B[j].slot && A[i].type == B[j].type) {
                A[i].peer = (int)j;
                B[j].peer = (int)i;
            }
    for (int pass = 0; pass < 2; pass++)
        for (size_t i = 0; i < A.size(); i++)
            for (size_t j = 0; j < B.size() && A[i].peer < 0; j++)
                if (B[j].peer < 0 && A[i].type == B[j].type &&
                    (pass == 1 || card_same_settings(*A[i].card, *B[j].card, card_info(A[i].type)))) {
                    A[i].peer = (int)j;
                    B[j].peer = (int)i;
                }
    auto ref = [](const CfgCardRef& c) { return "X" + std::to_string(c.slot) + " " + card_display_name(*c.card); };
    for (const CfgCardRef& a : A)
        if (a.peer < 0) out.push_back({ a.slot, ref(a) + (czech ? ": odebrána" : ": removed") });
    for (const CfgCardRef& b : B) {
        if (b.peer < 0) {
            out.push_back({ b.slot, ref(b) + (czech ? ": přidána" : ": added") });
            continue;
        }
        const CfgCardRef& a = A[(size_t)b.peer];
        if (a.slot != b.slot)
            out.push_back({ b.slot, ref(a) + (czech ? ": přesunuta do X" : ": moved to X") + std::to_string(b.slot) });
        const CardInfo* ci = card_info(b.type);
        for (const CardParam* p = ci ? ci->params : nullptr; p && p->path; p++) {
            if (!card_param_active(*b.card, ci, p)) continue;   // in a section switched off: the section says it
            json va = card_param_value(*a.card, p), vb = card_param_value(*b.card, p);
            if (cfg_values_equal(va, vb)) continue;
            std::string v = param_text(p, vb, czech);
            if (p->kind == PK_KEYBOARD && v == param_text(p, va, czech))   // the same keyboard, its options changed
                v += czech ? ", jiná nastavení" : ", other settings";
            out.push_back({ b.slot, ref(b) + ": " + param_name(ci, p, czech) + " \xE2\x86\x92 " + v });
        }
        if (other_keys_differ(*a.card, *b.card, ci))   // keys the catalog does not know
            out.push_back({ b.slot, ref(b) + (czech ? ": jiná nastavení" : ": other settings") });
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const std::pair<int, std::string>& x, const std::pair<int, std::string>& y) { return x.first < y.first; });
    std::vector<std::string> lines;
    for (const auto& o : out) lines.push_back(o.second);
    return lines;
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
            std::string type = jstr(c, "type");
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
    // battery files shared by two cards (port and memory conflicts: machine_map)
    if (cfg.contains("cards") && cfg["cards"].is_array()) {
        const json& cards = cfg["cards"];
        auto on = [](const json& c, const char* k) {
            const json* v = cfg_find(c, k);
            return !v || !v->is_boolean() || v->get<bool>();
        };
        for (size_t i = 0; i < cards.size(); i++) {
            for (size_t j = i + 1; j < cards.size(); j++) {
                if (jstr(cards[i], "type") != "DMR-1" || jstr(cards[j], "type") != "DMR-1")
                    continue;
                if (!on(cards[i], "ram") || !on(cards[j], "ram") || !on(cards[i], "battery") || !on(cards[j], "battery"))
                    continue;
                std::string fa = jstr(cards[i], "sram_file", "work/dmr-1/sram.bin");
                std::string fb = jstr(cards[j], "sram_file", "work/dmr-1/sram.bin");
                if (fa == fb)
                    out.push_back({ card_slot(cards[j]), false, "two DMR-1 keep their SRAM in the same file " + fa,
                                    "dvě DMR-1 drží SRAM ve stejném souboru " + fa });
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
    if (!w.ci) {
        if (path == "windows")
            return "Windows opened on top at the start: media, minigraf, aps5, diag, printer, terminal, ims2";
        if (path == "main_display")
            return "Main output: the display window with the big place at the top left (else the console screen)";
        if (path == "based_on") return "My machines: the supplied machine this copy was saved from";
        return std::string();
    }
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
        add("name"); add("description"); add("based_on"); add("windows"); add("main_display"); add("bus"); add("cards");
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
                cw.ci = card_info(jstr(v[i], "type"));
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
