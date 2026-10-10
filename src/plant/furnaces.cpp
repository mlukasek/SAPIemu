#include "plant/furnaces.h"
#include "plant/thermocouple.h"
#include "plant/ims2_bench.h"
#include "core/bus.h"
#include "core/card.h"
#include "core/state.h"

#include <math.h>
#include <stdio.h>

using nlohmann::json;

static const double MAX_STEP = 0.25;    // s: integration step (the heater has tens of seconds)
static const double MIN_ADVANCE = 0.05; // s: advance() integrates no finer (it comes every 64 cycles)

// Eight furnaces of a small heat treatment shop, different enough to see the adaptive regulator
// at work: name, thermocouple, power, steady temperature at full power, time constant (minutes)
struct FurnaceDefault {
    const char* name;
    int tc;
    double power_w, t_max, tau_min, heater_tau_s;
};
static const FurnaceDefault k_defaults[8] = {
    { "Pec 1", TC_CHA, 2000, 1100, 8, 30 },
    { "Pec 2", TC_CHA, 3000, 1200, 12, 40 },
    { "Pec 3", TC_FEKO, 1500, 650, 6, 25 },
    { "Pec 4", TC_S, 4000, 1450, 20, 60 },
    { "Pec 5", TC_CHA, 2500, 1050, 10, 35 },
    { "Pec 6", TC_CHK, 1200, 600, 5, 20 },
    { "Pec 7", TC_CHA, 5000, 1150, 25, 60 },
    { "Pec 8", TC_B, 6000, 1700, 30, 90 },
};

static bool get_num(const json& j, const char* key, double& out, double def, double lo, double hi, std::string& err)
{
    out = def;
    if (!j.contains(key)) return true;
    if (!j[key].is_number()) { err = std::string(key) + " must be a number"; return false; }
    out = j[key].get<double>();
    if (out < lo || out > hi) {
        char b[96];
        snprintf(b, sizeof(b), "%s must be %g-%g", key, lo, hi);
        err = b;
        return false;
    }
    return true;
}

bool FurnacePlant::configure(const json& cfg, std::string& err)
{
    if (!get_num(cfg, "ambient_c", ambient, 20, -20, 50, err)) return false;
    if (!get_num(cfg, "block_c", block_c, 25, 0, 50, err)) return false;
    if (!get_num(cfg, "battery_v", battery, 4.5, 0, 6, err)) return false;
    furnaces.clear();
    int n = 8;
    const json* list = nullptr;
    if (cfg.contains("furnaces")) {
        if (!cfg["furnaces"].is_array() || cfg["furnaces"].size() > 8) { err = "furnaces must be a list of at most 8"; return false; }
        list = &cfg["furnaces"];
        n = (int)list->size();
    }
    for (int i = 0; i < n; i++) {
        const FurnaceDefault& d = k_defaults[i];
        static const json empty = json::object();
        const json& f = list ? (*list)[i] : empty;
        if (!f.is_object()) { err = "furnaces: each item must be an object"; return false; }
        Furnace u;
        u.name = f.value("name", std::string(d.name));
        if (!cfg_bool(f, "connected", u.connected, true, err)) return false;
        std::string tc = f.value("thermocouple", std::string(tc_name(d.tc)));
        u.tc = tc_parse(tc);
        if (u.tc < 0) { err = "thermocouple must be Fe-Ko, ch-a, PtRh30-PtRh6, PtRh10-Pt or ch-k"; return false; }
        double tau_min, start;
        if (!get_num(f, "power_w", u.power_w, d.power_w, 1, 100000, err)) return false;
        if (!get_num(f, "t_max_c", u.t_max, d.t_max, ambient + 50, 2000, err)) return false;
        if (!get_num(f, "tau_min", tau_min, d.tau_min, 0.5, 1000, err)) return false;
        if (!get_num(f, "heater_tau_s", u.heater_tau_s, d.heater_tau_s, 1, 3600, err)) return false;
        if (!get_num(f, "tc_tau_s", u.tc_tau_s, 8, 0.5, 600, err)) return false;
        if (!get_num(f, "start_c", start, ambient, -20, 2000, err)) return false;
        u.tau_s = tau_min * 60;
        if (!cfg_bool(f, "li_a", u.li[0], true, err) || !cfg_bool(f, "li_b", u.li[1], true, err)) return false;
        u.th = u.tch = u.tm = start;
        furnaces.push_back(u);
    }
    for (int i = 0; i < 8; i++) {
        hist[i].assign(HIST_LEN, 0.0f);
        duty[i].assign(HIST_LEN, 0.0f);
    }
    return true;
}

void FurnacePlant::reset_process(void)
{
    for (Furnace& f : furnaces) f.th = f.tch = f.tm = ambient;
    hist_pos = hist_count = 0;
    slot_t = 0;
}

void FurnacePlant::integrate(double dt)
{
    while (dt > 0) {
        double h = dt > MAX_STEP ? MAX_STEP : dt;
        dt -= h;
        for (size_t i = 0; i < furnaces.size(); i++) {
            Furnace& f = furnaces[i];
            double g = f.power_w / (f.t_max - ambient);    // W/K to the surroundings
            double cc = g * f.tau_s;                         // J/K of the chamber
            double gh = 4 * g;                               // heater to chamber
            double ch = gh * f.heater_tau_s;
            bool on = ((heaters >> i) & 1) && !f.heater_broken;
            double p = on ? f.power_w : 0;
            double q = gh * (f.th - f.tch);
            f.th += h * (p - q) / ch;
            f.tch += h * (q - g * (f.tch - ambient)) / cc;
            f.tm += h * (f.tch - f.tm) / f.tc_tau_s;
            if (on) f.on_time += h;
        }
        t_sec += h;
        slot_t += h;
        if (slot_t >= HIST_STEP) {   // a history sample
            for (size_t i = 0; i < furnaces.size() && i < 8; i++) {
                hist[i][hist_pos] = (float)furnaces[i].tch;
                duty[i][hist_pos] = (float)(furnaces[i].on_time / slot_t);
                furnaces[i].on_time = 0;
            }
            hist_pos = (hist_pos + 1) % HIST_LEN;
            if (hist_count < HIST_LEN) hist_count++;
            slot_t = 0;
        }
    }
}

void FurnacePlant::advance(uint64_t now)
{
    if (!bus) return;
    if (!started || now < last) {
        started = true;
        last = now;
        return;
    }
    double dt = (double)(now - last) / bus->cpu_hz;
    if (dt < MIN_ADVANCE) return;
    integrate(dt);
    last = now;
}

void FurnacePlant::relays(uint8_t mask, uint64_t now)
{
    if (bus && started && now > last) {   // up to the switch with the old contacts
        integrate((double)(now - last) / bus->cpu_hz);
        last = now;
    }
    heaters = mask;
}

double FurnacePlant::tc_input_mv(int ch)
{
    if (ch < 0 || ch >= (int)furnaces.size()) return NAN;
    const Furnace& f = furnaces[ch];
    if (!f.connected || f.tc_broken) return NAN;
    // the compensating leads end on the terminal block: E(T_m) - E(T_block)
    return tc_emf_mv(f.tc, f.tm) - tc_emf_mv(f.tc, block_c);
}

uint8_t FurnacePlant::logic_in(int group)
{
    uint8_t v = 0xFF;
    for (size_t i = 0; i < furnaces.size(); i++)
        if (!furnaces[i].li[group & 1]) v &= (uint8_t)~(1 << i);
    return v;
}

void FurnacePlant::describe(json& out)
{
    out["type"] = type_name();
    out["ambient_c"] = ambient;
    out["block_c"] = block_c;
    out["battery_v"] = battery;
    json list = json::array();
    for (size_t i = 0; i < furnaces.size(); i++) {
        const Furnace& f = furnaces[i];
        json j;
        j["channel"] = (int)i + 1;
        j["name"] = f.name;
        j["thermocouple"] = f.connected ? (f.tc_broken ? "broken" : tc_name(f.tc)) : "none";
        j["temperature_c"] = round(f.tch * 10) / 10;
        j["heater_c"] = round(f.th * 10) / 10;
        j["heater_on"] = ((heaters >> i) & 1) != 0;
        if (f.heater_broken) j["heater_broken"] = true;
        j["li"] = std::string(f.li[0] ? "H" : "L") + (f.li[1] ? "H" : "L");
        j["lo"] = std::string((lo[0] >> i) & 1 ? "H" : "L") + ((lo[1] >> i) & 1 ? "H" : "L");
        list.push_back(j);
    }
    out["furnaces"] = list;
}

void FurnacePlant::state(StateIO& io)
{
    for (Furnace& f : furnaces) {
        io.v(f.th);
        io.v(f.tch);
        io.v(f.tm);
        io.v(f.on_time);
        io.v(f.tc_broken);
        io.v(f.heater_broken);
        io.v(f.li[0]);
        io.v(f.li[1]);
    }
    io.v(heaters);
    io.v(lo[0]);
    io.v(lo[1]);
    io.v(last);
    io.v(t_sec);
    io.v(slot_t);
    io.v(started);
    io.v(battery);
    // the plot: the samples there are, oldest first
    int32_t n = hist_count;
    io.v(n);
    if (io.loading && (n < 0 || n > HIST_LEN)) { io.fail("plant: bad history"); return; }
    for (int k = 0; k < n; k++) {
        int idx = io.loading ? k : (hist_pos - n + k + HIST_LEN) % HIST_LEN;
        for (int i = 0; i < 8; i++) {
            io.v(hist[i][(size_t)idx]);
            io.v(duty[i][(size_t)idx]);
        }
    }
    if (io.loading) {
        hist_count = n;
        hist_pos = n % HIST_LEN;
    }
}

Plant* plant_create(const json& cfg, std::string& err)
{
    std::string type = cfg.value("type", std::string("APS-5"));
    if (type == "IMS-2") {
        Ims2Bench* b = new Ims2Bench();
        if (!b->configure(cfg, err)) {
            delete b;
            return nullptr;
        }
        return b;
    }
    if (type != "APS-5") { err = "unknown plant type '" + type + "' (APS-5, IMS-2)"; return nullptr; }
    FurnacePlant* p = new FurnacePlant();
    if (!p->configure(cfg, err)) {
        delete p;
        return nullptr;
    }
    return p;
}
