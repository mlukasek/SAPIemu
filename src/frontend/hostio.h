#pragma once
// Sound output, game controllers and the mouse of the host (SDL3) for the sound and joystick cards.
#include <stdint.h>
#include <string>

class Machine;
struct SDL_AudioStream;
struct SDL_Gamepad;
struct SDL_Window;
union SDL_Event;
struct SDL_KeyboardEvent;

struct HostAudio {
    SDL_AudioStream* stream = nullptr;
    uint32_t rate = 0;          // sample rate of the stream (the card's)
    bool waiting = true;        // device paused until enough sound is queued
    bool failed = false;        // no audio device: do not try again
    bool enabled = true;        // settings: sound on
    int volume = 80;            // percent
};

// After each emulated frame: moves the sound of the machine to the device. `listen` = the
// emulation runs in real time (100 %, not paused); otherwise nothing is played.
void audio_frame(HostAudio& a, Machine* m, bool listen);
void audio_close(HostAudio& a);

struct HostJoystick {
    SDL_Gamepad* pad[2] = { nullptr, nullptr };   // first and second joystick of the machine
    bool keyboard = false;      // settings: arrows and space are the (first) joystick
    uint8_t last[2] = { 0, 0 }; // state last given to the machine (MCP may set it in between)
};

void joystick_event(HostJoystick& j, const SDL_Event& ev);   // gamepad added / removed
// A key the joystick takes from the keyboard (not typed on the SAPI keyboard).
bool joystick_key(const HostJoystick& j, int sdl_key);
// HostJoy bits of gamepad n and, for n = 0 with `keys`, of the keyboard.
uint8_t joystick_state(const HostJoystick& j, int n, bool keys);
std::string joystick_name(const HostJoystick& j, int n);   // empty = no gamepad
void joystick_close(HostJoystick& j);

// The host mouse for the mouse ports: when enabled, captured (relative mode, cursor hidden) by a
// click into a display window, released by the middle button, Ctrl+F10 or losing the focus.
struct HostMouse {
    bool enabled = false;        // settings: the host mouse is the mouse of the machine
    bool captured = false;
    bool gui_captured = false;   // the state the GUI was last told about (its buttons are released then)
    bool click = false;          // a display window was clicked in this frame
    float sensitivity = 1.0f;    // settings: mouse counts per host pixel
    float fx = 0, fy = 0;        // fractions of counts not sent yet
    uint8_t buttons = 0;         // MouseButton bits while captured
    uint8_t last = 0;            // buttons last given to the machine
};

void mouse_capture(HostMouse& ms, SDL_Window* w, bool on);
// Mouse events while captured go to the machine, not to the GUI: true = consumed.
bool mouse_event(HostMouse& ms, SDL_Window* w, Machine* m, const SDL_Event& ev);
// Once per frame: capture after a click, release when the machine has no mouse, the buttons.
void mouse_frame(HostMouse& ms, SDL_Window* w, Machine* m);
