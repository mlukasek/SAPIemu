#pragma once
// ARITMA Minigraf 0507 / Amagraf 0517 - a small flatbed plotter (A4) without logic of its own, see
// docs/desky/Minigraf.md. On SAPI-1 it hangs on port B of a DPP-1 channel (X2, 31h, control word 99h)
// and the driver V.1-MGF in EPROM 1800h moves it.
//
// - Inputs (TTL, one way, nothing comes back): a 3-bit phase code per axis, X = PB2..0 (X4 X2 X1),
//   Y = PB5..3, and the pen PB7 (H = down). The code counts up for + (pen to the right, paper towards
//   the user, i.e. the pen goes up the sheet); every change of the code by one is one step of 0.125 mm.
// - Inside only the phase decoders and the stepper drivers: a jump of the code by more than one
//   moves the motor by the shortest way (two, three phases) or not at all (four); counted as lost.
// - X has stops on both ends: the carriage just runs against them (steps are lost), the driver sets
//   its origin that way. The paper (Y) runs freely: the original driver moves it 212 mm when it sets
//   its origin and the test another 225 mm, more than an A4 sheet (chips/plotter.h).
#include "chips/i8255.h"
#include "chips/plotter.h"

class Minigraf : public Plotter, public I8255Peripheral {
public:
    static const int STEPS_PER_MM = 8;                 // 0.125 mm
    static const int32_t TRAVEL_MM = 200;              // carriage between its stops
    static const int32_t STOP_X_MM = 11;               // the left stop above this x of the paper

    Minigraf() : Plotter(STEPS_PER_MM, TRAVEL_MM, STOP_X_MM, 60, 400) {}
    const char* model(void) const override { return "ARITMA Minigraf 0507"; }

    I8255* ppi = nullptr;          // the channel it hangs on

    // I8255Peripheral
    void outputs_changed(int port, uint8_t value) override;
    void lines_now(void);          // port B as the 8255 drives it now (input = the lines float high)
    void set_lines(uint8_t v);     // the cable driven directly (the plotter of AR 1990: P0-OUT of JPR-1)

    void power_on(void) override;

    void state(StateIO& io) override;

private:
    bool known = false;            // the motors have taken a phase (the first code is the reference)
};
