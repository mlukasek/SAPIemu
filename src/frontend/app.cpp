// SDL3 + Dear ImGui frontend: screen window, menus, keyboard routing, debugger and
// terminal windows, MCP server. Emulation runs on this (main) thread via EmuHost.
#include "frontend/app.h"
#include "frontend/hostio.h"
#include "frontend/i18n.h"
#include "frontend/keymap.h"
#include "frontend/ui.h"

#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include "imgui.h"
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
    GfxView gfx[MAX_GFX_VIEWS];   // graphic display cards (DGD-1)

    EmuHost host;
    HostAudio audio;              // sound cards (MPH-1V)
    HostJoystick joy;             // gamepad / keyboard for the joystick ports
    HostMouse mouse;              // the host mouse for the mouse ports (captured by a click)
    McpServer* mcp = nullptr;
    UiState ui;
    std::string error_text;
    bool show_error = false;
    bool quit = false;
    uint64_t title_gen = ~0ull;   // machine generation shown in the window title
    bool integer_scale = true;
    bool tv_aspect = false;
    bool smooth = false;          // linear filtering of the screen texture
    char suppress_text = 0;       // text event of a keypad key already sent as a key
    std::string ini_path;

    // settings (sapiemu-settings.json next to the exe, else in the user's AppData)
    std::string settings_path;
    nlohmann::json settings_saved;   // as last written, to save only on change
    uint64_t settings_check_ns = 0;
    int win_x = 0, win_y = 0, win_w = 0, win_h = 0;   // restored (not maximized) window geometry
    bool win_pos_known = false;
    std::string language;         // "cs" / "en" chosen in View -> Language, empty = by the system
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
                     { "printer", ui.show_printer }, { "configuration", ui.show_rack } };
    j["screen"] = { { "integer_scale", app.integer_scale },
                    { "tv_aspect", app.tv_aspect }, { "smooth", app.smooth }, { "card_types", ui.rack.show_types } };
    j["terminal"] = { { "channel", ui.terminal_channel }, { "emulation", ui.term_emulation },
                      { "cr_newline", ui.term_cr_newline }, { "local_echo", ui.term_local_echo },
                      { "tcp_port", ui.tcp_port }, { "tcp_protocol", ui.tcp_protocol },
                      { "tcp_dcd_follows", ui.tcp_dcd_follows }, { "com_port", ui.com_port } };
    j["speed"] = { { "max", app.host.max_speed }, { "percent", app.host.pace_percent } };
    j["sound"] = { { "enabled", app.audio.enabled }, { "volume", app.audio.volume } };
    // the joystick from the keyboard and the PC mouse are off at every start (author): not saved
    j["mouse"] = { { "sensitivity", app.mouse.sensitivity } };
    j["cassette"] = { { "last_dir", ui.tape_dlg.last_dir } };
    j["load"] = { { "binary_address", std::string(ui.bin_addr) } };
    if (!app.language.empty()) j["language"] = app.language;
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
    get_opt(w, "configuration", ui.show_rack);
    const nlohmann::json& s = j.value("screen", nlohmann::json::object());
    get_opt(s, "integer_scale", app.integer_scale);
    get_opt(s, "tv_aspect", app.tv_aspect);
    get_opt(s, "smooth", app.smooth);
    get_opt(s, "card_types", ui.rack.show_types);
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
    get_opt(t, "tcp_port", ui.tcp_port);
    get_opt(t, "tcp_protocol", ui.tcp_protocol);
    get_opt(t, "tcp_dcd_follows", ui.tcp_dcd_follows);
    std::string com;
    get_opt(t, "com_port", com);
    if (!com.empty()) snprintf(ui.com_port, sizeof(ui.com_port), "%s", com.c_str());
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
}

// View -> Language: switches at once (the texts are translated every frame) and is saved.
void set_ui_language(App& app, UiLanguage lang)
{
    i18n_set_language(lang);
    app.language = i18n_code(lang);
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

void handle_key(App& app, const SDL_KeyboardEvent& ev)
{
    Machine* m = app.host.machine();
    switch (ev.key) {
    case SDLK_F5: app.host.paused = !app.host.paused; return;
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
    // serial terminal: the clipboard (Ctrl+C / Ctrl+V go to SAPI as control codes)
    if (app.ui.terminal_focused) {
        bool cs = (ev.mod & SDL_KMOD_CTRL) && (ev.mod & SDL_KMOD_SHIFT);
        if ((cs && ev.key == SDLK_V) || ((ev.mod & SDL_KMOD_SHIFT) && ev.key == SDLK_INSERT)) {
            terminal_paste(app.ui);
            return;
        }
        if ((cs && ev.key == SDLK_C) || ((ev.mod & SDL_KMOD_CTRL) && ev.key == SDLK_INSERT)) {
            terminal_copy(app.ui);
            return;
        }
    }
    // serial terminal: the EKL-1 (WordStar) codes of the cursor keys
    int tcode = ctrl >= 0 ? ctrl : (hk != HK_NONE ? wordstar_code(hk, mods) : -1);
    if (tcode >= 0 && terminal_key(app.ui, tcode)) return;
    if (!app.ui.screen_focused) return;
    if (ctrl >= 0) {
        char c[2] = { (char)ctrl, 0 };
        m->type_text(c);
    } else if (hk != HK_NONE && m->host_key(hk, mods)) {
        // a keypad key the keyboard has itself (Consul 262.3): drop its text event
        app.suppress_text = keymap_keypad_text(ev.key, ev.mod);
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
    if (app.ui.screen_focused) m->type_text(text);
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
    ImGui::SetNextWindowSize(s);
    zs.fixed = k;
    zs.size = s;
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

// Right click on the picture: the zoom of this window.
void zoom_context_menu(ZoomState& zs)
{
    if (ImGui::BeginPopupContextWindow("zoommenu")) {
        ImGui::TextDisabled("%s", tr("Zoom"));
        zoom_items(zs);
        ImGui::EndPopup();
    }
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
        ImGui::SeparatorText(head.c_str());
        std::string cur = d->ink();
        for (const ParamOption* o = p->options; o->value; o++) {
            nlohmann::json v = nlohmann::json::parse(o->value, nullptr, false);
            if (!v.is_string()) continue;
            std::string val = v.get<std::string>();
            if (ImGui::MenuItem(cs ? o->cs : o->en, nullptr, val == cur) && val != cur)
                rack_set_display_ink(app.ui, d, val);
        }
    }
    ImGui::EndMenu();
}

void draw_menu(App& app)
{
    if (!ImGui::BeginMainMenuBar()) return;
    Machine* m = app.host.machine();
    if (ImGui::BeginMenu(tr("File"))) {
        if (ImGui::BeginMenu(tr("Open machine"))) {
            for (const std::string& f : list_machine_files())
                if (ImGui::MenuItem(f.c_str(), nullptr, f == app.host.machine_file())) {
                    rack_start_machine(app.ui, f);   // asks first if the configurator has unsaved changes
                    m = app.host.machine();          // the old machine may be gone
                }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr("Open file..."))) rack_open_and_run(app.ui);
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
        if (ImGui::MenuItem(tr("Reset"), "F12 / F9", false, m != nullptr) && m) m->reset();
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
        ImGui::MenuItem(tr("Configuration..."), nullptr, &app.ui.show_rack);
        KeyboardDevice* kb = m ? m->keyboard() : nullptr;
        if (ImGui::BeginMenu(tr("Keyboard"), m != nullptr)) {
            for (const KeyboardPreset& p : keyboard_presets()) {
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
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("Sound, joystick and mouse"))) {
            ImGui::MenuItem(tr("Sound"), nullptr, &app.audio.enabled);
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
            ImGui::SliderInt(tr("Volume"), &app.audio.volume, 0, 100, "%d %%");
            if (app.audio.failed) ImGui::TextDisabled("%s", tr("No sound device."));
            ImGui::TextDisabled("%s", tr("Sound plays only at the emulation speed 100 %."));
            ImGui::Separator();
            ImGui::MenuItem(tr("Joystick from the keyboard (arrows, space = fire)"), "Ctrl+F8", &app.joy.keyboard);
            for (int i = 0; i < 2; i++) {
                std::string pad = joystick_name(app.joy, i);
                ImGui::TextDisabled("%s %d: %s", tr("Gamepad"), i + 1, pad.empty() ? tr("none") : pad.c_str());
            }
            ImGui::Separator();
            ImGui::MenuItem(tr("PC mouse as the SAPI mouse (a click into a screen captures it)"), nullptr, &app.mouse.enabled);
            ImGui::BeginDisabled(!app.mouse.enabled);
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
            ImGui::SliderFloat(tr("Mouse sensitivity"), &app.mouse.sensitivity, 0.25f, 4.0f, "%.2f");
            ImGui::EndDisabled();
            ImGui::TextDisabled("%s", tr("Middle button or Ctrl+F10 releases the captured mouse."));
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("Debug"))) {
        ImGui::MenuItem(tr("Debugger"), nullptr, &app.ui.show_debugger);
        ImGui::MenuItem(tr("Memory"), nullptr, &app.ui.show_memory);
        ImGui::MenuItem(tr("I/O log"), nullptr, &app.ui.show_iolog);
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Step"), "F11")) debug_step(app, false);
        if (ImGui::MenuItem(tr("Step over"), "F10")) debug_step(app, true);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("View"))) {
        ImGui::MenuItem(tr("Serial terminal"), nullptr, &app.ui.show_terminal);
        ImGui::MenuItem(tr("Media"), nullptr, &app.ui.show_media);
        ImGui::MenuItem(tr("Printer"), nullptr, &app.ui.show_printer);
        ImGui::MenuItem(tr("Log"), nullptr, &app.ui.show_log);
        ImGui::Separator();
        ink_menu(app, m);
        if (ImGui::BeginMenu(tr("Screen zoom"), m && m->display)) {
            zoom_items(app.screen_zs);
            ImGui::Separator();
            ImGui::TextDisabled("%s", tr("Right click on a picture: the zoom of its window."));
            ImGui::EndMenu();
        }
        ImGui::MenuItem(tr("Integer scaling"), nullptr, &app.integer_scale);
        ImGui::MenuItem(tr("TV aspect ratio"), nullptr, &app.tv_aspect);
        ImGui::MenuItem(tr("Smoothing (linear filtering)"), nullptr, &app.smooth);
        ImGui::Separator();
        if (ImGui::BeginMenu("Jazyk / Language")) {   // in both languages: findable in either
            UiLanguage cur = i18n_language();
            if (ImGui::MenuItem("Čeština", nullptr, cur == LANG_CS)) set_ui_language(app, LANG_CS);
            if (ImGui::MenuItem("English", nullptr, cur == LANG_EN)) set_ui_language(app, LANG_EN);
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("Help"))) {
        ImGui::TextUnformatted(tr("Keys: F5 run/pause, F8 emulation speed 100/200/400 %/max, F10 step over, F11 step, F12 or F9 reset"));
        ImGui::TextUnformatted(tr("Ctrl+F8 joystick from the keyboard on/off, Ctrl+F10 releases the captured mouse"));
        ImGui::TextUnformatted(tr("Ctrl+F2 quick save of the state, Ctrl+F3 quick load"));
        ImGui::TextUnformatted(tr("Click the screen to type on the SAPI keyboard."));
        ImGui::Separator();
        if (app.mcp && app.mcp->running()) {
            ImGui::Text("MCP: %s", app.mcp->url().c_str());
            ImGui::TextDisabled("claude mcp add --transport http sapiemu %s", app.mcp->url().c_str());
        } else {
            ImGui::TextUnformatted(tr("MCP server is not running."));
        }
        ImGui::Separator();
        ImGui::Text("SAPIemu %s", sapiemu_version());
        ImGui::EndMenu();
    }

    // status on the right side of the menu bar
    char st[200];
    st[0] = 0;
    if (!m) snprintf(st, sizeof(st), "%s", tr("Switched off"));
    if (m) {
        const char* state = app.host.paused ? (app.host.status.empty() ? tr("Paused") : app.host.status.c_str())
                                            : (app.host.max_speed ? tr("Maximum") : "");
        KeyboardDevice* kb = m->keyboard();
        std::string ks = kb ? kb->status() : std::string();
        if (!app.ui.notice.empty() && !app.ui.notice_error && SDL_GetTicksNS() - app.ui.notice_ns < 3000000000ull) {
            std::string n = app.ui.notice;
            size_t slash = n.find_last_of("/\\");   // the file name is enough here
            size_t colon = n.find(": ");
            if (slash != std::string::npos && colon != std::string::npos) n = n.substr(0, colon + 2) + n.substr(slash + 1);
            ks += "  " + n;
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
    ImGui::SameLine(ImGui::GetWindowWidth() - w);
    ImGui::TextUnformatted(st);
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
    ImGui::Begin(title);
    ImGui::PopStyleVar();
    zoom_track(app.screen_zs);
    app.ui.screen_focused = ImGui::IsWindowFocused();

    if (!m) {   // switched off: a dark screen
        ImVec2 a = ImGui::GetCursorScreenPos(), av = ImGui::GetContentRegionAvail();
        ImGui::GetWindowDrawList()->AddRectFilled(a, ImVec2(a.x + av.x, a.y + av.y), IM_COL32(8, 8, 8, 255));
        ImGui::TextDisabled("%s", tr("Switched off (POWER on the ZDR panel, Machine -> Switch on)"));
        ImGui::End();
        return;
    }
    if (!d) {
        ImGui::TextUnformatted(tr("No display card in this machine."));
        ImGui::End();
        return;
    }
    app.screen_zs.zoom = draw_display(app, d, app.screen_tex, app.tex_w, app.tex_h, app.pixels, app.screen_zs);
    zoom_context_menu(app.screen_zs);
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
        bool open = ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::PopStyleVar();
        if (open) {
            zoom_track(v.zs);
            if (ImGui::IsWindowFocused()) app.ui.screen_focused = true;   // typing goes to the machine too
            v.zs.zoom = draw_display(app, d, v.tex, v.w, v.h, v.pixels, v.zs);
            zoom_context_menu(v.zs);
        }
        ImGui::End();
    }
}

void draw_log(App& app)
{
    if (!app.ui.show_log) return;
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

int app_run(const std::string& machine_file, int mcp_port, const UiSnapshot* snap, const std::string& settings_file)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    App* app_ptr = new App();
    App& app = *app_ptr;
    app.ui.host = &app.host;

    float dpi = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (dpi <= 0) dpi = 1.0f;

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
    ini_rename_windows(app.ini_path);
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
    if (!app.window || !app.renderer) {
        fprintf(stderr, "cannot create window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(app.renderer, 1);
    app.ui.window = app.window;
    SDL_StartTextInput(app.window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = app.ini_path.empty() ? nullptr : app.ini_path.c_str();   // none for a self-check
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpi);
    style.FontScaleDpi = dpi;
    std::string font = data_path("third_party/fonts/DejaVuSans.ttf");
    std::string mono = data_path("third_party/fonts/DejaVuSansMono.ttf");
    if (file_exists(font)) io.Fonts->AddFontFromFileTTF(font.c_str(), 16.0f);
    if (file_exists(mono)) app.ui.mono = io.Fonts->AddFontFromFileTTF(mono.c_str(), 15.0f);
    ImGui_ImplSDL3_InitForSDLRenderer(app.window, app.renderer);
    ImGui_ImplSDLRenderer3_Init(app.renderer);

    settings_apply(app, settings);
    std::string first = machine_file;
    if (first.empty()) {
        std::string last;
        get_opt(settings, "machine", last);
        if (!last.empty() && !file_exists(data_path(last)) && last.size() > 6 &&
            last.compare(last.size() - 6, 6, ".jsonc") == 0)
            last = last.substr(0, last.size() - 6) + ".sapi";   // machine files were renamed to .sapi
        first = !last.empty() && file_exists(data_path(last)) ? last : std::string(DEFAULT_MACHINE_FILE);
    }
    load_machine(app, first);
    if (snap && !snap->state.empty()) {   // a picture of a saved moment
        std::string err;
        if (!app.host.load_state_file(snap->state, err)) fprintf(stderr, "UI snapshot: %s\n", err.c_str());
    }
    if (!app.host.machine() && first != DEFAULT_MACHINE_FILE) load_machine(app, DEFAULT_MACHINE_FILE);
    const nlohmann::json& sp = settings.value("speed", nlohmann::json::object());
    get_opt(sp, "max", app.host.max_speed);
    get_opt(sp, "percent", app.host.pace_percent);
    if (app.host.pace_percent != 200 && app.host.pace_percent != 400) app.host.pace_percent = 100;
    app.settings_saved = settings;
    int snap_frames = 0;
    if (snap) {
        app.ui.show_rack = snap->rack || !snap->open.empty();
        app.ui.show_media = snap->media;
        app.ui.show_printer = snap->printer;
        app.ui.rack.sel = snap->rack_slot;
        app.ui.rack.machine_gen = ~0ull;
    }

    if (mcp_port > 0) {
        app.mcp = new McpServer(app.host);
        app.ui.mcp = app.mcp;
        std::string err;
        if (!app.mcp->start(mcp_port, err)) LOGW("MCP server not started: %s", err.c_str());
    }

    uint64_t last = SDL_GetTicksNS();
    while (!app.quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (mouse_event(app.mouse, app.window, app.host.machine(), ev)) continue;   // captured: for the SAPI
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) rack_request_exit(app.ui);
            else if (ev.type == SDL_EVENT_WINDOW_MOVED || ev.type == SDL_EVENT_WINDOW_RESIZED) track_window(app);
            else if (ev.type == SDL_EVENT_KEY_DOWN) handle_key(app, ev.key);
            else if (ev.type == SDL_EVENT_TEXT_INPUT) handle_text(app, ev.text.text);
            else if (ev.type == SDL_EVENT_GAMEPAD_ADDED || ev.type == SDL_EVENT_GAMEPAD_REMOVED) joystick_event(app.joy, ev);
        }
        app.host.poll();   // MCP commands
        if (app.title_gen != app.host.machine_generation()) {   // loaded by MCP, the menu or the configurator
            app.title_gen = app.host.machine_generation();
            set_title(app);
        }
        if (app.ui.quit) app.quit = true;
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

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        // The ImGui SDL3 backend stops SDL text input whenever an ImGui text field
        // loses focus (debugger, configurator...); the SAPI keyboard and the serial
        // terminal need it all the time, otherwise no characters arrive.
        if (!SDL_TextInputActive(app.window)) SDL_StartTextInput(app.window);
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        draw_menu(app);
        draw_screen(app);
        draw_graphic_screens(app);
        draw_debugger(app.ui);
        draw_memory(app.ui);
        draw_iolog(app.ui);
        draw_terminal(app.ui);
        draw_media(app.ui);
        draw_printer(app.ui);
        draw_rack(app.ui);
        draw_log(app);
        draw_error(app);
        ImGui::Render();

        SDL_SetRenderScale(app.renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(app.renderer, 24, 24, 24, 255);
        SDL_RenderClear(app.renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), app.renderer);
        if (snap && ++snap_frames == 2) {
            // follow_machine() has run: open the requested description, select the slot
            if (!snap->open.empty()) rack_open_in_editor(app.ui, snap->open);
            app.ui.rack.sel = snap->rack_slot;
            app.ui.rack.open_map_tab = snap->map;
        }
        if (snap && snap_frames >= 12) {
            // window content as PNG (our own renderer, not the screen)
            SDL_Surface* s = SDL_RenderReadPixels(app.renderer, nullptr);
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
