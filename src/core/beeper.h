#pragma once
// The beeper of a keyboard as a sound source of the bus (docs/desky/klavesnice.md):
//   BUZZER  - an active buzzer with its own oscillator (EKL-1 SG1 F/CM12P on X2 pin 14 =
//             P0-OUT1): sounds while the line is on; the program only holds the level
//             (ECMON: BEL = P0-OUT1 low for 160 ms).
//   SPEAKER - a transducer that follows the line (ANK-1 D6): the program makes the tone by
//             toggling the bit (MIKROBASIC: about 1 kHz for 65 ms after a key).
// Every edge is placed at its bus time; a sample is the average of the line over its period,
// a DC blocker keeps a steady level silent.
#include <stdint.h>

struct Bus;
class StateIO;

struct Beeper {
    enum Kind { NONE = 0, BUZZER, SPEAKER };
    int kind = NONE;
    double tone_hz = 2400;      // BUZZER: its oscillator
    bool on = false;            // BUZZER: sounding; SPEAKER: the level of the line
    void set(Bus* b, bool level);   // the line changes now
    void advance(Bus* b);           // sound up to the bus time
    void state(StateIO& io);
private:
    bool started = false;
    double pos = 0;             // sample position rendered up to
    uint64_t index = 0;         // the sample being summed
    double acc = 0;             // its part so far (level times the time)
    double phase = 0;           // BUZZER oscillator, in periods
    float hp_x1 = 0, hp_y1 = 0; // DC blocker
};
