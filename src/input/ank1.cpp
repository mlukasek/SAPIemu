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

static int legend_key(char c)
{
    char s[2] = { c, 0 };
    return key_index(s);
}

static uint8_t row_bit(int key)   // the row of a matrix key on P0-OUT D4..D0
{
    return (uint8_t)(0x10 >> (key / 8));
}

bool Ank1::char_key(uint32_t cp, int& key, bool& shift) const
{
    key = -1;
    shift = false;
    if (cp >= 0x80) return false;
    uint8_t c = (uint8_t)cp;
    bool mikos = software == SW_MIKOS;
    if (c == '\r') key = key_index("CR");
    else if (mikos && c == '\n') { key = key_index("LF"); shift = true; }   // shift LF = 0Ah in MIKOS
    else if (mikos && c == 0x1B) key = key_index("LF");                     // LF key = ESC in MIKOS
    else if (mikos && c == '\t') { key = key_index("SP"); shift = true; }
    else if (!mikos && c == 0x08) key = key_index("LF");   // the key next to CR = BS in MIKROBASIC
    else if (c == ' ') key = key_index("SP");
    else if (isalnum(c)) key = legend_key((char)toupper(c));   // upper case only
    else {
        if (mikos)
            for (const ShiftChar& s : k_shift_mikos)
                if (s.ch == c) { key = legend_key(s.key); shift = true; }
        if (key < 0)
            for (const ShiftChar& s : k_shift)
                if (s.ch == c) { key = legend_key(s.key); shift = true; break; }
    }
    return key >= 0;
}

bool Ank1::special_key(int hk, int& key, bool& shift) const
{
    bool mikos = software == SW_MIKOS;
    key = -1;
    shift = false;
    switch (hk) {
    case HK_ENTER: case HK_KP_ENTER: key = key_index("CR"); break;
    case HK_ESCAPE: if (mikos) key = key_index("LF"); break;   // LF key = ESC in MIKOS
    case HK_BACKSPACE:
        if (mikos) { key = legend_key('T'); shift = true; }   // shift T = 08h
        else key = key_index("LF");                           // BS key in MIKROBASIC
        break;
    case HK_TAB: if (mikos) { key = key_index("SP"); shift = true; } break;
    case HK_DELETE: key = legend_key('P'); shift = true; break;   // shift P = DEL
    default: break;
    }
    return key >= 0;
}

void Ank1::type_char(uint32_t cp)
{
    int key;
    bool shift;
    if (char_key(cp, key, shift)) press(key, shift);
}

bool Ank1::host_key(int hk, int mods)
{
    if (hk == HK_F6) { int_key = true; int_until = now + hold_cycles; return true; }
    if (hk == HK_F7) { t_key = true; t_until = now + hold_cycles; return true; }
    int key;
    bool shift;
    if (!special_key(hk, key, shift)) return false;
    press(key, shift);
    return true;
}

bool Ank1::hold_key(int id, uint32_t cp, int hk, bool raw)
{
    int key = -1;
    bool shift = false;
    if (hk == HK_SHIFT) {
        if (!raw) return false;   // typing: the PC Shift only chooses the character
        key = SHIFT_KEY;
    } else if (hk != HK_NONE) {
        if (!special_key(hk, key, shift)) return false;
    } else if (raw) {   // the game mode: the key itself, SHIFT only from the Shift key
        if (cp < 0x80 && isalnum((int)cp)) key = legend_key((char)toupper((int)cp));
        else if (cp == ' ') key = key_index("SP");
        else if (cp == '\r') key = key_index("CR");
        if (key < 0) return false;
    } else if (!char_key(cp, key, shift)) {
        return false;
    }
    Held n = { id, key, shift, raw, raw, false, false, false, false, false, false, false, false, now };
    for (Held& h : held)
        if (h.id == id && h.raw && raw) {   // the game mode: pressed again before its release was seen
            h = n;
            return true;
        }
    if (held.size() >= 16) return false;
    held.push_back(n);   // typed: also a quick second tap of the same key, after the first one
    return true;
}

void Ank1::release_key(int id)
{
    for (Held& h : held)
        if (h.id == id) h.up = true;   // (the typed key waiting for its turn still goes down then)
}

void Ank1::release_all()
{
    for (Held& h : held) h.up = true;
}

// The program reads rows `rows` (1 = active): what it has seen of the held keys.
void Ank1::held_scan(uint8_t rows)
{
    for (Held& h : held) {
        if (!h.started) continue;
        uint8_t kr = row_bit(h.key), sr = row_bit(SHIFT_KEY);
        if (h.shift && !h.key_up && (rows & sr)) h.shift_seen = true;
        if (held_active(h) && (rows & kr)) h.seen = true;
        if (h.key_up && !h.shift_up && (rows & kr)) h.seen_up = true;
        if (h.shift_up && (rows & kr)) h.free_key = true;
        if (h.shift_up && (rows & sr)) h.free_shift = true;
    }
}

// Typed keys one after another: the next one when the previous one is gone.
void Ank1::held_start(void)
{
    for (const Held& h : held)
        if (!h.raw && h.started) return;
    for (Held& h : held)
        if (!h.raw && !h.started) {
            h.started = true;
            h.since = now;
            return;
        }
}

void Ank1::held_advance(void)
{
    const uint64_t give_up = 50 * min_hold;   // a program that does not read the row: 1 s
    for (size_t i = 0; i < held.size();) {
        Held& h = held[i];
        if (!h.started) {
            i++;
            continue;
        }
        // a program that does not read the SHIFT row gets the key after min_hold anyway
        if (h.shift && !h.shift_seen && now >= h.since + min_hold) h.shift_seen = true;
        uint64_t hold = h.raw ? min_hold : press_cycles;
        if (h.up && !h.key_up && now >= h.since + hold && (h.seen || now >= h.since + give_up)) {
            h.key_up = true;   // read at least once: up
            h.since = now;
        }
        if (h.key_up && !h.shift_up && (!h.shift || h.seen_up || now >= h.since + give_up)) {
            h.shift_up = true;   // SHIFT after the program has seen the key up
        }
        bool gone = h.raw ? h.shift_up
                          : h.shift_up && now >= h.since + release_cycles &&
                                ((h.free_key && (!h.shift || h.free_shift)) || now >= h.since + give_up);
        if (gone) {
            held.erase(held.begin() + (long)i);
            continue;
        }
        i++;
    }
}

bool Ank1::key_down(int key) const
{
    for (const Held& h : held) {
        if (h.started && h.shift && !h.shift_up && key == SHIFT_KEY) return true;
        if (held_active(h) && key == h.key) return true;
    }
    return false;
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
    held_advance();
    // typed text: the next character as a tap when no typed key is held (PC keys first)
    bool typing = false;
    for (const Held& h : held)
        if (!h.raw) typing = true;
    while (!typing && !queue.empty()) {
        Press p = queue.front();
        queue.pop_front();
        if (p.key < 0) continue;
        held.push_back(Held{ -1, p.key, p.shift, false, false, true, false, false, false, false, false, false, false, now });
        typing = true;
    }
    held_start();
}

size_t Ank1::pending() const
{
    size_t n = queue.size();
    for (const Held& h : held)
        if (h.id < 0) n++;   // typed taps not finished yet
    return n;
}

uint8_t Ank1::p0_in()
{
    uint8_t cols = 0xFF;
    uint8_t rows = (uint8_t)(~out & 0x1F);   // active rows (0 on the output)
    held_scan(rows);
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
    lit |= value;
}

// The LEDs: lit at any time since the last look (a program toggling D5 with its sound,
// Manic Miner, makes the LED glow; sampled once a frame it would flicker at random).
std::string Ank1::status() const
{
    uint8_t v = out | lit;
    lit = 0;
    std::string s;
    if (v & 0x20) s += "LED SHIFT";
    if (v & 0x80) s += s.empty() ? "LED ERR" : " ERR";
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
    n = (uint32_t)held.size();
    io.v(n);
    if (io.loading) held.assign(io.ok() && n <= 16 ? n : 0, Held{});
    for (Held& h : held) {
        io.e(h.id);
        io.e(h.key);
        io.v(h.shift);
        io.v(h.raw);
        io.v(h.started);
        io.v(h.up);
        io.v(h.shift_seen);
        io.v(h.seen);
        io.v(h.key_up);
        io.v(h.seen_up);
        io.v(h.shift_up);
        io.v(h.free_key);
        io.v(h.free_shift);
        io.v(h.since);
        if (io.loading && h.id >= 0) h.up = true;   // a key held on the PC then is not held now
    }
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
