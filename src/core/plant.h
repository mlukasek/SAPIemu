#pragma once
// The technological process outside the computer that its I/O cards measure and control (the
// furnaces of the APS-5 unit, docs/desky/APS-5.md). A machine has at most one, built from the
// "plant" object of its description (plant/plant.cpp); the cards reach it through Bus::plant.
// Time is the bus time (CPU cycles), as for the cards.
#include <math.h>
#include <stdint.h>
#include <string>
#include "json.hpp"

class StateIO;
struct Bus;

class Plant {
public:
    virtual ~Plant() {}
    virtual const char* type_name() const = 0;
    // Bring the process up to bus time `now`.
    virtual void advance(uint64_t now) {}
    // Analog signals at the inputs of the cards:
    // - thermocouple input `ch` (0..7) of DMT-1 in mV against the terminal, NAN = open circuit;
    // - temperature of the isothermal terminal block (the sensor transistor of DMT-1), NAN = none;
    // - voltage of the backup battery (an input of ADP-1).
    virtual double tc_input_mv(int ch) { return NAN; }
    virtual double block_temp_c() { return NAN; }
    virtual double battery_v() { return NAN; }
    // The relay contacts of DRV-1A (bit n = contact S(n+1) closed) from bus time `now` on.
    virtual void relays(uint8_t mask, uint64_t now) {}
    // Logic inputs LI of group a (0) / b (1): bit n = channel n + 1, 1 = H (open, pull-up).
    virtual uint8_t logic_in(int group) { return 0xFF; }
    // Logic outputs LO of group 1 (0) / 2 (1), bit n = channel n + 1.
    virtual void logic_out(int group, uint8_t value) {}
    virtual void reset_process(void) {}   // back to the ambient temperature (the window)
    virtual void state(StateIO& io) {}
    virtual void describe(nlohmann::json& out) {}

    Bus* bus = nullptr;   // set by the machine (bus time to seconds: Bus::cpu_hz)
};

// Build the plant of a machine description object ("plant"); nullptr + err on error.
Plant* plant_create(const nlohmann::json& cfg, std::string& err);
