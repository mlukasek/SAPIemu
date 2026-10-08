#pragma once
// Plant "APS-5": up to eight electric furnaces with thermocouples on the isothermal terminal block
// of the APS-5 unit (docs/desky/APS-5.md). Not a part of SAPI: the world the unit measures and
// controls, so that its firmware has something to do.
//
// Each furnace: heater power P switched by the contact S(n) of DRV-1A (through a solid state relay,
// NOCONTA), two heat capacities (the heating element and the chamber with the load) and the
// thermocouple in the chamber with its own lag:
//   C_h dT_h/dt = P u - G_h (T_h - T_c)
//   C_c dT_c/dt = G_h (T_h - T_c) - G (T_c - T_a)
//   dT_m/dt = (T_c - T_m) / tau_tc
// G = P / (t_max - T_a) (t_max = the steady temperature at full power), C_c = G tau, G_h = 4 G,
// C_h = G_h tau_h. The thermocouple gives E(T_m) - E(T_block) on its DMT-1 input.
#include <stdint.h>
#include <string>
#include <vector>
#include "core/plant.h"

struct Furnace {
    std::string name;
    bool connected = true;       // a thermocouple on the channel at all
    int tc = 1;                  // TcType of the thermocouple
    double power_w = 2000;
    double t_max = 1100;         // degC at full power, steady
    double tau_s = 600;          // main time constant
    double heater_tau_s = 40;
    double tc_tau_s = 8;
    // faults and inputs (the window)
    bool tc_broken = false;      // the thermocouple loop open
    bool heater_broken = false;  // a blown heater (or fuse)
    bool li[2] = { true, true }; // logic inputs a, b: H (open)
    // state
    double th = 20, tch = 20, tm = 20;
    double on_time = 0;          // seconds with the heater on in the current history slot
};

class FurnacePlant : public Plant {
public:
    const char* type_name() const override { return "APS-5"; }
    bool configure(const nlohmann::json& cfg, std::string& err);

    void advance(uint64_t now) override;
    double tc_input_mv(int ch) override;
    double block_temp_c() override { return block_c; }
    double battery_v() override { return battery; }
    void relays(uint8_t mask, uint64_t now) override;
    uint8_t logic_in(int group) override;
    void logic_out(int group, uint8_t value) override { lo[group & 1] = value; }
    void reset_process(void) override;
    void state(StateIO& io) override;
    void describe(nlohmann::json& out) override;

    // the window
    std::vector<Furnace> furnaces;   // channel n = furnaces[n] (up to 8)
    double ambient = 20;             // degC around the furnaces
    double block_c = 25;             // the terminal block in the cabinet
    double battery = 4.5;            // the backup battery (flat 4.5 V)
    uint8_t heaters = 0;             // DRV-1A contacts now
    uint8_t lo[2] = { 0, 0 };
    // history of the chamber temperatures for the plot: one sample a HIST_STEP seconds
    static const int HIST_STEP = 5;
    static const int HIST_LEN = 2880;   // 4 hours
    std::vector<float> hist[8];         // ring buffers
    std::vector<float> duty[8];         // heater on fraction in the slot
    int hist_pos = 0, hist_count = 0;
    double seconds(void) const { return t_sec; }

private:
    void integrate(double dt);
    uint64_t last = 0;               // bus time integrated up to
    double t_sec = 0;                // process time (s)
    double slot_t = 0;               // time in the current history slot
    bool started = false;
};
