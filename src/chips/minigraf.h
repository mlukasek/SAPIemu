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
//   its origin and the test another 225 mm, more than an A4 sheet; the paper is therefore a strip
//   as wide as A4 and as long as the drawing needs.
// - The drawing is kept as pen-down strokes in steps: x from the left edge of the paper, y up the
//   paper from where the pen was when the paper was put in.
#include <stdint.h>
#include <string>
#include <vector>
#include "chips/i8255.h"

struct Bus;
class StateIO;

class Minigraf : public I8255Peripheral {
public:
    static const int STEPS_PER_MM = 8;                 // 0.125 mm
    static const int32_t SHEET_W = 210 * STEPS_PER_MM, SHEET_H = 297 * STEPS_PER_MM;   // A4
    static const int32_t TRAVEL = 200 * STEPS_PER_MM;  // carriage between its stops
    static const int32_t STOP_X = 11 * STEPS_PER_MM;   // the left stop above this x of the paper

    Bus* bus = nullptr;
    I8255* ppi = nullptr;          // the channel it hangs on

    // I8255Peripheral
    void outputs_changed(int port, uint8_t value) override;
    void lines_now(void);          // port B as the 8255 drives it now (input = the lines float high)
    void set_lines(uint8_t v);     // the cable driven directly (the plotter of AR 1990: P0-OUT of JPR-1)

    void power_on(void);           // the carriage somewhere left, new paper
    void new_sheet(void);          // clean paper put in under the pen (y = 0 there, x stays)

    // the drawing: strokes of points (x, y pairs, steps on the paper)
    std::vector<std::vector<int32_t>> strokes;
    uint64_t generation = 0;       // changes with every new point (the window follows)
    int32_t x(void) const { return STOP_X + carriage; }
    int32_t y(void) const { return paper_y; }
    bool pen_down(void) const { return pen; }
    uint64_t steps = 0, lost = 0, stopped = 0, fast = 0;   // steps, code jumps, steps against a stop, over 400/s
    uint8_t lines(void) const { return last; }

    // the extent of the drawing (steps); false when nothing is drawn
    bool extent(int32_t& x0, int32_t& y0, int32_t& x1, int32_t& y1) const;
    // exports: the width of the paper, the height of the drawing with margins (at least A4); crop = just the
    // drawing with 5 mm around
    bool save_svg(const std::string& path, bool crop, std::string& err) const;
    bool save_png(const std::string& path, int px_per_mm, bool crop, std::string& err) const;

    void state(StateIO& io);

private:
    void step(int axis, int d, double t);
    void point(void);              // the pen moved: extend the drawing
    double now_s(void) const;

    uint8_t last = 0xFF;           // the lines last seen
    bool known = false;            // the motors have taken a phase (the first code is the reference)
    int32_t carriage = 60 * STEPS_PER_MM;   // from the left stop
    int32_t paper_y = 0;           // the pen above this y of the paper
    bool pen = false;
    double last_step_t[2] = { -1, -1 };
    std::vector<int32_t> run;      // points of the straight run that ends the last stroke
};
