// SDL3 + Dear ImGui frontend: screen window, menus, keyboard routing, debugger and
// terminal windows, MCP server. Emulation runs on this (main) thread via EmuHost.
#include "frontend/app.h"
#include "frontend/hostio.h"
#include "frontend/i18n.h"
#include "frontend/keymap.h"
#include "frontend/theme.h"
#include "frontend/ui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_internal.h"   // the default window layout: docking and window flags
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include "cards/catalog.h"
#include "core/headless.h"
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
    bool user = false;   // sized by hand (after the last zoom chosen): a machine change keeps its size
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
const int FS_OFF = -2;   // App::fullscreen: no full screen

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* screen_tex = nullptr;
    int tex_w = 0, tex_h = 0;
    std::vector<uint32_t> pixels;
    ZoomState screen_zs;          // of the console screen
    ImGuiID dockspace_id = 0;     // the dock space over the main window
    // frames in which the screen window takes the focus back: after the start of the program and of a machine
    // the windows opening with it (Media, plotter...) would take it, typing goes to the SAPI right away
    int focus_screen = 3;
    int main_output = -1;         // the display with the big place (plan_layout): -1 the console screen, k = gfx[k]
    float main_w = 0;             // the width planned for it
    float main_k = 0;             // and its zoom (0: no display)
    uint64_t plan_gen = ~0ull;    // plan_layout keeps its plan while the machine, the work area (width, height, gap),
    float plan_key[5] = {};       // the framebuffer scale, the zoom chosen for the output and the windows beside it do
                                  // not change
    unsigned plan_flags = 0;
    // refit_displays: the machine generation seen, the machine file and its displays whose sizes the windows have
    uint64_t fit_gen = ~0ull;
    std::string fit_sig;
    std::vector<const char*> top_windows;   // titles of the windows the machine puts on top ("windows")
    // the display used last (Ctrl+F5, View -> Screen zoom, Alt+Enter): the display window focused last, at a machine start
    // its main output; -1 the console screen, k = gfx[k]
    int shot_target = -1;
    bool start_focus = true;      // the first frame: the main output takes the keyboard (type right after the start)
    // Full screen (Alt+Enter, draw_fullscreen): the display shown alone over the whole OS window, -1 the console screen,
    // k = gfx[k], FS_OFF = none. fs_maximized: the OS window was maximized before (saved so in the meantime).
    int fullscreen = FS_OFF;
    bool fs_maximized = false;
    // View -> Displays: the display windows hidden by the user (index k + 1: 0 the console screen, 1 + k gfx[k]), not
    // drawn while the machine runs on. A machine with other displays (another file, other display cards) shows them
    // all again, a restart or a power cycle of the same one not (hidden_sig: its displays_sig). display_front: a
    // display just shown again comes to the front with the keyboard once its window is drawn (FS_OFF = none).
    bool display_hidden[1 + MAX_GFX_VIEWS] = {};
    std::string hidden_sig;
    int display_front = FS_OFF;
    // a display had the keyboard (ui.screen_focused) in the last frame without a popup: an open menu has the focus, so
    // View -> Displays asks this one whether the display it hides had the keyboard
    bool screen_keys = false;
    bool open_screen_menu = false;   // UI snapshot: open the right click menu of the console screen (full screen: its menu)
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
    // another GUI runs over the same settings or work folder (file_lock_try at the start): the warning shows until
    // answered (instance_paths: which of them). Until then this window writes nothing there: the machine does not run
    // (no autosave of the media, MCP commands wait), the window layout is not saved (ini_held: io.IniFilename, read at
    // the first frame, back after Continue). Quit leaves without saving the settings, the window layout and the machine
    // (EmuHost::drop: no power fail, the battery memories and RAM disks stay as the other window has them).
    bool instance_warn = false;
    std::string instance_paths;
    const char* ini_held = nullptr;
    bool quit_nosave = false;
    int errors_seen = 0;          // log_errors() when the Log window was shown last ("Errors: N" counts the rest)
    bool log_focus = false;       // "Errors: N" clicked: the Log in front
    bool quit = false;
    uint64_t title_gen = ~0ull;   // machine generation shown in the window title
    std::string title_file;       // and its file (the configurator's Save of a supplied machine: the copy in My machines)
    std::vector<std::string> drops;   // files dropped on the window, one taken in each frame (drop_frame)
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
    // the keyboard of SAPI: a key went to it from a display in this run (the hint "Click here and type" on the display
    // is not needed any more); a typed character was dropped because no display had the keyboard (told once a run in
    // the status bar, and each time by the hint for a few seconds)
    bool typed = false;
    bool drop_told = false;
    uint64_t drop_ns = 0;
    // a short text over the pictures for 2.5 s when the pause or the speed changes (F5, F8, the menu, MCP); the pause
    // and the speed seen last frame (speed: the percent, 0 = maximum, -1 = not seen yet)
    std::string osd;
    uint64_t osd_ns = 0;
    bool osd_paused = false;
    int osd_speed = -1;
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
    // View -> Settings -> Windows: floating (default) or docked (layout.cpp, layout_dock); --ui-layout takes its place
    // for one run, and then imgui.ini is neither read nor written (the places of the saved layout stay). Each layout
    // has its own file: imgui.ini (floating), imgui-docked.ini (ini_docked_path); a choice in the settings switches the
    // files between two frames (layout_switch).
    bool docked = false;
    bool layout_override = false;
    bool override_docked = false;
    std::string ini_docked_path;
    bool layout_switch = false;
    bool dock_follow = false;     // a machine changed: its main output into the middle of the dock (layout_dock_main)
    bool restyle = true;          // build the style before the next frame
    bool snapshot = false;        // a self-check: "as the system says" is light, the pictures do not depend on the PC
    float dpi = 1.0f;             // the scale of the display of the window (View -> Settings: text size on top)
    bool dpi_fixed = false;       // --ui-scale: the scale of a snapshot, not of the display
    bool style_editor = false;    // Debug -> Style editor (debug builds)
    std::string snap_menu;        // UI snapshot (--ui-menu): the menu kept open, its English name
    std::string snap_click;       // UI snapshot (--ui-window-item, --ui-front): a window item clicked in this frame
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

// The key of a display window in the settings, its id after "###": "screen" (k -1), "gfx0"... (k: gfx[k]).
static void display_key(char* out, size_t n, int k)
{
    if (k < 0) snprintf(out, n, "screen");
    else snprintf(out, n, "gfx%d", k);
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
    if (app.fullscreen != FS_OFF) maximized = app.fs_maximized;   // as it comes back when the full screen ends
    j["window"] = { { "x", app.win_x }, { "y", app.win_y }, { "w", app.win_w }, { "h", app.win_h },
                    { "position", app.win_pos_known }, { "maximized", maximized } };
    j["windows"] = { { "debugger", ui.show_debugger }, { "memory", ui.show_memory }, { "terminal", ui.show_terminal },
                     { "io_log", ui.show_iolog }, { "log", ui.show_log }, { "media", ui.show_media },
                     { "printer", ui.show_printer }, { "configuration", ui.show_rack },
                     { "configuration_auto", ui.show_rack && ui.rack_by_layout }, { "diag", ui.show_diag },
                     { "aps5", ui.show_aps5 }, { "minigraf", ui.show_plotter }, { "ims2", ui.show_ims2 } };
    j["screen"] = { { "integer_scale", app.integer_scale },
                    { "tv_aspect", app.tv_aspect }, { "smooth", app.smooth }, { "card_types", ui.rack.show_types } };
    j["terminal"] = { { "channel", ui.terminal_channel }, { "emulation", ui.term_emulation },
                      { "cr_newline", ui.term_cr_newline }, { "local_echo", ui.term_local_echo },
                      { "copy_on_select", ui.term_copy_on_select }, { "tcp_port", ui.tcp_port }, { "tcp_protocol", ui.tcp_protocol },
                      { "tcp_dcd_follows", ui.tcp_dcd_follows }, { "com_port", ui.com_port } };
    j["speed"] = { { "max", app.host.max_speed }, { "percent", app.host.pace_percent } };
    j["sound"] = { { "enabled", app.audio.enabled }, { "volume", app.audio.volume } };
    // each display window: the zoom chosen from its menu (0 = none) and whether it was sized by hand (settings_apply)
    nlohmann::json disp = nlohmann::json::object();
    for (int k = -1; k < MAX_GFX_VIEWS; k++) {
        const ZoomState& zs = k < 0 ? app.screen_zs : app.gfx[k].zs;
        char key[8];
        display_key(key, sizeof(key), k);
        int zoom = zs.request ? zs.request : (int)(zs.fixed * 100.0f + 0.5f);
        disp[key] = { { "zoom", zoom }, { "by_hand", zs.user } };
    }
    j["displays"] = disp;
    j["screenshot"] = { { "enlarged", ui.shot_enlarged } };
    if (!ui.ffmpeg_path.empty()) j["video"] = { { "ffmpeg", ui.ffmpeg_path } };
    // the joystick from the keyboard and the PC mouse are off at every start (author): not saved
    j["mouse"] = { { "sensitivity", app.mouse.sensitivity } };
    j["cassette"] = { { "last_dir", ui.tape_dlg.last_dir } };
    j["load"] = { { "binary_address", std::string(ui.bin_addr) } };
    if (!app.language.empty()) j["language"] = app.language;
    j["appearance"] = { { "theme", k_themes[app.look].key }, { "mode", theme_mode_key(app.look_mode) },
                        { "text_percent", app.text_percent }, { "layout", app.docked ? "docked" : "floating" } };
    j["browser"] = { { "recent", ui.recent } };
    j["mcp"] = { { "enabled", app.mcp_pref }, { "port", app.mcp_pref_port } };
    j["guides"] = { { "auto", ui.guides_auto }, { "hidden", ui.guides_hidden } };
    // the configurator: changes kept for the next start (Unsaved changes -> Keep for the next start)
    if (ui.rack.kept.is_object() && !ui.rack.kept.empty()) j["kept_machines"] = ui.rack.kept;
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
    get_opt(w, "configuration_auto", ui.rack_by_layout);   // opened by the layout (ui.h)
    if (!ui.show_rack) ui.rack_by_layout = false;
    const nlohmann::json& s = j.value("screen", nlohmann::json::object());
    get_opt(s, "integer_scale", app.integer_scale);
    get_opt(s, "tv_aspect", app.tv_aspect);
    get_opt(s, "smooth", app.smooth);
    get_opt(s, "card_types", ui.rack.show_types);
    get_opt(j.value("browser", nlohmann::json::object()), "recent", ui.recent);
    if (j.contains("kept_machines") && j["kept_machines"].is_object()) ui.rack.kept = j["kept_machines"];
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
    std::string layout;
    get_opt(ap, "layout", layout);
    app.docked = layout == "docked";
    ThemeVariant v;
    ThemeMode md;
    if (theme_parse(look.c_str(), &v, &md)) app.look = v;   // a look no longer offered: the default one
    if (theme_parse(mode.c_str(), &v, &md)) app.look_mode = md;
    if (app.text_percent < 100 || app.text_percent > 200) app.text_percent = 100;
    app.look_override = theme_override(&v, &md);
    app.restyle = true;
    // the display windows as they were left (author 2026-10-29: what one sets stays): the zoom chosen, the window
    // exactly that size at its first frame (zoom_resize; its place from imgui.ini), or sized by hand (a machine change
    // keeps its size). The floating layout only: a docked display fits its dock.
    const nlohmann::json& dz = j.value("displays", nlohmann::json::object());
    for (int k = -1; k < MAX_GFX_VIEWS && !app.docked; k++) {
        ZoomState& zs = k < 0 ? app.screen_zs : app.gfx[k].zs;
        char key[8];
        display_key(key, sizeof(key), k);
        const nlohmann::json& e = dz.value(key, nlohmann::json::object());
        int zoom = 0;
        get_opt(e, "zoom", zoom);
        get_opt(e, "by_hand", zs.user);
        for (int p : k_zoom_steps)
            if (zoom == p) {
                zs.fixed = p / 100.0f;   // the layout plans with it (plan_layout, place_display)
                zs.request = p;
            }
    }
}

void set_title(App& app);

// View -> Language: switches at once (the texts are translated every frame, the title of the window here) and is saved.
void set_ui_language(App& app, UiLanguage lang)
{
    i18n_set_language(lang);
    app.language = i18n_code(lang);
    set_title(app);
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
    theme_titlebar(app.window);   // after SDL: it sets the title bar by the system (theme.cpp)
}

// The content scale of a display (125 % = 1.25); 1 when SDL does not know it.
static float display_scale(SDL_DisplayID display)
{
    float s = display ? SDL_GetDisplayContentScale(display) : 0.0f;
    return s > 0 ? s : 1.0f;
}

// The window moved to a monitor with another scale, or the scale of its monitor changed: the style is built again
// for it (between frames, app.restyle). The windows of ImGui keep their place and size in pixels, as SDL keeps the
// size of the OS window in pixels. --ui-scale wins.
static void follow_display_scale(App& app)
{
    if (app.dpi_fixed || !app.window) return;
    float dpi = display_scale(SDL_GetDisplayForWindow(app.window));
    if (dpi == app.dpi) return;
    LOGI("display scale %.0f %% (was %.0f %%)", dpi * 100.0f, app.dpi * 100.0f);
    app.dpi = dpi;
    app.restyle = true;
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

int g_layout_override = -1;   // --ui-layout: 0 floating, 1 docked, -1 as the settings say

// The window layout in use: --ui-layout until one is chosen in the settings, else the saved one.
static bool layout_docked(const App& app)
{
    return app.layout_override ? app.override_docked : app.docked;
}

// The file of the window places of a layout: imgui.ini of the floating windows, imgui-docked.ini of the docked ones
// (FILE.imgui-docked.ini beside FILE.imgui.ini for --settings FILE); empty: none (a self-check).
static const std::string& layout_ini(const App& app, bool docked)
{
    return docked ? app.ini_docked_path : app.ini_path;
}

static std::string ini_docked_name(const std::string& ini)
{
    const char* tail = "imgui.ini";
    size_t n = strlen(tail);
    if (ini.empty()) return std::string();
    if (ini.size() >= n && ini.compare(ini.size() - n, n, tail) == 0) return ini.substr(0, ini.size() - n) + "imgui-docked.ini";
    return ini + ".docked";
}

// View -> Settings -> Windows: saved, it replaces --ui-layout. Each layout keeps its own window places (layout_ini):
// between two frames (switch_layout) the places of the layout left go into its file and those of the chosen one come
// from its file; the first time (no file yet) the windows take its default places, as View -> Default window layout.
static void set_layout(App& app, bool docked)
{
    if (layout_docked(app) == docked) return;
    app.docked = docked;
    app.layout_switch = true;
}

static void switch_layout(App& app)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.IniFilename) ImGui::SaveIniSettingsToDisk(io.IniFilename);   // not for --ui-layout: the saved ones stay
    app.layout_override = false;
    const std::string& ini = layout_ini(app, app.docked);
    ImGui::ClearIniSettings();   // every window undocked, no saved place
    for (int k = -1; k < MAX_GFX_VIEWS; k++) {   // the display windows of that layout have the sizes saved in it
        ZoomState& zs = k < 0 ? app.screen_zs : app.gfx[k].zs;
        zs.fixed = 0;
        zs.request = 0;
        zs.user = false;
    }
    io.IniFilename = ini.empty() ? nullptr : ini.c_str();
    if (!ini.empty() && file_exists(ini)) {
        ImGui::LoadIniSettingsFromDisk(ini.c_str());   // the windows take their places (and docks) from it
        layout_take_default_unsaved();                 // those with none in it their default places
        app.dock_follow = app.docked;                  // saved with another machine: its main output in the middle
    } else {
        app.ui.layout_reset = true;
    }
    LOGI("window layout: %s (%s)", app.docked ? "docked" : "floating", ini.empty() ? "not saved" : ini.c_str());
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

// The usable area of a display (without the task bar) and its scale: of the display under `at` when its place is
// known, else of the primary display.
static bool usable_area(const SDL_Rect* at, SDL_Rect* out, float* scale)
{
    SDL_DisplayID id = at ? SDL_GetDisplayForRect(at) : 0;
    if (!id) id = SDL_GetPrimaryDisplay();
    *scale = display_scale(id);
    return id && SDL_GetDisplayUsableBounds(id, out) && out->w > 0 && out->h > 0;
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
    std::string title = std::string("SAPIemu - ") + (m ? machine_title(app.ui, app.host.machine_file(), m->name) : "");
    if (m && app.host.paused) title += std::string(" ") + tr("[paused]");   // seen on the task bar too
    SDL_SetWindowTitle(app.window, title.c_str());
}

// Machine -> Switch off and on, Ctrl+F12, as the power switch of the real machine (EmuHost::power_cycle): the same
// cards, switches and media, the memory lost, the RAM disks and battery memories saved and read again. Not the file
// again (as Restart in the window Choose a machine): a medium put in stays in, and the configurator is not asked (it
// follows the machine and keeps its unsaved changes; changes waiting for a restart stay orange). A failure: Error.
static void power_cycle(App& app)
{
    std::string err;
    if (!app.host.power_cycle(err)) {   // the old machine was not switched off (EmuHost::load_config)
        app.error_text = std::string(tr("The machine cannot be started")) + ":\n" + tr_core(err) + "\n\n" +
                         tr("The machine runs on as it was.");
        app.show_error = true;
        LOGE("%s", err.c_str());
    }
    set_title(app);
}

// The pause or the speed changed (F5, F8, the menu, a breakpoint, MCP): a short text over the pictures, the pause
// also in the title of the window. The state at the start of the program is not announced.
static void osd_follow(App& app)
{
    bool paused = app.host.paused && app.host.machine();
    int speed = app.host.max_speed ? 0 : app.host.pace_percent;
    bool first = app.osd_speed < 0;
    std::string text;
    if (paused != app.osd_paused) {
        app.osd_paused = paused;
        set_title(app);
        if (paused) text = app.host.status.empty() ? tr("Pause (F5)") : app.host.status;   // a breakpoint says why
    }
    if (speed != app.osd_speed) {
        app.osd_speed = speed;
        char t[96];
        if (speed) snprintf(t, sizeof(t), "%s %d %%", tr("Speed"), speed);
        else snprintf(t, sizeof(t), "%s", tr("Speed: maximum"));
        text = t;
        Machine* m = app.host.machine();   // the sound plays only at 100 % (audio_frame): said where there is one
        if (speed != 100 && app.audio.enabled && !app.audio.failed && m && m->bus.audio_rate)
            text += std::string(" (") + tr("no sound") + ")";
    }
    if (first || text.empty()) return;
    app.osd = text;
    app.osd_ns = SDL_GetTicksNS();
}

void load_machine(App& app, const std::string& file)
{
    std::string err;
    if (!rack_load_machine(app.ui, file, err)) {   // with the changes kept for the next start
        app.error_text = std::string(tr("Cannot load machine")) + ":\n" + tr_core(err);
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
static void fullscreen_toggle(App& app);

static void paste_keyboard(App& app, Machine* m);

void handle_key(App& app, const SDL_KeyboardEvent& ev)
{
    Machine* m = app.host.machine();
    if (ImGui::GetIO().WantTextInput) return;   // typing into a text field: no shortcuts, nothing for SAPI
    // Alt+Enter: full screen on / off, before Enter goes to SAPI (AltGr = Ctrl+Alt is not it). Esc stays the key of SAPI
    // and F11 the step of the debugger.
    if ((ev.key == SDLK_RETURN || ev.key == SDLK_KP_ENTER) && (ev.mod & SDL_KMOD_ALT) &&
        !(ev.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) {
        if (!ev.repeat) fullscreen_toggle(app);
        return;
    }
    // Ctrl+Shift+C: the text of the console screen to the clipboard (Ctrl+C alone goes to SAPI as a control code); in
    // the serial terminal its own text, and Ctrl+Shift+V pastes there (as Alt+C, Alt+V; Windows Terminal does the same)
    bool ctrl_shift =
        (ev.mod & SDL_KMOD_CTRL) && (ev.mod & SDL_KMOD_SHIFT) && !(ev.mod & (SDL_KMOD_ALT | SDL_KMOD_GUI));
    if (ctrl_shift && ev.key == SDLK_C) {
        if (ev.repeat) return;
        if (app.ui.terminal_focused) terminal_copy(app.ui);
        else copy_screen_text(app.ui);
        return;
    }
    if (ctrl_shift && ev.key == SDLK_V && app.ui.terminal_focused && m) {
        if (!ev.repeat) terminal_paste(app.ui);
        return;
    }
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
    case SDLK_F12:   // Ctrl+F12: switch off and on. Under a debugger of the PC Windows takes F12 as its break hotkey
                     // (UserDebuggerHotKey): then Machine -> Reset.
        if (ev.repeat) return;   // a held key: one power cycle, one reset (a switch does not repeat)
        if (ev.mod & SDL_KMOD_CTRL) {
            if (m) power_cycle(app);
            return;
        }
        request_reset(app.ui);   // the APS-5 unit asks first
        return;
    case SDLK_F9:   // the breakpoint, as in Visual Studio (F5, F10, F11 too); the reset was here before 2026-10-29
        if (!ev.repeat) debug_toggle_breakpoint(app.ui);
        return;
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
    if (!app.ui.screen_focused) return;   // a character is told about in handle_text
    app.typed = true;
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
    if (!app.ui.screen_focused) {
        // no display has the keyboard (another window was clicked): the character is dropped, as before; said once
        // a run, in the status bar (not into a text field, where it went)
        if (!ImGui::GetIO().WantTextInput) {
            app.drop_ns = SDL_GetTicksNS();   // the hint shows again for a while
            if (!app.drop_told) {
                app.drop_told = true;
                ui_notice(app.ui, true, tr("Typing does not reach SAPI now: click its screen first."));
            }
        }
        return;
    }
    app.typed = true;
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

// The size of a display window that shows its picture at zoom k (screen pixels per emulated pixel), with a small
// border around the picture.
static ImVec2 display_window_size(App& app, DisplayDevice* d, float k)
{
    float fb = ImGui::GetIO().DisplayFramebufferScale.x;
    if (fb <= 0) fb = 1.0f;
    float pad = 2.0f * (k_display_pad + k_zoom_margin);
    return ImVec2(d->screen_width() * display_px_aspect(app, d) * k / fb + pad,
                  d->screen_height() * k / fb + pad + ImGui::GetFrameHeight());
}

// The largest zoom the layout gives a display by itself (author 2026-10-29: 200 % at the Windows scale 100 % looks
// good, 400 % monstrous and blocky): about 200 % of the picture as large on any display, so proportionally more at a
// higher scale (125 % and 150 %: 300 %). A zoom chosen from the menu and a saved layout are not limited.
static float display_zoom_cap(App& app)
{
    int cap = (int)(2.0f * app.dpi + 0.5f);
    return (float)(cap < 1 ? 1 : cap);
}

// The largest integer zoom whose window fits w x h; below 1:1 the largest zoom that fits.
static float display_fit_zoom(App& app, DisplayDevice* d, float w, float h)
{
    ImVec2 one = display_window_size(app, d, 1.0f), zero = display_window_size(app, d, 0.0f);
    if (one.x > w || one.y > h) {
        float kx = (w - zero.x) / (one.x - zero.x), ky = (h - zero.y) / (one.y - zero.y);
        float k = kx < ky ? kx : ky;
        return k > 0.1f ? k : 0.1f;
    }
    int k = 1;
    while (k < 16) {
        ImVec2 s = display_window_size(app, d, (float)(k + 1));
        if (s.x > w || s.y > h) break;
        k++;
    }
    return (float)k;
}

// Before Begin: a zoom chosen from the menu resizes the window to fit the picture at it.
void zoom_resize(App& app, DisplayDevice* d, ZoomState& zs)
{
    if (!zs.request || !d) return;
    float k = zs.request / 100.0f;
    zs.request = 0;
    ImVec2 s = display_window_size(app, d, k);
    ImGui::SetNextWindowDockID(0, ImGuiCond_Always);   // a docked window takes the size of its dock: undock it
    ImGui::SetNextWindowSize(s);
    zs.fixed = k;
    zs.size = s;
    zs.user = false;   // the size of the zoom chosen: a machine change gives the new picture that zoom (refit_displays)
}

// The graphic displays of the machine that have a window (draw_screens): the window ###gfx<k> shows out[k].
static int graphic_displays(Machine* m, DisplayDevice** out)
{
    int n = 0;
    for (DisplayDevice* d : m ? m->displays : std::vector<DisplayDevice*>())
        if (d != m->display && n < MAX_GFX_VIEWS) out[n++] = d;
    return n;
}

// The widest window the machine opens beside its output (APS-5, IMS-2, a plotter, Media of "windows"), 0 = none.
static float machine_side_width(App& app, Machine* m)
{
    UiState& ui = app.ui;
    std::string plant;
    if (m) {
        auto p = m->config.find("plant");
        if (p != m->config.end() && p->is_object()) plant = p->value("type", std::string());
    }
    bool plotter = ui.show_plotter;
    for (size_t i = 0; m && !plotter && i < m->cards.size(); i++) plotter = m->cards[i]->plotter() != nullptr;
    float w = 0;
    auto take = [&](bool open, int lw) {
        float x = open ? layout_width(lw) : 0.0f;
        if (x > w) w = x;
    };
    take(ui.show_aps5 || plant == "APS-5", LW_APS5);
    take(ui.show_ims2 || plant == "IMS-2", LW_IMS2);
    take(plotter, LW_PLOTTER);
    take(ui.show_media, LW_MEDIA);
    return w;
}

// The display named by "main_display" in the machine description (its card name or type): k = gfx[k], -1 = none
// (the console screen is the main output).
static int main_output(Machine* m, DisplayDevice** gfx, int n)
{
    if (!m) return -1;
    auto md = m->config.find("main_display");
    if (md == m->config.end() || !md->is_string()) return -1;
    const std::string& want = md->get_ref<const std::string&>();
    for (int k = 0; k < n; k++) {
        Card* c = dynamic_cast<Card*>(gfx[k]);
        if (c && (c->name == want || c->type == want)) return k;
    }
    return -1;
}

// The display used last (app.shot_target): k = gfx[k], -1 = the console screen. When it is gone, the console screen;
// a machine without one: its main output or its first graphic display. *d = its device, nullptr when there is none.
static int used_display(App& app, DisplayDevice** d)
{
    Machine* m = app.host.machine();
    DisplayDevice* gfx[MAX_GFX_VIEWS];
    int n = graphic_displays(m, gfx);
    int k = app.shot_target < n ? app.shot_target : -1;
    if (k < 0 && !(m && m->display) && n > 0) {
        k = main_output(m, gfx, n);
        if (k < 0) k = 0;
    }
    *d = k >= 0 ? gfx[k] : (m ? m->display : nullptr);
    return k;
}

static ZoomState& display_zoom(App& app, int k)
{
    return k >= 0 && k < MAX_GFX_VIEWS ? app.gfx[k].zs : app.screen_zs;
}

static const char* display_name(DisplayDevice* d, const char* none)
{
    Card* c = dynamic_cast<Card*>(d);
    return c ? c->name.c_str() : none;
}

// View -> Displays: the display window k (-1 the console screen, k = gfx[k]) is hidden.
static bool& display_hidden(App& app, int k)
{
    return app.display_hidden[k + 1];
}

// The display used last after it was hidden (View -> Displays, full screen left): the main output when it is shown,
// else the console screen, else the first graphic display shown; -1 when every one is hidden.
static int shown_display(App& app)
{
    Machine* m = app.host.machine();
    DisplayDevice* gfx[MAX_GFX_VIEWS];
    int n = graphic_displays(m, gfx);
    int main = main_output(m, gfx, n);
    if (main >= 0 && !display_hidden(app, main)) return main;
    if (!display_hidden(app, -1)) return -1;
    for (int k = 0; k < n; k++)
        if (!display_hidden(app, k)) return k;
    return -1;
}

// View -> Displays: the window of the display k shown or hidden. A hidden one is not drawn (the emulation goes on); the
// display used last moves to one shown. One shown again comes to the front with the keyboard (draw_screens).
static void display_show(App& app, int k, bool show)
{
    display_hidden(app, k) = !show;
    if (show) {
        app.display_front = k;
    } else if (app.shot_target == k) {
        // it had the keyboard before the menu opened: the display used now takes it (else ImGui gives the focus to
        // the window on top, maybe Media: typing would not reach SAPI)
        if (app.screen_keys) app.ui.focus_display = 2;
        app.shot_target = shown_display(app);
    }
}

// Full screen on (k: the display, -1 the console screen, k = gfx[k]) or off (FS_OFF): the OS window over the whole
// monitor (SDL_SetWindowFullscreen: borderless, the desktop mode), the display alone in it (draw_fullscreen). A short
// text over the picture says how to leave it. Off: the keyboard back to the window of that display. A self-check keeps
// its hidden window at --ui-size (the picture does not depend on the monitor of the PC).
static void fullscreen_set(App& app, int k)
{
    bool on = k != FS_OFF, was = app.fullscreen != FS_OFF;
    app.fullscreen = k;
    if (on) app.shot_target = k;   // it is the display used last
    if (on == was) return;   // another display: only the picture changes
    if (on) {
        app.fs_maximized = (SDL_GetWindowFlags(app.window) & SDL_WINDOW_MAXIMIZED) != 0;
        app.osd = tr("Full screen: Alt+Enter leaves it");
        app.osd_ns = SDL_GetTicksNS();
    } else {
        // the keyboard back to the window of that display; one hidden in View -> Displays stays hidden
        if (display_hidden(app, app.shot_target)) app.shot_target = shown_display(app);
        app.ui.focus_display = 2;
        app.osd.clear();   // its text would come over every display window
    }
    if (!app.snapshot && !SDL_SetWindowFullscreen(app.window, on)) LOGW("full screen: %s", SDL_GetError());
}

// Alt+Enter, View -> Full screen: the display used last over the whole monitor, or back.
static void fullscreen_toggle(App& app)
{
    if (app.fullscreen != FS_OFF) {
        fullscreen_set(app, FS_OFF);
        return;
    }
    DisplayDevice* d = nullptr;
    int k = used_display(app, &d);
    if (d) fullscreen_set(app, k);
}

// The first size of the main output d of machine m in a work area w x h: the largest integer zoom whose window fits
// about 65 % of the width and the height, at most display_zoom_cap; when a window the machine opens beside it
// (APS-5...) would not fit next to it, the zoom goes down, to 2 at least. No display (switched off): a dark screen of
// 65 % of the width. *zoom: the zoom (0 without a display).
static ImVec2 main_output_size(App& app, Machine* m, DisplayDevice* d, float w, float h, float gap, float* zoom = nullptr)
{
    ImVec2 s(0.65f * w, 0.65f * w * 0.75f);
    float k = 0;
    if (d) {
        k = display_fit_zoom(app, d, 0.65f * w, h);
        if (k > display_zoom_cap(app)) k = display_zoom_cap(app);
        float side = machine_side_width(app, m);
        while (k > 2.0f && display_window_size(app, d, k).x + gap + side > w) k -= 1.0f;
        s = display_window_size(app, d, k);
    }
    if (s.y > h) s.y = h;
    if (zoom) *zoom = k;
    return s;
}

// Before the windows of a frame: the main output of the machine and its first place (layout.cpp). It is the display
// named by "main_display" in the machine description (the games: CGA-1V, BGP-1S), else the console screen; at the top
// left, in the size of main_output_size, or at the zoom chosen for it from the menu (kept by View -> Arrange open
// windows). Not while the windows take no first places (full screen, minimized).
static void plan_layout(App& app)
{
    UiState& ui = app.ui;
    if (layout_held()) return;
    float w, h, gap;
    layout_work_size(&w, &h, &gap);
    Machine* m = app.host.machine();
    DisplayDevice* gfx[MAX_GFX_VIEWS];
    int n = graphic_displays(m, gfx);
    int main = main_output(m, gfx, n);
    float fixed = display_zoom(app, main).fixed;
    float key[5] = { w, h, gap, ImGui::GetIO().DisplayFramebufferScale.x, fixed };
    unsigned flags = (ui.show_aps5 ? 1u : 0u) | (ui.show_ims2 ? 2u : 0u) | (ui.show_plotter ? 4u : 0u) |
                     (ui.show_media ? 8u : 0u) | (app.tv_aspect ? 16u : 0u);
    uint64_t gen = app.host.machine_generation();
    if (gen == app.plan_gen && flags == app.plan_flags && !memcmp(key, app.plan_key, sizeof(key))) return;
    app.plan_gen = gen;
    app.plan_flags = flags;
    memcpy(app.plan_key, key, sizeof(key));
    app.main_output = main;
    DisplayDevice* d = main >= 0 ? gfx[main] : (m ? m->display : nullptr);
    char id[16];
    if (main >= 0) snprintf(id, sizeof(id), "###gfx%d", main);
    else snprintf(id, sizeof(id), "###screen");
    ImVec2 s = main_output_size(app, m, d, w, h, gap, &app.main_k);
    if (d && fixed > 0) {
        s = display_window_size(app, d, fixed);
        app.main_k = fixed;
    }
    app.main_w = s.x;
    layout_main(id, s.x, s.y, n);
}

// Before Begin of a display window: its first place (layout.cpp). The main output where plan_layout put it; another
// display next to it (below it, else right of it) at the largest integer zoom up to display_zoom_cap that has free room
// (that fits half the height when the machine opens an instrument or Media beside it), down to 1:1, then 3/4 of 1:1
// (drawn smooth below 1:1, draw_display: the letters keep their strokes), else at 1:1 over the windows that matter
// least. The window is as large as the picture at that zoom with the small border (display_window_size). Another
// display not much larger than the main output: its picture at most 1.5 x as wide and as high as that of the main output
// (down to 1:1; a game's CGA-1V at 200 % stays the largest, the AND-1V of its console menu goes to 100 %, DGD-1 beside
// AND-1 keeps 200 %). A zoom chosen from the menu (zs.fixed, kept by View -> Arrange open windows) stays.
// The zoom the layout gives the display d of window `id` by itself (place_display, refit_displays): the main output that
// of plan_layout; another display the largest integer zoom up to display_zoom_cap that fits the work area (half its
// height when the machine opens an instrument or Media beside it), its picture at most 1.5 x as wide and as high as that
// of the main output (down to 1:1).
static float display_layout_zoom(App& app, DisplayDevice* d, const char* id)
{
    Machine* m = app.host.machine();
    DisplayDevice* gfx[MAX_GFX_VIEWS];
    int n = graphic_displays(m, gfx);
    DisplayDevice* md = app.main_output >= 0 && app.main_output < n ? gfx[app.main_output] : (m ? m->display : nullptr);
    char main_id[16];
    if (app.main_output >= 0) snprintf(main_id, sizeof(main_id), "###gfx%d", app.main_output);
    else snprintf(main_id, sizeof(main_id), "###screen");
    if (!strcmp(id, main_id) && app.main_k > 0) return app.main_k;
    float w, h, gap;
    layout_work_size(&w, &h, &gap);
    if (machine_side_width(app, m) > 0) h = (h - gap) * 0.5f;
    float k = display_fit_zoom(app, d, w, h);
    if (k > display_zoom_cap(app)) k = display_zoom_cap(app);
    if (md && md != d && app.main_k > 0) {
        ImVec2 mz = display_window_size(app, md, 0.0f), ms = display_window_size(app, md, app.main_k);
        ImVec2 z = display_window_size(app, d, 0.0f);
        for (; k > 1.0f; k -= 1.0f) {
            ImVec2 s = display_window_size(app, d, k);
            if (s.x - z.x <= 1.5f * (ms.x - mz.x) + 0.5f && s.y - z.y <= 1.5f * (ms.y - mz.y) + 0.5f) break;
        }
    }
    return k;
}

static void place_display(App& app, DisplayDevice* d, const char* id, const ZoomState& zs)
{
    if (!layout_first_use(id)) return;
    if (!d) {
        layout_display(id, app.main_w, app.main_w * 0.75f, 0, 0, 0);
        return;
    }
    // a zoom chosen from the menu; the main output where another window is already: its zoom, or less where there is room
    float k = zs.fixed > 0 ? zs.fixed : display_layout_zoom(app, d, id);
    ImVec2 zero = display_window_size(app, d, 0.0f), one = display_window_size(app, d, 1.0f);
    layout_display(id, zero.x, zero.y, one.x - zero.x, one.y - zero.y, k, zs.fixed > 0);
}

// A machine change (author 2026-10-29: after choosing a machine its AND-1Z showed at 100 % in a window much larger than
// the picture): every display window of the new machine keeps its place and takes the size of its new picture at the
// zoom the layout gives it (display_layout_zoom: the cap, the small border), as at the first start and after View ->
// Default window layout; inside the work area, a lower zoom when it does not fit (layout_refit). A window given a zoom
// from the menu (zs.fixed) takes the size of its new picture at that zoom (fit to the window again when it does not fit
// the work area). Not a window sized by hand (ZoomState::user, kept for the next start), docked, or taking its first
// place now. The first machine of the run keeps the saved sizes (imgui.ini; a zoom chosen saved in the settings sizes its
// window at the first frame, settings_apply); a restart of the same machine (the same file, the same
// display cards) and switching off and on change nothing. Before the windows of the frame, after plan_layout (main_k):
// the windows opening now see the new sizes.
// The machine file and its displays (the card names, the picture sizes): another machine when it changes.
static std::string displays_sig(App& app, Machine* m)
{
    DisplayDevice* gfx[MAX_GFX_VIEWS];
    int n = graphic_displays(m, gfx);
    std::string sig = app.host.machine_file();
    for (int k = -1; k < n; k++) {
        DisplayDevice* d = k < 0 ? m->display : gfx[k];
        char part[96];
        if (d) snprintf(part, sizeof(part), "|%s %dx%d", display_name(d, "?"), d->screen_width(), d->screen_height());
        else snprintf(part, sizeof(part), "|-");
        sig += part;
    }
    return sig;
}

static void refit_displays(App& app)
{
    Machine* m = app.host.machine();
    uint64_t gen = app.host.machine_generation();
    // switched off, full screen, minimized: later; a machine loaded in this frame (a menu): after its windows open
    // (open_machine_windows at the next frame, machine_side_width)
    if (gen == app.fit_gen || gen != app.title_gen || !m || layout_held()) return;
    app.fit_gen = gen;
    DisplayDevice* gfx[MAX_GFX_VIEWS];
    int n = graphic_displays(m, gfx);
    std::string sig = displays_sig(app, m);
    bool first = app.fit_sig.empty();
    if (sig == app.fit_sig) return;
    app.fit_sig = sig;
    if (first) return;
    float w, h, gap;
    layout_work_size(&w, &h, &gap);
    for (int k = -1; k < n; k++) {
        DisplayDevice* d = k < 0 ? m->display : gfx[k];
        ZoomState& zs = display_zoom(app, k);
        if (!d || zs.user) continue;
        char id[16];
        if (k >= 0) snprintf(id, sizeof(id), "###gfx%d", k);
        else snprintf(id, sizeof(id), "###screen");
        float z = zs.fixed > 0 ? zs.fixed : display_layout_zoom(app, d, id), fit = display_fit_zoom(app, d, w, h);
        if (z > fit) {
            z = fit;
            zs.fixed = 0;   // the zoom chosen does not fit the work area: the picture fits its window
        }
        ImVec2 s = display_window_size(app, d, z);
        if (zs.fixed > 0) zs.size = s;   // zoom_track: the window keeps the zoom chosen
        layout_refit(id, s.x, s.y);
    }
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

// Focus `target` (the keyboard goes to it). Focusing raises a window, so the windows that were in front of it stay in
// front (the screen would cover the Minigraf window with READY, the Media window, the guide), then `top` on top.
static void focus_keeping_order(ImGuiWindow* target, const ImVector<ImGuiWindow*>& top)
{
    ImGuiContext& g = *GImGui;
    int at = g.Windows.index_from_ptr(g.Windows.find(target->RootWindowDockTree));
    ImVector<ImGuiWindow*> front;   // back to front, as g.Windows
    for (int i = at + 1; i < g.Windows.Size; i++) {
        ImGuiWindow* w = g.Windows[i];
        if (w->RootWindow == w && (w->Active || w->WasActive) && !w->DockIsActive &&
            !(w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip |
                          ImGuiWindowFlags_NoBringToFrontOnFocus)))
            front.push_back(w);
    }
    ImGui::FocusWindow(target);
    for (ImGuiWindow* w : front) ImGui::BringWindowToDisplayFront(w);
    for (ImGuiWindow* w : top) ImGui::BringWindowToDisplayFront(w);
}

// The window of the main output (plan_layout): the console screen, or the display named by "main_display" (the games:
// CGA-1V, BGP-1S); the console screen when that one has no window (yet, or hidden in View -> Displays); nullptr when
// that is hidden too.
static ImGuiWindow* main_output_window(App& app)
{
    ImGuiWindow* w = nullptr;
    if (app.main_output >= 0) {
        char id[16];
        snprintf(id, sizeof(id), "###gfx%d", app.main_output);
        w = ImGui::FindWindowByName(id);
        if (w && !(w->Active || w->WasActive)) w = nullptr;
    }
    if (!w) w = ImGui::FindWindowByName("###screen");
    return w && (w->Active || w->WasActive) ? w : nullptr;
}

// The keyboard to the main output after a machine starts (it is the display used last then, app.shot_target); the
// windows of the machine (top_windows) come on top, in a dock their tab is selected.
static void focus_screen_window(App& app)
{
    ImGuiWindow* screen = main_output_window(app);
    if (!screen) return;
    ImVector<ImGuiWindow*> top;
    for (const char* t : app.top_windows) {
        ImGuiWindow* w = ImGui::FindWindowByName(tr_id(t));
        if (!w || !(w->Active || w->WasActive)) continue;
        if (w->DockIsActive && w->DockNode && w->DockNode->TabBar)   // in a dock: its tab selected
            w->DockNode->TabBar->NextSelectedTabId = w->TabId;
        // not the dock space over the main window (it would cover the floating windows)
        ImGuiWindow* root = w->RootWindowDockTree;
        if (root != screen->RootWindowDockTree && !(root->Flags & ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            ImGui::FocusWindow(w);
            top.push_back(root);
        }
    }
    focus_keeping_order(screen, top);
}

// The keyboard back to the display used last (app.shot_target; the console screen when that window is gone), asked
// by ui.focus_display: after Type into SAPI, Paste text, a program loaded, a medium put in, the guide or the window
// Choose a machine closed. No other window comes to the front.
static void focus_display_window(App& app)
{
    ImGuiWindow* w = nullptr;
    if (app.shot_target >= 0) {
        char id[16];
        snprintf(id, sizeof(id), "###gfx%d", app.shot_target);
        w = ImGui::FindWindowByName(id);
        if (w && !(w->Active || w->WasActive)) w = nullptr;
    }
    if (!w) w = ImGui::FindWindowByName("###screen");
    if (!w || !(w->Active || w->WasActive)) return;
    ImGuiWindow* nav = GImGui->NavWindow;
    if (nav && nav->RootWindow == w) return;   // it has the keyboard
    focus_keeping_order(w, ImVector<ImGuiWindow*>());
}

// The docked layout for the running machine (layout.cpp, layout_dock): its main output in the middle, its other displays
// and the display windows of other machines in the column at the right edge, the tab of its first window ("windows")
// selected, and of the guide when it is open.
static void dock_layout(App& app)
{
    Machine* m = app.host.machine();
    DisplayDevice* gfx[MAX_GFX_VIEWS];
    int n = graphic_displays(m, gfx);
    int main = main_output(m, gfx, n);
    char ids[1 + MAX_GFX_VIEWS][16];
    DockPlan p = {};
    auto add = [&](int k, DisplayDevice* d) {   // k = gfx[k], -1 = the console screen
        if (p.n_displays >= 1 + MAX_GFX_VIEWS) return;
        char* id = ids[p.n_displays];
        if (k >= 0) snprintf(id, sizeof(ids[0]), "###gfx%d", k);
        else snprintf(id, sizeof(ids[0]), "###screen");
        for (int i = 0; i < p.n_displays; i++)
            if (!strcmp(p.displays[i], id)) return;
        p.displays[p.n_displays++] = id;
        if (d && p.n_displays > 1) {   // another display of the machine
            float w = display_window_size(app, d, 1.0f).x;
            if (w > p.display_w) p.display_w = w;
        }
    };
    if (main >= 0) add(main, gfx[main]);
    add(-1, m ? m->display : nullptr);
    for (int k = 0; k < n; k++) add(k, gfx[k]);
    p.machine_displays = p.n_displays;
    for (int k = n; k < MAX_GFX_VIEWS; k++) add(k, nullptr);   // other machines
    p.side_w = machine_side_width(app, m);
    {   // the width the main output has in the floating layout (the work area of the last frame, as layout_dock)
        ImGuiViewport* vp = ImGui::GetMainViewport();
        float gap = 0.5f * ImGui::GetFontSize();
        DisplayDevice* md = main >= 0 ? gfx[main] : (m ? m->display : nullptr);
        if (md) p.main_w = main_output_size(app, m, md, vp->WorkSize.x - 2.0f * gap, vp->WorkSize.y - 2.0f * gap, gap).x;
    }
    std::string tab;
    for (const char* t : app.top_windows) {
        if (!tab.empty()) break;
        tab = std::string("###") + t;
    }
    p.machine_tab = tab.empty() ? nullptr : tab.c_str();
    p.guide = app.ui.show_guide;
    layout_dock(layout_dockspace_id(), p);
}

// The docked layout after a machine change (or a docked layout loaded): the main output of the machine into the
// middle, the display there to its place in the column (layout_dock_main); the rest as the user left it.
static void dock_follow(App& app)
{
    Machine* m = app.host.machine();
    DisplayDevice* gfx[MAX_GFX_VIEWS];
    int main = main_output(m, gfx, graphic_displays(m, gfx));
    char id[16];
    if (main >= 0) snprintf(id, sizeof(id), "###gfx%d", main);
    else snprintf(id, sizeof(id), "###screen");
    layout_dock_main(layout_dockspace_id(), id);
}

// View -> Default window layout: every window undocked, at the place and size it gets at its first use (layout.cpp;
// the windows that give none go to the top left), not collapsed; the screen fits its window again. The windows not
// created in this run lose their saved places too (imgui.ini): they take the default ones when they open. In the docked
// layout (View -> Settings -> Windows) the windows then go into the dock space (dock_layout).
// open_only (View -> Arrange open windows): only the windows open now take their default places; the closed ones keep
// theirs (saved or of this run), a zoom chosen from the menu stays (place_display), the configurator is neither opened
// nor closed by it. Docked: the dock is built again as by Default window layout (the closed windows go into it too).
static void reset_window_layout(App& app, bool open_only)
{
    if (!open_only) layout_forget_closed();
    ImGuiContext& g = *GImGui;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    // the windows undocked, the dock space comes back empty; arranging the floating layout undocks the open windows only
    if (!open_only || layout_docked(app)) ImGui::DockBuilderRemoveNode(layout_dockspace_id());
    for (ImGuiWindow* w : g.Windows) {
        // an open docked window (a child of its dock node until it is undocked)
        if (open_only && w->DockNode && (w->Active || w->WasActive)) ImGui::SetWindowDock(w, 0, ImGuiCond_Always);
        if (w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip |
                        ImGuiWindowFlags_NoTitleBar))
            continue;   // the menu bar, the dock space host, popups
        if (w->DockNodeAsHost) continue;
        if (open_only && !(w->Active || w->WasActive)) continue;
        if (w->DockId) ImGui::SetWindowDock(w, 0, ImGuiCond_Always);
        ImGui::SetWindowCollapsed(w, false);
        ImGui::SetWindowPos(w, ImVec2(vp->WorkPos.x + 20.0f, vp->WorkPos.y + 10.0f));
        // after them (they clear it): the next Begin takes the first use place and size again
        w->SetWindowPosAllowFlags |= ImGuiCond_FirstUseEver;
        w->SetWindowSizeAllowFlags |= ImGuiCond_FirstUseEver;
        w->SetWindowCollapsedAllowFlags |= ImGuiCond_FirstUseEver;
    }
    if (!open_only) {
        app.screen_zs.fixed = 0;
        app.screen_zs.request = 0;
        for (GfxView& v : app.gfx) {
            v.zs.fixed = 0;
            v.zs.request = 0;
        }
    }
    // the displays have the sizes of the layout again (Arrange: the open ones; a zoom chosen stays): not sized by hand
    for (int k = -1; k < MAX_GFX_VIEWS; k++) {
        char id[16];
        if (k >= 0) snprintf(id, sizeof(id), "###gfx%d", k);
        else snprintf(id, sizeof(id), "###screen");
        ImGuiWindow* w = ImGui::FindWindowByName(id);
        ZoomState& zs = display_zoom(app, k);
        if (!open_only || (w && (w->Active || w->WasActive))) zs.user = false;
    }
    if (layout_docked(app)) {
        dock_layout(app);
        return;
    }
    if (open_only) return;
    // the configurator is part of the floating layout where it fits (draw_rack); one the layout opened is decided again
    if (app.ui.show_rack && app.ui.rack_by_layout) app.ui.show_rack = false;
    app.ui.rack_auto = !app.ui.show_rack;
}

// After Begin: resizing the window by hand (or docking it) goes back to fitting. A resize grip or border of the window
// held: it is sized by hand (zs.user; not the sizes the program gives it: the layout, a machine change, the work area).
void zoom_track(ZoomState& zs)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* w = g.CurrentWindow;
    if (!zs.user && g.ActiveId && g.ActiveIdWindow == w && !w->DockIsActive)
        for (int n = 0; n < 4; n++)
            if (g.ActiveId == ImGui::GetWindowResizeCornerID(w, n) ||
                g.ActiveId == ImGui::GetWindowResizeBorderID(w, (ImGuiDir)n))
                zs.user = true;
    if (!zs.fixed) return;
    ImVec2 s = ImGui::GetWindowSize();
    if (s.x < zs.size.x - 1.5f || s.x > zs.size.x + 1.5f || s.y < zs.size.y - 1.5f || s.y > zs.size.y + 1.5f)
        zs.fixed = 0;
}

// Fit to window (the picture as large as the window lets it be), then the fixed zooms that size the window.
void zoom_items(ZoomState& zs)
{
    if (ImGui::MenuItem(tr("Fit to window"), nullptr, zs.fixed <= 0)) {
        zs.fixed = 0;
        zs.request = 0;
    }
    if (ImGui::IsItemHovered())
        tip(tr("The picture as large as its window allows (with integer scaling in whole multiples). "
               "Resizing the window by hand fits it again."));
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
        tip(tr("MP4 (H.264 + AAC) into work/videa through ffmpeg: 50 frames per second of emulated time, "
               "enlarged 3x as shown, the sound of the machine."));
}

void shot_items(App& app, DisplayDevice* d, const char* name)
{
    float a = display_px_aspect(app, d);
    if (ImGui::MenuItem(tr("Save screenshot"), "Ctrl+F5", false, d != nullptr)) shot_save(app.ui, d, name, a);
    if (ImGui::MenuItem(tr("Save screenshot as..."), nullptr, false, d != nullptr)) shot_save_as(app.ui, d, name, a);
    if (ImGui::MenuItem(tr("Copy screenshot"), nullptr, false, d != nullptr)) shot_copy(app.ui, d, a);
    Machine* m = app.host.machine();
    if (d && m && d == m->display && ImGui::MenuItem(tr("Copy screen text"), "Ctrl+Shift+C")) copy_screen_text(app.ui);
    ImGui::MenuItem(tr("Screenshot enlarged 3x (as shown)"), nullptr, &app.ui.shot_enlarged);
    if (ImGui::IsItemHovered())
        tip(tr("Off: the exact dots of the emulated picture. On: every dot 3 lines high and as wide as on the screen (TV aspect)."));
    ImGui::Separator();
    video_item(app, d, name);
}

// Full screen (Alt+Enter) of the display k (-1 the console screen, k = gfx[k]); checked in full screen: leaves it.
static void fullscreen_item(App& app, int k)
{
    bool on = app.fullscreen != FS_OFF;
    if (ImGui::MenuItem(tr("Full screen"), "Alt+Enter", on)) fullscreen_set(app, on ? FS_OFF : k);
}

// Right click on the picture of the display k: the zoom of this window, full screen and its screenshot.
void display_context_menu(App& app, DisplayDevice* d, const char* name, ZoomState& zs, int k)
{
    if (ImGui::BeginPopupContextWindow("zoommenu")) {
        ImGui::TextDisabled("%s", tr("Zoom"));
        zoom_items(zs);
        ImGui::Separator();
        fullscreen_item(app, k);
        ImGui::Separator();
        shot_items(app, d, name);
        ImGui::EndPopup();
    }
}

// The output of the display used last (used_display): Ctrl+F5, View -> Screenshot.
DisplayDevice* shot_display(App& app, std::string& name)
{
    DisplayDevice* d = nullptr;
    used_display(app, &d);
    name = display_name(d, "screen");
    return d;
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

// View -> Settings by sections (UX review 5.1): General, Appearance, Picture, Sound, Controls, Recording, and Advanced
// (for developers: the MCP server; collapsed while the server is off). A form: the label of a row in the left column
// (as wide as the widest label in the UI language), its control right of it; a check box sits in the right column with
// its own text. Everything applies at once and is saved as before (the keys of the settings file are the same).
struct SettingsForm {
    float x;       // the column of the controls (window coordinates, as SameLine)
    float right;   // the right edge of the content (screen)
};

// the labels of the rows (their widest sets the column of the controls), besides "Jazyk / Language" and "ffmpeg"
static const char* const k_settings_labels[] = { "Text size", "Look", "Mode", "Windows", "Volume", "Mouse sensitivity",
                                                 "Screenshots", "Videos" };

// A row of the form: its label (nullptr: none) in the left column, the control next at f.x.
static void settings_row(const SettingsForm& f, const char* label)
{
    if (label) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(f.x);
    } else {
        ImGui::SetCursorPosX(f.x);
    }
}

// The next control (w wide) on the same row when it fits, else on the next one in the column of the controls.
static void settings_next(const SettingsForm& f, float w)
{
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + w <= f.right) ImGui::SameLine();
    else ImGui::SetCursorPosX(f.x);
}

static float checkbox_width(const char* label)
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;
}

// A line of explanation under a control, in the column of the controls, wrapped; warn: in the colour of a warning.
static void settings_note(const SettingsForm& f, const char* text, bool warn = false)
{
    ImGui::SetCursorPosX(f.x);
    ImGui::PushStyleColor(ImGuiCol_Text, warn ? theme().warn : theme().text_dim);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

// A folder the program writes into: Open folder (made when missing), its path (in the work root as work/...; the
// whole path in the tooltip).
static void settings_folder(const std::string& dir)
{
    ImGui::PushID(dir.c_str());
    if (ImGui::Button(tr("Open folder"))) open_folder(dir);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::TextUnformatted(data_relative(dir).c_str());
    if (ImGui::IsItemHovered()) tip(dir.c_str());
}

static void draw_settings(App& app)
{
    UiState& ui = app.ui;
    if (!ui.show_settings) return;
    float u = ImGui::GetFontSize();
    layout_window(LW_SETTINGS);
    if (!ImGui::Begin(tr_id("Settings"), &ui.show_settings)) {
        ImGui::End();
        return;
    }
    SettingsForm f;
    float lw = std::max(ImGui::CalcTextSize("Jazyk / Language").x, ImGui::CalcTextSize("ffmpeg").x);
    for (const char* l : k_settings_labels) lw = std::max(lw, ImGui::CalcTextSize(tr(l)).x);
    f.x = ImGui::GetCursorPosX() + lw + 1.5f * u;
    f.right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    float slider_w = 12.0f * u;

    theme_section(tr("General"));
    settings_row(f, "Jazyk / Language");   // in both languages: findable in either
    UiLanguage cur = i18n_language();
    if (ImGui::RadioButton("Čeština", cur == LANG_CS)) set_ui_language(app, LANG_CS);
    ImGui::SameLine();
    if (ImGui::RadioButton("English", cur == LANG_EN)) set_ui_language(app, LANG_EN);
    settings_row(f, tr("Text size"));
    static int text_edit = 100;   // dragged: the size changes when the slider is released, not under the mouse
    static bool text_drag = false;
    if (!text_drag) text_edit = app.text_percent;
    ImGui::SetNextItemWidth(slider_w);
    ImGui::SliderInt("##textsize", &text_edit, 100, 200, "%d %%");
    text_drag = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        app.text_percent = (text_edit + 5) / 10 * 10;
        app.restyle = true;
    }
    settings_row(f, nullptr);
    ImGui::Checkbox(tr("Guides open by themselves"), &ui.guides_auto);
    const char* show_guides = tr("Show the hidden guides again");
    settings_next(f, ImGui::CalcTextSize(show_guides).x + 2.0f * ImGui::GetStyle().FramePadding.x);
    ImGui::BeginDisabled(ui.guides_hidden.empty());
    if (ImGui::Button(show_guides)) ui.guides_hidden.clear();
    ImGui::EndDisabled();

    theme_section(tr("Appearance"));
    ThemeVariant cv = app.look;
    ThemeMode cm = app.look_mode;
    if (app.look_override) theme_override(&cv, &cm);
    for (int i = 0; i < THEME_VARIANTS && THEME_VARIANTS > 1; i++) {   // one look now: no choice
        if (i) ImGui::SameLine();
        else settings_row(f, tr("Look"));
        if (ImGui::RadioButton(cur == LANG_CS ? k_themes[i].name_cs : k_themes[i].name_en, cv == i))
            set_look(app, (ThemeVariant)i, cm);
    }
    settings_row(f, tr("Mode"));
    static const char* const k_modes[] = { "As the system", "Light", "Dark" };
    for (int i = 0; i < 3; i++) {
        if (i) ImGui::SameLine();
        if (ImGui::RadioButton(tr(k_modes[i]), cm == i)) set_look(app, cv, (ThemeMode)i);
    }
    if (app.look_override)
        settings_note(f, tr("The look of this run comes from --ui-theme; a choice here replaces it and is saved."));
    bool docked = layout_docked(app);
    const char* layout_tip = tr("Floating: every window where you put it. Docked: the main display in the middle, the "
                                "other windows in a column at the right edge, as tabs (a tab can be dragged out). Each "
                                "keeps its own window places: a choice brings back the places you left in it (the first "
                                "time its default ones).");
    settings_row(f, tr("Windows"));
    if (ImGui::RadioButton(tr("Floating"), !docked)) set_layout(app, false);
    if (ImGui::IsItemHovered()) tip(layout_tip);
    ImGui::SameLine();
    if (ImGui::RadioButton(tr("Docked"), docked)) set_layout(app, true);
    if (ImGui::IsItemHovered()) tip(layout_tip);
    if (app.layout_override)
        settings_note(f, tr("The window layout of this run comes from --ui-layout; a choice here replaces it and is "
                            "saved."));

    theme_section(tr("Picture"));   // as in the menu View
    settings_row(f, nullptr);
    ImGui::Checkbox(tr("Integer scaling"), &app.integer_scale);
    settings_next(f, checkbox_width(tr("TV aspect ratio")));
    ImGui::Checkbox(tr("TV aspect ratio"), &app.tv_aspect);
    settings_next(f, checkbox_width(tr("Smoothing (linear filtering)")));
    ImGui::Checkbox(tr("Smoothing (linear filtering)"), &app.smooth);

    theme_section(tr("Sound"));
    settings_row(f, nullptr);
    ImGui::Checkbox(tr("Sound on"), &app.audio.enabled);
    settings_row(f, tr("Volume"));
    ImGui::SetNextItemWidth(slider_w);
    ImGui::SliderInt("##volume", &app.audio.volume, 0, 100, "%d %%");
    if (app.audio.failed) settings_note(f, tr("No sound device."), true);
    settings_note(f, tr("Sound plays only at the emulation speed 100 %."));

    theme_section(tr("Controls"));
    settings_row(f, tr("Mouse sensitivity"));
    ImGui::SetNextItemWidth(slider_w);
    ImGui::SliderFloat("##mouse", &app.mouse.sensitivity, 0.25f, 4.0f, "%.2f");
    settings_row(f, nullptr);
    ImGui::Checkbox(tr("PC mouse as the SAPI mouse"), &app.mouse.enabled);   // not saved (author): said by its label
    if (ImGui::IsItemHovered())
        tip(tr("A click into a screen of SAPI captures the PC mouse, it then moves the mouse of SAPI; the middle "
               "button or Ctrl+F10 releases it. Not saved: off again at the next start."));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", tr("(until exit)"));

    theme_section(tr("Recording"));
    settings_row(f, nullptr);
    ImGui::Checkbox(tr("Screenshot enlarged 3x (as shown)"), &ui.shot_enlarged);
    if (ImGui::IsItemHovered())
        tip(tr("Off: the exact dots of the emulated picture. On: every dot 3 lines high and as wide as on the "
               "screen (TV aspect)."));
    static char ff[512];
    static bool ff_init = false;
    if (!ff_init) {
        snprintf(ff, sizeof(ff), "%s", ui.ffmpeg_path.c_str());
        ff_init = true;
    }
    settings_row(f, "ffmpeg");   // the name of the program
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##ffmpeg", tr("ffmpeg.exe (empty = from PATH)"), ff, sizeof(ff))) ui.ffmpeg_path = ff;
    // the ffmpeg the video would use: looked for when the window opens and when the field changes (PATH: many folders)
    static std::string ff_found, ff_for;
    static bool ff_looked = false;
    if (!ff_looked || ImGui::IsWindowAppearing() || ff_for != ui.ffmpeg_path) {
        ff_found = video_ffmpeg(ui);
        ff_for = ui.ffmpeg_path;
        ff_looked = true;
    }
    if (!ff_found.empty()) {
        settings_note(f, ui.ffmpeg_path.empty() ? tr("Videos use ffmpeg from PATH.") : tr("Videos use this ffmpeg."));
        if (ImGui::IsItemHovered()) tip(ff_found.c_str());
    } else if (!ui.ffmpeg_path.empty()) {
        settings_note(f, tr("No ffmpeg at this path: videos cannot be recorded."), true);
    } else {
        settings_note(f, tr("No ffmpeg: videos cannot be recorded (winget install ffmpeg, or its path above)."), true);
    }
    settings_row(f, tr("Screenshots"));
    settings_folder(shot_dir());
    settings_row(f, tr("Videos"));
    settings_folder(video_dir());

    ImGui::Spacing();
    // collapsed while the server is off: a developer who turned it on sees its address
    if (app.mcp_pref || !app.mcp_error.empty()) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::CollapsingHeader(tr("Advanced (for developers)"))) {
        settings_row(f, nullptr);
        bool on = app.mcp_pref;
        if (ImGui::Checkbox(tr("MCP server on"), &on)) {
            app.mcp_pref = on;
            mcp_set(app, on, app.mcp_pref_port);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(tr("Port"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(7 * u);
        int port = app.mcp_pref_port;
        if (ImGui::InputInt("##port", &port, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue) && port > 0 && port < 65536) {
            app.mcp_pref_port = port;
            if (app.mcp_pref) mcp_set(app, true, port);
        }
        if (app.mcp && app.mcp->running()) {
            std::string add = "claude mcp add --transport http sapiemu " + app.mcp->url();
            ImGui::SetCursorPosX(f.x);
            ImGui::TextUnformatted(app.mcp->url().c_str());
            settings_note(f, add.c_str());
        } else if (!app.mcp_error.empty()) {
            ImGui::SetCursorPosX(f.x);
            ImGui::PushStyleColor(ImGuiCol_Text, theme().error);
            ImGui::TextWrapped("%s", app.mcp_error.c_str());
            ImGui::PopStyleColor();
        } else if (app.mcp_pref) {
            settings_note(f, tr("Not running now (started with --no-mcp)."));
        } else {
            settings_note(f, tr("Off. Only programs on this PC (127.0.0.1) can connect, it is for the development of "
                                "software for SAPI with Claude (docs/mcp.md)."));
        }
    }
    ImGui::End();
}

static void status_add(std::string& s, const std::string& part)
{
    if (part.empty()) return;
    if (!s.empty()) s += "  ";
    s += part;
}

// The status on the right side of the menu bar. A narrow window takes a higher `level`, each one shorter: 1 without
// the notice, 2 the modes without their keys, 3 without the CPU clock and MCP, 4 without the keyboard (its LEDs, the
// keys waiting). The pause, the recording and the speed stay. kb_leds: the LEDs of the keyboard are in it.
static std::string status_text(App& app, Machine* m, const std::string& notice, int level, bool* kb_leds)
{
    std::string s;
    if (!m) {
        if (level < 1) status_add(s, notice);
        status_add(s, tr("Switched off"));
        return s;
    }
    KeyboardDevice* kb = m->keyboard();
    if (kb && level < 4) {
        std::string ks = kb->status();   // English ("APS-5 key 2, FAULT"); the LEDs of ANK-1 as printed
        *kb_leds = ks.find("LED") != std::string::npos;
        status_add(s, tr(ks.c_str()));
    }
    if (level < 1) status_add(s, notice);
    if (video_recording(app.ui)) {
        int t = (int)video_seconds(app.ui);
        char rec[48];
        snprintf(rec, sizeof(rec), "\xE2\x97\x8F REC %d:%02d", t / 60, t % 60);
        status_add(s, rec);
    }
    bool keys = level < 2;   // the keys that switch the modes off
    if (app.game_keys && kb && kb->held_keys()) status_add(s, tr(keys ? "game mode (Ctrl+F7)" : "game mode"));
    if (app.joy.keyboard)   // arrows and space do not type now
        status_add(s, tr(keys ? "joystick from the keyboard (Ctrl+F8)" : "joystick from the keyboard"));
    if (app.mouse.captured) status_add(s, tr(keys ? "mouse captured (middle button / Ctrl+F10)" : "mouse captured"));
    if (kb && kb->pending() > 1 && level < 4) {   // keys typed but not taken by the program
        char q[48];
        snprintf(q, sizeof(q), "%s %d", tr("keys waiting:"), (int)kb->pending());
        status_add(s, q);
    }
    if (app.host.paused) status_add(s, app.host.status.empty() ? tr("Paused") : app.host.status.c_str());
    else if (app.host.max_speed) status_add(s, tr("Maximum"));
    if (level < 3) {   // the clock of the CPU card (JPR-1V TURBO included)
        if (app.mcp && app.mcp->running()) status_add(s, "MCP");
        char cpu[32];
        snprintf(cpu, sizeof(cpu), "CPU %.2f MHz", (m->cpu ? m->cpu->cpu_display_hz() : m->bus.cpu_hz) / 1e6);
        status_add(s, cpu);
    }
    // the measured speed of the whole machine against real time; the first measurement comes after a second
    char speed[48];
    if (app.host.speed_percent > 0) snprintf(speed, sizeof(speed), "%s %3.0f %%", tr("speed"), app.host.speed_percent);
    else snprintf(speed, sizeof(speed), "%s -- %%", tr("speed"));
    status_add(s, speed);
    return s;
}

// A menu by its English name (parent: the menu of the menu bar it is in, else null); a UI snapshot (--ui-menu) keeps
// the one it names open ("View", "View/Windows": the submenu in it too).
static bool begin_menu(App& app, const char* key, const char* parent = nullptr)
{
    if (!app.snap_menu.empty()) {
        std::string path = parent ? std::string(parent) + "/" + key : std::string(key);
        bool want = app.snap_menu == path || (!parent && app.snap_menu.compare(0, path.size() + 1, path + "/") == 0);
        if (want && !ImGui::IsPopupOpen(tr(key))) ImGui::OpenPopup(tr(key));
    }
    return ImGui::BeginMenu(tr(key));
}

// The windows of the items of the menus View and Debug: the window id after ### and its open flag (also the UI snapshot
// --ui-window-item).
static bool* menu_window_open(App& app, const std::string& key)
{
    UiState& ui = app.ui;
    const struct {
        const char* key;
        bool* open;
    } k[] = {
        { "rack", &ui.show_rack }, { "Serial terminal", &ui.show_terminal }, { "Media", &ui.show_media },
        { "Printer", &ui.show_printer }, { "Minigraf", &ui.show_plotter }, { "IMS-2 measuring bench", &ui.show_ims2 },
        { "Diagnostic board (K3)", &ui.show_diag }, { "APS-5: panel and furnaces", &ui.show_aps5 },
        { "Debugger", &ui.show_debugger }, { "Memory", &ui.show_memory }, { "I/O log", &ui.show_iolog },
        { "Log", &ui.show_log },
    };
    for (const auto& e : k)
        if (key == e.key) return e.open;
    return nullptr;
}

// A window item of the menus View and Debug clicked (author 2026-10-29: a window hides behind another one and is then
// searched for): a closed window opens; an open one that is covered (layout_covered) comes to the front with the
// keyboard; one in front closes.
static void window_item_click(App& app, const char* key)
{
    bool* open = menu_window_open(app, key);
    if (!open) return;
    std::string id = std::string("###") + key;
    if (*open && layout_covered(id.c_str())) layout_to_front(id.c_str());
    else *open = !*open;
    if (open == &app.ui.show_rack) app.ui.rack_by_layout = false;   // the configurator wanted: the user's window now
}

// The item itself: the check mark says open, "covered" on the right that a click brings it to the front.
static void window_item(App& app, const char* label, const char* key, bool enabled = true)
{
    bool* open = menu_window_open(app, key);
    std::string id = std::string("###") + key;
    bool covered = *open && layout_covered(id.c_str());
    if (ImGui::MenuItem(label, covered ? tr("covered") : nullptr, *open, enabled)) window_item_click(app, key);
    if (covered && ImGui::IsItemHovered())
        tip(tr("The window is covered by another one: a click brings it to the front, the next "
               "click closes it."));
}

// An item that only opens a window (Settings, Keyboard shortcuts, About): an open one comes to the front.
static void open_window(bool* open, const char* id)
{
    if (*open) layout_to_front(id);
    *open = true;
}

// The item of the window of a device (View; device: its key in k_machine_windows): enabled when the machine has the
// device, or while the window is open (the item closes it); a disabled one tells why and where a machine with the
// device is (device_missing). Otherwise as window_item.
static void device_item(App& app, const char* label, const char* key, const char* device)
{
    bool has = machine_has_device(app.ui, device);
    window_item(app, label, key, has || *menu_window_open(app, key));
    if (!has && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        tip(device_missing(app.ui, device).c_str());
}

// View -> Displays: the display windows of the machine (the console screen, the graphic displays), named after their
// cards; a check mark = shown. A hidden one is not drawn, the machine runs on; another machine shows them all again.
static void displays_menu(App& app, Machine* m)
{
    if (!begin_menu(app, "Displays", "View")) return;
    DisplayDevice* gfx[MAX_GFX_VIEWS];
    int n = graphic_displays(m, gfx);
    for (int k = -1; k < n; k++) {
        bool shown = !display_hidden(app, k);
        const char* name =
            k < 0 ? display_name(m ? m->display : nullptr, tr("Screen")) : display_name(gfx[k], tr("Graphics"));
        ImGui::PushID(k);   // two cards of one type may have the same name
        if (ImGui::MenuItem(name, nullptr, shown)) display_show(app, k, !shown);
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::TextDisabled("%s", tr("A hidden display goes on running."));
    ImGui::EndMenu();
}

// View -> Open the folder of screenshots and videos: Explorer in work/snimky or work/videa (made when missing); the
// folder on the right, the whole path in the tooltip.
static void folder_item(const char* label, const std::string& dir)
{
    std::string shown = data_relative(dir);
    if (ImGui::MenuItem(label, shown.c_str())) open_folder(dir);
    if (ImGui::IsItemHovered()) tip(dir.c_str());
}

void draw_menu(App& app)
{
    if (!app.snap_click.empty()) {   // UI snapshot: as a click on the item in the menu
        if (app.snap_click.compare(0, 6, "front:") == 0) layout_to_front(("###" + app.snap_click.substr(6)).c_str());
        else window_item_click(app, app.snap_click.c_str());
        app.snap_click.clear();
    }
    if (!ImGui::BeginMainMenuBar()) return;
    Machine* m = app.host.machine();
    if (begin_menu(app, "File")) {
        if (ImGui::MenuItem(tr("Choose a machine..."))) browser_open(app.ui);
        if (ImGui::BeginMenu(tr("Recent machines"), !app.ui.recent.empty())) {
            for (const std::string& f : std::vector<std::string>(app.ui.recent)) {
                std::string label = machine_title(app.ui, f, std::string());
                if (ImGui::MenuItem((label + "##" + f).c_str(), nullptr, f == app.host.machine_file())) {
                    rack_start_machine(app.ui, f);   // its guide may open itself (a new machine generation)
                    m = app.host.machine();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr("Start a machine from a file..."))) rack_open_and_run(app.ui);
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
    if (begin_menu(app, "Machine")) {
        if (ImGui::MenuItem(m ? tr("Switch off") : tr("Switch on"))) {
            rack_power_switch(app.ui);
            m = app.host.machine();   // the old machine is gone
            set_title(app);
        }
        if (ImGui::MenuItem(tr("Switch off and on"), "Ctrl+F12", false, m != nullptr)) {
            power_cycle(app);
            m = app.host.machine();   // the old machine is gone
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            tip(tr("As the power switch: the memory is lost, the cards, switches and media stay as they "
                   "are. Reset (F12) keeps the memory."));
        if (ImGui::MenuItem(tr("Reset"), "F12", false, m != nullptr)) request_reset(app.ui);
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
                    // through the description, as View -> Colour: a change of the machine the configurator shows (save
                    // it or keep it for the next start), not lost at the next power cycle
                    std::string err;
                    if (!rack_set_keyboard(app.ui, p.config, err)) {
                        app.error_text = std::string(tr("Cannot change the keyboard")) + ":\n" + tr_core(err);
                        app.show_error = true;
                    }
                    kb = m->keyboard();   // the old keyboard is gone
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem(tr("Paste text"), "Ctrl+Shift+V", false, kb != nullptr)) {
                paste_keyboard(app, m);
                app.ui.focus_display = 2;   // what comes next is typed on the screen
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                tip(tr("The text of the clipboard is typed on the keyboard of SAPI (lines end with "
                       "Enter), e.g. a BASIC program from a file."));
            if (ImGui::MenuItem(tr("Game mode: keys by position, Shift = SHIFT"), "Ctrl+F7", &app.game_keys, kb && kb->held_keys()))
                kb->release_all();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                tip(tr("ANK-1: letters by their label, digits by position, Shift of the PC = SHIFT, keys "
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
            if (ImGui::IsItemHovered()) tip(tr("Not saved: off again at the next start."));
            ImGui::TextDisabled("%s", tr("Middle button or Ctrl+F10 releases the captured mouse."));
            ImGui::Separator();
            if (ImGui::MenuItem(tr("Volume, mouse sensitivity..."))) open_window(&app.ui.show_settings, "###Settings");
            ImGui::EndMenu();
        }
        window_item(app, tr("Media..."), "Media");   // the same window as View -> Media
        ImGui::EndMenu();
    }
    if (begin_menu(app, "Debug")) {
        window_item(app, tr("Debugger"), "Debugger");
        window_item(app, tr("Memory"), "Memory");
        window_item(app, tr("I/O log"), "I/O log");
        window_item(app, tr("Log"), "Log");
        ImGui::Separator();
        bool stepping = m && app.host.paused;   // the steps are for a paused machine (F5)
        if (ImGui::MenuItem(tr("Step"), "F11", false, stepping)) debug_step(app, false);
        if (ImGui::MenuItem(tr("Step over"), "F10", false, stepping)) debug_step(app, true);
        // as F9, but from the menu always at the PC (the mouse is on the menu)
        if (ImGui::MenuItem(tr("Breakpoint on / off"), "F9", false, m && m->cpu && app.ui.show_debugger))
            debug_toggle_breakpoint(app.ui);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            tip(tr("In the debugger: F9 sets or removes the breakpoint at the disassembly row under "
                   "the mouse, else at the PC (from this menu at the PC)."));
#ifndef NDEBUG
        ImGui::Separator();   // tuning the look live; Export copies the values for frontend/theme_data.cpp
        ImGui::MenuItem(tr("Style editor (development)"), nullptr, &app.style_editor);
#endif
        ImGui::EndMenu();
    }
    if (begin_menu(app, "View")) {
        window_item(app, tr("Machine configurator..."), "rack");
        ImGui::Separator();
        // the windows of devices: those the machine has (an open one closes)
        device_item(app, tr("Serial terminal"), "Serial terminal", "terminal");
        window_item(app, tr("Media"), "Media");
        device_item(app, tr("Printer"), "Printer", "printer");
        device_item(app, tr("Plotter (Minigraf, XY 4130)"), "Minigraf", "minigraf");
        device_item(app, tr("IMS-2 measuring bench"), "IMS-2 measuring bench", "ims2");
        device_item(app, tr("Diagnostic board (K3)"), "Diagnostic board (K3)", "diag");
        device_item(app, tr("APS-5: panel and furnaces"), "APS-5: panel and furnaces", "aps5");
        displays_menu(app, m);
        ImGui::Separator();
        ink_menu(app, m);
        DisplayDevice* ud = nullptr;   // the display used last: the zoom and the full screen are its
        int uk = used_display(app, &ud);
        if (ImGui::BeginMenu(tr("Screen zoom"), ud != nullptr)) {
            ImGui::TextDisabled("%s %s", tr("Display:"), display_name(ud, ""));
            zoom_items(display_zoom(app, uk));
            ImGui::Separator();
            ImGui::TextDisabled("%s", tr("Right click on a picture: the zoom of its window."));
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr("Full screen"), "Alt+Enter", false, ud != nullptr)) fullscreen_set(app, uk);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            tip(tr("The display used last alone over the whole monitor, without the menu and the other "
                   "windows. Alt+Enter or its right click menu leaves it; Esc goes to SAPI."));
        // the picture (View -> Settings -> Picture has them too), then what is taken from it
        ImGui::MenuItem(tr("Integer scaling"), nullptr, &app.integer_scale);
        ImGui::MenuItem(tr("TV aspect ratio"), nullptr, &app.tv_aspect);
        ImGui::MenuItem(tr("Smoothing (linear filtering)"), nullptr, &app.smooth);
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Screenshot"), "Ctrl+F5", false, m && m->display)) shot_now(app);
        if (ImGui::MenuItem(tr("Copy screen text"), "Ctrl+Shift+C", false, m && m->display)) copy_screen_text(app.ui);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            tip(tr("The text of the console display to the clipboard (in the serial terminal "
                   "Ctrl+Shift+C copies its text)."));
        if (ImGui::MenuItem(video_recording(app.ui) ? tr("Stop the video") : tr("Record video with sound"), "Ctrl+Alt+F5",
                            false, m && m->display))
            video_toggle(app);
        if (begin_menu(app, "Open the folder of screenshots and videos", "View")) {
            folder_item(tr("Screenshots"), shot_dir());
            folder_item(tr("Videos"), video_dir());
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (begin_menu(app, "Windows", "View")) {   // the open windows: a click brings one to the front
            std::vector<LayoutOpenWindow> wins;
            layout_open_windows(wins);
            for (size_t i = 0; i < wins.size(); i++) {
                ImGui::PushID((int)i);
                if (ImGui::MenuItem(wins[i].title.c_str(), wins[i].covered ? tr("covered") : nullptr))
                    layout_to_front(wins[i].id.c_str());
                ImGui::PopID();
            }
            ImGui::Separator();
            ImGui::TextDisabled("%s", tr("A click brings the window to the front."));
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr("Arrange open windows"))) app.ui.layout_arrange = true;
        if (ImGui::IsItemHovered())
            tip(layout_docked(app)
                    ? tr("The dock is built again as by Default window layout (the closed windows "
                         "go back into it too).")
                    : tr("The open windows go to the places of the default layout, a zoom chosen "
                         "from the menu stays; the closed ones keep their places (Default window "
                         "layout forgets them)."));
        if (ImGui::MenuItem(tr("Default window layout"))) app.ui.layout_reset = true;
        if (ImGui::IsItemHovered())
            tip(layout_docked(app)
                    ? tr("Docks the windows again: the main display in the middle, the other windows "
                         "in the column at the right edge (View -> Settings -> Appearance -> "
                         "Windows).")
                    : tr("Undocks all windows (a window dragged to an edge sticks there) and puts them "
                         "back at their first places and sizes."));
        ImGui::Separator();
        if (ImGui::BeginMenu("Jazyk / Language")) {   // in both languages: findable in either
            UiLanguage cur = i18n_language();
            if (ImGui::MenuItem("Čeština", nullptr, cur == LANG_CS)) set_ui_language(app, LANG_CS);
            if (ImGui::MenuItem("English", nullptr, cur == LANG_EN)) set_ui_language(app, LANG_EN);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr("Settings..."))) open_window(&app.ui.show_settings, "###Settings");
        ImGui::EndMenu();
    }
    if (begin_menu(app, "Help")) {
        if (ImGui::MenuItem(tr("Machine guide"))) {   // opens it in front (an instrument may hide it), X closes it
            app.ui.show_guide = true;
            app.ui.guide_focus = true;
            app.ui.guide_machine = app.host.machine_file();
        }
        if (ImGui::MenuItem(tr("Choose a machine..."))) browser_open(app.ui);
        ImGui::Separator();
        help_docs_menu(app.ui);
        if (ImGui::MenuItem(tr("Keyboard shortcuts"))) open_window(&app.ui.show_shortcuts, "###Keyboard shortcuts");
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Report a bug..."))) {
            app.ui.show_report = true;
            app.ui.report_status.clear();
            ImGui::SetWindowFocus(tr_id("Report a bug"));   // in front when it is open already
        }
        if (ImGui::MenuItem(tr("About SAPIemu"))) open_window(&app.ui.show_about, "###About SAPIemu");
        ImGui::EndMenu();
    }

    // status on the right side of the menu bar, never over the menus: a narrow window shortens it (status_text)
    float menus_end = ImGui::GetCursorPosX();
    std::string notice;   // the result of the last save, load, screenshot: 7 s, the file name is enough
    if (!app.ui.notice.empty() && !app.ui.notice_error && SDL_GetTicksNS() - app.ui.notice_ns < 7000000000ull) {
        notice = app.ui.notice;
        size_t slash = notice.find_last_of("/\\");
        size_t colon = notice.find(": ");
        if (slash != std::string::npos && colon != std::string::npos)
            notice = notice.substr(0, colon + 2) + notice.substr(slash + 1);
    }
    // changes in the configurator wait for a restart: visible with the configurator closed too
    bool pending = rack_pending(app.ui);
    int errors = log_errors() - app.errors_seen;   // errors in the Log not seen there yet
    float sp = ImGui::GetStyle().ItemSpacing.x;
    float room = ImGui::GetWindowWidth() - menus_end - sp;
    std::string st, pend, errs;
    float w = 0, pw = 0, ew = 0;
    bool kb_leds = false;   // LEDs of the keyboard in it (a tooltip explains them)
    bool fits = false;
    for (int level = 0; level <= 4 && !fits; level++) {
        kb_leds = false;
        st = status_text(app, m, notice, level, &kb_leds);
        pend.clear();
        if (pending) pend = level < 2 ? std::string("\xE2\x97\x8F ") + tr("Configuration changed: restart") : "\xE2\x97\x8F";
        errs.clear();
        if (errors > 0) {   // a cross and the word: not the colour alone
            char b[64];
            if (level < 2) snprintf(b, sizeof(b), "\xE2\x9C\x96 %s %d", tr("Errors:"), errors);
            else snprintf(b, sizeof(b), "\xE2\x9C\x96 %d", errors);
            errs = b;
        }
        w = ImGui::CalcTextSize(st.c_str()).x + sp * 2;
        pw = pend.empty() ? 0.0f : ImGui::CalcTextSize(pend.c_str()).x + sp * 2;
        ew = errs.empty() ? 0.0f : ImGui::CalcTextSize(errs.c_str()).x + sp * 2;
        fits = w + pw + ew <= room;
    }
    if (fits) {
        ImGui::SameLine(ImGui::GetWindowWidth() - w - pw - ew);
        if (!errs.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme().error);
            ImVec2 sz = ImGui::CalcTextSize(errs.c_str());
            if (ImGui::Selectable((errs + "##errors").c_str(), false, ImGuiSelectableFlags_None, sz)) {
                app.ui.show_log = true;
                app.log_focus = true;
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                tip(tr("Errors written into the Log since it was open last. Click: open the Log."));
            ImGui::SameLine();
        }
        if (!pend.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme().pending);
            ImVec2 sz = ImGui::CalcTextSize(pend.c_str());
            if (ImGui::Selectable((pend + "##pending").c_str(), false, ImGuiSelectableFlags_None, sz)) {
                open_window(&app.ui.show_rack, "###rack");
                app.ui.rack_by_layout = false;
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                tipf("%s\n%s", tr("Configuration changed: restart"),
                     tr("The configurator holds changes the running machine does not have yet. Click: open "
                        "the configurator (Restart with this configuration applies them)."));
            ImGui::SameLine();
        }
        ImGui::TextUnformatted(st.c_str());
        if (kb_leds && ImGui::IsItemHovered())
            tip(tr("LED SHIFT, LED ERR: the lamps on the SAPI keyboard (ANK-1), lit by the program "
                   "(P0-OUT D5, D7), not the Shift key of the PC. Programs making sound on D5 "
                   "(Manic Miner) light the LED SHIFT with it."));
    }
    ImGui::EndMainMenuBar();
}

float display_px_aspect(App& app, DisplayDevice* d)
{
    return app.tv_aspect ? (float)(k_tv_square_dot_hz / d->dot_clock_hz()) : 1.0f;
}

// A label in a dark box over the picture `a`-`b`, centred, at the top (top = true) or at the bottom; into the draw
// list of the window, not into the picture (screenshots and videos do not have it). Light on dark in every look: the
// picture is a monitor. alpha: 0..1 (fading out).
static void picture_label(ImVec2 a, ImVec2 b, const char* text, float size, bool top, float alpha)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* f = ImGui::GetFont();
    ImVec2 ts = f->CalcTextSizeA(size, 1e9f, 0.0f, text);
    float px = 0.6f * size, py = 0.3f * size, gap = 0.5f * size;
    float x = (a.x + b.x - ts.x) * 0.5f, y = top ? a.y + gap + py : b.y - gap - py - ts.y;
    dl->AddRectFilled(ImVec2(x - px, y - py), ImVec2(x + ts.x + px, y + ts.y + py), IM_COL32(0, 0, 0, (int)(170 * alpha)),
                      0.35f * size);
    dl->AddText(f, size, ImVec2(x, y), IM_COL32(235, 235, 235, (int)(255 * alpha)), text);
}

// Over the picture: the pause or the speed just changed (2.5 s, the last 0.5 s fading), and "Click here and type"
// (type_hint). pause: the pause stays said while it lasts (full screen: no title and no status bar tell it).
static void picture_labels(App& app, ImVec2 a, ImVec2 b, bool hint, bool pause)
{
    uint64_t age = SDL_GetTicksNS() - app.osd_ns;
    if (!app.osd.empty() && age < 2500000000ull) {
        float alpha = age < 2000000000ull ? 1.0f : (float)(2500000000ull - age) / 5e8f;
        picture_label(a, b, app.osd.c_str(), ImGui::GetFontSize() * 1.5f, true, alpha);
    } else if (pause && app.host.paused && app.host.machine()) {
        picture_label(a, b, app.host.status.empty() ? tr("Pause (F5)") : app.host.status.c_str(), ImGui::GetFontSize(),
                      true, 0.7f);
    }
    if (hint) picture_label(a, b, tr("Click here and type"), ImGui::GetFontSize(), false, 0.85f);
}

// Draw display output `d` into the current window, scaled to fit (or at the zoom chosen
// for the window), through its own texture. Returns the zoom: screen pixels per emulated
// pixel (vertically, with the TV aspect). hint: "Click here and type" over the picture; pause: the pause said over it
// while it lasts (full screen).
float draw_display(App& app, DisplayDevice* d, SDL_Texture*& tex, int& tex_w, int& tex_h, std::vector<uint32_t>& pixels,
                   const ZoomState& zs, bool hint, bool pause = false)
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
    // defaults to linear filtering; switch to nearest for the emulated screen. Below 1:1 (a small docked window) linear:
    // nearest would drop whole lines of dots from the letters.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool nearest = !app.smooth && scale >= 0.999f;
    if (nearest) dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
    ImGui::Image(ImTextureRef((ImTextureID)(intptr_t)tex), size);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) app.mouse.click = true;   // captures the mouse when the machine has one
    if (nearest) dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
    picture_labels(app, pos, ImVec2(pos.x + size.x, pos.y + size.y), hint, pause);
    return scale;
}

// The hint "Click here and type" on the display used last: no display has the keyboard, the machine has a keyboard
// to type on (not the plotter of AR 1990), and nothing was typed on the SAPI keyboard in this run yet or a character
// was just dropped (5 s). Not always: while the debugger or another window is used, a lasting label would only cover
// the picture.
static bool type_hint(App& app, Machine* m, int target)
{
    KeyboardDevice* kb = m ? m->keyboard() : nullptr;
    bool want = !app.typed || (app.drop_ns && SDL_GetTicksNS() - app.drop_ns < 5000000000ull);
    return want && app.shot_target == target && !app.ui.screen_focused && kb && strcmp(kb->type_name(), "AR 1990") != 0;
}

// The display windows cannot be collapsed (one saved collapsed by an older version opens again); a drag on the picture
// does not move them (io.ConfigWindowsMoveFromTitleBarOnly), a click there captures the mouse of SAPI. No keyboard
// navigation of ImGui in them: the arrows, Space, Enter, Esc and Alt are the keys of SAPI there.
const ImGuiWindowFlags k_display_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavInputs;

void draw_screen(App& app)
{
    if (display_hidden(app, -1)) return;   // View -> Displays
    if (app.start_focus && app.main_output < 0) ImGui::SetNextWindowFocus();   // type right after start
    Machine* m = app.host.machine();
    DisplayDevice* d = m ? m->display : nullptr;
    place_display(app, d, "###screen", app.screen_zs);
    Card* c = dynamic_cast<Card*>(d);
    char title[96];   // named after the display card, the same window for every machine
    display_title(title, sizeof(title), c ? c->name.c_str() : tr("Screen"), d ? app.screen_zs.zoom : 0, "screen");
    zoom_resize(app, d, app.screen_zs);
    ImGui::SetNextWindowCollapsed(false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(k_display_pad, k_display_pad));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(k_background));   // a monitor in every look
    // not focused by appearing: at the start the main output takes the keyboard (start_focus), it may be another display
    ImGui::Begin(title, nullptr, k_display_flags | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    zoom_track(app.screen_zs);
    if (ImGui::IsWindowFocused()) {   // typing goes to the machine (draw_screens)
        app.ui.screen_focused = true;
        app.shot_target = -1;
    }
    if (app.fullscreen != FS_OFF) {   // hidden: the full screen window draws the picture (draw_fullscreen)
        ImGui::End();
        return;
    }

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
    app.screen_zs.zoom = draw_display(app, d, app.screen_tex, app.tex_w, app.tex_h, app.pixels, app.screen_zs,
                                      type_hint(app, m, -1));
    if (app.open_screen_menu) {
        ImGui::OpenPopup("zoommenu");
        app.open_screen_menu = false;
    }
    display_context_menu(app, d, c ? c->name.c_str() : "screen", app.screen_zs, -1);
    ImGui::End();
}

// The window of the graphic display k (DGD-1...): its own video output, so its own window.
static void draw_graphic_screen(App& app, Machine* m, int k, DisplayDevice* d)
{
    if (display_hidden(app, k)) return;   // View -> Displays
    GfxView& v = app.gfx[k];
    Card* c = dynamic_cast<Card*>(d);
    char title[96], id[16], wid[20];
    snprintf(id, sizeof(id), "gfx%d", k);
    snprintf(wid, sizeof(wid), "###%s", id);
    display_title(title, sizeof(title), c ? c->name.c_str() : tr("Graphics"), v.zs.zoom, id);
    place_display(app, d, wid, v.zs);
    zoom_resize(app, d, v.zs);
    if (app.start_focus && k == app.main_output) ImGui::SetNextWindowFocus();   // the main output: type right after start
    ImGui::SetNextWindowCollapsed(false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(k_display_pad, k_display_pad));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(k_background));
    bool open = ImGui::Begin(title, nullptr, k_display_flags | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (open) {
        zoom_track(v.zs);
        if (ImGui::IsWindowFocused()) {
            app.ui.screen_focused = true;   // typing goes to the machine too
            app.shot_target = k;
        }
        if (app.fullscreen == FS_OFF) {   // else hidden: the full screen window draws the picture
            v.zs.zoom = draw_display(app, d, v.tex, v.w, v.h, v.pixels, v.zs, type_hint(app, m, k));
            display_context_menu(app, d, c ? c->name.c_str() : "graphics", v.zs, k);
        }
    }
    ImGui::End();
}

// All display windows (not those hidden in View -> Displays). The main output first: it takes its place before the
// others (layout.cpp). The keyboard goes to the machine when one of them is focused (ui.screen_focused, for the key
// events of the next frame; each window only sets it, so the main output drawn first keeps it).
void draw_screens(App& app)
{
    Machine* m = app.host.machine();
    DisplayDevice* gfx[MAX_GFX_VIEWS];
    int n = graphic_displays(m, gfx);
    app.ui.screen_focused = false;
    if (app.main_output >= 0 && app.main_output < n) draw_graphic_screen(app, m, app.main_output, gfx[app.main_output]);
    draw_screen(app);
    for (int k = 0; k < n; k++)
        if (k != app.main_output) draw_graphic_screen(app, m, k, gfx[k]);
    app.start_focus = false;
    if (app.display_front != FS_OFF) {   // shown again: to the front once its window was drawn in the frame before too
        char id[16];
        if (app.display_front >= 0) snprintf(id, sizeof(id), "###gfx%d", app.display_front);
        else snprintf(id, sizeof(id), "###screen");
        ImGuiWindow* w = ImGui::FindWindowByName(id);
        if (!w || app.display_front >= n || display_hidden(app, app.display_front)) app.display_front = FS_OFF;
        else if (w->Active && w->WasActive) {
            layout_to_front(id);
            app.display_front = FS_OFF;
        }
    }
}

// Full screen (Alt+Enter, fullscreen_set): the display alone over the whole OS window (also one hidden in View ->
// Displays, chosen from its right click menu), without the menu bar and the other windows. They are drawn as always
// (their work goes on, their places and the dock stay as they are), only hidden: not drawn and not under the mouse.
// The dialogs (an error, a question) come over the picture. The picture fits the monitor (integer scaling and the TV
// aspect apply); the right click menu leaves it, switches to another display of the machine, sets the scaling and takes
// screenshots. Called after the other windows.
static void draw_fullscreen(App& app)
{
    if (app.fullscreen == FS_OFF) return;
    ImGuiContext& g = *GImGui;
    Machine* m = app.host.machine();
    DisplayDevice* gfx[MAX_GFX_VIEWS];
    int n = graphic_displays(m, gfx);
    int k = app.fullscreen;
    DisplayDevice* d = k >= 0 ? (k < n ? gfx[k] : nullptr) : (m ? m->display : nullptr);
    if (!d) {   // that display is gone (another machine): the display used last
        k = used_display(app, &d);
        app.fullscreen = k;
    }
    app.shot_target = k;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);   // over the menu bar too (it stays, hidden: the work area does not change)
    ImGui::SetNextWindowSize(vp->Size);
    // the keyboard to it, unless a dialog or its menu is open (another window may have taken it: a machine started...)
    ImGuiWindow* fw = ImGui::FindWindowByName("###fullscreen");
    if (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) && !(fw && g.NavWindow && g.NavWindow->RootWindow == fw))
        ImGui::SetNextWindowFocus();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(k_background));
    // behind all others (created at the back, never raised): the dialogs come over it, the other windows are hidden
    ImGui::Begin("###fullscreen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                               ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavInputs);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    fw = ImGui::GetCurrentWindow();
    app.ui.screen_focused = ImGui::IsWindowFocused();   // the keys go to SAPI (Esc too), not with a dialog open
    if (d) {
        ZoomState fit;   // the picture as large as the monitor allows
        if (k >= 0) {
            GfxView& v = app.gfx[k];
            draw_display(app, d, v.tex, v.w, v.h, v.pixels, fit, false, true);
        } else {
            draw_display(app, d, app.screen_tex, app.tex_w, app.tex_h, app.pixels, fit, false, true);
        }
    } else {   // switched off
        ImGui::SetCursorPos(ImVec2(ImGui::GetFontSize(), ImGui::GetFontSize()));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.66f, 0.66f, 0.66f, 1.0f));
        ImGui::TextUnformatted(tr("Switched off (POWER on the ZDR panel, Machine -> Switch on)"));
        ImGui::PopStyleColor();
    }
    if (app.open_screen_menu) {   // UI snapshot
        ImGui::OpenPopup("fsmenu");
        app.open_screen_menu = false;
    }
    if (ImGui::BeginPopupContextWindow("fsmenu")) {
        fullscreen_item(app, k);
        if (m && (m->display ? 1 : 0) + n > 1) {   // more displays: which one fills the screen
            ImGui::Separator();
            ImGui::TextDisabled("%s", tr("Display:"));
            if (m->display && ImGui::MenuItem(display_name(m->display, tr("Screen")), nullptr, k == -1))
                fullscreen_set(app, -1);
            for (int i = 0; i < n; i++) {
                ImGui::PushID(i);   // two cards of one type may have the same name
                if (ImGui::MenuItem(display_name(gfx[i], tr("Graphics")), nullptr, k == i)) fullscreen_set(app, i);
                ImGui::PopID();
            }
        }
        ImGui::Separator();
        ImGui::MenuItem(tr("Integer scaling"), nullptr, &app.integer_scale);
        ImGui::MenuItem(tr("TV aspect ratio"), nullptr, &app.tv_aspect);
        ImGui::Separator();
        shot_items(app, d, display_name(d, "screen"));
        ImGui::EndPopup();
    }
    ImGui::End();
    // every other window hidden in this frame (drawn, not shown, not under the mouse in the next one), the dialogs,
    // menus and tooltips not; the menu bar too, but it keeps its place (the work area and the dock stay as they are)
    for (ImGuiWindow* w : g.Windows) {
        if (!w->Active) continue;
        ImGuiWindow* r = w->RootWindow ? w->RootWindow : w;
        if (r == fw || (r->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip))) continue;
        w->Hidden = true;
    }
}

void draw_log(App& app)
{
    if (!app.ui.show_log) return;
    layout_window(LW_LOG);
    if (app.log_focus) {
        ImGui::SetNextWindowFocus();
        app.log_focus = false;
    }
    if (ImGui::Begin(tr_id("Log"), &app.ui.show_log)) {
        app.errors_seen = log_errors();   // seen here: "Errors: N" in the status bar goes away
        std::vector<std::string> lines;
        log_recent(lines, 500);
        ImGui::BeginChild("loglines");
        for (const std::string& l : lines) {   // errors and warnings in their colours (each line starts with its level)
            bool e = l.compare(0, 6, "error:") == 0, w = l.compare(0, 5, "warn:") == 0;
            if (e || w) ImGui::PushStyleColor(ImGuiCol_Text, e ? theme().error : theme().warn);
            ImGui::TextUnformatted(l.c_str());
            if (e || w) ImGui::PopStyleColor();
        }
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
        // wrapped: a long path of a medium would push the reason past the edge of a narrow window
        float wrap = ImGui::GetIO().DisplaySize.x * 0.8f, most = 45.0f * ImGui::GetFontSize();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (wrap < most ? wrap : most));
        ImGui::TextUnformatted(app.error_text.c_str());
        ImGui::PopTextWrapPos();
        // the only button: the default one (Enter), Esc closes it too
        if (theme_accent_button(tr("Close")) || popup_enter() || popup_escape()) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// Another GUI runs over the same settings or work folder (app_run): said at the start, after an Error of the start
// (one modal popup at a time). Continue (the default, Enter, Esc) goes on (the machine runs, the layout is saved);
// Quit leaves without saving the settings, the window layout and the machine of this window (they stay as the other
// one has them).
void draw_instance_warning(App& app)
{
    if (!app.instance_warn) return;
    const char* id = tr_id("Another SAPIemu is running");
    if (!ImGui::IsPopupOpen(id) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) ImGui::OpenPopup(id);
    if (!ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    float u = ImGui::GetFontSize();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 34.0f * u);
    ImGui::TextUnformatted(tr("Another window of SAPIemu already runs with the same settings or the same work folder:"));
    ImGui::TextDisabled("%s", app.instance_paths.c_str());
    ImGui::TextUnformatted(tr("Both write there: one window may overwrite the settings, the window layout, the RAM "
                              "disks, disks and battery memories of the other. Use one window, or quit this one."));
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    bool go = theme_accent_button(tr("Continue"));
    ImGui::SetItemDefaultFocus();
    ImGui::SameLine();
    if (ImGui::Button(tr("Quit"))) {
        app.quit_nosave = true;
        app.quit = true;
    }
    if (go || app.quit || popup_enter() || popup_escape()) {
        app.instance_warn = false;
        if (!app.quit && app.ini_held) {   // Continue: the window layout is saved again from now on
            ImGui::GetIO().IniFilename = app.ini_held;
            ImGui::MarkIniSettingsDirty();   // the places taken while the warning waited
        }
        app.ini_held = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace

void ui_notice(UiState& ui, bool ok, const std::string& text)
{
    ui.notice = text;
    ui.notice_error = !ok;
    ui.notice_ns = SDL_GetTicksNS();
    if (ok) LOGI("%s", text.c_str());
    else LOGE("%s", text.c_str());
}

// Not in the frame the popup opens (the key that opened it, e.g. Enter of a path field, does not answer it) and only
// for the popup in front (an Error over another question takes the key alone).
static bool popup_key_ok(void)
{
    return !ImGui::IsWindowAppearing() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
}

// The frame of the keyboard navigation (io.ConfigNavCursorVisibleAuto off), after ImGui::NewFrame: Tab without Ctrl
// and Alt in a window that takes the navigation shows it (ImGui puts it on a control in this frame and moves it on
// with the next Tab or the arrows), a click of the mouse hides it. ImGui presses a control by Space or Enter only
// with the frame shown, so an arrow alone (a player's key for SAPI in another window) presses nothing.
static void nav_cursor_frame(void)
{
    ImGuiContext& g = *GImGui;
    for (int b = 0; b < ImGuiMouseButton_COUNT; b++)
        if (g.IO.MouseClicked[b]) {
            if (g.NavCursorVisible) ImGui::SetNavCursorVisible(false);
            return;
        }
    if (!g.NavCursorVisible && ImGui::IsKeyPressed(ImGuiKey_Tab, false) && !g.IO.KeyCtrl && !g.IO.KeyAlt &&
        g.NavWindow && !(g.NavWindow->Flags & ImGuiWindowFlags_NoNavInputs))
        ImGui::SetNavCursorVisible(true);
}

// The texts of Dear ImGui itself in the language of the UI: the list of Ctrl+Tab ("(Main menu bar)"), the tooltip and
// the menu of a link, the docked layout. Registered again when the language changes (tr() keeps its texts static).
static void imgui_localize(void)
{
    static int s_lang = -1;
    if (s_lang == (int)i18n_language()) return;
    s_lang = (int)i18n_language();
    const ImGuiLocEntry e[] = {
        { ImGuiLocKey_WindowingMainMenuBar, tr("(Main menu bar)") },
        { ImGuiLocKey_WindowingPopup, tr("(Popup)") },
        { ImGuiLocKey_WindowingUntitled, tr("(Untitled)") },
        { ImGuiLocKey_OpenLink_s, tr("Open '%s'") },
        { ImGuiLocKey_CopyLink, tr("Copy Link###CopyLink") },
        { ImGuiLocKey_DockingHideTabBar, tr("Hide tab bar###HideTabBar") },
        { ImGuiLocKey_DockingHoldShiftToDock, tr("Hold SHIFT to enable Docking window.") },
        { ImGuiLocKey_DockingDragToUndockOrMoveNode, tr("Click and drag to move or undock whole node.") },
    };
    ImGui::LocalizeRegisterEntries(e, (int)(sizeof(e) / sizeof(e[0])));
}

bool nav_cursor_on_item(void)
{
    ImGuiContext& g = *GImGui;
    return g.NavCursorVisible && g.NavId != 0 && g.ActiveId == 0 && g.NavWindow &&
           g.NavWindow->RootWindow == ImGui::GetCurrentWindow()->RootWindow;
}

bool popup_enter(void)
{
    // the keyboard navigation (Tab, arrows) shows its cursor on a button or a box of this window: Enter presses that
    // one (ImGui does it), not the default button; a text field being edited keeps Enter for the default
    if (nav_cursor_on_item()) return false;
    return popup_key_ok() &&
           (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
}

static bool g_picture_only = false;            // --ui-picture (UiSnapshot::picture)
static bool g_picture_set = false;             // marked in this frame
static ImVec2 g_picture_min, g_picture_max;    // their union (screen points)

bool picture_only(void)
{
    return g_picture_only;
}

void picture_mark(float x0, float y0, float x1, float y1)
{
    if (!g_picture_set) {
        g_picture_min = ImVec2(x0, y0);
        g_picture_max = ImVec2(x1, y1);
        g_picture_set = true;
        return;
    }
    g_picture_min = ImVec2(std::min(g_picture_min.x, x0), std::min(g_picture_min.y, y0));
    g_picture_max = ImVec2(std::max(g_picture_max.x, x1), std::max(g_picture_max.y, y1));
}

bool popup_escape(void)
{
    return popup_key_ok() && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
}

void copy_screen_text(UiState& ui)
{
    Machine* m = ui.host->machine();
    if (!m) return;
    if (!m->display) {
        ui_notice(ui, true, tr("The machine has no text display."));
        return;
    }
    std::string text = screen_text(*m);
    while (!text.empty() && text.back() == '\n') text.pop_back();   // the empty rows at the bottom
    size_t first = text.find_first_not_of('\n');                     // and at the top
    text = first == std::string::npos ? std::string() : text.substr(first);
    if (text.empty()) {
        ui_notice(ui, true, tr("The screen has no text."));
        return;
    }
    text += '\n';
    SDL_SetClipboardText(text.c_str());   // UTF-8; Windows gets CR LF
    ui_notice(ui, true, tr("Screen text copied to the clipboard."));
}

void debug_toggle_breakpoint(UiState& ui)
{
    Machine* m = ui.host->machine();
    if (!m || !m->cpu) return;
    if (!ui.show_debugger) {
        ui_notice(ui, true, tr("F9 sets a breakpoint in the debugger (Debug -> Debugger). Reset is F12."));
        return;
    }
    uint16_t a;
    if (ui.disasm_hover >= 0) a = (uint16_t)ui.disasm_hover;
    else {
        CpuRegs r;
        m->cpu->get_regs(r);
        a = r.pc;
    }
    bool on = !m->has_breakpoint(a);
    m->set_breakpoint(a, on);
    char t[96];
    snprintf(t, sizeof(t), "%s %04Xh", on ? tr("Breakpoint set at") : tr("Breakpoint removed at"), (unsigned)a);
    ui_notice(ui, true, t);
}

void app_layout_override(bool docked)
{
    g_layout_override = docked ? 1 : 0;
}

const MachineWindow* machine_windows(int* count)
{
    *count = k_machine_window_count;
    return k_machine_windows;
}

// the devices of machine_has_device remembered for a switched off machine (UiState::devices_last: bit i = k_devices[i])
static const char* const k_devices[] = { "aps5", "ims2", "diag", "terminal", "printer", "minigraf" };
static const int k_device_count = (int)(sizeof(k_devices) / sizeof(k_devices[0]));

static bool running_has_device(UiState& ui, Machine* m, const char* key)
{
    if (!strcmp(key, "aps5")) return aps5_in_machine(ui);
    if (!strcmp(key, "ims2")) return ims2_in_machine(ui);
    if (!strcmp(key, "diag")) return m->cpu && m->cpu->k3_diag();
    if (!strcmp(key, "terminal")) return !m->serials.empty();
    for (auto& c : m->cards) {
        if (!strcmp(key, "printer") && c->printer()) return true;
        if (!strcmp(key, "minigraf") && c->plotter()) return true;
    }
    return false;
}

void machine_devices_seen(UiState& ui)
{
    Machine* m = ui.host->machine();
    if (!m) return;   // switched off: the devices of the machine as it ran stay
    ui.devices_last = 0;
    for (int i = 0; i < k_device_count; i++)
        if (running_has_device(ui, m, k_devices[i])) ui.devices_last |= 1u << i;
    ui.devices_known = true;
}

bool machine_has_device(UiState& ui, const char* key)
{
    if (!strcmp(key, "media")) return true;   // the window says when there is none
    if (Machine* m = ui.host->machine()) return running_has_device(ui, m, key);
    // switched off: the machine that ran (it comes back as it was); before any ran, its windows say it is off
    for (int i = 0; i < k_device_count; i++)
        if (!strcmp(key, k_devices[i])) return !ui.devices_known || (ui.devices_last >> i & 1u);
    return false;
}

bool device_window_off(UiState& ui)
{
    if (ui.host->machine()) return false;
    ImGui::TextWrapped("%s", tr("The machine is switched off (Machine -> Switch on)."));
    return true;
}

// "File -> Choose a machine -> <category>" of a machine file in the index, "-> <its title>" after it with `title`, in
// the language of the UI (the names of the index, so they never differ from the window); empty when it is not there.
static std::string browser_place(UiState& ui, const char* file, bool title)
{
    std::string t = machine_title(ui, file, std::string());   // reads the index once
    const MachineEntry* e = machine_index_find(ui.machine_index, file);
    if (!e || e->category < 0 || e->category >= (int)ui.machine_index.categories.size()) return std::string();
    const MachineCategory& c = ui.machine_index.categories[(size_t)e->category];
    bool en = i18n_language() == LANG_EN;
    std::string s = std::string(tr("File")) + " -> " + tr("Choose a machine") + " -> " +
                    (en && !c.name_en.empty() ? c.name_en : c.name);
    if (title) s += " -> " + t;
    return s;
}

std::string device_missing(UiState& ui, const char* key)
{
    const struct {
        const char* key;
        const char* none;     // tr() key: the machine has no such device
        const char* file;     // a machine with it in the window Choose a machine, or nullptr
        bool title;           // name that machine (else its category only: there are more)
        const char* config;   // tr() key: where it goes in the configurator, or nullptr
    } k[] = {
        { "aps5", "This machine has no APS-5 measuring unit.", "machines/aps5.sapi", true, nullptr },
        { "ims2", "This machine has no IMS-2 measuring bench.", "machines/zps/ZPS-2_DPS-1_IMS-2.sapi", true, nullptr },
        { "minigraf", "This machine has no plotter.", "machines/zps/ZPS-2_MIKOS_V5_Minigraf.sapi", false,
          "In the configurator: DPP-1 (Minigraf 0507, XY 4130) or JPR-1 (XY 4130 on X3)." },
        { "diag", "This machine has no diagnostic board.", "machines/jpr1s_bgp1s.sapi", true,
          "In the configurator: K3 of JPR-1S or JPR-1X = diagnostic board." },
        { "printer", "This machine has no printer.", nullptr, false,
          "In the configurator: the printer on X3 of the CPU card, on DPP-1 or on PKT-1." },
        { "terminal", "This machine has no serial port.", nullptr, false,
          "In the configurator: a card with a serial port (DSM-1, DSP-1, RSW-1...)." },
    };
    for (const auto& d : k) {
        if (strcmp(key, d.key) != 0) continue;
        std::string s = tr(d.none);
        std::string place = d.file ? browser_place(ui, d.file, d.title) : std::string();
        if (!place.empty()) s += std::string("\n") + tr("Machines with this device:") + " " + place + ".";
        if (d.config) s += std::string("\n") + tr(d.config);
        return s;
    }
    return std::string();
}

static size_t snap_keys(const UiSnapshot* snap)
{
    return snap ? snap->keys.size() : 0;
}

// --ui-key "Ctrl+Shift+C", "F9", "Enter", "Esc": the key event into the queue of SDL, as the keyboard sends it.
static void snap_key(App& app, const std::string& spec, bool down)
{
    static const struct {
        const char* prefix;
        SDL_Keymod mod;
    } k_mods[] = { { "Ctrl+", SDL_KMOD_LCTRL }, { "Shift+", SDL_KMOD_LSHIFT }, { "Alt+", SDL_KMOD_LALT } };
    std::string name = spec;
    unsigned mod = 0;
    for (bool more = true; more;) {
        more = false;
        for (const auto& k : k_mods)
            if (name.rfind(k.prefix, 0) == 0) {
                mod |= k.mod;
                name.erase(0, strlen(k.prefix));
                more = true;
            }
    }
    if (name == "Enter") name = "Return";
    if (name == "Esc") name = "Escape";
    SDL_Keycode key = SDL_GetKeyFromName(name.c_str());
    if (key == SDLK_UNKNOWN) {
        if (down) fprintf(stderr, "UI snapshot: unknown key %s\n", spec.c_str());
        return;
    }
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    ev.key.windowID = SDL_GetWindowID(app.window);
    ev.key.key = key;
    ev.key.scancode = SDL_GetScancodeFromKey(key, nullptr);
    ev.key.mod = (SDL_Keymod)mod;
    ev.key.down = down;
    ev.key.timestamp = SDL_GetTicksNS();
    SDL_PushEvent(&ev);
    // a character key without Ctrl and Alt types its character too, as the keyboard does (the text event after the key)
    unsigned char c = name.size() == 1 ? (unsigned char)name[0] : 0;
    if (down && c > ' ' && c < 127 && !(mod & (SDL_KMOD_LCTRL | SDL_KMOD_LALT))) {
        static char text[128][2];   // the event keeps the pointer
        text[c][0] = (char)((mod & SDL_KMOD_LSHIFT) ? toupper(c) : tolower(c));
        text[c][1] = 0;
        SDL_Event t;
        SDL_zero(t);
        t.type = SDL_EVENT_TEXT_INPUT;
        t.text.windowID = SDL_GetWindowID(app.window);
        t.text.text = text[c];
        t.text.timestamp = SDL_GetTicksNS();
        SDL_PushEvent(&t);
    }
}

// Files dropped on the window (SDL_EVENT_DROP_FILE, --ui-drop): one in each frame, after the menu bar (file_drop does what
// the menus do with it). Not while a modal window waits for its answer (the question of the file before, an Error): the
// file is not taken, the status bar says so. The keyboard goes back to the display (Explorer had the mouse).
static void drop_frame(App& app)
{
    if (app.drops.empty()) return;
    UiState& ui = app.ui;
    // a modal window opens in this frame (a question of the file before, an Error not shown yet): the next frame decides
    if (ui.hex_ask || ui.bin_ask || ui.tprog_ask || ui.rack.ask_unsaved || app.show_error || ui.notice_error) return;
    std::string path = app.drops.front();
    app.drops.erase(app.drops.begin());
    if (ImGui::GetTopMostPopupModal()) {
        ui_notice(ui, true, std::string(tr("Close the open dialog first, then drop the file again")) + ": " + path);
        return;
    }
    LOGI("file dropped on the window: %s", path.c_str());
    file_drop(ui, path);
    ui.focus_display = 2;
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

    // the scale for the size of a new window: the primary display, where SDL centres it; once the window exists, the
    // scale of its display (follow_display_scale)
    float dpi = display_scale(SDL_GetPrimaryDisplay());
    app.dpi_fixed = snap && snap->scale > 0;
    if (app.dpi_fixed) dpi = snap->scale;   // --ui-scale: pictures at 125 %, 150 %... on any PC
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
            // (the layout of the versions before 2026-10-03 in AppData is no longer copied here: a deleted imgui.ini
            // brought back a stale layout, decision 2026-10-29)
            app.ini_path = dir + "imgui.ini";
        }
    }
    if (snap && settings_file.empty()) app.ini_path.clear();   // a self-check uses a layout only when given
    app.ini_docked_path = ini_docked_name(app.ini_path);
    // one GUI over its settings and its work folder (the RAM disks, disks and battery memories): a second one is warned
    // (draw_instance_warning), not stopped. sapiemu-cli and the UI snapshots run beside it freely and take no lock.
    bool settings_taken = false;
    if (!snap) {
        settings_taken = !app.settings_path.empty() && file_lock_try(app.settings_path + ".lock") == 0;
        make_dirs(work_root());
        bool work_taken = file_lock_try(path_join(work_root(), "sapiemu-gui.lock")) == 0;
        if (settings_taken) app.instance_paths = app.settings_path;
        if (work_taken) app.instance_paths += (settings_taken ? "\n" : "") + work_root();
        app.instance_warn = settings_taken || work_taken;
        if (app.instance_warn) LOGW("another SAPIemu window runs over the same settings or work folder: %s",
                                    app.instance_paths.c_str());
    } else if (snap->second) {   // --ui-second: the picture of the warning
        app.instance_paths = (app.settings_path.empty() ? std::string("sapiemu-settings.json") : app.settings_path) +
                             "\n" + work_root();
        app.instance_warn = true;
    }
    // the log of this run next to the settings (sapiemu.log; FILE.log for --settings FILE), for error reports; a
    // second GUI over the same settings does not take the log of the first one (sapiemu.2.log)
    if (!app.settings_path.empty()) {
        std::string log_file = !settings_file.empty() ? settings_file + ".log"
                                                      : path_join(path_dir(app.settings_path), "sapiemu.log");
        if (settings_taken) log_file = log_file.substr(0, log_file.size() - 4) + ".2.log";
        // the log of the run before stays as sapiemu.prev.log (one generation): after a crash the tester starts
        // SAPIemu again to reach Report a bug, and that start must not throw the log of the crash away
        std::string prev = log_file.substr(0, log_file.size() - 4) + ".prev.log";   // log_file ends with ".log"
        if (file_exists(log_file) && file_rename(log_file, prev)) app.ui.log_prev_file = prev;
        else if (file_exists(prev)) app.ui.log_prev_file = prev;   // not renamed (held open): the older one is still there
        if (log_set_file(log_file)) {
            LOGI("SAPIemu %s, data %s, work %s", sapiemu_version(), data_root().c_str(), work_root().c_str());
            app.ui.log_file = log_file;   // Help -> Report a bug
        }
    }
    app.ui.settings_file = app.settings_path;
    // no settings yet: the first start of a fresh copy offers the machines (window Choose a machine, author
    // 2026-10-17; before it was the configuration behind the screen)
    bool first_start = !app.settings_path.empty() && !file_exists(app.settings_path);
    ini_rename_windows(app.ini_path);
    app.ui.layout_fresh = app.ini_path.empty() || !file_exists(app.ini_path);   // no window has a saved place
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
    bool sized = sww >= 320 && swh >= 240;
    if (sized) {
        win_w = sww;
        win_h = swh;
    }
    SDL_Rect ua;
    float ds = 1.0f;
    SDL_Rect at = { sx, sy, win_w, win_h };
    if (!snap && usable_area(spos ? &at : nullptr, &ua, &ds)) {
        // the first start: 88 % of the usable area of the primary display (without the task bar); a saved size no
        // larger than the usable area of its display (a smaller display or a larger scale than before), and then the
        // window moved into it. fw, fh: the frame and the title bar of the system around the client area.
        if (!sized) {
            win_w = (int)(ua.w * 0.88f);
            win_h = (int)(ua.h * 0.88f);
        }
        int fw = (int)(16 * ds), fh = (int)(40 * ds);
        bool clamped = win_w > ua.w - fw || win_h > ua.h - fh;
        if (win_w > ua.w - fw) win_w = ua.w - fw;
        if (win_h > ua.h - fh) win_h = ua.h - fh;
        if (win_w < 320) win_w = 320;
        if (win_h < 240) win_h = 240;
        if (clamped && spos) {
            if (sx + win_w + fw / 2 > ua.x + ua.w) sx = ua.x + ua.w - win_w - fw / 2;
            if (sx - fw / 2 < ua.x) sx = ua.x + fw / 2;
            if (sy + win_h + fw / 2 > ua.y + ua.h) sy = ua.y + ua.h - win_h - fw / 2;
            if (sy - (fh - fw / 2) < ua.y) sy = ua.y + fh - fw / 2;
        }
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
    follow_display_scale(app);   // a saved place on another monitor; the style is not built yet (app.restyle)
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
    // Tab, the arrows, Space and Enter move and press in the windows of the emulator (WCAG 2.1.1). The windows the
    // keys of SAPI go to have ImGuiWindowFlags_NoNavInputs (the displays, the full screen, the serial terminal): there
    // the arrows, Space, Enter, Esc and Alt stay SAPI's (handle_key routes by the focus, not by ImGui). The frame of
    // the navigation shows only after Tab (nav_cursor_frame), not after an arrow: a player's arrows and Space, meant
    // for SAPI while another window has the focus, do not press its buttons (a guide's "Type into SAPI").
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigNavCursorVisibleAuto = false;
    io.ConfigWindowsMoveFromTitleBarOnly = true;   // a drag on a picture or a panel does not move its window
    io.IniFilename = app.ini_path.empty() ? nullptr : app.ini_path.c_str();   // none for a self-check
    // the fonts of all looks (baked on demand); the style is built before the first frame (app.restyle)
    theme_fonts_load(io);
    app.ui.mono = theme_fonts().mono;
    ImGui_ImplSDL3_InitForSDLRenderer(app.window, app.renderer);
    ImGui_ImplSDLRenderer3_Init(app.renderer);

    settings_apply(app, settings);
    {   // the window places of the saved layout (read at the first frame)
        const std::string& ini = layout_ini(app, app.docked);
        io.IniFilename = ini.empty() ? nullptr : ini.c_str();
        app.ui.layout_fresh = ini.empty() || !file_exists(ini);
    }
    if (g_layout_override >= 0 && (g_layout_override == 1) != app.docked) {
        // --ui-layout other than the saved layout: its default places for this run, the saved ones untouched
        app.layout_override = true;
        app.override_docked = g_layout_override == 1;
        io.IniFilename = nullptr;
        app.ui.layout_fresh = true;
    }
    if (layout_docked(app) && app.ui.layout_fresh) app.ui.layout_reset = true;   // no saved layout: dock the windows
    // no saved window places and the configurator never opened or closed (the first start): it opens where it fits
    // (draw_rack), as View -> Default window layout opens it
    if (!layout_docked(app) && app.ui.layout_fresh && !app.ui.show_rack &&
        !settings.value("windows", nlohmann::json::object()).contains("configuration"))
        app.ui.rack_auto = true;
    if (first_start) {   // closed without starting a machine, it opens the guide of the running one (browser.cpp)
        app.ui.show_browser = true;
        app.ui.browser_first = true;
    }
    // --machine as the rest of the program keeps it (machines/x.sapi): a backslash or a whole path inside the data
    // finds the machine in the index (its English title) and in Recent machines
    std::string first = machine_file.empty() ? std::string() : data_relative(data_path(machine_file));
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
        int back = STATE_BACK_SAME;
        if (!app.host.load_state_file(snap->state, err, &back)) {   // the Error window in the picture, as File -> Load state
            fprintf(stderr, "UI snapshot: %s\n", err.c_str());
            ui_notice(app.ui, false, state_load_error(app.ui, snap->state, err, back));
        }
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
        app.ui.show_report = snap->report;
        if (!snap->help.empty()) {   // the window Help (not over Choose a machine of a first start)
            if (snap->help == "glossary") help_open(app.ui, HELP_GLOSSARY);
            else if (snap->help == "first") help_open(app.ui, HELP_FIRST_STEPS);
            else help_open_doc(app.ui, snap->help);   // a .md from the data root (docs/navod-kzd1.md)
            app.ui.show_browser = snap->browser;
            app.ui.help_set_open = snap->help_open;
            app.ui.help_scroll = snap->help_scroll;
        }
        if (snap->debugger) app.ui.show_debugger = true;
        if (!snap->memory.empty()) {   // as typed into Go to with Enter
            app.ui.show_memory = true;
            snprintf(app.ui.mem_goto, sizeof(app.ui.mem_goto), "%s", snap->memory.c_str());
            app.ui.mem_jump = true;
        }
        app.ui.show_media = snap->media;
        app.ui.show_printer = snap->printer;
        app.ui.show_plotter = snap->plotter;
        app.ui.show_ims2 = snap->ims2;
        app.ui.show_diag = snap->diag;
        app.ui.show_aps5 = snap->aps5 || snap->aps5_demo;
        app.ui.aps5_demo_autostart = snap->aps5_demo;
        app.ui.show_terminal = snap->terminal;
        app.ui.show_log = snap->log;
        app.ui.rack.sel = snap->rack_slot;
        app.ui.rack.machine_gen = ~0ull;
        g_picture_only = snap->picture && !snap->crop.empty();
        if (snap->save_copy) rack_save_copies_set(true);   // Save as a release build does it (My machines)
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
                theme_titlebar(app.window);   // the dimmed text of an inactive window
                // the disks and RAM disks written so far into their files (autosave); not before the answer to the
                // warning about another window
                if (!app.instance_warn) app.host.save_media_new();
            }
            else if (ev.type == SDL_EVENT_WINDOW_FOCUS_GAINED) theme_titlebar(app.window);
            else if (ev.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN && app.fullscreen != FS_OFF && !app.snapshot)
                fullscreen_set(app, FS_OFF);   // the system took the full screen away
            else if (ev.type == SDL_EVENT_TEXT_INPUT) handle_text(app, ev.text.text);
            else if (ev.type == SDL_EVENT_DROP_FILE && ev.drop.data) app.drops.push_back(ev.drop.data);   // drop_frame
            else if (ev.type == SDL_EVENT_GAMEPAD_ADDED || ev.type == SDL_EVENT_GAMEPAD_REMOVED) joystick_event(app.joy, ev);
            else if (ev.type == SDL_EVENT_SYSTEM_THEME_CHANGED) app.restyle = true;   // Windows light / dark
            else if (ev.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED || ev.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED ||
                     ev.type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED)
                follow_display_scale(app);   // another monitor, or its scale changed in the system
        }
        if (!app.instance_warn) app.host.poll();   // MCP commands (they wait for the answer to the warning)
        if (app.title_gen != app.host.machine_generation()) {   // loaded by MCP, the menu or the configurator
            app.title_gen = app.host.machine_generation();
            app.title_file = app.host.machine_file();
            set_title(app);
            open_machine_windows(app);
            app.focus_screen = 3;
            // the main output is the display used last now (it takes the keyboard, focus_screen_window); a full
            // screen shows it
            Machine* nm = app.host.machine();
            machine_devices_seen(app.ui);   // the View items of its devices, also once it is switched off
            if (nm) {   // another machine shows all its displays (View -> Displays); switched off: as they were
                std::string sig = displays_sig(app, nm);
                if (sig != app.hidden_sig) {
                    app.hidden_sig = sig;
                    memset(app.display_hidden, 0, sizeof(app.display_hidden));
                }
            }
            DisplayDevice* gfx[MAX_GFX_VIEWS];
            app.shot_target = main_output(nm, gfx, graphic_displays(nm, gfx));
            if (display_hidden(app, app.shot_target)) app.shot_target = shown_display(app);
            if (app.fullscreen != FS_OFF) app.fullscreen = app.shot_target;
            app.dock_follow = layout_docked(app);   // docked: its main output into the middle (dock_follow)
        } else if (app.title_file != app.host.machine_file()) {   // saved as a copy in My machines: its name
            app.title_file = app.host.machine_file();
            set_title(app);
        }
        osd_follow(app);
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
        // not while the warning about another window waits: nothing of this one goes into the shared files before the
        // answer (Quit drops the machine as it was loaded)
        if (!app.instance_warn) app.host.frame(now - last);
        last = now;
        audio_frame(app.audio, app.host.machine(),
                    !snap && !app.host.paused && !app.host.max_speed && app.host.pace_percent == 100);

        if (app.restyle) {   // a look chosen, the system switched between light and dark: between frames only
            apply_look(app);
            app.restyle = false;
        }
        if (app.layout_switch) {   // View -> Settings -> Windows: the window places of the other layout
            switch_layout(app);
            app.layout_switch = false;
        }
        imgui_localize();
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        if (app.instance_warn && io.IniFilename) {
            // the layout read by the first NewFrame; while the warning waits, ImGui's autosave writes nothing
            app.ini_held = io.IniFilename;
            io.IniFilename = nullptr;
        }
        nav_cursor_frame();
        g_picture_set = false;   // the windows mark their pictures again in this frame
        // The ImGui SDL3 backend stops SDL text input whenever an ImGui text field
        // loses focus (debugger, configurator...); the SAPI keyboard and the serial
        // terminal need it all the time, otherwise no characters arrive.
        if (!SDL_TextInputActive(app.window)) SDL_StartTextInput(app.window);
        // full screen: the windows opening take their first places in the main window after it (layout.cpp)
        layout_hold(app.fullscreen != FS_OFF);
        layout_rack_low(app.ui.show_rack && app.ui.rack_by_layout);   // the windows opening later may cover it
        if (app.ui.layout_reset || app.ui.layout_arrange) {   // before the dock space and the windows of this frame
            reset_window_layout(app, !app.ui.layout_reset);
            app.ui.layout_reset = app.ui.layout_arrange = false;
            app.dock_follow = false;   // the dock is built for the machine
        } else if (app.dock_follow) {
            if (layout_docked(app)) dock_follow(app);
            app.dock_follow = false;
        }
        // without the close button at the right end of a dock (it closed the tab shown, whose tab has its own close button)
        app.dockspace_id = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode |
                                                                        ImGuiDockNodeFlags_NoCloseButton);
        IM_ASSERT(app.dockspace_id == layout_dockspace_id());   // the docked layout is built into this one
        draw_menu(app);   // the menu bar first: the work area of the windows is below it
        drop_frame(app);  // a file dropped on the window, before the windows that show what it did (Media, questions)
        layout_keep_inside();   // the windows outside a work area that got smaller come back into it
        plan_layout(app);
        refit_displays(app);   // a machine change: the display windows the size of their new pictures
        // The order is the order in which the windows take their first places (layout.cpp): the displays, the
        // instruments and Media (the machine opens them), the guide beside them, the tools, the dialogs.
        draw_screens(app);
        if (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) app.screen_keys = app.ui.screen_focused;
        shot_poll(app.ui);
        video_poll(app.ui);
        draw_plotter(app.ui);
        draw_ims2(app.ui);
        draw_diag(app.ui);
        draw_aps5(app.ui);
        draw_media(app.ui);
        draw_guide(app.ui);
        draw_debugger(app.ui);
        draw_memory(app.ui);
        draw_iolog(app.ui);
        draw_terminal(app.ui);
        draw_printer(app.ui);
        draw_browser(app.ui);
        draw_settings(app);
        draw_help(app.ui);
        draw_about(app.ui);
        draw_shortcuts(app.ui);
        draw_report(app.ui);
        draw_reset_question(app.ui);
        draw_rack(app.ui);
        draw_log(app);
        draw_error(app);
        draw_instance_warning(app);
#ifndef NDEBUG
        if (app.style_editor) {
            if (ImGui::Begin(tr_id("Style editor (development)"), &app.style_editor)) ImGui::ShowStyleEditor();
            ImGui::End();
        }
#endif
        layout_dock_frame();   // the tabs chosen by the docked layout stay in front while its windows open
        if (app.focus_screen > 0) {   // after the windows that opened with the machine
            app.focus_screen--;
            if (!snap && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) focus_screen_window(app);
        } else if (app.ui.focus_display > 0) {   // Type into SAPI, Paste text, a load, a closed guide...
            app.ui.focus_display--;
            if (!snap && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) focus_display_window(app);
        }
        draw_fullscreen(app);   // last: it takes the keyboard back and hides the other windows
        theme_focus_frame();
        ImGui::Render();

        SDL_SetRenderScale(app.renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        ImU32 desk = theme().desktop;   // behind the windows: the colour of the look
        SDL_SetRenderDrawColor(app.renderer, (Uint8)(desk & 0xFF), (Uint8)((desk >> 8) & 0xFF), (Uint8)((desk >> 16) & 0xFF),
                               255);
        SDL_RenderClear(app.renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), app.renderer);
        if (snap && snap_frames == 3) {   // the menu items, after the windows have their places
            if (snap->reset_layout) app.ui.layout_reset = true;
            if (snap->arrange) app.ui.layout_arrange = true;
            if (snap->layout_switch) set_layout(app, !layout_docked(app));
            if (snap->zoom) {   // View -> Screen zoom: the display used last
                DisplayDevice* zd = nullptr;
                display_zoom(app, used_display(app, &zd)).request = snap->zoom;
            }
            if (snap->fullscreen > 0) fullscreen_toggle(app);   // Alt+Enter
            for (const std::string& h : snap->hide_displays) {   // View -> Displays
                int k = h.compare(0, 3, "gfx") == 0 ? atoi(h.c_str() + 3) : -1;
                if (h == "screen" || (k >= 0 && k < MAX_GFX_VIEWS)) display_show(app, k, false);
                else fprintf(stderr, "UI snapshot: no display window %s\n", h.c_str());
            }
        }
        if (snap && snap_frames == 6 && snap->fullscreen > 1) {   // and again: the windows come back as they were
            fullscreen_toggle(app);
        }
        if (snap && ++snap_frames == 2) {
            // follow_machine() has run: open the requested description, select the slot
            if (!snap->open.empty()) rack_open_in_editor(app.ui, snap->open);
            app.ui.rack.sel = snap->rack_slot;
            app.ui.rack.scroll_props = snap->rack_slot > 0;   // as after a click on the slot
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
            if (snap->pause) app.host.paused = true;   // the text over the picture comes in the next frame
            if (snap->reset) request_reset(app.ui);
            if (snap->screen_menu) app.open_screen_menu = true;
            if (snap->shot) shot_now(app);
            if (snap->copy) {
                std::string name;
                DisplayDevice* d = shot_display(app, name);
                if (d) shot_copy(app.ui, d, display_px_aspect(app, d));
            }
            if (snap->video > 0) video_toggle(app);
            app.snap_menu = snap->menu;
            if (snap->rack_new) app.ui.rack.new_menu = true;
            if (snap->save) app.ui.rack.save_now = true;   // Save in the configurator (after the changes of --ui-set)
            if (snap->exit) rack_request_exit(app.ui);
        }
        // --ui-drop: a file dropped on the window every second frame from the ninth, as Explorer drops it (the begin, the
        // file, the end); the picture three frames after the last
        int drops = snap ? (int)snap->drops.size() : 0;
        if (snap && snap_frames >= 9 && (snap_frames - 9) % 2 == 0 && (snap_frames - 9) / 2 < drops) {
            SDL_Event ev;
            const SDL_EventType types[3] = { SDL_EVENT_DROP_BEGIN, SDL_EVENT_DROP_FILE, SDL_EVENT_DROP_COMPLETE };
            for (SDL_EventType t : types) {
                SDL_zero(ev);
                ev.type = t;
                ev.drop.windowID = SDL_GetWindowID(app.window);
                ev.drop.timestamp = SDL_GetTicksNS();
                if (t == SDL_EVENT_DROP_FILE) ev.drop.data = snap->drops[(size_t)(snap_frames - 9) / 2].c_str();   // kept
                SDL_PushEvent(&ev);
            }
        }
        int drops_end = 10 + 2 * drops;
        // --ui-click: the mouse at the point, the left button down in the next frame and up in the one after
        int mclicks = snap ? (int)snap->click_xy.size() / 2 : 0;
        if (snap && snap_frames >= 9 && (snap_frames - 9) / 3 < mclicks) {
            size_t k = (size_t)(snap_frames - 9) / 3;
            int phase = (snap_frames - 9) % 3;
            io.AddMousePosEvent(snap->click_xy[2 * k], snap->click_xy[2 * k + 1]);
            if (phase > 0) io.AddMouseButtonEvent(ImGuiMouseButton_Left, phase == 1);
        }
        int mclicks_end = 9 + 3 * mclicks;
        // --ui-mouse, --ui-key: the mouse placed (two frames before the keys: what it hovers is known when they come),
        // then each key down and up in two frames, through the event queue as typed (handle_key and ImGui see them)
        int keys_end = 10 + 2 * (int)snap_keys(snap);
        // --ui-window-item, --ui-front: one click every second frame from the ninth, the picture two frames after the last
        int clicks = snap ? (int)snap->window_clicks.size() : 0;
        if (snap && snap_frames >= 9 && (snap_frames - 9) % 2 == 0 && (snap_frames - 9) / 2 < clicks)
            app.snap_click = snap->window_clicks[(size_t)(snap_frames - 9) / 2];
        int clicks_end = 10 + 2 * clicks;
        // --ui-switch: a machine started every fourth frame from the ninth, the picture four frames after the last
        int switches = snap ? (int)snap->switches.size() : 0;
        if (snap && snap_frames >= 9 && (snap_frames - 9) % 4 == 0 && (snap_frames - 9) / 4 < switches)
            rack_start_machine(app.ui, data_relative(data_path(snap->switches[(size_t)(snap_frames - 9) / 4])));
        if (snap && snap_frames >= 8 && snap->mouse_x >= 0) io.AddMousePosEvent(snap->mouse_x, snap->mouse_y);
        if (snap && snap_frames >= 10 && snap_frames < keys_end)
            snap_key(app, snap->keys[(size_t)(snap_frames - 10) / 2], (snap_frames - 10) % 2 == 0);
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
        if (snap && snap_frames >= 12 && snap_frames >= keys_end + 2 && snap_frames >= clicks_end + 2 &&
            snap_frames >= drops_end + 2 && snap_frames >= mclicks_end + 3 && snap_frames >= 9 + 4 * switches &&
            !(snap->video > 0 && video_busy(app.ui))) {
            // window content as PNG (our own renderer, not the screen)
            SDL_Rect crop, *rect = nullptr;   // --ui-crop: only that window (previews of machines)
            if (!snap->crop.empty()) {
                if (ImGuiWindow* w = ImGui::FindWindowByName(("###" + snap->crop).c_str())) {
                    ImVec2 sc = io.DisplayFramebufferScale;
                    ImVec2 a = w->Pos, b(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y);
                    if (g_picture_only) {   // --ui-picture: the drawn hardware it marked, inside the window
                        if (!g_picture_set) fprintf(stderr, "UI snapshot: no picture marked in ###%s\n", snap->crop.c_str());
                        else {   // the marks have their margins (picture_mark)
                            a = ImVec2(std::max(a.x, g_picture_min.x), std::max(a.y, g_picture_min.y));
                            b = ImVec2(std::min(b.x, g_picture_max.x), std::min(b.y, g_picture_max.y));
                        }
                    }
                    crop = { (int)(a.x * sc.x), (int)(a.y * sc.y), (int)((b.x - a.x) * sc.x), (int)((b.y - a.y) * sc.y) };
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
        // keep the file current (crash, kill); not while the warning about another window waits for its answer
        if (now - app.settings_check_ns > 3000000000ull && !app.instance_warn) {
            app.settings_check_ns = now;
            settings_save(app, false);
        }
    }

    if (app.quit_nosave) {   // Quit at that warning: the layout and the files of the machine of the other window stay
        ImGui::GetIO().IniFilename = nullptr;
        app.host.drop();
    } else {
        settings_save(app, false);
    }
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
