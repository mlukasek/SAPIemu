#include "chips/plotter.h"
#include "core/bus.h"
#include "core/png.h"
#include "core/state.h"
#include "core/util.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

Plotter::Plotter(int32_t steps_per_mm, int32_t travel_mm, int32_t stop_x_mm, int32_t start_mm, double rate)
    : spm(steps_per_mm), sheet_w(210 * steps_per_mm), sheet_h(297 * steps_per_mm), travel(travel_mm * steps_per_mm),
      stop_x(stop_x_mm * steps_per_mm), max_rate(rate), carriage(start_mm * steps_per_mm), start(start_mm * steps_per_mm)
{
}

double Plotter::now_s(void) const
{
    return bus ? (double)bus->now / bus->cpu_hz : 0.0;
}

void Plotter::power_on(void)
{
    last = 0xFF;
    pen = false;
    carriage = start;
    last_step_t[0] = last_step_t[1] = -1;
    steps = lost = stopped = fast = 0;
    strokes.clear();
    run.clear();
    paper_y = 0;
    generation++;
}

void Plotter::new_sheet(void)
{
    strokes.clear();
    run.clear();
    paper_y = 0;
    if (pen) strokes.push_back({ x(), y() });
    generation++;
}

void Plotter::set_pen(bool down)
{
    if (down == pen) return;
    pen = down;
    run.clear();
    if (pen) strokes.push_back({ x(), y() });
    generation++;
}

bool Plotter::step(int axis, int d, double t)
{
    steps++;
    if (last_step_t[axis] >= 0 && t - last_step_t[axis] < 1.0 / max_rate * 0.95) fast++;
    last_step_t[axis] = t;
    if (axis == 0) {
        int32_t c = carriage + d;
        if (c < 0 || c > travel) {   // against the stop (the drivers set their origin so)
            stopped++;
            return false;
        }
        carriage = c;
    } else {
        paper_y += d;   // + = the pen goes up the sheet
    }
    point();
    return true;
}

// The pen moved by one step: the stroke gets the point. Points along a straight run (within one step of
// the line from where the run started: the staircase of a slanted line) replace each other, so lines stay
// a few points.
void Plotter::point(void)
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

bool Plotter::extent(int32_t& x0, int32_t& y0, int32_t& x1, int32_t& y1) const
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

// The part of the paper exported (steps): the width of the paper and the drawing with 10 mm above and
// below, at least A4 high; cropped: the drawing with 5 mm around.
static void export_range(const Plotter& g, bool crop, int32_t& left, int32_t& top, int32_t& w, int32_t& h)
{
    int32_t x0, y0, x1, y1;
    if (!g.extent(x0, y0, x1, y1)) {
        x0 = x1 = g.x();
        y0 = y1 = g.y();
    }
    if (crop) {
        const int32_t m = 5 * g.spm;
        left = x0 - m;
        top = y1 + m;
        w = x1 - x0 + 2 * m;
        h = y1 - y0 + 2 * m;
        return;
    }
    const int32_t m = 10 * g.spm;
    left = 0;
    w = g.sheet_w;
    top = y1 + m;
    h = y1 - y0 + 2 * m;
    if (h < g.sheet_h) {
        top += (g.sheet_h - h) / 2;
        h = g.sheet_h;
    }
}

bool Plotter::save_svg(const std::string& path, bool crop, std::string& err) const
{
    std::string o;
    char b[512];   // the header alone is about 300 bytes
    int32_t left, top, ww, hh;
    export_range(*this, crop, left, top, ww, hh);
    double pen_w = 0.375 * spm;   // the pen line, about 0.4 mm
    snprintf(b, sizeof(b),
             "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%gmm\" "
             "height=\"%gmm\" viewBox=\"0 0 %d %d\">\n<rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n"
             "<g fill=\"none\" stroke=\"black\" stroke-width=\"%g\" stroke-linecap=\"round\" stroke-linejoin=\"round\">\n",
             ww / (double)spm, hh / (double)spm, (int)ww, (int)hh, pen_w);
    o += b;
    for (const std::vector<int32_t>& s : strokes) {
        if (s.size() < 2) continue;
        if (s.size() == 2) {
            snprintf(b, sizeof(b), "<circle r=\"%g\" fill=\"black\" cx=\"%d\" cy=\"%d\"/>\n", pen_w / 2, (int)(s[0] - left),
                     (int)(top - s[1]));
            o += b;
            continue;
        }
        o += "<polyline points=\"";
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            snprintf(b, sizeof(b), "%s%d,%d", i ? " " : "", (int)(s[i] - left), (int)(top - s[i + 1]));
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

bool Plotter::save_png(const std::string& path, int px_per_mm, bool crop, std::string& err) const
{
    if (px_per_mm < 1) px_per_mm = 1;
    if (px_per_mm > 16) px_per_mm = 16;
    int32_t left, top, ww, hh;
    export_range(*this, crop, left, top, ww, hh);
    int w = (int)(ww / spm) * px_per_mm, h = (int)(hh / spm) * px_per_mm;
    std::vector<uint32_t> img((size_t)w * h, 0xFFFFFFFFu);
    double k = (double)px_per_mm / spm;
    int r = px_per_mm >= 6 ? 1 : 0;   // a pen about 0.3 mm wide
    auto dot = [&](int px, int py) {
        for (int yy = py - r; yy <= py + r; yy++)
            for (int xx = px - r; xx <= px + r; xx++)
                if (xx >= 0 && yy >= 0 && xx < w && yy < h) img[(size_t)yy * w + xx] = 0xFF101820u;
    };
    for (const std::vector<int32_t>& s : strokes) {
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            double x0 = (s[i] - left) * k, y0 = (top - s[i + 1]) * k;
            double x1 = x0, y1 = y0;
            if (i + 3 < s.size()) {
                x1 = (s[i + 2] - left) * k;
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

void Plotter::drawing_state(StateIO& io)
{
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
