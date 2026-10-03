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
    int row = 0, col = 0;              // cursor, row is an index into lines
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
