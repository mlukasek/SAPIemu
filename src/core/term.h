#pragma once
// Character cell terminal model used by the serial terminal window.
#include <stdint.h>
#include <string>
#include <vector>

enum TermEmulation {
    TERM_SAPI = 0,     // ECMON / AND-1Z console codes
    TERM_ANSI,         // ANSI / VT100 subset
    TERM_RAW,          // control characters shown as ^X
};

// Character cell terminal: a scrollback of lines (SAPI codes) whose last
// `rows` lines form the screen; cursor addressing works within the screen.
struct TerminalState {
    uint64_t pos = 0;                  // read position in the channel stream
    std::vector<std::string> lines;    // byte codes per line, at most `cols` long
    uint64_t dropped = 0;              // lines dropped from the top of the scrollback so far
    int row = 0, col = 0;              // cursor, row is an index into lines
    // selection between two cell boundaries: (line, column), line = dropped + index into lines;
    // anchor where the mouse went down, end where it is now, in any order
    bool sel = false;
    uint64_t sel_line0 = 0, sel_line1 = 0;
    int sel_col0 = 0, sel_col1 = 0;
    int cols = 80, rows = 24;
    int emulation = TERM_SAPI;
    bool cr_newline = false;           // received CR also starts a new line
    bool local_echo = false;
    bool scroll_to_bottom = true;
    // escape sequence parser
    int esc = 0;                       // 0 none, SAPI: bytes pending; ANSI: 1 = ESC, 2 = CSI
    std::string esc_buf;
};

// Feed bytes received from SAPI into a terminal (exposed for tests).
void terminal_feed(TerminalState& t, const std::string& data);
int terminal_top(const TerminalState& t);   // index of the first screen line

// A line of SAPI codes as UTF-8 text, control codes as spaces.
std::string terminal_line_utf8(const std::string& codes);
bool terminal_has_selection(const TerminalState& t);   // something (not an empty range) selected
// Columns [*c0, *c1) of line index i inside the selection; false when none of it is.
bool terminal_sel_span(const TerminalState& t, int i, int* c0, int* c1);
// Text for the clipboard as UTF-8: the selection, or the whole text when nothing is selected;
// spaces at line ends and empty lines at the end are left out.
std::string terminal_copy_text(const TerminalState& t);
// Selects the word (non space characters) or the whole line at line index i, column c.
void terminal_select_word(TerminalState& t, int i, int c);
void terminal_select_line(TerminalState& t, int i);
void terminal_clear(TerminalState& t);
