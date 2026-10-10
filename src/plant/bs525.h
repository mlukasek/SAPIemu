#pragma once
// Laboratory power supply Tesla BS 525: two independent outputs 0-30 V with a current limit 50 mA - 1.1 A
// (docs/desky/DPS-1.md, the measuring bench). Per output: the voltage knob, the current limit knob, the
// meter M1 / M2 (upper scale volts, lower scale amperes) with the switch "Mereni" V / A, the protection lamp
// E41 / E42 (the limit works) and the + and - terminals. The mains switch with the lamp E40.
// No bus: it is set by hand (the window). An output feeds its load (a resistor of the bench, or nothing);
// when the load wants more than the limit, the output holds the current and the voltage drops.
#include <stdint.h>

class StateIO;

struct Bs525Output {
    double u_set = 10.0;      // V, the voltage knob
    double i_lim = 0.5;       // A, the current limit knob
    bool meter_a = false;     // the switch "Mereni": false = V, true = A
    int load = 0;             // index of BS525_LOADS (0 = nothing but the voltmeter)
    // computed
    double u = 0, i = 0;
    bool limiting = false;    // E41 / E42
};

static const int BS525_LOAD_COUNT = 7;
extern const double BS525_LOADS[BS525_LOAD_COUNT];   // ohms, 0 = open

struct Bs525 {
    bool power = true;        // E40
    Bs525Output out[2];
    // the voltmeter on an output takes 10 MOhm: no current worth showing
    void update(void);
    void state(StateIO& io);
};
