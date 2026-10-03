#pragma once
// Translation of host key presses to SAPI keyboard codes (as produced by EKL-1).
// Printable characters come from SDL text input events; this handles special keys.
#include <SDL3/SDL.h>

// Returns the SAPI code for a key press, or -1 if the key is not special
// (or produces text through a text input event instead).
int keymap_special(SDL_Keycode key, SDL_Keymod mod);
