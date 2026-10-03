#include "frontend/keymap.h"
#include "core/devices.h"

int keymap_host_key(SDL_Keycode key)
{
    switch (key) {
    case SDLK_RETURN:    return HK_ENTER;
    case SDLK_BACKSPACE: return HK_BACKSPACE;
    case SDLK_TAB:       return HK_TAB;
    case SDLK_ESCAPE:    return HK_ESCAPE;
    case SDLK_UP:        return HK_UP;
    case SDLK_DOWN:      return HK_DOWN;
    case SDLK_LEFT:      return HK_LEFT;
    case SDLK_RIGHT:     return HK_RIGHT;
    case SDLK_HOME:      return HK_HOME;
    case SDLK_END:       return HK_END;
    case SDLK_PAGEUP:    return HK_PGUP;
    case SDLK_PAGEDOWN:  return HK_PGDN;
    case SDLK_INSERT:    return HK_INSERT;
    case SDLK_DELETE:    return HK_DELETE;
    case SDLK_PAUSE:     return HK_PAUSE;
    case SDLK_F1:        return HK_F1;
    case SDLK_F2:        return HK_F2;
    case SDLK_F3:        return HK_F3;
    case SDLK_F4:        return HK_F4;
    case SDLK_F6:        return HK_F6;
    case SDLK_F7:        return HK_F7;
    case SDLK_KP_0:      return HK_KP0;
    case SDLK_KP_1:      return HK_KP1;
    case SDLK_KP_2:      return HK_KP2;
    case SDLK_KP_3:      return HK_KP3;
    case SDLK_KP_4:      return HK_KP4;
    case SDLK_KP_5:      return HK_KP5;
    case SDLK_KP_6:      return HK_KP6;
    case SDLK_KP_7:      return HK_KP7;
    case SDLK_KP_8:      return HK_KP8;
    case SDLK_KP_9:      return HK_KP9;
    case SDLK_KP_PERIOD:
    case SDLK_KP_DECIMAL:
    case SDLK_KP_COMMA:  return HK_KP_DOT;
    case SDLK_KP_ENTER:  return HK_KP_ENTER;
    default:             return HK_NONE;
    }
}

int keymap_mods(SDL_Keymod mod)
{
    int m = 0;
    if (mod & SDL_KMOD_SHIFT) m |= HM_SHIFT;
    if (mod & SDL_KMOD_CTRL) m |= HM_CTRL;
    return m;
}

int keymap_ctrl_code(SDL_Keycode key, SDL_Keymod mod)
{
    if (!(mod & SDL_KMOD_CTRL) || (mod & SDL_KMOD_ALT)) return -1;
    if (key >= SDLK_A && key <= SDLK_Z) return (int)(key - SDLK_A + 1);
    if (key == SDLK_LEFTBRACKET) return 0x1B;
    if (key == SDLK_BACKSLASH) return 0x1C;
    if (key == SDLK_RIGHTBRACKET) return 0x1D;
    return -1;
}

char keymap_keypad_text(SDL_Keycode key, SDL_Keymod mod)
{
    if (!(mod & SDL_KMOD_NUM)) return 0;
    if (key >= SDLK_KP_1 && key <= SDLK_KP_9) return (char)('1' + (key - SDLK_KP_1));
    if (key == SDLK_KP_0) return '0';
    if (key == SDLK_KP_PERIOD || key == SDLK_KP_DECIMAL || key == SDLK_KP_COMMA) return '.';
    return 0;
}
