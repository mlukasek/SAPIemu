#include "cards/pmd32link.h"
#include "core/card.h"
#include "core/log.h"
#include "core/util.h"

using nlohmann::json;

bool pmd32_configure(Pmd32Sd& pmd, const json& p, bool probe, uint64_t now, const std::string& who, std::string& err)
{
    std::string sd, path;
    bool work;
    long long startup;
    if (!cfg_string(p, "sd_card", sd, "media/pmd32sd", err)) return false;
    if (!cfg_bool(p, "working_copy", work, true, err)) return false;
    if (!cfg_bool(p, "card_locked", pmd.card_locked, false, err)) return false;
    if (!cfg_number(p, "startup_ms", startup, 0, err)) return false;
    pmd.startup_ms = (uint32_t)startup;
    if (probe) return true;   // only the decoding is wanted: no SD card copy
    if (!media_path(sd, work, path, err)) return false;
    if (!pmd.init(path, err)) return false;
    if (p.contains("drives")) {
        const json& drv = p["drives"];
        if (!drv.is_array() || drv.size() > Pmd32Sd::DRIVES) {
            err = "pmd32sd.drives must be an array of up to 4 image names (A-D)";
            return false;
        }
        for (size_t i = 0; i < drv.size(); i++) {
            if (drv[i].is_null()) continue;
            bool wp = false;
            std::string fn;
            if (drv[i].is_string()) {
                fn = drv[i].get<std::string>();
            } else if (drv[i].is_object()) {
                fn = drv[i].value("image", std::string());
                wp = drv[i].value("wp", false);
            }
            if (fn.empty()) continue;
            int rv = pmd.mount((int)i, fn, wp);
            if (rv != 0) {
                err = "PMD32-SD drive " + std::string(1, (char)('A' + i)) + ": cannot insert '" + fn + "' (error " +
                      std::to_string(rv) + ")";
                return false;
            }
        }
    }
    pmd.power_on(now);
    LOGI("%s: PMD32-SD card %s", who.c_str(), path.c_str());
    return true;
}

void pmd32_describe(Pmd32Sd& pmd, json& out)
{
    out["sd_card"] = pmd.root();
    json drives = json::array();
    for (int d = 0; d < Pmd32Sd::DRIVES; d++) {
        if (pmd.drive_present(d))
            drives.push_back(json{ { "drive", std::string(1, (char)('A' + d)) }, { "image", pmd.drive_path(d) },
                                   { "wp", pmd.drive_wp(d) }, { "tracks", pmd.drive_tracks(d) },
                                   { "sectors", pmd.drive_sectors(d) } });
        else
            drives.push_back(json{ { "drive", std::string(1, (char)('A' + d)) }, { "image", nullptr } });
    }
    out["pmd32sd"] = drives;
}
