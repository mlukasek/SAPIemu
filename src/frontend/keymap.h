#pragma once
// Translation of host key presses for the SAPI keyboards. Printable characters
// come from SDL text input events; this handles keys without text.
#include <SDL3/SDL.h>

// HostKey (core/devices.h) of a key, HK_NONE if it is not one of them.
int keymap_host_key(SDL_Keycode key);
// HostMod flags of the modifier state.
int keymap_mods(SDL_Keymod mod);
// Ctrl+letter control code, -1 if none (Ctrl+Alt = AltGr produces text).
int keymap_ctrl_code(SDL_Keycode key, SDL_Keymod mod);
// Character the key also produces as a text input event ('0'..'9', '.' or ',' on
// the numeric keypad with NumLock on), 0 if none.
char keymap_keypad_text(SDL_Keycode key, SDL_Keymod mod);
