// SDL3 + Dear ImGui frontend: screen window, menus, keyboard routing, debugger and
// terminal windows, MCP server. Emulation runs on this (main) thread via EmuHost.
#include "frontend/app.h"
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

#include "core/host.h"
#include "core/log.h"
#include "core/util.h"
#include "core/png.h"
#include "core/version.h"
#include "input/keyboards.h"
#include "input/parallel_kbd.h"
#include "mcp/mcp_server.h"

namespace {

enum Phosphor { PHOSPHOR_WHITE = 0, PHOSPHOR_GREEN, PHOSPHOR_AMBER };

const uint32_t k_phosphor[3] = { 0xFFE8E8E8u, 0xFF40FF70u, 0xFFFFB020u };
const uint32_t k_background = 0xFF101010u;

// PAL TV: 10 MHz dot clock vs. 7.375 MHz square-pixel clock for 288 lines
const float k_tv_pixel_aspect = 7.375f / 10.0f;

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* screen_tex = nullptr;
    int tex_w = 0, tex_h = 0;
    std::vector<uint32_t> pixels;

    EmuHost host;
    McpServer* mcp = nullptr;
    UiState ui;
    std::string error_text;
    bool show_error = false;
    bool quit = false;
    int phosphor = PHOSPHOR_WHITE;
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
                     { "configuration", ui.show_rack } };
    j["screen"] = { { "phosphor", app.phosphor }, { "integer_scale", app.integer_scale },
                    { "tv_aspect", app.tv_aspect }, { "smooth", app.smooth }, { "card_types", ui.rack.show_types } };
    j["terminal"] = { { "channel", ui.terminal_channel }, { "emulation", ui.term_emulation },
                      { "cr_newline", ui.term_cr_newline }, { "local_echo", ui.term_local_echo },
                      { "tcp_port", ui.tcp_port }, { "tcp_protocol", ui.tcp_protocol },
                      { "tcp_dcd_follows", ui.tcp_dcd_follows }, { "com_port", ui.com_port } };
    j["speed"] = { { "turbo", app.host.turbo }, { "speed_hz", app.host.speed_hz } };
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
    get_opt(w, "configuration", ui.show_rack);
    const nlohmann::json& s = j.value("screen", nlohmann::json::object());
    get_opt(s, "phosphor", app.phosphor);
    if (app.phosphor < 0 || app.phosphor > PHOSPHOR_AMBER) app.phosphor = PHOSPHOR_WHITE;
    get_opt(s, "integer_scale", app.integer_scale);
    get_opt(s, "tv_aspect", app.tv_aspect);
    get_opt(s, "smooth", app.smooth);
    get_opt(s, "card_types", ui.rack.show_types);
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
    case SDLK_F8: app.host.cycle_speed(); return;
    case SDLK_F10: debug_step(app, true); return;
    case SDLK_F11: debug_step(app, false); return;
    case SDLK_F12:   // under a debugger Windows takes F12 as the break hotkey, F9 still works
    case SDLK_F9: if (m) m->reset(); return;
    default: break;
    }
    app.suppress_text = 0;
    if (!m) return;
    int hk = keymap_host_key(ev.key);
    int mods = keymap_mods(ev.mod);
    int ctrl = keymap_ctrl_code(ev.key, ev.mod);
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
    if (app.ui.screen_focused) m->type_text(text);
}

void draw_menu(App& app)
{
    if (!ImGui::BeginMainMenuBar()) return;
    Machine* m = app.host.machine();
    if (ImGui::BeginMenu(tr("File"))) {
        if (ImGui::BeginMenu(tr("Open machine"))) {
            for (const std::string& f : list_machine_files())
                if (ImGui::MenuItem(f.c_str(), nullptr, f == app.host.machine_file())) {
                    load_machine(app, f);
                    m = app.host.machine();   // the old machine is gone
                }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr("Open file..."))) rack_open_and_run(app.ui);
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Exit"))) app.quit = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("Machine"))) {
        if (ImGui::MenuItem(tr("Reset"), "F12 / F9") && m) m->reset();
        ImGui::MenuItem(tr("Pause"), "F5", &app.host.paused);
        if (ImGui::BeginMenu(tr("Speed"))) {
            char nominal[64];
            snprintf(nominal, sizeof(nominal), "%s (%.2f MHz)", tr("CPU clock"), m ? m->bus.cpu_hz / 1e6 : 0.0);
            bool at_nominal = !app.host.turbo && app.host.speed_hz == 0;
            if (ImGui::MenuItem(nominal, nullptr, at_nominal)) { app.host.turbo = false; app.host.speed_hz = 0; }
            if (m && m->bus.cpu_hz < 4000000 &&
                ImGui::MenuItem("4 MHz", nullptr, !app.host.turbo && app.host.speed_hz == 4000000)) {
                app.host.turbo = false;
                app.host.speed_hz = 4000000;
            }
            if (ImGui::MenuItem(tr("Maximum (turbo)"), nullptr, app.host.turbo)) app.host.turbo = true;
            ImGui::Separator();
            if (ImGui::MenuItem(tr("Next"), "F8")) app.host.cycle_speed();
            ImGui::EndMenu();
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
        ImGui::MenuItem(tr("Log"), nullptr, &app.ui.show_log);
        ImGui::Separator();
        if (ImGui::BeginMenu(tr("Phosphor"))) {
            if (ImGui::MenuItem(tr("White"), nullptr, app.phosphor == PHOSPHOR_WHITE)) app.phosphor = PHOSPHOR_WHITE;
            if (ImGui::MenuItem(tr("Green"), nullptr, app.phosphor == PHOSPHOR_GREEN)) app.phosphor = PHOSPHOR_GREEN;
            if (ImGui::MenuItem(tr("Amber"), nullptr, app.phosphor == PHOSPHOR_AMBER)) app.phosphor = PHOSPHOR_AMBER;
            ImGui::EndMenu();
        }
        ImGui::MenuItem(tr("Integer scaling"), nullptr, &app.integer_scale);
        ImGui::MenuItem(tr("TV aspect ratio"), nullptr, &app.tv_aspect);
        ImGui::MenuItem(tr("Smoothing (linear filtering)"), nullptr, &app.smooth);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("Help"))) {
        ImGui::TextUnformatted(tr("Keys: F5 run/pause, F8 speed 2 / 4 MHz / max, F10 step over, F11 step, F12 or F9 reset"));
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
    if (m) {
        char pace[32] = "";
        if (app.host.speed_hz) snprintf(pace, sizeof(pace), "%s %.0f MHz", tr("Speed"), app.host.speed_hz / 1e6);
        const char* state = app.host.paused ? (app.host.status.empty() ? tr("Paused") : app.host.status.c_str())
                                            : (app.host.turbo ? tr("Turbo") : pace);
        KeyboardDevice* kb = m->keyboard();
        std::string ks = kb ? kb->status() : std::string();
        if (kb && kb->pending() > 1) {   // keys typed but not taken by the program
            char q[48];
            snprintf(q, sizeof(q), "  %s %d", tr("keys waiting:"), (int)kb->pending());
            ks += q;
        }
        snprintf(st, sizeof(st), "%s  %s  %s  %.2f MHz  %3.0f %%", ks.c_str(), state,
                 (app.mcp && app.mcp->running()) ? "MCP" : "", m->bus.cpu_hz / 1e6, app.host.speed_percent);
    }
    float w = ImGui::CalcTextSize(st).x + ImGui::GetStyle().ItemSpacing.x * 2;
    ImGui::SameLine(ImGui::GetWindowWidth() - w);
    ImGui::TextUnformatted(st);
    ImGui::EndMainMenuBar();
}

void draw_screen(App& app)
{
    ImGui::SetNextWindowSize(ImVec2(820, 560), ImGuiCond_FirstUseEver);
    static bool first = true;
    if (first) { ImGui::SetNextWindowFocus(); first = false; }   // type right after start
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::Begin(tr("Screen"));
    ImGui::PopStyleVar();
    app.ui.screen_focused = ImGui::IsWindowFocused();

    Machine* m = app.host.machine();
    DisplayDevice* d = m ? m->display : nullptr;
    if (!d) {
        ImGui::TextUnformatted(tr("No display card in this machine."));
        ImGui::End();
        return;
    }
    int w = d->screen_width(), h = d->screen_height();
    if (!app.screen_tex || w != app.tex_w || h != app.tex_h) {
        if (app.screen_tex) SDL_DestroyTexture(app.screen_tex);
        app.screen_tex = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetTextureScaleMode(app.screen_tex, SDL_SCALEMODE_NEAREST);
        app.tex_w = w;
        app.tex_h = h;
        app.pixels.assign((size_t)w * h, 0);
    }
    d->render(app.pixels.data(), k_phosphor[app.phosphor], k_background);
    SDL_UpdateTexture(app.screen_tex, nullptr, app.pixels.data(), w * 4);

    // Scale in physical pixels (window points * framebuffer scale) so that integer
    // scaling stays sharp with Windows display scaling.
    float fb = ImGui::GetIO().DisplayFramebufferScale.x;
    if (fb <= 0) fb = 1.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float px_aspect = app.tv_aspect ? k_tv_pixel_aspect : 1.0f;
    float img_w = (float)w * px_aspect, img_h = (float)h;
    float scale = avail.x * fb / img_w;
    if (avail.y * fb / img_h < scale) scale = avail.y * fb / img_h;
    if (app.integer_scale && scale >= 1.0f) scale = (float)(int)scale;
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
    ImGui::Image(ImTextureRef((ImTextureID)(intptr_t)app.screen_tex), size);
    if (!app.smooth) dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
    ImGui::End();
}

void draw_log(App& app)
{
    if (!app.ui.show_log) return;
    ImGui::SetNextWindowSize(ImVec2(600, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(tr("Log"), &app.ui.show_log)) {
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
    if (app.show_error) {
        ImGui::OpenPopup(tr("Error"));
        app.show_error = false;
    }
    if (ImGui::BeginPopupModal(tr("Error"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(app.error_text.c_str());
        if (ImGui::Button(tr("Close"))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace

int app_run(const std::string& machine_file, int mcp_port, const UiSnapshot* snap, const std::string& settings_file)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
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
    if (snap) app.ini_path.clear();   // the self-check never touches a window layout
    if (!app.settings_path.empty()) {
        std::string text;
        if (file_read_text(app.settings_path, text)) {
            settings = nlohmann::json::parse(text, nullptr, false);
            if (settings.is_discarded() || !settings.is_object()) settings = nlohmann::json::object();
        }
        LOGI("settings: %s", app.settings_path.c_str());
    }
    int win_w = (int)(1100 * dpi), win_h = (int)(760 * dpi);
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
        first = !last.empty() && file_exists(data_path(last)) ? last : std::string(DEFAULT_MACHINE_FILE);
    }
    load_machine(app, first);
    if (!app.host.machine() && first != DEFAULT_MACHINE_FILE) load_machine(app, DEFAULT_MACHINE_FILE);
    const nlohmann::json& sp = settings.value("speed", nlohmann::json::object());
    get_opt(sp, "turbo", app.host.turbo);
    get_opt(sp, "speed_hz", app.host.speed_hz);
    if (app.host.machine() && app.host.speed_hz <= app.host.machine()->bus.cpu_hz) app.host.speed_hz = 0;
    app.settings_saved = settings;
    int snap_frames = 0;
    if (snap) {
        app.ui.show_rack = snap->rack || !snap->open.empty();
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
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) app.quit = true;
            else if (ev.type == SDL_EVENT_WINDOW_MOVED || ev.type == SDL_EVENT_WINDOW_RESIZED) track_window(app);
            else if (ev.type == SDL_EVENT_KEY_DOWN) handle_key(app, ev.key);
            else if (ev.type == SDL_EVENT_TEXT_INPUT) handle_text(app, ev.text.text);
        }
        uint64_t gen = app.host.machine_generation();
        app.host.poll();   // MCP commands
        if (gen != app.host.machine_generation()) set_title(app);
        uint64_t now = SDL_GetTicksNS();
        app.host.frame(now - last);
        last = now;

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
        draw_debugger(app.ui);
        draw_memory(app.ui);
        draw_iolog(app.ui);
        draw_terminal(app.ui);
        draw_media(app.ui);
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
    app.host.shutdown();
    if (app.mcp) { app.mcp->stop(); delete app.mcp; }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (app.screen_tex) SDL_DestroyTexture(app.screen_tex);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    delete app_ptr;   // flushes the machine (RAM disks)
    SDL_Quit();
    return 0;
}
