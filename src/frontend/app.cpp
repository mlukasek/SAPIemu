// SDL3 + Dear ImGui frontend: screen window, menus, keyboard routing, debugger and
// terminal windows, MCP server. Emulation runs on this (main) thread via EmuHost.
#include "frontend/app.h"
#include "frontend/hostio.h"
#include "frontend/i18n.h"
#include "frontend/keymap.h"
#include "frontend/theme.h"
#include "frontend/ui.h"

#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_internal.h"   // the default window layout: docking and window flags
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include "cards/catalog.h"
#include "core/host.h"
#include "core/log.h"
#include "core/util.h"
#include "core/png.h"
#include "core/version.h"
#include "input/keyboards.h"
#include "input/parallel_kbd.h"
#include "mcp/mcp_server.h"

namespace {

const uint32_t k_background = 0xFF101010u;

// PAL TV: pixel aspect = 7.375 MHz square-pixel clock (288 lines) / dot clock of the card
const double k_tv_square_dot_hz = 7.375e6;   // square pixels of a PAL picture (288 lines)

// Zoom of a display window: fitted to the window, or a fixed 100/200/300 % chosen from
// its context menu (the window is resized to it; resizing it by hand fits again).
struct ZoomState {
    float zoom = 0;    // screen pixels per emulated pixel in the last frame (0 = not drawn yet)
    float fixed = 0;   // chosen zoom, 0 = fit the window
    int request = 0;   // percent chosen: resize the window before its next Begin
    ImVec2 size;       // window size set for `fixed`
};
const int k_zoom_steps[] = { 100, 200, 300, 400 };
const float k_display_pad = 4.0f;   // window padding of the display windows
const float k_zoom_margin = 12.0f;  // border around the picture when the window is sized to a zoom

struct GfxView {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;
    std::vector<uint32_t> pixels;
    ZoomState zs;
};
const int MAX_GFX_VIEWS = 4;

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* screen_tex = nullptr;
    int tex_w = 0, tex_h = 0;
    std::vector<uint32_t> pixels;
    ZoomState screen_zs;          // of the console screen
    ImGuiID dockspace_id = 0;     // the dock space over the main window
    bool reset_layout = false;    // View -> Default window layout: done before the next frame's windows
    // frames in which the screen window takes the focus back: after the start of the program and of a machine
    // the windows opening with it (Media, plotter...) would take it, typing goes to the SAPI right away
    int focus_screen = 3;
    bool rack_behind = false;   // the first start: the machine configuration opens behind the screen window
    bool rack_placed = false;
    bool rack_in_ini = false;   // imgui.ini has a place for the configuration (then it is used)
    std::vector<const char*> top_windows;   // titles of the windows the machine puts on top ("windows")
    int shot_target = -1;         // output window focused last (Ctrl+F5): -1 the console screen, k = gfx[k]
    bool open_screen_menu = false;   // UI snapshot: open the right click menu of the console screen
    GfxView gfx[MAX_GFX_VIEWS];   // graphic display cards (DGD-1)

    EmuHost host;
    HostAudio audio;              // sound cards (MPH-1V)
    HostJoystick joy;             // gamepad / keyboard for the joystick ports
    HostMouse mouse;              // the host mouse for the mouse ports (captured by a click)
    McpServer* mcp = nullptr;
    bool mcp_pref = false;        // View -> Settings: the MCP server on (saved; new installs off, author 2026-10-17)
    int mcp_pref_port = MCP_DEFAULT_PORT;
    std::string mcp_error;        // the last start failed (a port in use...)
    UiState ui;
    std::string error_text;
    bool show_error = false;
    bool quit = false;
    uint64_t title_gen = ~0ull;   // machine generation shown in the window title
    bool integer_scale = true;
    bool tv_aspect = false;
    bool smooth = false;          // linear filtering of the screen texture
    char suppress_text = 0;       // text event of a keypad key already sent as a key
    // keyboards with held keys (ANK-1): the PC key whose text event is awaited, the text of
    // an autorepeat (not typed: the program repeats by itself), the game mode (keys by position,
    // Shift = SHIFT, Ctrl+F7)
    int held_text_id = -1;
    bool held_drop_text = false;
    bool game_keys = false;
    std::string ini_path;

    // settings (sapiemu-settings.json next to the exe, else in the user's AppData)
    std::string settings_path;
    nlohmann::json settings_saved;   // as last written, to save only on change
    uint64_t settings_check_ns = 0;
    int win_x = 0, win_y = 0, win_w = 0, win_h = 0;   // restored (not maximized) window geometry
    bool win_pos_known = false;
    std::string language;         // "cs" / "en" chosen in View -> Language, empty = by the system
    // View -> Settings -> Look (frontend/theme.h): the choice saved; --ui-theme takes its place for one run
    ThemeVariant look = THEME_WIN11;
    ThemeMode look_mode = THEME_SYSTEM;
    int text_percent = 100;       // the size of the text
    bool look_override = false;   // --ui-theme in use (not saved) until a look is chosen in the settings
    bool restyle = true;          // build the style before the next frame
    bool snapshot = false;        // a self-check: "as the system says" is light, the pictures do not depend on the PC
    float dpi = 1.0f;
    bool style_editor = false;    // Debug -> Style editor (debug builds)
};

// ---------------------------------------------------------------- settings

// Directory for the settings and imgui.ini: next to the executable (portable)
// when it is writable, otherwise the per-user application data directory.
std::string settings_dir(void)
{
    const char* base = SDL_GetBasePath();   // owned by SDL, ends with a separator
    if (base && *base) {
        std::string dir = base;
        std::string probe = dir + "sapiemu-settings.json";
        bool exists = file_exists(probe);
        if (!exists) probe = dir + "sapiemu-write-test.tmp";
        FILE* f = file_open(probe, "ab");
        if (f) {
            fclose(f);
            if (!exists) file_remove(probe);
            return dir;
        }
    }
    char* pref = SDL_GetPrefPath("SAPIemu", "SAPIemu");
    std::string dir = pref ? pref : "";
    if (pref) SDL_free(pref);
    return dir;
}

nlohmann::json settings_json(App& app)
{
    UiState& ui = app.ui;
    if (ui.terminal_channel < (int)ui.terms.size()) {   // options of the terminal in use
        const TerminalState& t = ui.terms[(size_t)ui.terminal_channel];
        ui.term_emulation = t.emulation;
        ui.term_cr_newline = t.cr_newline;
        ui.term_local_echo = t.local_echo;
    }
    nlohmann::json j;
    j["version"] = 1;
    j["machine"] = app.host.machine_file();
    bool maximized = (SDL_GetWindowFlags(app.window) & SDL_WINDOW_MAXIMIZED) != 0;
    j["window"] = { { "x", app.win_x }, { "y", app.win_y }, { "w", app.win_w }, { "h", app.win_h },
                    { "position", app.win_pos_known }, { "maximized", maximized } };
    j["windows"] = { { "debugger", ui.show_debugger }, { "memory", ui.show_memory }, { "terminal", ui.show_terminal },
                     { "io_log", ui.show_iolog }, { "log", ui.show_log }, { "media", ui.show_media },
                     { "printer", ui.show_printer }, { "configuration", ui.show_rack }, { "diag", ui.show_diag },
                     { "aps5", ui.show_aps5 }, { "minigraf", ui.show_plotter }, { "ims2", ui.show_ims2 } };
    j["screen"] = { { "integer_scale", app.integer_scale },
                    { "tv_aspect", app.tv_aspect }, { "smooth", app.smooth }, { "card_types", ui.rack.show_types } };
    j["terminal"] = { { "channel", ui.terminal_channel }, { "emulation", ui.term_emulation },
                      { "cr_newline", ui.term_cr_newline }, { "local_echo", ui.term_local_echo },
                      { "copy_on_select", ui.term_copy_on_select }, { "tcp_port", ui.tcp_port }, { "tcp_protocol", ui.tcp_protocol },
                      { "tcp_dcd_follows", ui.tcp_dcd_follows }, { "com_port", ui.com_port } };
    j["speed"] = { { "max", app.host.max_speed }, { "percent", app.host.pace_percent } };
    j["sound"] = { { "enabled", app.audio.enabled }, { "volume", app.audio.volume } };
    j["screenshot"] = { { "enlarged", ui.shot_enlarged } };
    if (!ui.ffmpeg_path.empty()) j["video"] = { { "ffmpeg", ui.ffmpeg_path } };
    // the joystick from the keyboard and the PC mouse are off at every start (author): not saved
    j["mouse"] = { { "sensitivity", app.mouse.sensitivity } };
    j["cassette"] = { { "last_dir", ui.tape_dlg.last_dir } };
    j["load"] = { { "binary_address", std::string(ui.bin_addr) } };
    if (!app.language.empty()) j["language"] = app.language;
    j["appearance"] = { { "theme", k_themes[app.look].key }, { "mode", theme_mode_key(app.look_mode) },
                        { "text_percent", app.text_percent } };
    j["browser"] = { { "recent", ui.recent } };
    j["mcp"] = { { "enabled", app.mcp_pref }, { "port", app.mcp_pref_port } };
    j["guides"] = { { "auto", ui.guides_auto }, { "hidden", ui.guides_hidden } };
    return j;
}

void settings_save(App& app, bool force)
{
    if (app.settings_path.empty()) return;
    nlohmann::json j = settings_json(app);
    if (!force && j == app.settings_saved) return;
    if (j["machine"].get<std::string>().empty() && app.settings_saved.contains("machine"))
        j["machine"] = app.settings_saved["machine"];   // an unsaved configuration is not a "last machine"
    std::string text = j.dump(2);
    if (file_write_all(app.settings_path, text.data(), text.size())) app.settings_saved = j;
}

// Values of the settings file, applied before the window opens (geometry) or after.
template <class T> void get_opt(const nlohmann::json& j, const char* key, T& out)
{
    if (j.is_object() && j.contains(key)) {
        try {
            out = j[key].get<T>();
        } catch (...) {
        }
    }
}

void settings_apply(App& app, const nlohmann::json& j)
{
    UiState& ui = app.ui;
    const nlohmann::json& w = j.value("windows", nlohmann::json::object());
    get_opt(w, "debugger", ui.show_debugger);
    get_opt(w, "memory", ui.show_memory);
    get_opt(w, "terminal", ui.show_terminal);
    get_opt(w, "io_log", ui.show_iolog);
    get_opt(w, "log", ui.show_log);
    get_opt(w, "media", ui.show_media);
    get_opt(w, "printer", ui.show_printer);
    get_opt(w, "diag", ui.show_diag);
    get_opt(w, "aps5", ui.show_aps5);
    get_opt(w, "minigraf", ui.show_plotter);
    get_opt(w, "ims2", ui.show_ims2);
    get_opt(w, "configuration", ui.show_rack);
    const nlohmann::json& s = j.value("screen", nlohmann::json::object());
    get_opt(s, "integer_scale", app.integer_scale);
    get_opt(s, "tv_aspect", app.tv_aspect);
    get_opt(s, "smooth", app.smooth);
    get_opt(s, "card_types", ui.rack.show_types);
    get_opt(j.value("browser", nlohmann::json::object()), "recent", ui.recent);
    const nlohmann::json& gd = j.value("guides", nlohmann::json::object());
    get_opt(gd, "auto", ui.guides_auto);
    get_opt(gd, "hidden", ui.guides_hidden);
    const nlohmann::json& cas = j.value("cassette", nlohmann::json::object());
    get_opt(cas, "last_dir", ui.tape_dlg.last_dir);
    std::string ba;
    get_opt(j.value("load", nlohmann::json::object()), "binary_address", ba);
    if (!ba.empty() && ba.size() < sizeof(ui.bin_addr)) snprintf(ui.bin_addr, sizeof(ui.bin_addr), "%s", ba.c_str());
    const nlohmann::json& t = j.value("terminal", nlohmann::json::object());
    get_opt(t, "channel", ui.terminal_channel);
    if (ui.terminal_channel < 0) ui.terminal_channel = 0;
    get_opt(t, "emulation", ui.term_emulation);
    get_opt(t, "cr_newline", ui.term_cr_newline);
    get_opt(t, "local_echo", ui.term_local_echo);
    get_opt(t, "copy_on_select", ui.term_copy_on_select);
    get_opt(t, "tcp_port", ui.tcp_port);
    get_opt(t, "tcp_protocol", ui.tcp_protocol);
    get_opt(t, "tcp_dcd_follows", ui.tcp_dcd_follows);
    std::string com;
    get_opt(t, "com_port", com);
    if (!com.empty()) snprintf(ui.com_port, sizeof(ui.com_port), "%s", com.c_str());
    get_opt(j.value("screenshot", nlohmann::json::object()), "enlarged", ui.shot_enlarged);
    get_opt(j.value("video", nlohmann::json::object()), "ffmpeg", ui.ffmpeg_path);
    const nlohmann::json& snd = j.value("sound", nlohmann::json::object());
    get_opt(snd, "enabled", app.audio.enabled);
    get_opt(snd, "volume", app.audio.volume);
    if (app.audio.volume < 0 || app.audio.volume > 100) app.audio.volume = 80;
    const nlohmann::json& mo = j.value("mouse", nlohmann::json::object());
    get_opt(mo, "sensitivity", app.mouse.sensitivity);
    if (!(app.mouse.sensitivity >= 0.1f && app.mouse.sensitivity <= 8.0f)) app.mouse.sensitivity = 1.0f;
    // UI language: --lang, else the saved choice, else the system language
    get_opt(j, "language", app.language);
    UiLanguage lang;
    if (!app.language.empty() && !i18n_parse(app.language.c_str(), &lang)) app.language.clear();
    if (!i18n_override(&lang) && !i18n_parse(app.language.c_str(), &lang)) lang = i18n_system_language();
    i18n_set_language(lang);
    // the look: --ui-theme, else the saved one, else Windows 11 light or dark as the system says
    const nlohmann::json& ap = j.value("appearance", nlohmann::json::object());
    std::string look, mode;
    get_opt(ap, "theme", look);
    get_opt(ap, "mode", mode);
    get_opt(ap, "text_percent", app.text_percent);
    ThemeVariant v;
    ThemeMode md;
    if (theme_parse(look.c_str(), &v, &md)) app.look = v;   // a look no longer offered: the default one
    if (theme_parse(mode.c_str(), &v, &md)) app.look_mode = md;
    if (app.text_percent < 100 || app.text_percent > 200) app.text_percent = 100;
    app.look_override = theme_override(&v, &md);
    app.restyle = true;
}

// View -> Language: switches at once (the texts are translated every frame) and is saved.
void set_ui_language(App& app, UiLanguage lang)
{
    i18n_set_language(lang);
    app.language = i18n_code(lang);
}

// The look in use: --ui-theme until a look is chosen in the settings, else the saved one; "as the system says"
// asks SDL (light when the system does not tell, and in a self-check). Between frames only (app.restyle).
static void apply_look(App& app)
{
    ThemeVariant v = app.look;
    ThemeMode md = app.look_mode;
    if (app.look_override) theme_override(&v, &md);
    bool dark = md == THEME_DARK;
    if (md == THEME_SYSTEM && !app.snapshot) dark = SDL_GetSystemTheme() == SDL_SYSTEM_THEME_DARK;
    theme_apply(v, dark, app.dpi, (float)app.text_percent / 100.0f);
}

// View -> Settings: a look chosen here is saved and replaces --ui-theme.
static void set_look(App& app, ThemeVariant v, ThemeMode md)
{
    if (app.look_override) {
        ThemeVariant ov;
        ThemeMode om;
        theme_override(&ov, &om);
        if (v == ov && md == om) return;
    }
    app.look = v;
    app.look_mode = md;
    app.look_override = false;
    app.restyle = true;
}

// Replaces the section "[Window][old]" (a whole line) by "[Window][now]"; false when missing.
static bool ini_rename(std::string& text, const std::string& old, const std::string& now)
{
    std::string sec = "[Window][" + old + "]";
    size_t at = text.find(sec), end = at + sec.size();
    if (at == std::string::npos || end >= text.size() || (text[end] != '\r' && text[end] != '\n')) return false;
    text.replace(at, sec.size(), "[Window][" + now + "]");
    return true;
}

// Windows got fixed ids so that their saved positions survive a change of the language: the
// console window "Obrazovka" (or "Screen") is named after the display card with the id
// "###screen", the tool windows are "<translated>###<English>". ImGui saves such a window under
// the part after "###" ([Window][Media]). Keep the layout saved under the old Czech names.
void ini_rename_windows(const std::string& path)
{
    std::string text;
    if (path.empty() || !file_read_text(path, text)) return;
    bool changed = false;
    if (text.find("###screen]") == std::string::npos)
        for (const char* old : { "Obrazovka", "Screen" })
            if (ini_rename(text, old, "Obrazovka###screen")) {
                changed = true;
                break;
            }
    for (const char* key : { "Debugger", "Memory", "I/O log", "Media", "Printer", "Serial terminal", "Log" }) {
        std::string cs = tr_lang(key, LANG_CS);
        if (cs == key || text.find(std::string("[Window][") + key + "]") != std::string::npos) continue;
        if (ini_rename(text, cs, key)) changed = true;
    }
    if (changed) file_write_all(path, text.data(), text.size());
}

// The restored window must lie (at least partly) on a connected display.
bool rect_on_display(int x, int y, int w, int h)
{
    int n = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&n);
    bool ok = false;
    for (int i = 0; ids && i < n; i++) {
        SDL_Rect r;
        if (!SDL_GetDisplayUsableBounds(ids[i], &r)) continue;
        if (x + 64 <= r.x + r.w && x + w - 64 >= r.x && y >= r.y - 8 && y + 32 <= r.y + r.h) ok = true;
    }
    SDL_free(ids);
    return ok;
}

void track_window(App& app)
{
    SDL_WindowFlags f = SDL_GetWindowFlags(app.window);
    if (f & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_FULLSCREEN)) return;
    int x, y, w, h;
    if (SDL_GetWindowPosition(app.window, &x, &y) && SDL_GetWindowSize(app.window, &w, &h) && w > 0 && h > 0) {
        app.win_x = x;
        app.win_y = y;
        app.win_w = w;
        app.win_h = h;
        app.win_pos_known = true;
    }
}

void set_title(App& app)
{
    Machine* m = app.host.machine();
    std::string title = std::string("SAPIemu - ") + (m ? m->name : "");
    SDL_SetWindowTitle(app.window, title.c_str());
}

void load_machine(App& app, const std::string& file)
{
    std::string err;
    if (!app.host.load(file, err)) {
        app.error_text = std::string(tr("Cannot load machine")) + ":\n" + err;
        app.show_error = true;
        LOGE("%s", err.c_str());
        return;
    }
    set_title(app);
}

void debug_step(App& app, bool over)
{
    Machine* m = app.host.machine();
    if (!m || !app.host.paused) return;
    if (over) m->step_over(); else m->step();
    app.host.status = m->stop_text();
    m->clear_break();
    app.ui.show_debugger = true;
}

// The keyboard of the machine when it takes held keys (ANK-1), else nullptr.
static KeyboardDevice* held_keyboard(App& app)
{
    Machine* m = app.host.machine();
    KeyboardDevice* k = m ? m->keyboard() : nullptr;
    return k && k->held_keys() ? k : nullptr;
}

// The game mode: a PC key by its position (digits) or its label (letters), Shift, Space, Enter.
static bool game_key(KeyboardDevice* k, const SDL_KeyboardEvent& ev)
{
    int id = (int)ev.scancode;
    if (ev.scancode == SDL_SCANCODE_LSHIFT || ev.scancode == SDL_SCANCODE_RSHIFT) return k->hold_key(id, 0, HK_SHIFT, true);
    if (ev.key >= SDLK_A && ev.key <= SDLK_Z) return k->hold_key(id, (uint32_t)('A' + (ev.key - SDLK_A)), HK_NONE, true);
    if (ev.scancode >= SDL_SCANCODE_1 && ev.scancode <= SDL_SCANCODE_0)   // the top row, whatever the layout
        return k->hold_key(id, ev.scancode == SDL_SCANCODE_0 ? '0' : (uint32_t)('1' + (ev.scancode - SDL_SCANCODE_1)), HK_NONE, true);
    if (ev.key == SDLK_SPACE) return k->hold_key(id, ' ', HK_NONE, true);
    int hk = keymap_host_key(ev.key);
    return hk != HK_NONE && k->hold_key(id, 0, hk, true);
}

// Held keys (ANK-1) on the screen: the key goes down here, its character comes with the text
// event (handle_text), it goes up with the key-up event. An autorepeat of the PC is dropped.
// Returns true when the key was taken.
static bool held_key_down(App& app, const SDL_KeyboardEvent& ev)
{
    KeyboardDevice* k = app.ui.screen_focused ? held_keyboard(app) : nullptr;
    app.held_text_id = -1;
    app.held_drop_text = false;
    if (!k || ((ev.mod & SDL_KMOD_CTRL) && !(ev.mod & SDL_KMOD_ALT))) return false;   // Ctrl+key: as before
    if (ev.repeat) {
        app.held_drop_text = true;
        return true;
    }
    if (app.game_keys) {
        app.held_drop_text = true;   // no text in the game mode
        game_key(k, ev);
        return ev.key != SDLK_F6 && ev.key != SDLK_F7;   // INT and T keys as before
    }
    int hk = keymap_host_key(ev.key);
    if (hk != HK_NONE) return k->hold_key((int)ev.scancode, 0, hk, false);   // Enter, Backspace...: no text
    app.held_text_id = (int)ev.scancode;   // its character comes as text
    return true;
}

void handle_key_up(App& app, const SDL_KeyboardEvent& ev)
{
    if (KeyboardDevice* k = held_keyboard(app)) k->release_key((int)ev.scancode);
}

void shot_now(App& app);
void video_toggle(App& app);

static void paste_keyboard(App& app, Machine* m);

void handle_key(App& app, const SDL_KeyboardEvent& ev)
{
    Machine* m = app.host.machine();
    if (ImGui::GetIO().WantTextInput) return;   // typing into a text field: no shortcuts, nothing for SAPI
    switch (ev.key) {
    case SDLK_F5:   // Ctrl+F5: a screenshot of the focused output (as in DOSBox); F5 run/pause
        if ((ev.mod & SDL_KMOD_CTRL) && (ev.mod & SDL_KMOD_ALT)) video_toggle(app);   // Ctrl+Alt+F5: video
        else if (ev.mod & SDL_KMOD_CTRL) shot_now(app);
        else app.host.paused = !app.host.paused;
        return;
    case SDLK_F7:   // Ctrl+F7: the game mode of the ANK-1 keyboard on/off
        if (ev.mod & SDL_KMOD_CTRL) {
            app.game_keys = !app.game_keys;
            if (KeyboardDevice* k = held_keyboard(app)) k->release_all();
            return;
        }
        break;
    case SDLK_F8:   // Ctrl+F8: the joystick from the keyboard on/off (arrows, space = fire)
        if (ev.mod & SDL_KMOD_CTRL) app.joy.keyboard = !app.joy.keyboard;
        else app.host.cycle_speed();
        return;
    case SDLK_F10:
        if (app.mouse.captured && (ev.mod & SDL_KMOD_CTRL)) mouse_capture(app.mouse, app.window, false);
        else debug_step(app, true);
        return;
    case SDLK_F11: debug_step(app, false); return;
    case SDLK_F12:   // under a debugger Windows takes F12 as the break hotkey, F9 still works
    case SDLK_F9: if (m) m->reset(); return;
    case SDLK_F2:   // Ctrl+F2 / Ctrl+F3: the quick slot of the machine; without Ctrl the SAPI keys
    case SDLK_F3:
        if (ev.mod & SDL_KMOD_CTRL) {
            state_quick(app.ui, ev.key == SDLK_F2);
            return;
        }
        break;
    default: break;
    }
    app.suppress_text = 0;
    if (!m) return;
    if (app.ui.screen_focused && joystick_key(app.joy, (int)ev.key)) return;   // arrows, space: the joystick
    int hk = keymap_host_key(ev.key);
    int mods = keymap_mods(ev.mod);
    int ctrl = keymap_ctrl_code(ev.key, ev.mod);
    // serial terminal: the clipboard as in TeraTerm (Alt+C, Alt+V; Ctrl+C / Ctrl+V go to SAPI as control
    // codes; AltGr = Ctrl+Alt writes characters)
    if (app.ui.terminal_focused) {
        bool alt = (ev.mod & SDL_KMOD_ALT) && !(ev.mod & SDL_KMOD_CTRL);
        if ((alt && ev.key == SDLK_V) || ((ev.mod & SDL_KMOD_SHIFT) && ev.key == SDLK_INSERT)) {
            terminal_paste(app.ui);
            app.suppress_text = alt ? 'v' : 0;   // a text event of the key, if any, is not typed
            return;
        }
        if ((alt && ev.key == SDLK_C) || ((ev.mod & SDL_KMOD_CTRL) && ev.key == SDLK_INSERT)) {
            terminal_copy(app.ui);
            app.suppress_text = alt ? 'c' : 0;
            return;
        }
    }
    // serial terminal: the EKL-1 (WordStar) codes of the cursor keys
    int tcode = ctrl >= 0 ? ctrl : (hk != HK_NONE ? wordstar_code(hk, mods) : -1);
    if (tcode >= 0 && terminal_key(app.ui, tcode)) return;
    if (!app.ui.screen_focused) return;
    if (ev.key == SDLK_V && (ev.mod & SDL_KMOD_CTRL) && (ev.mod & SDL_KMOD_SHIFT)) {
        paste_keyboard(app, m);
        return;
    }
    if (held_key_down(app, ev)) return;   // ANK-1: held while held
    if (ctrl >= 0) {
        char c[2] = { (char)ctrl, 0 };
        m->type_text(c);
    } else if (hk != HK_NONE && m->host_key(hk, mods)) {
        // a keypad key the keyboard has itself (Consul 262.3): drop its text event
        app.suppress_text = keymap_keypad_text(ev.key, ev.mod);
    }
}

// Machine -> Keyboard -> Paste text (Ctrl+Shift+V on the screen): the clipboard typed on the keyboard of
// SAPI, lines ended by CR (a BASIC program from a file, a monitor command...)
static void paste_keyboard(App& app, Machine* m)
{
    if (!m || !SDL_HasClipboardText()) return;
    char* t = SDL_GetClipboardText();
    if (!t) return;
    std::string s;
    for (const char* p = t; *p; p++) {
        if (*p == '\r' && p[1] == '\n') continue;
        s += *p == '\n' ? '\r' : *p;
    }
    SDL_free(t);
    if (!m->type_text(s)) {
        app.error_text = tr("This machine has no keyboard to type on.");
        app.show_error = true;
    }
}

void handle_text(App& app, const char* text)
{
    Machine* m = app.host.machine();
    char sup = app.suppress_text;
    app.suppress_text = 0;
    if (!m) return;
    if (sup && text[1] == 0 && (text[0] == sup || (sup == '.' && text[0] == ','))) return;
    if (terminal_text(app.ui, text)) return;
    if (app.ui.screen_focused && app.joy.keyboard && !strcmp(text, " ")) return;   // fire of the joystick
    if (!app.ui.screen_focused) return;
    int id = app.held_text_id;
    bool drop = app.held_drop_text;
    app.held_text_id = -1;
    app.held_drop_text = false;
    if (drop) return;   // an autorepeat or the game mode
    KeyboardDevice* k = held_keyboard(app);
    if (k && id >= 0) {   // the character of the key just pressed: held until the key goes up
        const unsigned char* s = (const unsigned char*)text;
        uint32_t cp = s[0];
        int n = 1;
        if (s[0] >= 0xF0) { cp = s[0] & 0x07; n = 4; }
        else if (s[0] >= 0xE0) { cp = s[0] & 0x0F; n = 3; }
        else if (s[0] >= 0xC0) { cp = s[0] & 0x1F; n = 2; }
        for (int i = 1; i < n && s[i]; i++) cp = (cp << 6) | (s[i] & 0x3F);
        if (!s[n] && k->hold_key(id, cp, HK_NONE, false)) return;
    }
    m->type_text(text);   // several characters (a dead key...) or none of this keyboard: typed
}

// Window title of a display output: the card name and the current zoom ("CGA-1V  200 %").
void display_title(char* out, size_t n, const char* name, float zoom, const char* id)
{
    char z[16] = "";
    if (zoom > 0) snprintf(z, sizeof(z), "  %d %%", (int)(zoom * 100.0f + 0.5f));
    snprintf(out, n, "%s%s###%s", name, z, id);
}

float display_px_aspect(App& app, DisplayDevice* d);

// Before Begin: a zoom chosen from the menu resizes the window to fit the picture at it.
void zoom_resize(App& app, DisplayDevice* d, ZoomState& zs)
{
    if (!zs.request || !d) return;
    float k = zs.request / 100.0f;
    zs.request = 0;
    float fb = ImGui::GetIO().DisplayFramebufferScale.x;
    if (fb <= 0) fb = 1.0f;
    float pad = 2.0f * (k_display_pad + k_zoom_margin);   // a small border around the picture
    ImVec2 s(d->screen_width() * display_px_aspect(app, d) * k / fb + pad,
             d->screen_height() * k / fb + pad + ImGui::GetFrameHeight());
    ImGui::SetNextWindowDockID(0, ImGuiCond_Always);   // a docked window takes the size of its dock: undock it
    ImGui::SetNextWindowSize(s);
    zs.fixed = k;
    zs.size = s;
}

static const MachineWindow k_machine_windows[] = {
    { "media", "Media", "Media", "Média" },
    { "minigraf", "Minigraf", "Plotter (Minigraf, XY 4130)", "Zapisovač (Minigraf, XY 4130)" },
    { "ims2", "IMS-2 measuring bench", "IMS-2 measuring bench", "Měřicí pracoviště IMS-2" },
    { "aps5", "APS-5: panel and furnaces", "APS-5: panel and furnaces", "APS-5: panel a pece" },
    { "diag", "Diagnostic board (K3)", "Diagnostic board (K3)", "Deska diagnostiky (K3)" },
    { "printer", "Printer", "Printer", "Tiskárna" },
    { "terminal", "Serial terminal", "Serial terminal", "Sériový terminál" },
};
static const int k_machine_window_count = (int)(sizeof(k_machine_windows) / sizeof(k_machine_windows[0]));

static bool* machine_window_flag(UiState& ui, const char* key)
{
    if (!strcmp(key, "media")) return &ui.show_media;
    if (!strcmp(key, "minigraf")) return &ui.show_plotter;
    if (!strcmp(key, "ims2")) return &ui.show_ims2;
    if (!strcmp(key, "aps5")) return &ui.show_aps5;
    if (!strcmp(key, "diag")) return &ui.show_diag;
    if (!strcmp(key, "printer")) return &ui.show_printer;
    if (!strcmp(key, "terminal")) return &ui.show_terminal;
    return nullptr;
}

// A machine started: the windows of its "windows" open (they come on top with the screen focus).
static void open_machine_windows(App& app)
{
    app.top_windows.clear();
    Machine* m = app.host.machine();
    if (!m || !m->config.contains("windows") || !m->config["windows"].is_array()) return;
    for (const nlohmann::json& k : m->config["windows"]) {
        if (!k.is_string()) continue;
        std::string key = k.get<std::string>();
        for (int i = 0; i < k_machine_window_count; i++) {
            if (key != k_machine_windows[i].key) continue;
            *machine_window_flag(app.ui, k_machine_windows[i].key) = true;
            app.top_windows.push_back(k_machine_windows[i].title);
        }
    }
}

// The keyboard to the screen window after a machine starts. Focusing raises a window, so the windows that were
// in front of the screen stay in front (the screen would cover the Minigraf window with READY or the Media
// window), and the windows of the machine (top_windows) come on top, in a dock their tab is selected.
static void focus_screen_window(App& app)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* screen = ImGui::FindWindowByName("###screen");
    if (!screen) return;
    ImVector<ImGuiWindow*> top;
    for (const char* t : app.top_windows) {
        ImGuiWindow* w = ImGui::FindWindowByName(tr_id(t));
        if (w && (w->Active || w->WasActive) && w->RootWindowDockTree != screen->RootWindowDockTree) {
            ImGui::FocusWindow(w);
            top.push_back(w->RootWindowDockTree);
        }
    }
    int at = g.Windows.index_from_ptr(g.Windows.find(screen->RootWindowDockTree));
    ImVector<ImGuiWindow*> front;   // back to front, as g.Windows
    for (int i = at + 1; i < g.Windows.Size; i++) {
        ImGuiWindow* w = g.Windows[i];
        if (w->RootWindow == w && (w->Active || w->WasActive) && !w->DockIsActive &&
            !(w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip |
                          ImGuiWindowFlags_NoBringToFrontOnFocus)))
            front.push_back(w);
    }
    ImGui::FocusWindow(screen);
    for (ImGuiWindow* w : front) ImGui::BringWindowToDisplayFront(w);
    for (ImGuiWindow* w : top) ImGui::BringWindowToDisplayFront(w);
}

// View -> Default window layout: every window undocked, at the place and size it gets at its first use
// (the windows that give none go to the top left), not collapsed; the screen fits its window again.
static void reset_window_layout(App& app)
{
    ImGuiContext& g = *GImGui;
    if (app.dockspace_id) ImGui::DockBuilderRemoveNodeDockedWindows(app.dockspace_id, true);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    for (ImGuiWindow* w : g.Windows) {
        if (w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip |
                        ImGuiWindowFlags_NoTitleBar))
            continue;   // the menu bar, the dock space host, popups
        if (w->DockNodeAsHost) continue;
        if (w->DockId) ImGui::SetWindowDock(w, 0, ImGuiCond_Always);
        ImGui::SetWindowCollapsed(w, false);
        ImGui::SetWindowPos(w, ImVec2(vp->WorkPos.x + 20.0f, vp->WorkPos.y + 10.0f));
        // after them (they clear it): the next Begin takes the first use place and size again
        w->SetWindowPosAllowFlags |= ImGuiCond_FirstUseEver;
        w->SetWindowSizeAllowFlags |= ImGuiCond_FirstUseEver;
        w->SetWindowCollapsedAllowFlags |= ImGuiCond_FirstUseEver;
    }
    app.screen_zs.fixed = 0;
    app.screen_zs.request = 0;
    for (GfxView& v : app.gfx) {
        v.zs.fixed = 0;
        v.zs.request = 0;
    }
}

// After Begin: resizing the window by hand (or docking it) goes back to fitting.
void zoom_track(ZoomState& zs)
{
    if (!zs.fixed) return;
    ImVec2 s = ImGui::GetWindowSize();
    if (s.x < zs.size.x - 1.5f || s.x > zs.size.x + 1.5f || s.y < zs.size.y - 1.5f || s.y > zs.size.y + 1.5f)
        zs.fixed = 0;
}

void zoom_items(ZoomState& zs)
{
    for (int p : k_zoom_steps) {
        char label[16];
        snprintf(label, sizeof(label), "%d %%", p);
        if (ImGui::MenuItem(label, nullptr, zs.fixed > 0 && (int)(zs.fixed * 100.0f + 0.5f) == p)) zs.request = p;
    }
}

// Record video / stop it (with the length so far)
void video_item(App& app, DisplayDevice* d, const char* name)
{
    if (video_recording(app.ui)) {
        char label[64];
        int s = (int)video_seconds(app.ui);
        snprintf(label, sizeof(label), "%s (%d:%02d)", tr("Stop the video"), s / 60, s % 60);
        if (ImGui::MenuItem(label, "Ctrl+Alt+F5")) video_stop(app.ui);
        return;
    }
    if (ImGui::MenuItem(tr("Record video with sound"), "Ctrl+Alt+F5", false, d != nullptr))
        video_start(app.ui, d, name, display_px_aspect(app, d));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("MP4 (H.264 + AAC) into work/videa through ffmpeg: 50 frames per second of emulated time, "
                                   "enlarged 3x as shown, the sound of the machine."));
}

void shot_items(App& app, DisplayDevice* d, const char* name)
{
    float a = display_px_aspect(app, d);
    if (ImGui::MenuItem(tr("Save screenshot"), "Ctrl+F5", false, d != nullptr)) shot_save(app.ui, d, name, a);
    if (ImGui::MenuItem(tr("Save screenshot as..."), nullptr, false, d != nullptr)) shot_save_as(app.ui, d, name, a);
    if (ImGui::MenuItem(tr("Copy screenshot"), nullptr, false, d != nullptr)) shot_copy(app.ui, d, a);
    ImGui::MenuItem(tr("Screenshot enlarged 3x (as shown)"), nullptr, &app.ui.shot_enlarged);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Off: the exact dots of the emulated picture. On: every dot 3 lines high and as wide as on the screen (TV aspect)."));
    ImGui::Separator();
    video_item(app, d, name);
}

// Right click on the picture: the zoom of this window and its screenshot.
void display_context_menu(App& app, DisplayDevice* d, const char* name, ZoomState& zs)
{
    if (ImGui::BeginPopupContextWindow("zoommenu")) {
        ImGui::TextDisabled("%s", tr("Zoom"));
        zoom_items(zs);
        ImGui::Separator();
        shot_items(app, d, name);
        ImGui::EndPopup();
    }
}

// The output of the window focused last (the console screen if there is no such window): Ctrl+F5,
// View -> Screenshot.
DisplayDevice* shot_display(App& app, std::string& name)
{
    Machine* m = app.host.machine();
    if (!m) return nullptr;
    DisplayDevice* found = m->display;
    int n = 0;
    for (DisplayDevice* d : m->displays) {
        if (d == m->display || n >= MAX_GFX_VIEWS) continue;
        if (n++ == app.shot_target) found = d;
    }
    Card* c = dynamic_cast<Card*>(found);
    name = c ? c->name : std::string("screen");
    return found;
}

void shot_now(App& app)
{
    std::string name;
    DisplayDevice* d = shot_display(app, name);
    if (d) shot_save(app.ui, d, name.c_str(), display_px_aspect(app, d));
}

// Ctrl+Alt+F5: start or stop the video of the focused output
void video_toggle(App& app)
{
    if (video_recording(app.ui)) {
        video_stop(app.ui);
        return;
    }
    std::string name;
    DisplayDevice* d = shot_display(app, name);
    if (d) video_start(app.ui, d, name.c_str(), display_px_aspect(app, d));
}

// View -> Colour: the ink of the monochrome displays. It is a card parameter (jumpers
// S1 of AND-1V, the monitor phosphor of the others), the same one the configurator
// edits; the change shows at once and is saved with the machine.
void ink_menu(App& app, Machine* m)
{
    std::vector<DisplayDevice*> mono;
    for (DisplayDevice* d : m ? m->displays : std::vector<DisplayDevice*>())
        if (d->ink_param()) mono.push_back(d);
    if (!ImGui::BeginMenu(tr("Colour"), !mono.empty())) return;
    bool cs = i18n_language() == LANG_CS;
    for (DisplayDevice* d : mono) {
        Card* c = dynamic_cast<Card*>(d);
        const CardInfo* ci = c ? card_info(c->type) : nullptr;
        const CardParam* p = ci ? ci->params : nullptr;
        while (p && p->path && strcmp(p->path, d->ink_param()) != 0) p++;
        if (!p || !p->path || !p->options) continue;
        std::string head = c->name + " - " + (cs ? p->cs : p->en);
        theme_section(head.c_str());
        std::string cur = d->ink();
        ImGui::PushID(d);   // two displays may offer the same colours (AND-1V and DGD-1V)
        for (const ParamOption* o = p->options; o->value; o++) {
            nlohmann::json v = nlohmann::json::parse(o->value, nullptr, false);
            if (!v.is_string()) continue;
            std::string val = v.get<std::string>();
            if (ImGui::MenuItem(cs ? o->cs : o->en, nullptr, val == cur) && val != cur)
                rack_set_display_ink(app.ui, d, val);
        }
        ImGui::PopID();
    }
    ImGui::EndMenu();
}

// The MCP server on or off now (View -> Settings, the start).
static void mcp_set(App& app, bool on, int port)
{
    if (app.mcp) {
        app.mcp->stop();
        delete app.mcp;
        app.mcp = nullptr;
        app.ui.mcp = nullptr;
    }
    app.mcp_error.clear();
    if (!on) return;
    app.mcp = new McpServer(app.host);
    std::string err;
    if (!app.mcp->start(port, err)) {
        LOGW("MCP server not started: %s", err.c_str());
        app.mcp_error = err;
        delete app.mcp;
        app.mcp = nullptr;
        return;
    }
    app.ui.mcp = app.mcp;
}

// View -> Settings: what used to be scattered in the menus (language, sound, mouse) and what had no place (MCP,
// ffmpeg, the guides).
static void draw_settings(App& app)
{
    UiState& ui = app.ui;
    if (!ui.show_settings) return;
    float u = ImGui::GetFontSize();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(34 * u, 30 * u), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    if (!ImGui::Begin(tr_id("Settings"), &ui.show_settings)) {
        ImGui::End();
        return;
    }
    theme_section("Jazyk / Language");
    UiLanguage cur = i18n_language();
    if (ImGui::RadioButton("Čeština", cur == LANG_CS)) set_ui_language(app, LANG_CS);
    ImGui::SameLine();
    if (ImGui::RadioButton("English", cur == LANG_EN)) set_ui_language(app, LANG_EN);

    theme_section(tr("Look"));
    ThemeVariant cv = app.look;
    ThemeMode cm = app.look_mode;
    if (app.look_override) theme_override(&cv, &cm);
    for (int i = 0; i < THEME_VARIANTS && THEME_VARIANTS > 1; i++) {   // one look now: no choice
        if (i) ImGui::SameLine();
        if (ImGui::RadioButton(cur == LANG_CS ? k_themes[i].name_cs : k_themes[i].name_en, cv == i))
            set_look(app, (ThemeVariant)i, cm);
    }
    static const char* const k_modes[] = { "As the system", "Light", "Dark" };
    for (int i = 0; i < 3; i++) {
        if (i) ImGui::SameLine();
        if (ImGui::RadioButton(tr(k_modes[i]), cm == i)) set_look(app, cv, (ThemeMode)i);
    }
    static int text_edit = 100;   // dragged: the size changes when the slider is released, not under the mouse
    static bool text_drag = false;
    if (!text_drag) text_edit = app.text_percent;
    ImGui::SetNextItemWidth(12 * u);
    ImGui::SliderInt(tr("Text size"), &text_edit, 100, 200, "%d %%");
    text_drag = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        app.text_percent = (text_edit + 5) / 10 * 10;
        app.restyle = true;
    }
    if (app.look_override)
        ImGui::TextDisabled("%s", tr("The look of this run comes from --ui-theme; a choice here replaces it and is saved."));

    theme_section(tr("Sound"));
    ImGui::Checkbox(tr("Sound"), &app.audio.enabled);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(12 * u);
    ImGui::SliderInt(tr("Volume"), &app.audio.volume, 0, 100, "%d %%");
    if (app.audio.failed) ImGui::TextDisabled("%s", tr("No sound device."));
    ImGui::TextDisabled("%s", tr("Sound plays only at the emulation speed 100 %."));

    theme_section(tr("Mouse"));
    ImGui::Checkbox(tr("PC mouse as the SAPI mouse (a click into a screen captures it)"), &app.mouse.enabled);
    ImGui::SetNextItemWidth(12 * u);
    ImGui::SliderFloat(tr("Mouse sensitivity"), &app.mouse.sensitivity, 0.25f, 4.0f, "%.2f");

    theme_section(tr("Machine guides"));
    ImGui::Checkbox(tr("Guides open by themselves"), &ui.guides_auto);
    ImGui::BeginDisabled(ui.guides_hidden.empty());
    if (ImGui::Button(tr("Show the hidden guides again"))) ui.guides_hidden.clear();
    ImGui::EndDisabled();

    theme_section(tr("MCP server (development with Claude)"));
    bool on = app.mcp_pref;
    if (ImGui::Checkbox(tr("MCP server on"), &on)) {
        app.mcp_pref = on;
        mcp_set(app, on, app.mcp_pref_port);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(7 * u);
    int port = app.mcp_pref_port;
    if (ImGui::InputInt(tr("Port"), &port, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue) && port > 0 && port < 65536) {
        app.mcp_pref_port = port;
        if (app.mcp_pref) mcp_set(app, true, port);
    }
    if (app.mcp && app.mcp->running()) {
        ImGui::TextUnformatted(app.mcp->url().c_str());
        ImGui::TextDisabled("claude mcp add --transport http sapiemu %s", app.mcp->url().c_str());
    } else if (!app.mcp_error.empty()) {
        ImGui::TextColored(theme().error, "%s", app.mcp_error.c_str());
    } else if (app.mcp_pref) {
        ImGui::TextDisabled("%s", tr("Not running now (started with --no-mcp)."));
    } else {
        ImGui::TextDisabled("%s", tr("Off. Only programs on this PC (127.0.0.1) can connect, it is for the development "
                                     "of software for SAPI with Claude (docs/mcp.md)."));
    }

    theme_section(tr("Video"));
    static char ff[512];
    static bool ff_init = false;
    if (!ff_init) {
        snprintf(ff, sizeof(ff), "%s", ui.ffmpeg_path.c_str());
        ff_init = true;
    }
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##ffmpeg", tr("ffmpeg.exe (empty = from PATH)"), ff, sizeof(ff))) ui.ffmpeg_path = ff;
    ImGui::Checkbox(tr("Screenshot enlarged 3x (as shown)"), &ui.shot_enlarged);
    ImGui::End();
}

void draw_menu(App& app)
{
    if (!ImGui::BeginMainMenuBar()) return;
    Machine* m = app.host.machine();
    if (ImGui::BeginMenu(tr("File"))) {
        if (ImGui::MenuItem(tr("Choose a machine..."))) browser_open(app.ui);
        if (ImGui::BeginMenu(tr("Recent machines"), !app.ui.recent.empty())) {
            for (const std::string& f : std::vector<std::string>(app.ui.recent)) {
                const MachineEntry* e = machine_index_find(app.ui.machine_index, f);
                std::string label = e ? (e->title.empty() ? e->name : e->title) : f;
                if (ImGui::MenuItem((label + "##" + f).c_str(), nullptr, f == app.host.machine_file())) {
                    rack_start_machine(app.ui, f);   // its guide may open itself (a new machine generation)
                    m = app.host.machine();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr("Open machine file..."))) rack_open_and_run(app.ui);
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Load program into memory (Intel HEX)..."), nullptr, false, m != nullptr))
            hex_load_dialog(app.ui);
        if (ImGui::MenuItem(tr("Load binary file into memory..."), nullptr, false, m != nullptr))
            bin_load_dialog(app.ui);
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Save state..."), nullptr, false, m != nullptr)) state_save_dialog(app.ui);
        if (ImGui::MenuItem(tr("Load state..."))) state_load_dialog(app.ui);
        if (ImGui::MenuItem(tr("Quick save state"), "Ctrl+F2", false, m != nullptr)) state_quick(app.ui, true);
        if (ImGui::MenuItem(tr("Quick load state"), "Ctrl+F3")) state_quick(app.ui, false);
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Exit"))) rack_request_exit(app.ui);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("Machine"))) {
        if (ImGui::MenuItem(m ? tr("Switch off") : tr("Switch on"))) {
            rack_power_switch(app.ui);
            m = app.host.machine();   // the old machine is gone
            set_title(app);
        }
        if (ImGui::MenuItem(tr("Reset"), "F12", false, m != nullptr) && m) m->reset();
        ImGui::MenuItem(tr("Pause"), "F5", &app.host.paused);
        if (ImGui::BeginMenu(tr("Emulation speed"))) {   // the whole machine against real time
            for (int p = 100; p <= 400; p *= 2) {
                char item[32];
                snprintf(item, sizeof(item), "%d %%", p);
                if (ImGui::MenuItem(item, nullptr, !app.host.max_speed && app.host.pace_percent == p)) {
                    app.host.max_speed = false;
                    app.host.pace_percent = p;
                }
            }
            if (ImGui::MenuItem(tr("Maximum"), nullptr, app.host.max_speed)) app.host.max_speed = true;
            ImGui::Separator();
            if (ImGui::MenuItem(tr("Next"), "F8")) app.host.cycle_speed();
            ImGui::EndMenu();
        }
        // switches of the CPU card that work while running (JPR-1V; also the TURBO and BIOS buttons)
        if (m && m->cpu && m->live_param(m->cpu, "turbo")) {
            bool on = m->cpu->cpu_turbo();
            if (ImGui::MenuItem(tr("CPU card TURBO (CPU 4 MHz)"), nullptr, on)) rack_set_live_param(app.ui, m->cpu, "turbo", !on);
        }
        if (m && m->cpu && m->live_param(m->cpu, "bios")) {
            nlohmann::json* cc = m->card_config(m->cpu);
            bool bank1 = cc && cc->value("bios", std::string("fitted")) == "open";
            if (ImGui::MenuItem(tr("BIOS jumper open (EEPROM bank 1)"), nullptr, bank1))
                rack_set_live_param(app.ui, m->cpu, "bios", bank1 ? "fitted" : "open");
        }
        ImGui::Separator();
        KeyboardDevice* kb = m ? m->keyboard() : nullptr;
        if (ImGui::BeginMenu(tr("Keyboard"), m != nullptr)) {
            bool plotter = kb && !strcmp(kb->type_name(), "AR 1990");   // the plotter of AR 1990 on the connector
            if (plotter) ImGui::TextDisabled("%s", tr("The plotter of AR 1990 is on the keyboard connector (configurator)."));
            for (const KeyboardPreset& p : keyboard_presets()) {
                if (!p.keyboard || plotter) continue;
                bool sel = kb && strcmp(kb->type_name(), p.name) == 0;
                if (ImGui::MenuItem(p.name, nullptr, sel) && !sel) {
                    std::string err;
                    if (!m->set_keyboard(p.name, err)) {
                        app.error_text = std::string(tr("Cannot change the keyboard")) + ":\n" + err;
                        app.show_error = true;
                    }
                    kb = m->keyboard();   // the old keyboard is gone
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem(tr("Paste text"), "Ctrl+Shift+V", false, kb != nullptr)) paste_keyboard(app, m);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", tr("The text of the clipboard is typed on the keyboard of SAPI (lines end with "
                                           "Enter), e.g. a BASIC program from a file."));
            if (ImGui::MenuItem(tr("Game mode: keys by position, Shift = SHIFT"), "Ctrl+F7", &app.game_keys, kb && kb->held_keys()))
                kb->release_all();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", tr("ANK-1: letters by their label, digits by position, Shift of the PC = SHIFT, keys "
                                           "pressed together stay together. Symbols as on the ANK-1 (SHIFT + key). Off: "
                                           "typing by characters (comma = SHIFT + N)."));
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("Sound, joystick and mouse"))) {
            ImGui::MenuItem(tr("Sound"), nullptr, &app.audio.enabled);
            if (app.audio.failed) ImGui::TextDisabled("%s", tr("No sound device."));
            ImGui::Separator();
            ImGui::MenuItem(tr("Joystick from the keyboard (arrows, space = fire)"), "Ctrl+F8", &app.joy.keyboard);
            for (int i = 0; i < 2; i++) {
                std::string pad = joystick_name(app.joy, i);
                ImGui::TextDisabled("%s %d: %s", tr("Gamepad"), i + 1, pad.empty() ? tr("none") : pad.c_str());
            }
            ImGui::Separator();
            ImGui::MenuItem(tr("PC mouse as the SAPI mouse (a click into a screen captures it)"), nullptr, &app.mouse.enabled);
            ImGui::TextDisabled("%s", tr("Middle button or Ctrl+F10 releases the captured mouse."));
            ImGui::Separator();
            if (ImGui::MenuItem(tr("Volume, mouse sensitivity..."))) app.ui.show_settings = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("Debug"))) {
        ImGui::MenuItem(tr("Debugger"), nullptr, &app.ui.show_debugger);
        ImGui::MenuItem(tr("Memory"), nullptr, &app.ui.show_memory);
        ImGui::MenuItem(tr("I/O log"), nullptr, &app.ui.show_iolog);
        ImGui::MenuItem(tr("Log"), nullptr, &app.ui.show_log);
        ImGui::Separator();
        bool stepping = m && app.host.paused;   // the steps are for a paused machine (F5)
        if (ImGui::MenuItem(tr("Step"), "F11", false, stepping)) debug_step(app, false);
        if (ImGui::MenuItem(tr("Step over"), "F10", false, stepping)) debug_step(app, true);
#ifndef NDEBUG
        ImGui::Separator();   // tuning the look live; Export copies the values for frontend/theme_data.cpp
        ImGui::MenuItem(tr("Style editor (development)"), nullptr, &app.style_editor);
#endif
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("View"))) {
        ImGui::MenuItem(tr("Configuration..."), nullptr, &app.ui.show_rack);
        ImGui::MenuItem(tr("Serial terminal"), nullptr, &app.ui.show_terminal);
        ImGui::MenuItem(tr("Media"), nullptr, &app.ui.show_media);
        bool has_printer = false, has_plotter = false;   // windows of devices the machine has
        if (m)
            for (auto& c : m->cards) {
                if (c->printer()) has_printer = true;
                if (c->plotter()) has_plotter = true;
            }
        ImGui::MenuItem(tr("Printer"), nullptr, &app.ui.show_printer, has_printer || app.ui.show_printer);
        ImGui::MenuItem(tr("Plotter (Minigraf, XY 4130)"), nullptr, &app.ui.show_plotter, has_plotter || app.ui.show_plotter);
        ImGui::MenuItem(tr("IMS-2 measuring bench"), nullptr, &app.ui.show_ims2, (m && m->plant) || app.ui.show_ims2);
        ImGui::MenuItem(tr("Diagnostic board (K3)"), nullptr, &app.ui.show_diag, m && m->cpu && m->cpu->k3_diag());
        ImGui::MenuItem(tr("APS-5: panel and furnaces"), nullptr, &app.ui.show_aps5, m && m->plant);
        ImGui::Separator();
        ink_menu(app, m);
        if (ImGui::BeginMenu(tr("Screen zoom"), m && m->display)) {
            zoom_items(app.screen_zs);
            ImGui::Separator();
            ImGui::TextDisabled("%s", tr("Right click on a picture: the zoom of its window."));
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr("Screenshot"), "Ctrl+F5", false, m && m->display)) shot_now(app);
        if (ImGui::MenuItem(video_recording(app.ui) ? tr("Stop the video") : tr("Record video with sound"), "Ctrl+Alt+F5",
                            false, m && m->display))
            video_toggle(app);
        ImGui::MenuItem(tr("Integer scaling"), nullptr, &app.integer_scale);
        ImGui::MenuItem(tr("TV aspect ratio"), nullptr, &app.tv_aspect);
        ImGui::MenuItem(tr("Smoothing (linear filtering)"), nullptr, &app.smooth);
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Default window layout"))) app.reset_layout = true;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr("Undocks all windows (a window dragged to an edge sticks there) and puts them "
                                       "back at their first places and sizes."));
        ImGui::Separator();
        if (ImGui::BeginMenu("Jazyk / Language")) {   // in both languages: findable in either
            UiLanguage cur = i18n_language();
            if (ImGui::MenuItem("Čeština", nullptr, cur == LANG_CS)) set_ui_language(app, LANG_CS);
            if (ImGui::MenuItem("English", nullptr, cur == LANG_EN)) set_ui_language(app, LANG_EN);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr("Settings..."))) app.ui.show_settings = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("Help"))) {
        if (ImGui::MenuItem(tr("Machine guide"))) {   // opens it in front (an instrument may hide it), X closes it
            app.ui.show_guide = true;
            app.ui.guide_focus = true;
            app.ui.guide_machine = app.host.machine_file();
        }
        if (ImGui::MenuItem(tr("Choose a machine..."))) browser_open(app.ui);
        ImGui::Separator();
        help_docs_menu(app.ui);
        if (ImGui::MenuItem(tr("Keyboard shortcuts"))) app.ui.show_shortcuts = true;
        ImGui::Separator();
        if (ImGui::MenuItem(tr("About SAPIemu"))) app.ui.show_about = true;
        ImGui::EndMenu();
    }

    // status on the right side of the menu bar
    char st[200];
    st[0] = 0;
    bool kb_leds = false;   // LEDs of the keyboard in it (a tooltip explains them)
    std::string notice;   // the result of the last save, load, screenshot: 3 s, the file name is enough
    if (!app.ui.notice.empty() && !app.ui.notice_error && SDL_GetTicksNS() - app.ui.notice_ns < 3000000000ull) {
        notice = app.ui.notice;
        size_t slash = notice.find_last_of("/\\");
        size_t colon = notice.find(": ");
        if (slash != std::string::npos && colon != std::string::npos)
            notice = notice.substr(0, colon + 2) + notice.substr(slash + 1);
    }
    if (!m) snprintf(st, sizeof(st), "%s  %s", notice.c_str(), tr("Switched off"));
    if (m) {
        const char* state = app.host.paused ? (app.host.status.empty() ? tr("Paused") : app.host.status.c_str())
                                            : (app.host.max_speed ? tr("Maximum") : "");
        KeyboardDevice* kb = m->keyboard();
        std::string ks = kb ? kb->status() : std::string();
        kb_leds = ks.find("LED") != std::string::npos;
        if (!notice.empty()) ks += "  " + notice;
        if (video_recording(app.ui)) {
            int s = (int)video_seconds(app.ui);
            char rec[48];
            snprintf(rec, sizeof(rec), "  \xE2\x97\x8F REC %d:%02d", s / 60, s % 60);
            ks += rec;
        }
        if (app.game_keys && kb && kb->held_keys()) {
            ks += "  ";
            ks += tr("game mode (Ctrl+F7)");
        }
        if (app.joy.keyboard) {   // arrows and space do not type now
            ks += "  ";
            ks += tr("joystick from the keyboard (Ctrl+F8)");
        }
        if (app.mouse.captured) {
            ks += "  ";
            ks += tr("mouse captured (middle button / Ctrl+F10)");
        }
        if (kb && kb->pending() > 1) {   // keys typed but not taken by the program
            char q[48];
            snprintf(q, sizeof(q), "  %s %d", tr("keys waiting:"), (int)kb->pending());
            ks += q;
        }
        // the clock of the CPU card (JPR-1V TURBO included) and the measured speed of the emulation
        char speed[16] = " -- %";   // the first measurement comes after a second
        if (app.host.speed_percent > 0) snprintf(speed, sizeof(speed), "%3.0f %%", app.host.speed_percent);
        snprintf(st, sizeof(st), "%s  %s  %s  CPU %.2f MHz  %s %s", ks.c_str(), state,
                 (app.mcp && app.mcp->running()) ? "MCP" : "", (m->cpu ? m->cpu->cpu_display_hz() : m->bus.cpu_hz) / 1e6,
                 tr("emulation"), speed);
    }
    float w = ImGui::CalcTextSize(st).x + ImGui::GetStyle().ItemSpacing.x * 2;
    // changes in the configurator wait for a restart: visible with the configurator closed too
    std::string pend;
    if (rack_pending(app.ui)) pend = std::string("\xE2\x97\x8F ") + tr("Configuration changed: restart");
    float pw = pend.empty() ? 0.0f : ImGui::CalcTextSize(pend.c_str()).x + ImGui::GetStyle().ItemSpacing.x * 2;
    ImGui::SameLine(ImGui::GetWindowWidth() - w - pw);
    if (!pend.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme().pending);
        ImVec2 sz = ImGui::CalcTextSize(pend.c_str());
        if (ImGui::Selectable(pend.c_str(), false, ImGuiSelectableFlags_None, sz)) app.ui.show_rack = true;
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr("The configurator holds changes the running machine does not have yet. Click: open "
                                       "the configurator (Restart with this configuration applies them)."));
        ImGui::SameLine();
    }
    ImGui::TextUnformatted(st);
    if (kb_leds && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("LED SHIFT, LED ERR: the lamps on the SAPI keyboard (ANK-1), lit by the program "
                                   "(P0-OUT D5, D7), not the Shift key of the PC. Programs making sound on D5 "
                                   "(Manic Miner) light the LED SHIFT with it."));
    ImGui::EndMainMenuBar();
}

float display_px_aspect(App& app, DisplayDevice* d)
{
    return app.tv_aspect ? (float)(k_tv_square_dot_hz / d->dot_clock_hz()) : 1.0f;
}

// Draw display output `d` into the current window, scaled to fit (or at the zoom chosen
// for the window), through its own texture. Returns the zoom: screen pixels per emulated
// pixel (vertically, with the TV aspect).
float draw_display(App& app, DisplayDevice* d, SDL_Texture*& tex, int& tex_w, int& tex_h, std::vector<uint32_t>& pixels,
                   const ZoomState& zs)
{
    int w = d->screen_width(), h = d->screen_height();
    if (!tex || w != tex_w || h != tex_h) {
        if (tex) SDL_DestroyTexture(tex);
        tex = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        tex_w = w;
        tex_h = h;
        pixels.assign((size_t)w * h, 0);
    }
    d->render(pixels.data(), k_background);
    SDL_UpdateTexture(tex, nullptr, pixels.data(), w * 4);

    // Scale in physical pixels (window points * framebuffer scale) so that integer
    // scaling stays sharp with Windows display scaling.
    float fb = ImGui::GetIO().DisplayFramebufferScale.x;
    if (fb <= 0) fb = 1.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float px_aspect = display_px_aspect(app, d);
    float img_w = (float)w * px_aspect, img_h = (float)h;
    float scale = avail.x * fb / img_w;
    if (avail.y * fb / img_h < scale) scale = avail.y * fb / img_h;
    if (zs.fixed > 0 && scale >= zs.fixed * 0.999f) scale = zs.fixed;
    else if (app.integer_scale && scale >= 1.0f) scale = (float)(int)scale;
    if (scale <= 0.1f) scale = 0.1f;
    ImVec2 size(img_w * scale / fb, img_h * scale / fb);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    pos.x += (avail.x - size.x) * 0.5f;
    pos.y += (avail.y - size.y) * 0.5f;
    pos.x = (float)(int)(pos.x * fb + 0.5f) / fb;   // align to the pixel grid
    pos.y = (float)(int)(pos.y * fb + 0.5f) / fb;
    ImGui::SetCursorScreenPos(pos);
    // The SDL renderer backend applies the sampler state per draw command and
    // defaults to linear filtering; switch to nearest for the emulated screen.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (!app.smooth) dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
    ImGui::Image(ImTextureRef((ImTextureID)(intptr_t)tex), size);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) app.mouse.click = true;   // captures the mouse when the machine has one
    if (!app.smooth) dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
    return scale;
}

void draw_screen(App& app)
{
    ImGui::SetNextWindowSize(ImVec2(820, 560), ImGuiCond_FirstUseEver);
    static bool first = true;
    if (first) { ImGui::SetNextWindowFocus(); first = false; }   // type right after start
    Machine* m = app.host.machine();
    DisplayDevice* d = m ? m->display : nullptr;
    Card* c = dynamic_cast<Card*>(d);
    char title[96];   // named after the display card, the same window for every machine
    display_title(title, sizeof(title), c ? c->name.c_str() : tr("Screen"), d ? app.screen_zs.zoom : 0, "screen");
    zoom_resize(app, d, app.screen_zs);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(k_display_pad, k_display_pad));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(k_background));   // a monitor in every look
    ImGui::Begin(title);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    zoom_track(app.screen_zs);
    app.ui.screen_focused = ImGui::IsWindowFocused();
    if (app.ui.screen_focused) app.shot_target = -1;

    if (!m || !d) {   // switched off: a dark screen; the text light on it in every look
        ImVec2 a = ImGui::GetCursorScreenPos(), av = ImGui::GetContentRegionAvail();
        ImGui::GetWindowDrawList()->AddRectFilled(a, ImVec2(a.x + av.x, a.y + av.y), IM_COL32(8, 8, 8, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.66f, 0.66f, 0.66f, 1.0f));
        ImGui::TextUnformatted(!m ? tr("Switched off (POWER on the ZDR panel, Machine -> Switch on)")
                                  : tr("No display card in this machine."));
        ImGui::PopStyleColor();
        ImGui::End();
        return;
    }
    app.screen_zs.zoom = draw_display(app, d, app.screen_tex, app.tex_w, app.tex_h, app.pixels, app.screen_zs);
    if (app.open_screen_menu) {
        ImGui::OpenPopup("zoommenu");
        app.open_screen_menu = false;
    }
    display_context_menu(app, d, c ? c->name.c_str() : "screen", app.screen_zs);
    ImGui::End();
}

// Graphic display cards (DGD-1): their own video output, so their own window.
void draw_graphic_screens(App& app)
{
    Machine* m = app.host.machine();
    if (!m) return;
    int n = 0;
    for (DisplayDevice* d : m->displays) {
        if (d == m->display || n >= MAX_GFX_VIEWS) continue;
        int k = n++;
        GfxView& v = app.gfx[k];
        Card* c = dynamic_cast<Card*>(d);
        char title[96], id[16];
        snprintf(id, sizeof(id), "gfx%d", k);
        display_title(title, sizeof(title), c ? c->name.c_str() : tr("Graphics"), v.zs.zoom, id);
        // first use: right of the console screen (820 wide from x 60), below it on a narrow desktop
        ImVec2 vp = ImGui::GetMainViewport()->WorkSize;
        ImVec2 at = vp.x >= 60 + 820 + 10 + 500 ? ImVec2(890.0f + 30.0f * k, 60.0f + 30.0f * k)
                                                 : ImVec2(60.0f + 30.0f * k, 630.0f + 30.0f * k);
        ImGui::SetNextWindowPos(at, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
        zoom_resize(app, d, v.zs);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(k_display_pad, k_display_pad));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(k_background));
        bool open = ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        if (open) {
            zoom_track(v.zs);
            if (ImGui::IsWindowFocused()) {
                app.ui.screen_focused = true;   // typing goes to the machine too
                app.shot_target = k;
            }
            v.zs.zoom = draw_display(app, d, v.tex, v.w, v.h, v.pixels, v.zs);
            display_context_menu(app, d, c ? c->name.c_str() : "graphics", v.zs);
        }
        ImGui::End();
    }
}

void draw_log(App& app)
{
    if (!app.ui.show_log) return;
    first_use_right(600, 250, 5);
    ImGui::SetNextWindowSize(ImVec2(600, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(tr_id("Log"), &app.ui.show_log)) {
        std::vector<std::string> lines;
        log_recent(lines, 500);
        ImGui::BeginChild("loglines");
        for (const std::string& l : lines) ImGui::TextUnformatted(l.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }
    ImGui::End();
}

void draw_error(App& app)
{
    if (app.ui.notice_error) {   // save state failed
        app.error_text = app.ui.notice;
        app.show_error = true;
        app.ui.notice_error = false;
        app.ui.notice.clear();
    }
    if (app.show_error) {
        ImGui::OpenPopup(tr_id("Error"));
        app.show_error = false;
    }
    if (ImGui::BeginPopupModal(tr_id("Error"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(app.error_text.c_str());
        if (ImGui::Button(tr("Close"))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace

const MachineWindow* machine_windows(int* count)
{
    *count = k_machine_window_count;
    return k_machine_windows;
}

// A start that fails: the message on stderr, in the log and in a message box (a user who started the
// exe from Explorer sees no console).
static int fatal(const std::string& text, bool box)
{
    LOGE("%s", text.c_str());
    if (box) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SAPIemu", text.c_str(), nullptr);
    return 1;
}

int app_run(const std::string& machine_file, int mcp_port, const UiSnapshot* snap, const std::string& settings_file)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        return fatal(std::string(tr("Cannot start the graphics (SDL)")) + ":\n" + SDL_GetError(), !snap);
    if (!data_root_valid())
        return fatal(std::string(tr("The data of the emulator were not found (the folders roms and machines next to "
                                    "sapiemu.exe). Unpack the whole archive and start sapiemu.exe from it.")) +
                         "\n\n" + (SDL_GetBasePath() ? SDL_GetBasePath() : ""),
                     !snap);
    App* app_ptr = new App();
    App& app = *app_ptr;
    app.ui.host = &app.host;

    float dpi = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (dpi <= 0) dpi = 1.0f;
    if (snap && snap->scale > 0) dpi = snap->scale;   // --ui-scale: pictures at 125 %, 150 %... on any PC
    app.dpi = dpi;
    app.snapshot = snap != nullptr;

    // settings of the previous run: --settings FILE, else next to the exe / AppData;
    // a layout self-check uses none unless a file is given
    nlohmann::json settings = nlohmann::json::object();
    if (!settings_file.empty()) {
        app.settings_path = settings_file;
        app.ini_path = settings_file + ".imgui.ini";
    } else if (!snap) {
        std::string dir = settings_dir();
        if (!dir.empty()) {
            app.settings_path = dir + "sapiemu-settings.json";
            app.ini_path = dir + "imgui.ini";
            // window layout of older versions lived in AppData only
            char* pref = SDL_GetPrefPath("SAPIemu", "SAPIemu");
            if (pref && !file_exists(app.ini_path)) {
                std::vector<uint8_t> ini;
                if (file_read_all(std::string(pref) + "imgui.ini", ini)) file_write_all(app.ini_path, ini.data(), ini.size());
            }
            if (pref) SDL_free(pref);
        }
    }
    if (snap && settings_file.empty()) app.ini_path.clear();   // a self-check uses a layout only when given
    // the log of this run next to the settings (sapiemu.log; FILE.log for --settings FILE), for error reports
    if (!app.settings_path.empty()) {
        std::string log_file = !settings_file.empty() ? settings_file + ".log"
                                                      : path_join(path_dir(app.settings_path), "sapiemu.log");
        if (log_set_file(log_file)) LOGI("SAPIemu %s, data %s, work %s", sapiemu_version(), data_root().c_str(),
                                         work_root().c_str());
    }
    // no settings yet: the first start of a fresh copy offers the machines (window Choose a machine, author
    // 2026-10-17; before it was the configuration behind the screen)
    bool first_start = !app.settings_path.empty() && !file_exists(app.settings_path);
    ini_rename_windows(app.ini_path);
    if (!app.ini_path.empty()) {   // the layout knows the configuration: its place is kept at a first start too
        std::string ini;
        app.rack_in_ini = file_read_text(app.ini_path, ini) && ini.find("###rack]") != std::string::npos;
    }
    if (!app.settings_path.empty()) {
        std::string text;
        if (file_read_text(app.settings_path, text)) {
            settings = nlohmann::json::parse(text, nullptr, false);
            if (settings.is_discarded() || !settings.is_object()) settings = nlohmann::json::object();
        }
        LOGI("settings: %s", app.settings_path.c_str());
    }
    int win_w = (int)(1100 * dpi), win_h = (int)(760 * dpi);
    if (snap) win_h = (int)(1300 * dpi);   // room for the configurator with its map
    if (snap && snap->w > 0 && snap->h > 0) {
        win_w = (int)(snap->w * dpi);
        win_h = (int)(snap->h * dpi);
    }
    const nlohmann::json& sw = settings.value("window", nlohmann::json::object());
    int sx = 0, sy = 0, sww = 0, swh = 0;
    bool spos = false, smax = false;
    get_opt(sw, "x", sx);
    get_opt(sw, "y", sy);
    get_opt(sw, "w", sww);
    get_opt(sw, "h", swh);
    get_opt(sw, "position", spos);
    get_opt(sw, "maximized", smax);
    if (sww >= 320 && swh >= 240) {
        win_w = sww;
        win_h = swh;
    }
    app.window = SDL_CreateWindow("SAPIemu", win_w, win_h,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                      (snap ? SDL_WINDOW_HIDDEN : 0));
    if (app.window && spos && rect_on_display(sx, sy, win_w, win_h)) SDL_SetWindowPosition(app.window, sx, sy);
    if (app.window) track_window(app);
    if (app.window && smax) SDL_MaximizeWindow(app.window);
    app.renderer = SDL_CreateRenderer(app.window, nullptr);
    if (!app.window || !app.renderer)
        return fatal(std::string(tr("Cannot create the window")) + ":\n" + SDL_GetError(), !snap);
#ifndef _WIN32
    // Windows takes the icon from the exe resources (src/res/sapiemu.rc.in)
    if (SDL_Surface* icon = SDL_LoadPNG(data_path("docs/img/ikona.png").c_str())) {
        SDL_SetWindowIcon(app.window, icon);
        SDL_DestroySurface(icon);
    }
#endif
    SDL_SetRenderVSync(app.renderer, 1);
    app.ui.window = app.window;
    SDL_StartTextInput(app.window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = app.ini_path.empty() ? nullptr : app.ini_path.c_str();   // none for a self-check
    // the fonts of all looks (baked on demand); the style is built before the first frame (app.restyle)
    theme_fonts_load(io);
    app.ui.mono = theme_fonts().mono;
    apply_look(app);
    ImGui_ImplSDL3_InitForSDLRenderer(app.window, app.renderer);
    ImGui_ImplSDLRenderer3_Init(app.renderer);

    settings_apply(app, settings);
    if (first_start) {   // closed without starting a machine, it opens the guide of the running one (browser.cpp)
        app.ui.show_browser = true;
        app.ui.browser_first = true;
    }
    std::string first = machine_file;
    if (first.empty()) {
        std::string last;
        get_opt(settings, "machine", last);
        if (!last.empty() && !file_exists(data_path(last)) && last.size() > 6 &&
            last.compare(last.size() - 6, 6, ".jsonc") == 0)
            last = last.substr(0, last.size() - 6) + ".sapi";   // machine files were renamed to .sapi
        if (!last.empty() && file_exists(data_path(last))) first = last;
        else first = file_exists(data_path(GUI_FIRST_MACHINE_FILE)) ? GUI_FIRST_MACHINE_FILE : DEFAULT_MACHINE_FILE;
    }
    load_machine(app, first);
    if (snap && !snap->state.empty()) {   // a picture of a saved moment
        std::string err;
        if (!app.host.load_state_file(snap->state, err)) fprintf(stderr, "UI snapshot: %s\n", err.c_str());
    }
    if (!app.host.machine() && first != DEFAULT_MACHINE_FILE) load_machine(app, DEFAULT_MACHINE_FILE);
    // the machine of the last session: its guide does not open by itself at every start (only a machine started
    // from the window Choose a machine or a menu)
    app.ui.guide_gen = app.host.machine_generation();
    const nlohmann::json& sp = settings.value("speed", nlohmann::json::object());
    get_opt(sp, "max", app.host.max_speed);
    get_opt(sp, "percent", app.host.pace_percent);
    if (app.host.pace_percent != 200 && app.host.pace_percent != 400) app.host.pace_percent = 100;
    app.settings_saved = settings;
    int snap_frames = 0;
    if (snap) {
        app.ui.show_rack = snap->rack || !snap->open.empty();
        app.ui.show_browser = snap->browser || first_start;   // --settings NEW: as a first start
        app.ui.show_guide = snap->guide;
        app.ui.show_about = snap->about;
        app.ui.show_settings = snap->settings;
        app.ui.show_shortcuts = snap->shortcuts;
        app.ui.show_media = snap->media;
        app.ui.show_printer = snap->printer;
        app.ui.show_plotter = snap->plotter;
        app.ui.show_ims2 = snap->ims2;
        app.ui.show_diag = snap->diag;
        app.ui.show_aps5 = snap->aps5 || snap->aps5_demo;
        app.ui.aps5_demo_autostart = snap->aps5_demo;
        app.ui.show_terminal = snap->terminal;
        app.ui.rack.sel = snap->rack_slot;
        app.ui.rack.machine_gen = ~0ull;
    }

    // the MCP server: View -> Settings; settings of an older version without the key keep it on as it was, a new
    // install has it off (author 2026-10-17); --mcp-port starts it, --no-mcp (and a snapshot) not
    if (settings.contains("mcp")) {
        const nlohmann::json& mj = settings["mcp"];
        get_opt(mj, "enabled", app.mcp_pref);
        get_opt(mj, "port", app.mcp_pref_port);
    } else {
        app.mcp_pref = !first_start;
    }
    if (mcp_port > 0) mcp_set(app, true, mcp_port);
    else if (mcp_port < 0 && app.mcp_pref) mcp_set(app, true, app.mcp_pref_port);

    uint64_t last = SDL_GetTicksNS();
    while (!app.quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (mouse_event(app.mouse, app.window, app.host.machine(), ev)) continue;   // captured: for the SAPI
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) rack_request_exit(app.ui);
            else if (ev.type == SDL_EVENT_WINDOW_MOVED || ev.type == SDL_EVENT_WINDOW_RESIZED) track_window(app);
            else if (ev.type == SDL_EVENT_KEY_DOWN) handle_key(app, ev.key);
            else if (ev.type == SDL_EVENT_KEY_UP) handle_key_up(app, ev.key);
            else if (ev.type == SDL_EVENT_WINDOW_FOCUS_LOST) {   // keys released elsewhere are not seen
                if (KeyboardDevice* k = held_keyboard(app)) k->release_all();
            }
            else if (ev.type == SDL_EVENT_TEXT_INPUT) handle_text(app, ev.text.text);
            else if (ev.type == SDL_EVENT_GAMEPAD_ADDED || ev.type == SDL_EVENT_GAMEPAD_REMOVED) joystick_event(app.joy, ev);
            else if (ev.type == SDL_EVENT_SYSTEM_THEME_CHANGED) app.restyle = true;   // Windows light / dark
        }
        app.host.poll();   // MCP commands
        if (app.title_gen != app.host.machine_generation()) {   // loaded by MCP, the menu or the configurator
            app.title_gen = app.host.machine_generation();
            set_title(app);
            open_machine_windows(app);
            app.focus_screen = 3;
        }
        if (app.ui.quit || app.host.quit_requested) app.quit = true;
        uint64_t now = SDL_GetTicksNS();
        if (Machine* jm = app.host.machine()) {   // the joystick ports follow the gamepads and the keyboard
            for (int i = 0; i < 2; i++) {
                uint8_t js = joystick_state(app.joy, i, app.ui.screen_focused);
                if (js != app.joy.last[i]) jm->bus.joystick[i] = js;
                app.joy.last[i] = js;
            }
        }
        mouse_frame(app.mouse, app.window, app.host.machine());
        if (app.mouse.captured != app.mouse.gui_captured) {
            // The click that captured the mouse went to the GUI, its release did not (nor anything
            // pressed before the release): let go of all buttons, or the GUI drags a window after.
            for (int b = 0; b < ImGuiMouseButton_COUNT; b++) io.AddMouseButtonEvent(b, false);
            if (!app.mouse.captured) {
                float mx = 0, my = 0;
                SDL_GetMouseState(&mx, &my);
                io.AddMousePosEvent(mx, my);
            }
            app.mouse.gui_captured = app.mouse.captured;
        }
        if (app.mouse.captured) io.ConfigFlags |= ImGuiConfigFlags_NoMouse;   // the GUI does not see it
        else io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        app.host.frame(now - last);
        last = now;
        audio_frame(app.audio, app.host.machine(),
                    !snap && !app.host.paused && !app.host.max_speed && app.host.pace_percent == 100);

        if (app.restyle) {   // a look chosen, the system switched between light and dark: between frames only
            apply_look(app);
            app.restyle = false;
        }
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        // The ImGui SDL3 backend stops SDL text input whenever an ImGui text field
        // loses focus (debugger, configurator...); the SAPI keyboard and the serial
        // terminal need it all the time, otherwise no characters arrive.
        if (!SDL_TextInputActive(app.window)) SDL_StartTextInput(app.window);
        if (app.reset_layout) {   // before the dock space and the windows of this frame
            reset_window_layout(app);
            app.reset_layout = false;
        }
        app.dockspace_id = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        draw_menu(app);
        draw_screen(app);
        draw_graphic_screens(app);
        shot_poll(app.ui);
        video_poll(app.ui);
        draw_debugger(app.ui);
        draw_memory(app.ui);
        draw_iolog(app.ui);
        draw_terminal(app.ui);
        draw_media(app.ui);
        draw_printer(app.ui);
        draw_plotter(app.ui);
        draw_browser(app.ui);
        draw_settings(app);
        draw_about(app.ui);
        draw_shortcuts(app.ui);
        draw_guide(app.ui);
        draw_ims2(app.ui);
        draw_diag(app.ui);
        draw_aps5(app.ui);
        draw_rack(app.ui);
        draw_log(app);
        draw_error(app);
#ifndef NDEBUG
        if (app.style_editor) {
            if (ImGui::Begin(tr_id("Style editor (development)"), &app.style_editor)) ImGui::ShowStyleEditor();
            ImGui::End();
        }
#endif
        if (app.focus_screen > 0) {   // after the windows that opened with the machine
            app.focus_screen--;
            if (!snap && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) focus_screen_window(app);
        }
        if (app.rack_behind) {   // the first start: the configuration behind the screen, the keyboard stays there
            ImGuiWindow* rack = ImGui::FindWindowByName("###rack");
            ImGuiWindow* screen = ImGui::FindWindowByName("###screen");
            if (rack && screen && rack->WasActive && rack->RootWindowDockTree != screen->RootWindowDockTree) {
                if (!app.rack_placed && !app.rack_in_ini) {   // no place in imgui.ini: at the right, at the top
                    ImGuiViewport* vp = ImGui::GetMainViewport();
                    float u = ImGui::GetFontSize();
                    ImVec2 size(vp->WorkSize.x * 0.92f < 62.0f * u ? vp->WorkSize.x * 0.92f : 62.0f * u,
                                vp->WorkSize.y * 0.9f);
                    if (!rack->DockId) {
                        ImGui::SetWindowSize(rack, size, ImGuiCond_Always);
                        ImGui::SetWindowPos(rack, ImVec2(vp->WorkPos.x + vp->WorkSize.x - size.x - 0.5f * u,
                                                         vp->WorkPos.y + 0.5f * u), ImGuiCond_Always);
                        ImGui::SetWindowCollapsed(rack, false, ImGuiCond_Always);
                    }
                    app.rack_placed = true;
                }
                ImGui::BringWindowToDisplayBehind(rack->RootWindowDockTree, screen->RootWindowDockTree);
                if (app.focus_screen == 0) app.rack_behind = false;
            }
        }
        theme_focus_frame();
        ImGui::Render();

        SDL_SetRenderScale(app.renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        ImU32 desk = theme().desktop;   // behind the windows: the colour of the look
        SDL_SetRenderDrawColor(app.renderer, (Uint8)(desk & 0xFF), (Uint8)((desk >> 8) & 0xFF), (Uint8)((desk >> 16) & 0xFF),
                               255);
        SDL_RenderClear(app.renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), app.renderer);
        if (snap && snap_frames == 3) {   // the menu items, after the windows have their places
            if (snap->reset_layout) app.reset_layout = true;
            if (snap->zoom) app.screen_zs.request = snap->zoom;
        }
        if (snap && ++snap_frames == 2) {
            // follow_machine() has run: open the requested description, select the slot
            if (!snap->open.empty()) rack_open_in_editor(app.ui, snap->open);
            app.ui.rack.sel = snap->rack_slot;
            app.ui.rack.open_map_tab = snap->map;
            for (const std::string& e : snap->set) {   // SLOT:KEY=VALUE into the card of the slot
                size_t colon = e.find(':'), eq = e.find('=');
                nlohmann::json& cards = app.ui.rack.cfg["cards"];
                int slot = atoi(e.c_str());
                if (colon != std::string::npos && e.substr(colon + 1) == "remove" && cards.is_array()) {   // SLOT:remove
                    for (size_t i = 0; i < cards.size(); i++)
                        if (cards[i].value("slot", 0) == slot) {
                            cards.erase(i);
                            break;
                        }
                    continue;
                }
                if (colon == std::string::npos || eq == std::string::npos || eq < colon || !cards.is_array()) continue;
                std::string key = e.substr(colon + 1, eq - colon - 1), val = e.substr(eq + 1);
                nlohmann::json v = nlohmann::json::parse(val, nullptr, false);
                if (v.is_discarded()) v = val;
                for (nlohmann::json& c : cards)
                    if (c.value("slot", 0) == slot) c[key] = v;
            }
        }
        if (snap && snap_frames == 9) {
            if (snap->screen_menu) app.open_screen_menu = true;
            if (snap->shot) shot_now(app);
            if (snap->copy) {
                std::string name;
                DisplayDevice* d = shot_display(app, name);
                if (d) shot_copy(app.ui, d, display_px_aspect(app, d));
            }
            if (snap->video > 0) video_toggle(app);
        }
        if (snap && snap->video > 0 && video_recording(app.ui) && video_seconds(app.ui) >= snap->video)
            video_toggle(app);   // stop: the picture is taken once the video is saved
        if (snap && snap->terminal && snap_frames == 9 && !app.ui.terms.empty()) {
            // late, so that what the machine sends at start does not push the text away
            TerminalState& t = app.ui.terms[0];
            std::string text = snap->term_text;
            for (size_t p; (p = text.find("\\n")) != std::string::npos;) text.replace(p, 2, "\r\n");
            if (!text.empty()) {   // only this text: line indexes of the selection from 0
                terminal_clear(t);
                terminal_feed(t, text);
            }
            unsigned l0, c0, l1, c1;
            if (sscanf(snap->term_sel.c_str(), "%u,%u-%u,%u", &l0, &c0, &l1, &c1) == 4) {
                t.sel = true;
                t.sel_line0 = t.dropped + l0;
                t.sel_col0 = (int)c0;
                t.sel_line1 = t.dropped + l1;
                t.sel_col1 = (int)c1;
            }
        }
        if (snap && snap->drag_from > 0 && app.ui.rack.geo_n > 0 && snap_frames >= 4 && snap_frames <= 11) {
            // a card dragged by simulated mouse events: down on its slot, five frames to the target,
            // released before the picture with :drop
            const RackEditor& r = app.ui.rack;
            auto cx = [&](int s) { return r.geo_x0 + ((float)(r.geo_n - s) + 0.5f) * r.geo_pitch; };
            float t = snap_frames <= 5 ? 0.0f : (snap_frames >= 10 ? 1.0f : (float)(snap_frames - 5) / 5.0f);
            io.AddMousePosEvent(cx(snap->drag_from) + (cx(snap->drag_to) - cx(snap->drag_from)) * t,
                                (r.geo_y0 + r.geo_y1) * 0.5f);
            if (snap_frames == 5) io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
            if (snap_frames == 10 && snap->drag_drop) io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        }
        if (snap && snap_frames >= 12 && !(snap->video > 0 && video_busy(app.ui))) {
            // window content as PNG (our own renderer, not the screen)
            SDL_Rect crop, *rect = nullptr;   // --ui-crop: only that window (previews of machines)
            if (!snap->crop.empty()) {
                if (ImGuiWindow* w = ImGui::FindWindowByName(("###" + snap->crop).c_str())) {
                    ImVec2 sc = io.DisplayFramebufferScale;
                    crop = { (int)(w->Pos.x * sc.x), (int)(w->Pos.y * sc.y), (int)(w->Size.x * sc.x), (int)(w->Size.y * sc.y) };
                    rect = &crop;
                } else {
                    fprintf(stderr, "UI snapshot: no window ###%s\n", snap->crop.c_str());
                }
            }
            SDL_Surface* s = SDL_RenderReadPixels(app.renderer, rect);
            SDL_Surface* c = s ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888) : nullptr;
            bool ok = c && png_write_argb(snap->path, (const uint32_t*)c->pixels, c->w, c->h, 1, 1);
            if (c) SDL_DestroySurface(c);
            if (s) SDL_DestroySurface(s);
            fprintf(stderr, ok ? "UI snapshot: %s\n" : "UI snapshot failed: %s\n", snap->path.c_str());
            app.quit = true;
        }
        SDL_RenderPresent(app.renderer);
        if (now - app.settings_check_ns > 3000000000ull) {   // keep the file current (crash, kill)
            app.settings_check_ns = now;
            settings_save(app, false);
        }
    }

    settings_save(app, false);
    audio_close(app.audio);
    joystick_close(app.joy);
    mouse_capture(app.mouse, app.window, false);
    video_shutdown(app.ui);
    app.host.shutdown();
    if (app.mcp) { app.mcp->stop(); delete app.mcp; }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (app.screen_tex) SDL_DestroyTexture(app.screen_tex);
    for (GfxView& v : app.gfx)
        if (v.tex) SDL_DestroyTexture(v.tex);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    delete app_ptr;   // flushes the machine (RAM disks)
    SDL_Quit();
    return 0;
}
