#pragma once
// Grafická jednotka XY 4130 (Laboratorní přístroje Praha), a small plotter A4 with stepper motors and the
// interface of the whole family XY 4131 / 4140 / 4150 (docs/desky/XY4130.md):
//
// - Inputs (TTL): PEN (H = down), STEP (KROK: one step on the rising edge), X/Y (L = the carriage, H = the
//   paper), +/- (the carriage: L = to the left stop; the paper: L = the pen goes up the sheet). Output READY
//   (open collector, L = ready).
// - 0.1 mm a step, 255 x 180 mm, 100 mm/s (1000 steps/s); the carriage has a left and a right stop.
// - READY (the latch U12A/U11A of the XY 4140 schematic): cleared at power on and by the paper buttons (a
//   new sheet), set when the carriage reaches the left stop. Without READY the pen does not go down; a driver
//   therefore runs the carriage to the left stop first.
// - The PEN button toggles the readiness to write (the LED PEN, flip-flop U9A); after power on it is off
//   and the pen stays up until it is pressed.
// - The cable on SAPI-1: "mikos" = the driver in the EPROM behind MIKOS V5 (JPR-1 X3, P2-OUT 2C00h: D0 PEN,
//   D1 X/Y, D6 +/-, D7 STEP, no READY), "kres" = KRES of BASIC EXP V5.0/G+SZ (DPP-1 port B or JPR-1 P2:
//   D0 STEP, D2 X/Y, D3 +/-, D5 READY, D7 PEN).
#include <string>
#include "chips/i8255.h"
#include "chips/plotter.h"
#include "json.hpp"

class Xy4130 : public Plotter, public I8255Peripheral {
public:
    static const int STEPS_PER_MM = 10;                // 0.1 mm
    static const int32_t TRAVEL_MM = 190;              // carriage between its stops (estimated)
    static const int32_t STOP_X_MM = 10;               // the left stop above this x of the paper (estimated)
    enum Cable { CABLE_MIKOS = 0, CABLE_KRES };

    Xy4130() : Plotter(STEPS_PER_MM, TRAVEL_MM, STOP_X_MM, 60, 1000) {}
    const char* model(void) const override { return "XY 4130"; }

    int cable = CABLE_MIKOS;
    bool pen_on_at_power = false;  // a deviation: the PEN button pressed already at power on
    I8255* ppi = nullptr;          // the DPP-1 channel it hangs on

    // I8255Peripheral (DPP-1 port B)
    void outputs_changed(int port, uint8_t value) override;
    void lines_now(void);
    void set_lines(uint8_t v);     // the cable as driven (P2-OUT of JPR-1, port B of the 8255)

    void power_on(void) override;
    void new_sheet(void) override;   // the paper buttons: READY goes off
    void pen_button(void);           // PEN: toggles the readiness to write
    bool ready(void) const { return ready_latch || carriage == 0; }
    bool pen_enabled(void) const { return pen_en; }
    int ready_bit(void) const { return cable == CABLE_KRES ? 5 : -1; }   // READY on the cable, -1 = not wired

    // I8255Peripheral: READY on port B (the kres cable)
    uint8_t pins_b() override;

    void state(StateIO& io) override;

private:
    void update_pen(void);
    bool ready_latch = false;
    bool pen_en = false;
    bool pen_in = false;           // the PEN line
    bool step_in = true;           // the STEP line
};

// The section "xy4130" of a card: "cable" (mikos, kres; default `def_cable`), "pen_on" (the deviation).
bool xy4130_configure(Xy4130& xy, const nlohmann::json& sec, const char* def_cable, std::string& err);
