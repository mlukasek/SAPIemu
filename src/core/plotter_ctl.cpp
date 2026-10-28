#include "core/plotter_ctl.h"
#include "chips/ar1990.h"
#include "chips/plotter.h"
#include "chips/xy4130.h"
#include "core/card.h"
#include "core/machine.h"
#include "core/util.h"

#include <stdio.h>
#include <vector>

const char* const PLOTTER_NONE =
    "no plotter in this machine (DPP-1 parameter minigraf or xy4130, JPR-1 parameter xy4130 or the keyboard AR 1990)";

static const char* const AR1990_DEMO = "media/ar1990/ukazka.txt";

bool plotter_find(Machine& m, PlotterRef& p)
{
    p = PlotterRef();
    for (auto& c : m.cards)
        if (Plotter* g = c->plotter()) {
            p.g = g;
            p.card = c->name;
        }
    if (!p.g) return false;
    p.xy = dynamic_cast<Xy4130*>(p.g);
    p.ar = dynamic_cast<Ar1990Plotter*>(m.keyboard());
    return true;
}

static std::string normalized(const std::string& action)
{
    std::string a = action;
    for (char& c : a)
        if (c == '_') c = '-';
    return a;
}

bool plotter_action_has_arg(const std::string& action)
{
    std::string a = normalized(action);
    return a == "send" || a == "send-file";
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string plotter_line(const std::string& text)
{
    std::string s;
    for (size_t i = 0; i < text.size(); i++) {
        int h, l;
        if (text[i] == '\\' && i + 3 < text.size() && (text[i + 1] == 'x' || text[i + 1] == 'X') &&
            (h = hex_digit(text[i + 2])) >= 0 && (l = hex_digit(text[i + 3])) >= 0) {
            s += (char)(h << 4 | l);
            i += 3;
        } else s += text[i];
    }
    s += '\r';   // the end of the parameters
    return s;
}

bool plotter_action(Machine& m, const std::string& action, const std::string& arg, std::string& err)
{
    PlotterRef p;
    if (!plotter_find(m, p)) {
        err = PLOTTER_NONE;
        return false;
    }
    std::string a = normalized(action);
    if (a == "pen") {
        if (!p.xy) {
            err = "only the XY 4130 has the button PEN";
            return false;
        }
        p.xy->pen_button();
        return true;
    }
    if (a == "new-sheet") {
        p.g->new_sheet();
        return true;
    }
    int button = a == "ready" ? Ar1990Plotter::BTN_READY
               : a == "test" || a == "whole-test" ? Ar1990Plotter::BTN_TEST
               : a == "stop" ? Ar1990Plotter::BTN_STOP : -1;
    if (button < 0 && a != "demo" && a != "send" && a != "send-file") {
        err = "unknown plotter action '" + action + "' (pen, new-sheet, ready, test, stop, whole-test, demo, send TEXT, send-file FILE)";
        return false;
    }
    if (!p.ar) {
        err = "only the plotter of AR 1990 has it (the XY 4130 has pen, any plotter new-sheet)";
        return false;
    }
    if (button >= 0) {
        p.ar->press(button);
        if (a == "whole-test") m.reset();   // TEST held while the JPR-1 starts
        return true;
    }
    if (a == "send") {
        std::string s = plotter_line(arg);
        p.ar->send((const uint8_t*)s.data(), s.size());
        return true;
    }
    if (a == "send-file" && arg.empty()) {
        err = "the file is missing";
        return false;
    }
    std::string path = data_path(a == "demo" ? std::string(AR1990_DEMO) : arg);
    std::vector<uint8_t> d;
    if (!file_read_all(path, d)) {
        err = "cannot read " + path;
        return false;
    }
    p.ar->send(d.data(), d.size());
    return true;
}

std::string plotter_status(Machine& m)
{
    PlotterRef p;
    if (!plotter_find(m, p)) return std::string(PLOTTER_NONE) + "\n";
    const Plotter* g = p.g;
    const double k = 1.0 / g->spm;
    std::string s;
    char b[256];
    if (p.ar && !p.xy)
        snprintf(b, sizeof(b), "plotter: AR 1990 (%s) on %s\n", g->model(), p.card.c_str());
    else if (p.xy)
        snprintf(b, sizeof(b), "plotter: %s on %s, cable %s\n", g->model(), p.card.c_str(),
                 p.xy->cable == Xy4130::CABLE_KRES ? "kres" : "mikos");
    else
        snprintf(b, sizeof(b), "plotter: %s on %s\n", g->model(), p.card.c_str());
    s += b;
    if (p.ar) {
        snprintf(b, sizeof(b), "lamps: RDY %s, BUFF %s; host: %zu B waiting, %llu B taken\n", p.ar->led_ready() ? "on" : "off",
                 p.ar->led_buff() ? "on" : "off", p.ar->pending(), (unsigned long long)p.ar->sent);
        s += b;
    }
    if (p.xy) {
        snprintf(b, sizeof(b), "lamps: READY %s, PEN %s\n", p.xy->ready() ? "on" : "off", p.xy->pen_enabled() ? "on" : "off");
        s += b;
    }
    snprintf(b, sizeof(b), "pen: %s at x %.1f mm, y %.1f mm\n", g->pen_down() ? "down" : "up", g->x() * k, g->y() * k);
    s += b;
    int32_t x0, y0, x1, y1;
    if (g->extent(x0, y0, x1, y1))
        snprintf(b, sizeof(b), "drawing: %zu strokes, %.1f x %.1f mm from x %.1f, y %.1f mm\n", g->strokes.size(),
                 (x1 - x0) * k, (y1 - y0) * k, x0 * k, y0 * k);
    else
        snprintf(b, sizeof(b), "drawing: %zu strokes\n", g->strokes.size());
    s += b;
    snprintf(b, sizeof(b), "steps: %llu, against the stop %llu, lost %llu, too fast %llu\n", (unsigned long long)g->steps,
             (unsigned long long)g->stopped, (unsigned long long)g->lost, (unsigned long long)g->fast);
    s += b;
    return s;
}
