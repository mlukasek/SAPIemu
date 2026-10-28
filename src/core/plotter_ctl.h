#pragma once
// The buttons of the plotters without the window: the plotter of AR 1990 (READY, TEST, STOP, the whole test, the
// demo, a line of commands or a file from the host computer), the XY 4130 (PEN) and a new sheet of any of them.
// The window Minigraf, MCP (tool plotter) and sapiemu-cli (--plotter, --print-plotter) use the same code.
//
//   pen               XY 4130: the button PEN (the readiness to write, off after power on)
//   new-sheet         clean paper under the pen (the drawing is cleared; XY 4130: READY goes off)
//   ready, test, stop AR 1990: the buttons of the panel (held 150 ms of emulated time)
//   whole-test        AR 1990: TEST held while the JPR-1 is reset (the whole character set, then READY)
//   demo              AR 1990: media/ar1990/ukazka.txt from the host computer
//   send TEXT         AR 1990: a line of commands as the window sends it (\xNN = the byte NN, CR at the end)
//   send-file FILE    AR 1990: the bytes of a file (a relative path from the data root)
//
// '_' may stand for '-' (MCP: whole_test).
#include <string>

class Machine;
class Plotter;
class Xy4130;
class Ar1990Plotter;

struct PlotterRef {
    Plotter* g = nullptr;          // the mechanics with the drawing; nullptr: no plotter
    Xy4130* xy = nullptr;          // the XY 4130
    Ar1990Plotter* ar = nullptr;   // the plotter of AR 1990 (the keyboard device of the JPR-1)
    std::string card;              // the card it hangs on
};

extern const char* const PLOTTER_NONE;   // the error when the machine has no plotter

// The plotter of the machine (the last card with one); false when there is none.
bool plotter_find(Machine& m, PlotterRef& p);
// One action of the list above; false with `err` when it is unknown or the plotter has no such button.
bool plotter_action(Machine& m, const std::string& action, const std::string& arg, std::string& err);
bool plotter_action_has_arg(const std::string& action);   // send, send-file
// A line of commands as the window sends it: \xNN = the byte NN, CR at the end.
std::string plotter_line(const std::string& text);
// The status for people (sapiemu-cli --print-plotter), a few lines: the plotter, its lamps, bytes waiting and taken
// (AR 1990), the pen, the strokes and the extent of the drawing, the steps.
std::string plotter_status(Machine& m);
