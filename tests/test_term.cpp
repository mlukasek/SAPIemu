// Serial terminal model: line endings (CR CR CR LF from ECMON), SAPI console
// codes, ANSI subset, raw mode.
#include "core/term.h"
#include "test_util.h"

static std::string screen_line(const TerminalState& t, int r)
{
    int i = terminal_top(t) + r;
    return i < (int)t.lines.size() ? t.lines[i] : std::string();
}

int main(void)
{
    // ECMON start: 1D 1F (home, clear to end of screen), lines end with CR CR CR LF
    {
        TerminalState t;
        terminal_feed(t, std::string("\x1d\x1fJPR-1Z 62K MON\r\r\r\nUzivatel: Martin\r\r\r\n\r\r\n0 - A", 49));
        CHECK(t.lines[0] == "JPR-1Z 62K MON");
        CHECK(t.lines[1] == "Uzivatel: Martin");
        CHECK(t.lines[2].empty());
        CHECK(t.lines[3] == "0 - A");
        CHECK_EQ(t.row, 3);
        CHECK_EQ((int)t.lines.size(), 24);   // cursor addressing (home) makes a full 24-line screen
    }
    // CR alone returns to column 0 and overwrites
    {
        TerminalState t;
        terminal_feed(t, "ABCDEF\rxy");
        CHECK(t.lines[0] == "xyCDEF");
        t.cr_newline = true;
        terminal_feed(t, "\r\nnext");
        CHECK(t.lines[1] == "next");
        CHECK_EQ((int)t.lines.size(), 2);
    }
    // SAPI: ESC row+20h col+20h, clear to end of line, home
    {
        TerminalState t;
        terminal_feed(t, "line0\r\nline1\r\nline2");
        terminal_feed(t, std::string("\x1b") + (char)(0x20 + 1) + (char)(0x20 + 2) + "X");
        CHECK(screen_line(t, 1) == "liXe1");
        terminal_feed(t, "\x1e");   // clear to end of line after X
        CHECK(screen_line(t, 1) == "liX");
        terminal_feed(t, "\x1dH");  // home
        CHECK(screen_line(t, 0) == "Hine0");
        terminal_feed(t, "\x08\x08Z");   // backspace stops at column 0
        CHECK(screen_line(t, 0) == "Zine0");
    }
    // ANSI: cursor position, erase line
    {
        TerminalState t;
        t.emulation = TERM_ANSI;
        terminal_feed(t, "hello\r\nworld\x1b[1;2HE\x1b[K");
        CHECK(screen_line(t, 0) == "hE");
        CHECK(screen_line(t, 1) == "world");
        terminal_feed(t, "\x1b[31mred\x1b[0m");   // attributes are ignored
        CHECK(screen_line(t, 0) == "hEred");
    }
    // raw: control characters are visible
    {
        TerminalState t;
        t.emulation = TERM_RAW;
        terminal_feed(t, "\x1d\x1f" "A");
        CHECK(t.lines[0] == "^]^_A");
    }
    // auto wrap at the right margin
    {
        TerminalState t;
        t.cols = 4;
        terminal_feed(t, "abcdef");
        CHECK(t.lines[0] == "abcd");
        CHECK(t.lines[1] == "ef");
    }
    return TEST_RESULT();
}
