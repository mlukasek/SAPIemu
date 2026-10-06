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
    // copy: everything without trailing spaces and empty lines, or the selection
    {
        TerminalState t;
        t.cr_newline = true;
        terminal_feed(t, "first line  \rsecond\r\rthird word\r\r");
        CHECK(!terminal_has_selection(t));
        CHECK(terminal_copy_text(t) == "first line\nsecond\n\nthird word");
        // from column 6 of line 0 to column 3 of line 3, dragged backwards
        t.sel = true;
        t.sel_line0 = 3; t.sel_col0 = 3;
        t.sel_line1 = 0; t.sel_col1 = 6;
        CHECK(terminal_has_selection(t));
        CHECK(terminal_copy_text(t) == "line\nsecond\n\nthi");
        int c0 = -1, c1 = -1;
        CHECK(terminal_sel_span(t, 0, &c0, &c1) && c0 == 6 && c1 == t.cols);
        CHECK(terminal_sel_span(t, 1, &c0, &c1) && c0 == 0 && c1 == t.cols);
        CHECK(terminal_sel_span(t, 3, &c0, &c1) && c0 == 0 && c1 == 3);
        CHECK(!terminal_sel_span(t, 4, &c0, &c1));
        t.sel_line1 = 3; t.sel_col1 = 3;   // a click without dragging selects nothing
        CHECK(!terminal_has_selection(t));
        CHECK(terminal_copy_text(t) == "first line\nsecond\n\nthird word");
        terminal_select_word(t, 3, 8);
        CHECK(terminal_copy_text(t) == "word");
        terminal_select_word(t, 3, 5);     // a space: nothing
        CHECK(!terminal_has_selection(t));
        terminal_select_line(t, 1);
        CHECK(terminal_copy_text(t) == "second");
        terminal_clear(t);
        CHECK(!terminal_has_selection(t) && terminal_copy_text(t).empty());
    }
    // the selection stays on its text when the scrollback drops lines from the top
    {
        TerminalState t;
        t.cr_newline = true;
        for (int i = 0; i < 3000; i++) terminal_feed(t, i == 2999 ? "keep me\r" : "x\r");
        int i = (int)t.lines.size() - 2;
        CHECK(t.lines[i] == "keep me");
        terminal_select_word(t, i, 0);
        terminal_feed(t, "more\rand more\r");
        CHECK(t.dropped > 0);
        CHECK(terminal_copy_text(t) == "keep");
    }
    // control codes copy as spaces
    {
        CHECK(terminal_line_utf8("A\x01") == "A ");
    }
    return TEST_RESULT();
}
