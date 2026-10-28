#pragma once
// Headless runner: executes command line actions in order, for tests and scripting.
//
//   --run-ms N            run N ms of emulated time
//   --type TEXT           type text on the keyboard (escapes: \r \e \t \xNN)
//   --wait-text TEXT      run until TEXT appears on the screen (timeout --timeout-ms)
//   --timeout-ms N        timeout for --wait-text (default 5000 emulated ms)
//   --print-screen        print the visible screen text
//   --serial-send TEXT    send TEXT to the first serial channel
//   --print-serial        print and clear data received from the first serial channel
//   --break ADDR          set an execution breakpoint (hex)
//   --print-regs          print CPU registers
//   --screenshot FILE     save the screen as PNG (2x)
//   --disasm ADDR N       disassemble N instructions from ADDR (hex)
//   --load-bin FILE ADDR  load a binary file into memory at ADDR (hex)
//   --save-state FILE     save the state of the machine (docs/stav.md)
//   --load-state FILE     load a state (the machine of the state is built when it differs)
//   --audio-wav FILE      record the sound card output from here on, saved as WAV at the end
//   --plotter ACTION [ARG]  a button of the plotter: pen, new-sheet, ready, test, stop, whole-test, demo,
//                         send TEXT, send-file FILE (core/plotter_ctl.h)
//   --print-plotter       print the plotter: lamps, bytes waiting and taken, pen, strokes, extent, steps
//   --plotter-png FILE    save the drawing (with 5 mm around) as PNG, 8 px/mm; --plotter-svg FILE as SVG
//
// Returns the process exit code (0 = all actions succeeded).
#include <string>
#include <vector>

class Machine;

int headless_run(Machine& m, const std::vector<std::string>& actions);
bool screen_contains(Machine& m, const std::string& text);
std::string screen_text(Machine& m);
// Run until `text` is on screen; returns false on timeout.
bool run_until_text(Machine& m, const std::string& text, double timeout_s);
