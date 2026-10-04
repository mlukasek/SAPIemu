#pragma once
// Sound output and game controllers of the host (SDL3) for the sound and joystick cards.
#include <stdint.h>
#include <string>

class Machine;
struct SDL_AudioStream;
struct SDL_Gamepad;
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
    SDL_Gamepad* pad = nullptr;
    bool keyboard = false;      // settings: arrows and space are the joystick
    uint8_t last = 0;           // state last given to the machine (MCP may set it in between)
};

void joystick_event(HostJoystick& j, const SDL_Event& ev);   // gamepad added / removed
// A key the joystick takes from the keyboard (not typed on the SAPI keyboard).
bool joystick_key(const HostJoystick& j, int sdl_key);
// HostJoy bits of the gamepad and, with `keys`, of the keyboard.
uint8_t joystick_state(const HostJoystick& j, bool keys);
std::string joystick_name(const HostJoystick& j);   // empty = no gamepad
void joystick_close(HostJoystick& j);
