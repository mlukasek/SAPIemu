#include "frontend/hostio.h"

#include <SDL3/SDL.h>
#include "core/bus.h"
#include "core/log.h"
#include "core/machine.h"

// Latency of the sound: what is queued is kept around TARGET_MS; past HIGH_MS (a stall of
// the main loop) the oldest is dropped; after running dry the device waits for START_MS.
static const int TARGET_MS = 60;
static const int HIGH_MS = 150;
static const int START_MS = 40;

void audio_close(HostAudio& a)
{
    if (a.stream) SDL_DestroyAudioStream(a.stream);
    a.stream = nullptr;
    a.rate = 0;
    a.waiting = true;
}

void audio_frame(HostAudio& a, Machine* m, bool listen)
{
    Bus* b = m ? &m->bus : nullptr;
    if (!b || !b->audio_rate || !listen || !a.enabled || a.failed) {
        if (b) {
            b->audio_cap = 0;
            b->audio.clear();
        }
        if (a.stream && !a.waiting) {
            SDL_PauseAudioStreamDevice(a.stream);
            SDL_ClearAudioStream(a.stream);
            a.waiting = true;
        }
        return;
    }
    if (a.stream && a.rate != b->audio_rate) audio_close(a);
    if (!a.stream) {
        if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            LOGW("sound: %s", SDL_GetError());
            a.failed = true;
            return;
        }
        SDL_AudioSpec spec;
        spec.format = SDL_AUDIO_S16;
        spec.channels = 1;
        spec.freq = (int)b->audio_rate;
        a.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!a.stream) {
            LOGW("sound: %s", SDL_GetError());
            a.failed = true;
            return;
        }
        a.rate = b->audio_rate;
        a.waiting = true;   // the device of a new stream starts paused
        b->audio.clear();
    }
    b->audio_cap = a.rate / 2;
    SDL_SetAudioStreamGain(a.stream, (float)a.volume / 100.0f);

    size_t n = b->audio.size();
    const int16_t* p = b->audio.data();
    size_t queued = (size_t)SDL_GetAudioStreamQueued(a.stream) / 2;
    size_t target = (size_t)a.rate * TARGET_MS / 1000, high = (size_t)a.rate * HIGH_MS / 1000;
    if (queued + n > high) {   // behind: keep only the newest, back to the target latency
        size_t keep = target > queued ? target - queued : 0;
        if (n > keep) {
            p += n - keep;
            n = keep;
        }
    }
    if (n) SDL_PutAudioStreamData(a.stream, p, (int)(n * 2));
    b->audio.clear();
    queued = (size_t)SDL_GetAudioStreamQueued(a.stream) / 2;
    if (a.waiting && queued >= (size_t)a.rate * START_MS / 1000) {
        SDL_ResumeAudioStreamDevice(a.stream);
        a.waiting = false;
    } else if (!a.waiting && queued == 0) {   // ran dry: collect some again
        SDL_PauseAudioStreamDevice(a.stream);
        a.waiting = true;
    }
}

static void open_first_gamepad(HostJoystick& j)
{
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids && count > 0) {
        j.pad = SDL_OpenGamepad(ids[0]);
        if (j.pad) LOGI("gamepad: %s", SDL_GetGamepadName(j.pad));
    }
    if (ids) SDL_free(ids);
}

void joystick_event(HostJoystick& j, const SDL_Event& ev)
{
    if (ev.type == SDL_EVENT_GAMEPAD_ADDED && !j.pad) {
        j.pad = SDL_OpenGamepad(ev.gdevice.which);
        if (j.pad) LOGI("gamepad: %s", SDL_GetGamepadName(j.pad));
    } else if (ev.type == SDL_EVENT_GAMEPAD_REMOVED && j.pad && SDL_GetGamepadID(j.pad) == ev.gdevice.which) {
        SDL_CloseGamepad(j.pad);
        j.pad = nullptr;
        open_first_gamepad(j);
    }
}

bool joystick_key(const HostJoystick& j, int key)
{
    return j.keyboard && (key == SDLK_UP || key == SDLK_DOWN || key == SDLK_LEFT || key == SDLK_RIGHT ||
                          key == SDLK_SPACE);
}

uint8_t joystick_state(const HostJoystick& j, bool keys)
{
    uint8_t s = 0;
    if (j.pad) {
        const int dead = 16000;
        int x = SDL_GetGamepadAxis(j.pad, SDL_GAMEPAD_AXIS_LEFTX);
        int y = SDL_GetGamepadAxis(j.pad, SDL_GAMEPAD_AXIS_LEFTY);
        if (SDL_GetGamepadButton(j.pad, SDL_GAMEPAD_BUTTON_DPAD_UP) || y < -dead) s |= JOY_UP;
        if (SDL_GetGamepadButton(j.pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || y > dead) s |= JOY_DOWN;
        if (SDL_GetGamepadButton(j.pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || x < -dead) s |= JOY_LEFT;
        if (SDL_GetGamepadButton(j.pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || x > dead) s |= JOY_RIGHT;
        if (SDL_GetGamepadButton(j.pad, SDL_GAMEPAD_BUTTON_SOUTH) || SDL_GetGamepadButton(j.pad, SDL_GAMEPAD_BUTTON_WEST))
            s |= JOY_FIRE1;
        if (SDL_GetGamepadButton(j.pad, SDL_GAMEPAD_BUTTON_EAST) || SDL_GetGamepadButton(j.pad, SDL_GAMEPAD_BUTTON_NORTH))
            s |= JOY_FIRE2;
    }
    if (keys && j.keyboard) {
        const bool* k = SDL_GetKeyboardState(nullptr);
        if (k[SDL_SCANCODE_UP]) s |= JOY_UP;
        if (k[SDL_SCANCODE_DOWN]) s |= JOY_DOWN;
        if (k[SDL_SCANCODE_LEFT]) s |= JOY_LEFT;
        if (k[SDL_SCANCODE_RIGHT]) s |= JOY_RIGHT;
        if (k[SDL_SCANCODE_SPACE]) s |= JOY_FIRE1;
    }
    return s;
}

std::string joystick_name(const HostJoystick& j)
{
    const char* n = j.pad ? SDL_GetGamepadName(j.pad) : nullptr;
    return n ? n : (j.pad ? "gamepad" : "");
}

void joystick_close(HostJoystick& j)
{
    if (j.pad) SDL_CloseGamepad(j.pad);
    j.pad = nullptr;
}
