#include "chips/minigraf.h"
#include "core/bus.h"
#include "core/log.h"
#include "core/png.h"
#include "core/state.h"
#include "core/util.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

static const double MAX_RATE = 400;   // steps per second (the leaflet), faster ones are counted

double Minigraf::now_s(void) const
{
    return bus ? (double)bus->now / bus->cpu_hz : 0.0;
}

void Minigraf::power_on(void)
{
    known = false;
    last = 0xFF;
    pen = false;
    carriage = 60 * STEPS_PER_MM;
    last_step_t[0] = last_step_t[1] = -1;
    steps = lost = stopped = fast = 0;
    strokes.clear();
    run.clear();
    paper_y = 0;
    generation++;
}

void Minigraf::new_sheet(void)
{
    strokes.clear();
    run.clear();
    paper_y = 0;
    if (pen) strokes.push_back({ x(), y() });
    generation++;
}

void Minigraf::outputs_changed(int, uint8_t)
{
    lines_now();
}

void Minigraf::lines_now(void)
{
    // port B as an output (modes 0 and 1) drives the cable; as an input the 8255 lets its pins float
    // and the buffers of the DPP-1 (set to output) pass high levels
    uint8_t v = 0xFF;
    if (ppi && !(ppi->control() & 0x02)) v = ppi->out_b;
    if (known && v == last) return;
    double t = now_s();
    if (!known) {   // the motors take this phase, nothing moves
        known = true;
        last = v;
        pen = (v & 0x80) != 0;
        if (pen) strokes.push_back({ x(), y() });
        generation++;
        return;
    }
    for (int axis = 0; axis < 2; axis++) {
        int sh = axis ? 3 : 0;
        int d = (((v >> sh) & 7) - ((last >> sh) & 7)) & 7;
        if (d == 0) continue;
        if (d == 4) {   // the opposite phase: the rotor stays (or jumps either way)
            lost++;
            continue;
        }
        int n = d < 4 ? d : d - 8;
        if (n > 1 || n < -1) lost++;   // a skipped phase: the motor follows the shortest way
        for (int i = 0; i < abs(n); i++) step(axis, n > 0 ? 1 : -1, t);
    }
    bool down = (v & 0x80) != 0;
    if (down != pen) {
        pen = down;
        run.clear();
        if (pen) strokes.push_back({ x(), y() });
        generation++;
    }
    last = v;
}

void Minigraf::step(int axis, int d, double t)
{
    steps++;
    if (last_step_t[axis] >= 0 && t - last_step_t[axis] < 1.0 / MAX_RATE * 0.95) fast++;
    last_step_t[axis] = t;
    if (axis == 0) {
        int32_t c = carriage + d;
        if (c < 0 || c > TRAVEL) {   // against the stop (the driver sets its origin so)
            stopped++;
            return;
        }
        carriage = c;
    } else {
        paper_y += d;   // + = the paper towards the user: the pen goes up the sheet
    }
    point();
}

// The pen moved by one step: the stroke gets the point. Points along a straight run (within one step of
// the line from where the run started: the staircase of a slanted line) replace each other, so lines stay
// a few points.
void Minigraf::point(void)
{
    generation++;
    if (!pen) return;
    if (strokes.empty()) strokes.push_back({ x(), y() });
    std::vector<int32_t>& s = strokes.back();
    int32_t qx = x(), qy = y();
    if (run.size() >= 2 && run.size() < 512) {
        double ax = run[0], ay = run[1], dx = qx - ax, dy = qy - ay, len = sqrt(dx * dx + dy * dy);
        bool straight = len > 0;
        for (size_t i = 2; straight && i + 1 < run.size(); i += 2) {
            double dist = fabs((run[i] - ax) * dy - (run[i + 1] - ay) * dx) / len;
            if (dist > 1.0) straight = false;
        }
        if (straight && s.size() >= 4) {
            s[s.size() - 2] = qx;   // the run goes on: its end moves
            s[s.size() - 1] = qy;
            run.push_back(qx);
            run.push_back(qy);
            return;
        }
    }
    // a new run from the last point of the stroke
    int32_t lx = s[s.size() - 2], ly = s[s.size() - 1];
    run.assign({ lx, ly, qx, qy });
    s.push_back(qx);
    s.push_back(qy);
}

// ---------------------------------------------------------------- export

bool Minigraf::extent(int32_t& x0, int32_t& y0, int32_t& x1, int32_t& y1) const
{
    bool any = false;
    for (const std::vector<int32_t>& s : strokes)
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            if (!any || s[i] < x0) x0 = s[i];
            if (!any || s[i] > x1) x1 = s[i];
            if (!any || s[i + 1] < y0) y0 = s[i + 1];
            if (!any || s[i + 1] > y1) y1 = s[i + 1];
            any = true;
        }
    return any;
}

// the part of the paper exported: the drawing with 10 mm above and below, at least A4 high
static void export_range(const Minigraf& g, int32_t& top, int32_t& h)
{
    int32_t x0, y0, x1, y1;
    const int32_t m = 10 * Minigraf::STEPS_PER_MM;
    if (!g.extent(x0, y0, x1, y1)) {
        y0 = y1 = g.y();
    }
    top = y1 + m;
    h = y1 - y0 + 2 * m;
    if (h < Minigraf::SHEET_H) {
        top += (Minigraf::SHEET_H - h) / 2;
        h = Minigraf::SHEET_H;
    }
}

bool Minigraf::save_svg(const std::string& path, std::string& err) const
{
    std::string o;
    char b[200];
    int32_t top, hh;
    export_range(*this, top, hh);
    snprintf(b, sizeof(b),
             "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"210mm\" "
             "height=\"%gmm\" viewBox=\"0 0 %d %d\">\n<rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n"
             "<g fill=\"none\" stroke=\"black\" stroke-width=\"3\" stroke-linecap=\"round\" stroke-linejoin=\"round\">\n",
             hh / (double)STEPS_PER_MM, (int)SHEET_W, (int)hh);
    o += b;
    for (const std::vector<int32_t>& s : strokes) {
        if (s.size() < 2) continue;
        o += s.size() == 2 ? "<circle r=\"1.5\" fill=\"black\" " : "<polyline points=\"";
        if (s.size() == 2) {
            snprintf(b, sizeof(b), "cx=\"%d\" cy=\"%d\"/>\n", (int)s[0], (int)(top - s[1]));
            o += b;
            continue;
        }
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            snprintf(b, sizeof(b), "%s%d,%d", i ? " " : "", (int)s[i], (int)(top - s[i + 1]));
            o += b;
        }
        o += "\"/>\n";
    }
    o += "</g>\n</svg>\n";
    if (!file_write_all(path, o.data(), o.size())) {
        err = "cannot write " + path;
        return false;
    }
    return true;
}

bool Minigraf::save_png(const std::string& path, int px_per_mm, std::string& err) const
{
    if (px_per_mm < 1) px_per_mm = 1;
    if (px_per_mm > 16) px_per_mm = 16;
    int32_t top, hh;
    export_range(*this, top, hh);
    int w = 210 * px_per_mm, h = (int)(hh / STEPS_PER_MM) * px_per_mm;
    std::vector<uint32_t> img((size_t)w * h, 0xFFFFFFFFu);
    double k = (double)px_per_mm / STEPS_PER_MM;
    int r = px_per_mm >= 6 ? 1 : 0;   // a pen about 0.3 mm wide
    auto dot = [&](int px, int py) {
        for (int yy = py - r; yy <= py + r; yy++)
            for (int xx = px - r; xx <= px + r; xx++)
                if (xx >= 0 && yy >= 0 && xx < w && yy < h) img[(size_t)yy * w + xx] = 0xFF101820u;
    };
    for (const std::vector<int32_t>& s : strokes) {
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            double x0 = s[i] * k, y0 = (top - s[i + 1]) * k;
            double x1 = x0, y1 = y0;
            if (i + 3 < s.size()) {
                x1 = s[i + 2] * k;
                y1 = (top - s[i + 3]) * k;
            }
            int n = (int)std::max(fabs(x1 - x0), fabs(y1 - y0)) + 1;
            for (int j = 0; j <= n; j++) dot((int)lround(x0 + (x1 - x0) * j / n), (int)lround(y0 + (y1 - y0) * j / n));
        }
    }
    if (!png_write_argb(path, img.data(), w, h, 1, 1)) {
        err = "cannot write " + path;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- state

static void ints(StateIO& io, std::vector<int32_t>& v)
{
    size_t n = v.size();
    io.sz(n);
    if (io.loading) {
        if (!io.ok() || n > 100000000) return;
        v.resize(n);
    }
    for (int32_t& x : v) io.v(x);
}

void Minigraf::state(StateIO& io)
{
    io.v(last);
    io.v(known);
    io.v(carriage);
    io.v(paper_y);
    io.v(pen);
    io.v(last_step_t[0]);
    io.v(last_step_t[1]);
    io.v(steps);
    io.v(lost);
    io.v(stopped);
    io.v(fast);
    size_t n = strokes.size();
    io.sz(n);
    if (io.loading) {
        if (!io.ok() || n > 10000000) return;
        strokes.resize(n);
    }
    for (std::vector<int32_t>& s : strokes) ints(io, s);
    ints(io, run);
    if (io.loading) generation++;
}
