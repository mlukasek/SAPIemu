#pragma once
// APS-5 demo: an operator who types the example of docs/aps5-ukazka.md on the front panel HRK-1 and
// waits for each prompt of the firmware (nothing is written into the memory): calibration, program 1
// (20 min to 700 degC, 20 min hold, 20 min down to 300 degC), Pec 1, 2, 5 regulated by it, Pec 4 and
// 6 by hand, the table of the furnaces. Then the furnaces heat up for 25 minutes (the window runs the
// emulation at full speed meanwhile).
#include <string>
#include <vector>

class Machine;

class Aps5Demo {
public:
    // Start on a machine with the HRK-1 panel and a text display; false + err otherwise.
    bool start(Machine& m, std::string& err);
    // Go on: call often while the machine runs (every frame, or after each bit of emulated time).
    void poll(Machine& m);
    void stop(void) { phase = IDLE; }

    enum Phase { IDLE, TYPING, HEATING, DONE, FAILED };
    Phase phase = IDLE;
    bool running(void) const { return phase == TYPING || phase == HEATING; }
    int step = 0, steps = 0;      // progress of the typing
    const char* label = "";       // what is being entered now (English, for tr())
    std::string error;            // FAILED: why
    double heat_left = 0;         // HEATING: emulated seconds still to go

    static const double HEAT_S;   // the heating after the typing

private:
    struct Step {
        std::string keys;         // panel keys (0-9, M, R, \r = ->)
        std::string expect;       // text on the screen when the firmware has answered
        const char* label;
        std::string alt = "";     // another answer that is fine too (an error the step may get)
        std::string skip = "";    // an answer that jumps to step skip_to, if the screen before had need
        int skip_to = -1;
        std::string need = "";
    };
    std::vector<Step> list;
    bool typed = false;           // keys of the current step given to the panel
    std::string before;           // the screen before them
    double keys_at = 0;           // emulated time the keys were given
    double seen_at = -1;          // the expected text first seen
    double heat_end = 0;
};
