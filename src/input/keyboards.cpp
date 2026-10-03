#include "input/keyboards.h"
#include "input/ank1.h"
#include "input/consul.h"
#include "input/ekl1.h"

using nlohmann::json;

static uint64_t us_to_cycles(long long us, uint32_t cpu_hz)
{
    return (uint64_t)((double)us * cpu_hz / 1e6);
}

KeyboardDevice* keyboard_from_config(const json& cfg, uint32_t cpu_hz, std::string& err)
{
    std::string type = cfg.value("type", std::string("EKL-1"));
    long long gap_us;
    if (type == "EKL-1" || type == "Consul 259.11" || type == "Consul 262.3") {
        ParallelKeyboard* k;
        if (type == "EKL-1") {
            k = new Ekl1();
        } else if (type == "Consul 259.11") {
            Consul25911* c = new Consul25911();
            std::string code = cfg.value("code", std::string("koi7"));
            if (code != "koi7" && code != "koi8cs") {
                err = "Consul 259.11 code must be koi7 or koi8cs";
                delete c;
                return nullptr;
            }
            c->set_koi8(code == "koi8cs");
            k = c;
        } else {
            Consul2623* c = new Consul2623();
            c->latched = cfg.value("latch_mod", true);   // 7474 modification on JPR-1Z
            k = c;
        }
        // the unlatched 1 ms strobe needs a longer pause between characters
        if (!cfg_number(cfg, "char_gap_us", gap_us, k->latched ? 1000 : 20000, err) ||
            !cfg_bool(cfg, "strict", k->strict, false, err)) {
            delete k;
            return nullptr;
        }
        k->gap_cycles = us_to_cycles(gap_us, cpu_hz);
        k->pulse_cycles = us_to_cycles(1000, cpu_hz);
        k->hold_cycles = us_to_cycles(100000, cpu_hz);
        return k;
    }
    if (type == "ANK-1") {
        Ank1* a = new Ank1();
        long long press_ms;
        if (!cfg_number(cfg, "press_ms", press_ms, 40, err)) { delete a; return nullptr; }
        std::string sw = cfg.value("software", std::string("mikrobasic"));
        if (sw != "mikrobasic" && sw != "mikos") {
            err = "ANK-1 software must be mikrobasic or mikos";
            delete a;
            return nullptr;
        }
        a->software = sw == "mikos" ? Ank1::SW_MIKOS : Ank1::SW_MIKROBASIC;
        a->press_cycles = a->release_cycles = us_to_cycles(press_ms * 1000, cpu_hz);
        a->hold_cycles = us_to_cycles(100000, cpu_hz);
        return a;
    }
    err = "unknown keyboard type '" + type + "' (EKL-1, Consul 262.3, Consul 259.11, ANK-1)";
    return nullptr;
}

const std::vector<KeyboardPreset>& keyboard_presets(void)
{
    static const std::vector<KeyboardPreset> presets = {
        { "EKL-1", R"({"type":"EKL-1"})" },
        { "Consul 262.3", R"({"type":"Consul 262.3","latch_mod":true})" },
        { "Consul 262.3 (bez 7474)", R"({"type":"Consul 262.3","latch_mod":false})" },
        { "Consul 259.11", R"({"type":"Consul 259.11","code":"koi7"})" },
        { "Consul 259.11 (KOI-8 cs)", R"({"type":"Consul 259.11","code":"koi8cs"})" },
        { "ANK-1", R"({"type":"ANK-1"})" },
        { "ANK-1 (MIKOS)", R"({"type":"ANK-1","software":"mikos"})" },
    };
    return presets;
}

KeyboardDevice* keyboard_from_preset(const std::string& name, uint32_t cpu_hz, std::string& err)
{
    for (const KeyboardPreset& p : keyboard_presets())
        if (name == p.name) return keyboard_from_config(json::parse(p.config), cpu_hz, err);
    err = "unknown keyboard '" + name + "'";
    return nullptr;
}
