// SDL3 + Dear ImGui frontend: screen window, menus, keyboard input, real-time pacing.
#include "frontend/app.h"
#include "frontend/i18n.h"
#include "frontend/keymap.h"

#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include "core/log.h"
#include "core/machine.h"
#include "core/util.h"
#include "core/version.h"

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

    std::unique_ptr<Machine> machine;
    std::string machine_file;
    std::string error_text;
    bool show_error = false;

    bool paused = false;
    bool turbo = false;
    bool quit = false;
    bool screen_focused = false;
    bool show_log = false;
    int phosphor = PHOSPHOR_WHITE;
    bool integer_scale = true;
    bool tv_aspect = false;
    std::string status;

    // speed measurement
    uint64_t speed_t0 = 0;
    uint64_t speed_c0 = 0;
    double speed_percent = 0;
    std::string ini_path;
};

void load_machine(App& app, const std::string& file)
{
    std::unique_ptr<Machine> m(new Machine());
    std::string err;
    if (!m->load_file(file, err)) {
        app.error_text = std::string(tr("Cannot load machine")) + ":\n" + err;
        app.show_error = true;
        LOGE("%s", err.c_str());
        return;
    }
    app.machine = std::move(m);
    app.machine_file = file;
    app.paused = false;
    app.status.clear();
    app.speed_t0 = SDL_GetTicksNS();
    app.speed_c0 = app.machine->bus.now;
    std::string title = std::string("SAPIemu - ") + app.machine->name;
    SDL_SetWindowTitle(app.window, title.c_str());
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

void handle_key(App& app, const SDL_KeyboardEvent& ev)
{
    if (ev.key == SDLK_F5) { app.paused = !app.paused; return; }
    if (ev.key == SDLK_F11) { app.turbo = !app.turbo; return; }
    if (ev.key == SDLK_F12) { if (app.machine) app.machine->reset(); return; }
    if (!app.screen_focused || !app.machine) return;
    int code = keymap_special(ev.key, ev.mod);
    if (code >= 0) app.machine->type_code((uint8_t)code);
}

void handle_text(App& app, const char* text)
{
    if (!app.screen_focused || !app.machine) return;
    app.machine->type_text(text);
}

void run_emulation(App& app, uint64_t frame_ns)
{
    Machine* m = app.machine.get();
    if (!m || app.paused) return;
    app.status.clear();
    if (app.turbo) {
        uint64_t start = SDL_GetTicksNS();
        while (SDL_GetTicksNS() - start < 14000000ull) {
            m->run_cycles(m->bus.cpu_hz / 200);
            if (m->stopped_on_breakpoint()) break;
        }
    } else {
        if (frame_ns > 100000000ull) frame_ns = 100000000ull;   // after a stall don't catch up
        m->run_cycles((uint64_t)((double)frame_ns * m->bus.cpu_hz / 1e9));
    }
    if (m->stopped_on_breakpoint()) {
        app.paused = true;
        CpuRegs r;
        m->cpu->get_regs(r);
        char buf[64];
        snprintf(buf, sizeof(buf), "%s %04Xh", tr("Breakpoint at"), r.pc);
        app.status = buf;
        m->clear_break();
    }
}

void update_speed(App& app)
{
    if (!app.machine) return;
    uint64_t t = SDL_GetTicksNS();
    if (t - app.speed_t0 < 500000000ull) return;
    double emu_s = (double)(app.machine->bus.now - app.speed_c0) / app.machine->bus.cpu_hz;
    double real_s = (double)(t - app.speed_t0) / 1e9;
    app.speed_percent = 100.0 * emu_s / real_s;
    app.speed_t0 = t;
    app.speed_c0 = app.machine->bus.now;
}

void draw_menu(App& app)
{
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu(tr("File"))) {
        if (ImGui::BeginMenu(tr("Open machine"))) {
            for (const std::string& f : list_machines())
                if (ImGui::MenuItem(f.c_str(), nullptr, f == app.machine_file)) load_machine(app, f);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr("Exit"))) app.quit = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("Machine"))) {
        if (ImGui::MenuItem(tr("Reset"), "F12") && app.machine) app.machine->reset();
        ImGui::MenuItem(tr("Pause"), "F5", &app.paused);
        ImGui::MenuItem(tr("Turbo"), "F11", &app.turbo);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("View"))) {
        if (ImGui::BeginMenu(tr("Phosphor"))) {
            if (ImGui::MenuItem(tr("White"), nullptr, app.phosphor == PHOSPHOR_WHITE)) app.phosphor = PHOSPHOR_WHITE;
            if (ImGui::MenuItem(tr("Green"), nullptr, app.phosphor == PHOSPHOR_GREEN)) app.phosphor = PHOSPHOR_GREEN;
            if (ImGui::MenuItem(tr("Amber"), nullptr, app.phosphor == PHOSPHOR_AMBER)) app.phosphor = PHOSPHOR_AMBER;
            ImGui::EndMenu();
        }
        ImGui::MenuItem(tr("Integer scaling"), nullptr, &app.integer_scale);
        ImGui::MenuItem(tr("TV aspect ratio"), nullptr, &app.tv_aspect);
        ImGui::Separator();
        ImGui::MenuItem(tr("Log"), nullptr, &app.show_log);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr("Help"))) {
        ImGui::TextUnformatted(tr("Keys: F5 pause, F11 turbo, F12 reset"));
        ImGui::TextUnformatted(tr("Click the screen to type on the SAPI keyboard."));
        ImGui::Separator();
        ImGui::Text("SAPIemu %s", sapiemu_version());
        ImGui::EndMenu();
    }

    // status on the right side of the menu bar
    char st[160];
    if (app.machine) {
        snprintf(st, sizeof(st), "%s%s  %.2f MHz  %3.0f %%", app.status.c_str(),
                 app.paused ? (app.status.empty() ? tr("Paused") : "") : "",
                 app.machine->bus.cpu_hz / 1e6, app.paused ? 0.0 : app.speed_percent);
    } else {
        st[0] = 0;
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
    app.screen_focused = ImGui::IsWindowFocused();

    DisplayDevice* d = app.machine ? app.machine->display : nullptr;
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

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float px_aspect = app.tv_aspect ? k_tv_pixel_aspect : 1.0f;
    float img_w = (float)w * px_aspect, img_h = (float)h;
    float scale = avail.x / img_w;
    if (avail.y / img_h < scale) scale = avail.y / img_h;
    if (app.integer_scale && scale >= 1.0f) scale = (float)(int)scale;
    if (scale <= 0.1f) scale = 0.1f;
    ImVec2 size(img_w * scale, img_h * scale);
    ImVec2 cur = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cur.x + (avail.x - size.x) * 0.5f, cur.y + (avail.y - size.y) * 0.5f));
    ImGui::Image(ImTextureRef((ImTextureID)(intptr_t)app.screen_tex), size);
    if (ImGui::IsItemClicked()) ImGui::SetWindowFocus();
    ImGui::End();
}

void draw_log(App& app)
{
    if (!app.show_log) return;
    ImGui::SetNextWindowSize(ImVec2(600, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(tr("Log"), &app.show_log)) {
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

int app_run(const std::string& machine_file)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    App app;
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
    if (file_exists(font)) io.Fonts->AddFontFromFileTTF(font.c_str(), 16.0f);
    ImGui_ImplSDL3_InitForSDLRenderer(app.window, app.renderer);
    ImGui_ImplSDLRenderer3_Init(app.renderer);

    load_machine(app, machine_file);

    uint64_t last = SDL_GetTicksNS();
    while (!app.quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) app.quit = true;
            else if (ev.type == SDL_EVENT_KEY_DOWN) handle_key(app, ev.key);
            else if (ev.type == SDL_EVENT_TEXT_INPUT) handle_text(app, ev.text.text);
        }
        uint64_t now = SDL_GetTicksNS();
        run_emulation(app, now - last);
        last = now;
        update_speed(app);

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        draw_menu(app);
        draw_screen(app);
        draw_log(app);
        draw_error(app);
        ImGui::Render();

        SDL_SetRenderScale(app.renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(app.renderer, 24, 24, 24, 255);
        SDL_RenderClear(app.renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), app.renderer);
        SDL_RenderPresent(app.renderer);
    }

    if (app.machine) app.machine->flush();
    app.machine.reset();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (app.screen_tex) SDL_DestroyTexture(app.screen_tex);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}
