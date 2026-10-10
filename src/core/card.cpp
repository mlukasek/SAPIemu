#include "core/card.h"
#include "core/util.h"

#include <ctype.h>
#include "chips/floppy.h"

using nlohmann::json;

const char* Card::media_param(int u)
{
    static const char* const drives[4] = { "drive_a.image", "drive_b.image", "drive_c.image", "drive_d.image" };
    return u >= 0 && u < 4 && floppy_drive(u) ? drives[u] : nullptr;
}

bool Card::media_change(int u, const std::string& image, std::string& err)
{
    FloppyDrive* d = floppy_drive(u);
    if (!d) {
        err = "no such drive";
        return false;
    }
    if (image.empty()) {
        floppy_eject(u);
        return true;
    }
    return floppy_insert(u, image, d->write_protect, err);   // the tab of the disk stays as it was
}

void Card::media_discard(int u)
{
    if (FloppyDrive* d = floppy_drive(u)) d->dirty = false;
}

bool cfg_number(const json& cfg, const char* key, long long& out, long long def, std::string& err)
{
    out = def;
    if (!cfg.contains(key)) return true;
    const json& v = cfg[key];
    if (v.is_number_integer()) { out = v.get<long long>(); return true; }
    if (v.is_number()) { out = (long long)v.get<double>(); return true; }
    if (v.is_string() && parse_number(v.get<std::string>(), out)) return true;
    err = std::string("invalid number for '") + key + "'";
    return false;
}

bool cfg_bool(const json& cfg, const char* key, bool& out, bool def, std::string& err)
{
    out = def;
    if (!cfg.contains(key)) return true;
    const json& v = cfg[key];
    if (v.is_boolean()) { out = v.get<bool>(); return true; }
    if (v.is_number_integer()) { out = v.get<long long>() != 0; return true; }
    err = std::string("invalid boolean for '") + key + "'";
    return false;
}

bool cfg_letter(const json& cfg, char& out, std::string& err)
{
    out = 0;
    if (!cfg.contains("cpm") || cfg["cpm"].is_null()) return true;
    std::string s = cfg["cpm"].is_string() ? cfg["cpm"].get<std::string>() : std::string();
    if (s.empty()) return true;
    char c = (char)toupper((unsigned char)s[0]);
    if (s.size() > 2 || c < 'A' || c > 'P' || (s.size() == 2 && s[1] != ':')) {
        err = "cpm must be a drive letter A-P";
        return false;
    }
    out = c;
    return true;
}

bool cfg_string(const json& cfg, const char* key, std::string& out, const char* def, std::string& err)
{
    out = def ? def : "";
    if (!cfg.contains(key)) return true;
    const json& v = cfg[key];
    if (v.is_string()) { out = v.get<std::string>(); return true; }
    err = std::string("invalid string for '") + key + "'";
    return false;
}
