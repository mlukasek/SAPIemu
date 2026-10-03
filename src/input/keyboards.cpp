#include "input/keyboards.h"
#include "input/ekl1.h"

using nlohmann::json;

KeyboardDevice* keyboard_create(const std::string& type)
{
    if (type == "EKL-1") return new Ekl1();
    return nullptr;
}

bool keyboard_configure(KeyboardDevice* kbd, const json& cfg, uint32_t cpu_hz, std::string& err)
{
    Ekl1* e = dynamic_cast<Ekl1*>(kbd);
    if (e) {
        long long gap_us;
        if (!cfg_number(cfg, "char_gap_us", gap_us, 1000, err)) return false;
        if (!cfg_bool(cfg, "strict", e->strict, false, err)) return false;
        e->gap_cycles = (uint64_t)((double)gap_us * cpu_hz / 1e6);
    }
    return true;
}
