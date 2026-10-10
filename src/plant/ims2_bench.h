#pragma once
// Plant "IMS-2": the measuring bench at the DPS-1 card (docs/desky/DPS-1.md, docs/navod-dps1.md). The
// IMS-2 cable from DPS-1 X2 goes to the voltmeter Metra M1T 330; its input (H, L) is wired to an output of
// the power supply Tesla BS 525. The DPS-1 reaches the bus through Plant::ims2().
//
// Machine description: "plant": { "type": "IMS-2", "m1t330": { "address": 3 }, "bs525": { "u1": 12.564,
// "i1": 0.5, "u2": 5, "i2": 0.5 }, "input": "I" }  (input: the BS 525 output on the voltmeter, I / II / none)
#include <stdint.h>
#include "core/ims2.h"
#include "core/plant.h"
#include "plant/bs525.h"
#include "plant/m1t330.h"

class Ims2Bench : public Plant {
public:
    ~Ims2Bench() override;
    const char* type_name() const override { return "IMS-2"; }
    bool configure(const nlohmann::json& cfg, std::string& err);
    void advance(uint64_t now) override;
    Ims2Bus* ims2() override { return &ims; }
    void state(StateIO& io) override;
    void describe(nlohmann::json& out) override;

    // the window and the MCP tool
    void set_power_m1t(bool on);
    void changed(void);   // the supply or the wiring changed: the voltmeter input follows
    double input_v(void) const;

    Ims2Bus ims;
    M1t330 m1t;
    Bs525 bs;
    int input = 0;        // BS 525 output on the voltmeter: 0 = I, 1 = II, -1 = none

private:
    bool started = false;
};
