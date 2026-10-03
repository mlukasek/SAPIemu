#include "input/consul.h"

// ---------------------------------------------------------------- Consul 259.11

void Consul25911::type_char(uint32_t cp)
{
    if (koi8) {
        bool ok;
        uint8_t c = koi8cs_keyboard_code(cp, ok);
        if (ok) push_code(c);
        return;
    }
    if (cp < 0x80) push_code((uint8_t)cp);   // KOI-7: ASCII (24h is printed as ¤)
}

// Keys from the 259.11 code table: blue keys DC1/DC2/HT/VT/ESC/FF/CR, black
// keys DEL, <- (BS), down (LF); CLEAR is a separate signal on P1-IN7.
bool Consul25911::host_key(int key, int mods)
{
    switch (key) {
    case HK_ENTER: case HK_KP_ENTER: push_code(0x0D); return true;
    case HK_BACKSPACE: case HK_LEFT: push_code(0x08); return true;
    case HK_TAB: push_code(0x09); return true;
    case HK_ESCAPE: push_code(0x1B); return true;
    case HK_DELETE: push_code(0x7F); return true;
    case HK_DOWN: push_code(0x0A); return true;
    case HK_PGUP: push_code(0x0B); return true;    // VT
    case HK_PGDN: push_code(0x0C); return true;    // FF
    case HK_F1: push_code(0x11); return true;      // DC1
    case HK_F2: push_code(0x12); return true;      // DC2
    case HK_PAUSE:
        if (koi8) return false;                    // P1-IN7 carries BIT8 in this variant
        press_clear();
        return true;
    default: return false;
    }
}

// ---------------------------------------------------------------- Consul 262.3

void Consul2623::type_char(uint32_t cp)
{
    if (cp < 0x80) push_code((uint8_t)cp);   // LAT version: ASCII, CTRL register = letter & 1Fh
}

// Code table of the 262.3 LAT keyboard (manual 864.795): cursor keys C1-C4,
// keypad B0-B9/AE, ROL/COPY/BREAK 80-85, blank keys 61-63 = D0-D2 (used by
// Monitor V5.0/F as háček, čárka and CS lock), key 79 (arrow to the top left) = 8D.
bool Consul2623::host_key(int key, int mods)
{
    bool shift = (mods & HM_SHIFT) != 0;
    if (key >= HK_KP0 && key <= HK_KP9) {
        if (key == HK_KP0 && shift) push_code(0x80);
        else push_code((uint8_t)(0xB0 + (key - HK_KP0)));
        return true;
    }
    switch (key) {
    case HK_ENTER:
        push_code((mods & HM_CTRL) ? 0x0A : 0x0D);   // Ctrl+Enter = LF key
        return true;
    case HK_KP_ENTER: push_code(0x0D); return true;
    case HK_BACKSPACE: push_code(0x08); return true;
    case HK_TAB: push_code(0x09); return true;
    case HK_ESCAPE: push_code(0x1B); return true;
    case HK_DELETE: push_code(0x7F); return true;
    case HK_UP: push_code(0xC1); return true;
    case HK_DOWN: push_code(0xC2); return true;
    case HK_RIGHT: push_code(0xC3); return true;
    case HK_LEFT: push_code(0xC4); return true;
    case HK_HOME: push_code(0x8D); return true;
    case HK_KP_DOT: push_code(0xAE); return true;
    case HK_F1: push_code(shift ? 0x81 : 0x80); return true;   // ROL
    case HK_F2: push_code(shift ? 0x83 : 0x82); return true;   // COPY
    case HK_F3: push_code(shift ? 0x85 : 0x84); return true;   // BREAK
    case HK_F4: push_code(0xD0); return true;                  // key 61
    case HK_F6: push_code(0xD1); return true;                  // key 62
    case HK_F7: push_code(0xD2); return true;                  // key 63
    default: return false;
    }
}
