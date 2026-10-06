#include "input/ank1.h"
#include "core/state.h"
#include "core/log.h"

#include <ctype.h>

// Key index = row * 8 + (7 - column bit); row 0 is selected by P0-OUT D4.
// Legends from appendix VII (ank-1pri07.png) and the MIKOS V4.0 tables.
static const char* const k_keys[40] = {
    "0", "P", "CR", "LF", "1", "Q", "A", "SH",
    "9", "O", "L", "SP", "2", "W", "S", "Z",
    "8", "I", "K", "M", "3", "E", "D", "X",
    "7", "U", "J", "N", "4", "R", "F", "C",
    "6", "Y", "H", "B", "5", "T", "G", "V",
};

// Characters produced with SHIFT. Printed legends, the same in MIKROBASIC 2.4
// (table at 0E22h) and MIKOS V4.0; MIKOS adds control codes on some letters.
struct ShiftChar { uint8_t ch; char key; };
static const ShiftChar k_shift[] = {
    { '!', '1' }, { '?', '2' }, { '"', '3' }, { '$', '4' }, { '^', '5' }, { '\'', '6' },
    { '<', '7' }, { '>', '8' }, { '(', '9' }, { ')', '0' },
    { '/', 'Y' }, { ':', 'U' }, { '@', 'I' }, { '#', 'O' }, { 0x7F, 'P' },
    { '+', 'H' }, { '-', 'J' }, { '*', 'K' }, { '=', 'L' }, { ';', 'B' }, { ',', 'N' }, { '.', 'M' },
    { '_', 'G' }, { '%', 'R' }, { '&', 'D' }, { '\\', 'F' }, { '[', 'C' }, { ']', 'V' },
};
// MIKOS V4.0 only: SHIFT + letter = control code
static const ShiftChar k_shift_mikos[] = {
    { 0x08, 'T' }, { 0x11, 'Q' }, { 0x17, 'W' }, { 0x10, 'A' }, { 0x13, 'S' }, { 0x03, 'X' },
};

static int key_index(const char* legend)
{
    for (int i = 0; i < 40; i++) {
        const char* k = k_keys[i];
        int j = 0;
        while (k[j] && legend[j] && k[j] == legend[j]) j++;
        if (!k[j] && !legend[j]) return i;
    }
    return -1;
}

static int char_key(char c)
{
    char s[2] = { c, 0 };
    return key_index(s);
}

void Ank1::type_char(uint32_t cp)
{
    if (cp >= 0x80) return;
    uint8_t c = (uint8_t)cp;
    bool mikos = software == SW_MIKOS;
    if (c == '\r') { press(key_index("CR"), false); return; }
    if (mikos) {
        if (c == '\n') { press(key_index("LF"), true); return; }   // shift LF = 0Ah in MIKOS
        if (c == 0x1B) { press(key_index("LF"), false); return; }  // LF key = ESC in MIKOS
        if (c == '\t') { press(key_index("SP"), true); return; }
        for (const ShiftChar& s : k_shift_mikos)
            if (s.ch == c) { press(char_key(s.key), true); return; }
    } else {
        if (c == 0x08) { press(key_index("LF"), false); return; }  // the key next to CR = BS in MIKROBASIC
    }
    if (c == ' ') { press(key_index("SP"), false); return; }
    if (isalnum(c)) { press(char_key((char)toupper(c)), false); return; }   // upper case only
    for (const ShiftChar& s : k_shift)
        if (s.ch == c) { press(char_key(s.key), true); return; }
}

bool Ank1::host_key(int key, int mods)
{
    bool mikos = software == SW_MIKOS;
    switch (key) {
    case HK_ENTER: case HK_KP_ENTER: press(key_index("CR"), false); return true;
    case HK_ESCAPE:
        if (!mikos) return false;
        press(key_index("LF"), false);   // LF key = ESC in MIKOS
        return true;
    case HK_BACKSPACE:
        if (mikos) press(char_key('T'), true);   // shift T = 08h
        else press(key_index("LF"), false);      // BS key in MIKROBASIC
        return true;
    case HK_TAB:
        if (!mikos) return false;
        press(key_index("SP"), true);
        return true;
    case HK_DELETE: press(char_key('P'), true); return true;      // shift P = DEL
    case HK_F6: int_key = true; int_until = now + hold_cycles; return true;
    case HK_F7: t_key = true; t_until = now + hold_cycles; return true;
    default: return false;
    }
}

bool Ank1::key_down(int key) const
{
    if (cur_key < 0 || releasing) return false;
    return key == cur_key || (cur_shift && key == SHIFT_KEY);
}

Ank1::~Ank1()
{
    if (bus) bus->mouse_connected &= (uint8_t)~MOUSE_PORT_KEYBOARD;   // unplugged with the keyboard
}

bool Ank1::set_joyport(int dev)
{
    if (!m2) return false;
    joy_dev = dev;
    update_mouse_flag();
    return true;
}

void Ank1::update_mouse_flag(void)
{
    if (!bus) return;
    if (m2 && joy_device_is_mouse(joy_dev)) bus->mouse_connected |= MOUSE_PORT_KEYBOARD;
    else bus->mouse_connected &= (uint8_t)~MOUSE_PORT_KEYBOARD;
}

void Ank1::advance(uint64_t t)
{
    beeper.advance(bus);
    now = t;
    if (m2 && bus) {
        update_mouse_flag();   // the bus is known only after the keyboard was made
        if (joy_device_is_mouse(joy_dev)) {
            if (now > mouse_next + 1000 * mouse_period) mouse_next = now;   // long pause: no catching up
            while (now >= mouse_next) {
                mouse.step(bus);
                mouse_next += mouse_period;
            }
        }
    }
    if (int_key && now >= int_until) int_key = false;
    if (t_key && now >= t_until) t_key = false;
    // each phase lasts its minimum time and until the program has scanned the key's row
    if (cur_key >= 0 && now >= phase_end && scanned) {
        scanned = false;
        if (!releasing) {
            releasing = true;            // key up, keep a pause for the debouncing
            phase_end = now + release_cycles;
        } else {
            cur_key = -1;
            releasing = false;
        }
    }
    if (cur_key < 0 && !queue.empty()) {
        Press p = queue.front();
        queue.pop_front();
        if (p.key < 0) return;
        cur_key = p.key;
        cur_shift = p.shift;
        releasing = false;
        scanned = false;
        phase_end = now + press_cycles;
    }
}

uint8_t Ank1::p0_in()
{
    uint8_t cols = 0xFF;
    uint8_t rows = (uint8_t)(~out & 0x1F);   // active rows (0 on the output)
    if (cur_key >= 0 && (rows & (0x10 >> (cur_key / 8)))) scanned = true;
    for (int r = 0; r < 5; r++) {
        if (!(rows & (0x10 >> r))) continue;
        for (int bit = 0; bit < 8; bit++)
            if (key_down(r * 8 + 7 - bit)) cols &= (uint8_t)~(1 << bit);
    }
    return cols;
}

uint8_t Ank1::p1_in()
{
    uint8_t v = 0xFF;
    if (int_key) v &= 0x7F;
    if (t_key) v &= 0xBF;
    if (m2 && bus) v &= (uint8_t)(0xC0 | joy_port_read(joy_dev, 0, mouse, bus));   // the joystick connector
    return v;
}

void Ank1::p0_out(uint8_t value)
{
    beeper.set(bus, (value & 0x40) != 0);
    out = value;
}

std::string Ank1::status() const
{
    std::string s;
    if (out & 0x20) s += "SHIFT";
    if (out & 0x80) s += s.empty() ? "ERR" : " ERR";
    return s;
}

void Ank1::state(StateIO& io)
{
    uint32_t n = (uint32_t)queue.size();
    io.v(n);
    if (io.loading) queue.assign(io.ok() ? n : 0, Press{ 0, false });
    for (Press& p : queue) {
        io.e(p.key);
        io.v(p.shift);
    }
    io.e(cur_key);
    io.v(scanned);
    io.v(cur_shift);
    io.v(releasing);
    io.v(phase_end);
    io.v(now);
    io.v(out);
    io.v(int_key);
    io.v(t_key);
    io.v(int_until);
    io.v(t_until);
    beeper.state(io);
    if (m2) {   // the type is in the machine description: ANK-1 states stay as they were
        io.e(joy_dev);
        io.v(mouse.mx);
        io.v(mouse.my);
        io.v(mouse_next);
        if (io.loading) update_mouse_flag();
    }
}
