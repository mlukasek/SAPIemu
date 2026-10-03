#include "core/term.h"

#include <stdio.h>
#include <stdlib.h>

static const size_t MAX_LINES = 3000;

// ---------------------------------------------------------------- terminal model

int terminal_top(const TerminalState& t)
{
    int n = (int)t.lines.size();
    return n > t.rows ? n - t.rows : 0;
}

static void ensure_row(TerminalState& t)
{
    if (t.lines.empty()) t.lines.push_back(std::string());
    while (t.row >= (int)t.lines.size()) t.lines.push_back(std::string());
}

static void trim_scrollback(TerminalState& t)
{
    if (t.lines.size() <= MAX_LINES) return;
    size_t drop = t.lines.size() - MAX_LINES;
    t.lines.erase(t.lines.begin(), t.lines.begin() + drop);
    t.row -= (int)drop;
    if (t.row < 0) t.row = 0;
}

static void line_feed(TerminalState& t)
{
    t.row++;
    ensure_row(t);
    trim_scrollback(t);
}

static void put_char(TerminalState& t, uint8_t c)
{
    ensure_row(t);
    if (t.col >= t.cols) {   // auto wrap
        t.col = 0;
        line_feed(t);
    }
    std::string& l = t.lines[t.row];
    if ((int)l.size() <= t.col) l.append(t.col - l.size() + 1, ' ');
    l[t.col] = (char)c;
    t.col++;
}

static void goto_rc(TerminalState& t, int r, int c)
{
    int top = terminal_top(t);
    if (r < 0) r = 0;
    if (r >= t.rows) r = t.rows - 1;
    if (c < 0) c = 0;
    if (c >= t.cols) c = t.cols - 1;
    // the screen always has `rows` lines once the cursor is addressed
    while ((int)t.lines.size() < top + t.rows && (int)t.lines.size() < t.rows) t.lines.push_back(std::string());
    t.row = terminal_top(t) + r;
    t.col = c;
    ensure_row(t);
}

static void clear_eol(TerminalState& t)
{
    ensure_row(t);
    if ((int)t.lines[t.row].size() > t.col) t.lines[t.row].resize(t.col);
}

static void clear_eos(TerminalState& t)
{
    clear_eol(t);
    for (size_t i = t.row + 1; i < t.lines.size(); i++) t.lines[i].clear();
}

static void clear_screen(TerminalState& t)
{
    // keep the old screen in the scrollback, start a fresh one
    int n = (int)t.lines.size();
    for (int i = 0; i < t.rows; i++) t.lines.push_back(std::string());
    t.row = n;
    t.col = 0;
    trim_scrollback(t);
}

static void insert_line(TerminalState& t)
{
    ensure_row(t);
    t.lines.insert(t.lines.begin() + t.row, std::string());
    int bottom = terminal_top(t) + t.rows;
    if ((int)t.lines.size() > bottom && t.row < bottom) t.lines.erase(t.lines.begin() + bottom);
}

static void delete_line(TerminalState& t)
{
    ensure_row(t);
    t.lines.erase(t.lines.begin() + t.row);
    ensure_row(t);
}

static void ctrl_char(TerminalState& t, uint8_t b)
{
    char buf[4];
    snprintf(buf, sizeof(buf), "^%c", '@' + (b & 0x1F));
    put_char(t, (uint8_t)buf[0]);
    put_char(t, (uint8_t)buf[1]);
}

// common C0 controls
static bool basic_control(TerminalState& t, uint8_t b)
{
    switch (b) {
    case 0x0D: t.col = 0; if (t.cr_newline) line_feed(t); return true;
    case 0x0A: line_feed(t); return true;
    case 0x08: if (t.col > 0) t.col--; return true;
    case 0x09: t.col = (t.col + 8) & ~7; if (t.col >= t.cols) t.col = t.cols - 1; return true;
    case 0x07: return true;   // bell
    case 0x00: return true;   // padding
    default: return false;
    }
}

// ECMON / AND-1Z console codes (from the ECMON screen driver)
static void feed_sapi(TerminalState& t, uint8_t b)
{
    if (t.esc) {
        // ESC row+20h col+20h
        t.esc_buf += (char)b;
        if (--t.esc == 0) goto_rc(t, (uint8_t)t.esc_buf[0] - 0x20, (uint8_t)t.esc_buf[1] - 0x20);
        return;
    }
    if (b >= 0x20) { put_char(t, b); return; }
    if (t.cr_newline && b == 0x0A) return;   // CR already made the new line
    if (basic_control(t, b)) return;
    switch (b) {
    case 0x1B: t.esc = 2; t.esc_buf.clear(); break;
    case 0x1D: goto_rc(t, 0, 0); break;                      // home
    case 0x1E: clear_eol(t); break;
    case 0x1F: clear_eos(t); break;
    case 0x0C: clear_screen(t); break;
    case 0x18: if (t.col < t.cols - 1) t.col++; break;        // right
    case 0x1A: if (t.row > terminal_top(t)) t.row--; break;   // up
    case 0x0B: if (t.row < (int)t.lines.size() - 1) t.row++; else line_feed(t); break;   // down
    case 0x02: insert_line(t); break;
    case 0x03: delete_line(t); break;
    default: break;                                           // 04, 05 ...: no visible effect
    }
}

// ANSI / VT100 subset
static void csi(TerminalState& t, char final)
{
    int p[8] = { 0 };
    int n = 0;
    const char* s = t.esc_buf.c_str();
    if (*s == '?') s++;
    while (*s && n < 8) {
        p[n++] = atoi(s);
        while (*s && *s != ';') s++;
        if (*s == ';') s++;
    }
    int top = terminal_top(t);
    int r = t.row - top, c = t.col;
    int a = (n > 0 && p[0] > 0) ? p[0] : 1;
    switch (final) {
    case 'H': case 'f': goto_rc(t, (n > 0 && p[0] > 0 ? p[0] : 1) - 1, (n > 1 && p[1] > 0 ? p[1] : 1) - 1); break;
    case 'A': goto_rc(t, r - a, c); break;
    case 'B': goto_rc(t, r + a, c); break;
    case 'C': goto_rc(t, r, c + a); break;
    case 'D': goto_rc(t, r, c - a); break;
    case 'J':
        if (p[0] == 2) { int keep_r = r, keep_c = c; clear_screen(t); goto_rc(t, keep_r, keep_c); }
        else clear_eos(t);
        break;
    case 'K': clear_eol(t); break;
    case 'L': insert_line(t); break;
    case 'M': delete_line(t); break;
    default: break;   // m (attributes), h/l (modes)... ignored
    }
}

static void feed_ansi(TerminalState& t, uint8_t b)
{
    if (t.esc == 1) {
        if (b == '[') { t.esc = 2; t.esc_buf.clear(); }
        else t.esc = 0;   // other ESC sequences ignored
        return;
    }
    if (t.esc == 2) {
        if (b >= 0x40 && b <= 0x7E) { t.esc = 0; csi(t, (char)b); }
        else if (t.esc_buf.size() < 32) t.esc_buf += (char)b;
        return;
    }
    if (b == 0x1B) { t.esc = 1; return; }
    if (b >= 0x20) { put_char(t, b); return; }
    if (t.cr_newline && b == 0x0A) return;
    if (basic_control(t, b)) return;
    if (b == 0x0C) clear_screen(t);
}

static void feed_raw(TerminalState& t, uint8_t b)
{
    if (b >= 0x20) { put_char(t, b); return; }
    if (t.cr_newline && b == 0x0A) return;
    if (b == 0x0D || b == 0x0A) { basic_control(t, b); return; }
    ctrl_char(t, b);
}

void terminal_feed(TerminalState& t, const std::string& data)
{
    for (char c : data) {
        uint8_t b = (uint8_t)c;
        switch (t.emulation) {
        case TERM_ANSI: feed_ansi(t, b); break;
        case TERM_RAW: feed_raw(t, b); break;
        default: feed_sapi(t, b); break;
        }
    }
    ensure_row(t);
    t.scroll_to_bottom = true;
}

