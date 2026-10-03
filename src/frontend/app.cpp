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
};

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

std::vector<std::string> list_machines(void)
{
    std::vector<std::string> out;
    std::error_code ec;
    std::string dir = path_join(data_root(), "machines");
    std::filesystem::path p(std::u8string(dir.begin(), dir.end()));
    for (auto& e : std::filesystem::directory_iterator(p, ec)) {
        if (e.path().extension() == ".jsonc" || e.path().extension() == ".json") {
            std::u8string n = e.path().filename().u8string();
            out.push_back("machines/" + std::string(n.begin(), n.end()));
        }
    }
    return out;
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
    case SDLK_F8: app.host.turbo = !app.host.turbo; return;
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
            for (const std::string& f : list_machines())
                if (ImGui::MenuItem(f.c_str(), nullptr, f == app.host.machine_file())) load_machine(app, f);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Exit"))) app.quit = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("Machine"))) {
        if (ImGui::MenuItem(tr("Reset"), "F12 / F9") && m) m->reset();
        ImGui::MenuItem(tr("Pause"), "F5", &app.host.paused);
        ImGui::MenuItem(tr("Turbo"), "F8", &app.host.turbo);
        ImGui::Separator();
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
        ImGui::TextUnformatted(tr("Keys: F5 run/pause, F8 turbo, F10 step over, F11 step, F12 or F9 reset"));
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
        const char* state = app.host.paused ? (app.host.status.empty() ? tr("Paused") : app.host.status.c_str())
                                            : (app.host.turbo ? tr("Turbo") : "");
        KeyboardDevice* kb = m->keyboard();
        std::string ks = kb ? kb->status() : std::string();
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

int app_run(const std::string& machine_file, int mcp_port)
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
    app.window = SDL_CreateWindow("SAPIemu", (int)(1100 * dpi), (int)(760 * dpi),
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    app.renderer = SDL_CreateRenderer(app.window, nullptr);
    if (!app.window || !app.renderer) {
        fprintf(stderr, "cannot create window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(app.renderer, 1);
    SDL_StartTextInput(app.window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    char* pref = SDL_GetPrefPath("SAPIemu", "SAPIemu");
    if (pref) {
        app.ini_path = std::string(pref) + "imgui.ini";
        SDL_free(pref);
        io.IniFilename = app.ini_path.c_str();
    }
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

    load_machine(app, machine_file);

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
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        draw_menu(app);
        draw_screen(app);
        draw_debugger(app.ui);
        draw_memory(app.ui);
        draw_iolog(app.ui);
        draw_terminal(app.ui);
        draw_media(app.ui);
        draw_log(app);
        draw_error(app);
        ImGui::Render();

        SDL_SetRenderScale(app.renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(app.renderer, 24, 24, 24, 255);
        SDL_RenderClear(app.renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), app.renderer);
        SDL_RenderPresent(app.renderer);
    }

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
