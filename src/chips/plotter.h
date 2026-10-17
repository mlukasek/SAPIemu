#pragma once
// The mechanics of a small plotter: a carriage with the pen between two stops (x, across the paper) and the
// paper moved under it (y). The plotters decode their cables into steps and the pen (chips/minigraf.h,
// chips/xy4130.h); this keeps the drawing, which the window, MCP and the exports show.
//
// - The paper runs freely: it is a strip as wide as A4 and as long as the drawing needs.
// - The drawing is kept as pen-down strokes in steps: x from the left edge of the paper, y up the paper
//   from where the pen was when the paper was put in.
#include <stdint.h>
#include <string>
#include <vector>

struct Bus;
class StateIO;

class Plotter {
public:
    virtual ~Plotter() {}
    virtual const char* model(void) const = 0;   // "ARITMA Minigraf 0507"

    Bus* bus = nullptr;
    const int32_t spm;             // steps per mm
    const int32_t sheet_w, sheet_h;   // the sheet (steps), A4
    const int32_t travel;          // the carriage between its stops (steps)
    const int32_t stop_x;          // the left stop above this x of the paper
    const double max_rate;         // steps per second of the leaflet, faster ones are counted

    virtual void power_on(void);   // the carriage somewhere left, new paper
    virtual void new_sheet(void);  // clean paper put in under the pen (y = 0 there, x stays)

    // the drawing: strokes of points (x, y pairs, steps on the paper)
    std::vector<std::vector<int32_t>> strokes;
    uint64_t generation = 0;       // changes with every new point (the window follows)
    int32_t x(void) const { return stop_x + carriage; }
    int32_t y(void) const { return paper_y; }
    bool pen_down(void) const { return pen; }
    uint64_t steps = 0, lost = 0, stopped = 0, fast = 0;   // steps, lost steps, steps against a stop, too fast
    uint8_t lines(void) const { return last; }   // the cable as last seen

    // the extent of the drawing (steps); false when nothing is drawn
    bool extent(int32_t& x0, int32_t& y0, int32_t& x1, int32_t& y1) const;
    // exports: the width of the paper, the height of the drawing with margins (at least A4); crop = just the
    // drawing with 5 mm around
    bool save_svg(const std::string& path, bool crop, std::string& err) const;
    bool save_png(const std::string& path, int px_per_mm, bool crop, std::string& err) const;

    virtual void state(StateIO& io) = 0;

protected:
    // sheet and travel in mm, the carriage after power on (mm from the left stop)
    Plotter(int32_t steps_per_mm, int32_t travel_mm, int32_t stop_x_mm, int32_t start_mm, double rate);
    bool step(int axis, int d, double t);   // false: against a stop (axis 0)
    void set_pen(bool down);
    void point(void);              // the pen moved: extend the drawing
    double now_s(void) const;
    void drawing_state(StateIO& io);   // strokes and run

    uint8_t last = 0xFF;
    int32_t carriage;              // from the left stop
    int32_t paper_y = 0;           // the pen above this y of the paper
    bool pen = false;
    double last_step_t[2] = { -1, -1 };
    std::vector<int32_t> run;      // points of the straight run that ends the last stroke

private:
    const int32_t start;
};
