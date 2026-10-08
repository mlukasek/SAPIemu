#include "input/keyboards.h"
#include "input/ank1.h"
#include "input/consul.h"
#include "input/ekl1.h"
#include "cards/hrk1.h"

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
        k->set_beeper(cfg.value("beeper", true));
        k->gap_cycles = us_to_cycles(gap_us, cpu_hz);
        k->pulse_cycles = us_to_cycles(1000, cpu_hz);
        k->hold_cycles = us_to_cycles(100000, cpu_hz);
        return k;
    }
    // ANK-1M5 (2021) is the predecessor of ANK-1M2 (2024): taken as ANK-1M2 (author)
    if (type == "ANK-1" || type == "ANK-1M2" || type == "ANK-1M5") {
        Ank1* a = new Ank1();
        a->m2 = type != "ANK-1";
        if (a->m2 && !joy_device_parse(cfg.value("joyport", json("joystick")), a->joy_dev, err, "joyport")) {
            delete a;
            return nullptr;
        }
        a->mouse_period = cpu_hz / QuadMouse::RATE;
        a->set_beeper(cfg.value("beeper", true));
        long long press_ms;
        if (!cfg_number(cfg, "press_ms", press_ms, 40, err)) { delete a; return nullptr; }
        std::string sw = cfg.value("software", std::string("mikrobasic"));
        if (sw != "mikrobasic" && sw != "mikos") {
            err = type + " software must be mikrobasic or mikos";
            delete a;
            return nullptr;
        }
        a->software = sw == "mikos" ? Ank1::SW_MIKOS : Ank1::SW_MIKROBASIC;
        a->press_cycles = a->release_cycles = us_to_cycles(press_ms * 1000, cpu_hz);
        a->min_hold = us_to_cycles(20000, cpu_hz);   // a held key: at least 20 ms
        a->hold_cycles = us_to_cycles(100000, cpu_hz);
        return a;
    }
    if (type == "HRK-1") return new Hrk1(cpu_hz);   // the front panel of APS-5 with its cables on X2 and X3
    err = "unknown keyboard type '" + type + "' (EKL-1, Consul 262.3, Consul 259.11, ANK-1, ANK-1M2, HRK-1)";
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
        { "ANK-1M2", R"({"type":"ANK-1M2"})" },   // joyport: joystick unless set
        { "HRK-1 (APS-5)", R"({"type":"HRK-1"})" },
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
