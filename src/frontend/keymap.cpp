#include "frontend/keymap.h"

// Codes from the EKL-1 firmware tables (KEYB_REPAIR.a51, EXTEND / SHIFT EXTEND).
int keymap_special(SDL_Keycode key, SDL_Keymod mod)
{
    bool shift = (mod & SDL_KMOD_SHIFT) != 0;
    bool ctrl = (mod & SDL_KMOD_CTRL) != 0;
    bool alt = (mod & SDL_KMOD_ALT) != 0;

    switch (key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:  return 0x0D;
    case SDLK_BACKSPACE: return 0x08;
    case SDLK_TAB:       return 0x09;
    case SDLK_ESCAPE:    return 0x1B;
    case SDLK_UP:        return shift ? 0x19 : 0x05;
    case SDLK_DOWN:      return shift ? 0x17 : 0x18;
    case SDLK_LEFT:      return shift ? 0x01 : 0x13;
    case SDLK_RIGHT:     return shift ? 0x06 : 0x04;
    case SDLK_HOME:      return shift ? 0x15 : 0x1E;
    case SDLK_END:       return shift ? 0x0F : 0x11;
    case SDLK_PAGEUP:    return shift ? -1 : 0x12;
    case SDLK_PAGEDOWN:  return shift ? -1 : 0x03;
    case SDLK_INSERT:    return shift ? -1 : 0x16;
    case SDLK_DELETE:    return shift ? 0x1A : 0x0B;
    default: break;
    }
    // Ctrl+letter -> control code. Ctrl+Alt is AltGr on Czech layouts and
    // produces text, so it is left to the text input event.
    if (ctrl && !alt) {
        if (key >= SDLK_A && key <= SDLK_Z) return (int)(key - SDLK_A + 1);
        if (key == SDLK_LEFTBRACKET) return 0x1B;
        if (key == SDLK_BACKSLASH) return 0x1C;
        if (key == SDLK_RIGHTBRACKET) return 0x1D;
    }
    return -1;
}
