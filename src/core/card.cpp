#include "core/card.h"
#include "core/util.h"

using nlohmann::json;

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

bool cfg_string(const json& cfg, const char* key, std::string& out, const char* def, std::string& err)
{
    out = def ? def : "";
    if (!cfg.contains(key)) return true;
    const json& v = cfg[key];
    if (v.is_string()) { out = v.get<std::string>(); return true; }
    err = std::string("invalid string for '") + key + "'";
    return false;
}
