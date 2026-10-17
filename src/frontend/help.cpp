// Help: the windows "About SAPIemu" and "Keyboard shortcuts" (one table for the whole program), the user guides
// of docs/ opened in the program the system has for them.
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <stdio.h>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "core/util.h"
#include "core/version.h"

void open_doc(const char* rel)
{
    std::string p = data_path(rel);
    for (char& c : p)
        if (c == '\\') c = '/';
    SDL_OpenURL(("file:///" + p).c_str());
}

// the guides of docs/ for users, in the order of the machines
static const struct {
    const char* file;
    const char* title;   // tr() key
} k_docs[] = {
    { "docs/navod-minigraf.md", "Plotter Minigraf 0507" },
    { "docs/navod-xy4130.md", "Plotter XY 4130" },
    { "docs/navod-ar1990.md", "Plotter of AR 1990" },
    { "docs/navod-kzd1.md", "Cassette data recorder KZD-1" },
    { "docs/navod-rkd1.md", "Cartridge disk RKD-1" },
    { "docs/navod-rmp1.md", "Magnetic tape RMP-1" },
    { "docs/navod-dps1.md", "Measuring bench IMS-2 (DPS-1)" },
    { "docs/aps5-ukazka.md", "APS-5 step by step" },
};

void help_docs_menu(UiState& ui)
{
    (void)ui;
    if (ImGui::MenuItem(tr("First steps"))) open_doc("docs/prvni-kroky.md");
    if (ImGui::BeginMenu(tr("User guides"))) {
        for (const auto& d : k_docs)
            if (file_exists(data_path(d.file)) && ImGui::MenuItem(tr(d.title))) open_doc(d.file);
        ImGui::Separator();
        ImGui::TextDisabled("%s", tr("The guides are in Czech, they open in the program for .md files."));
        ImGui::EndMenu();
    }
}

// ---------------------------------------------------------------- keyboard shortcuts

static const struct {
    const char* keys;
    const char* what;   // tr() key
} k_shortcuts[] = {
    { "F5", "Pause / run" },
    { "F8", "Emulation speed 100 / 200 / 400 % / maximum" },
    { "F12, F9", "Reset of the machine (the memory stays)" },
    { "F11, F10", "Step, step over (when paused)" },
    { "Ctrl+F5", "Screenshot of the picture focused last" },
    { "Ctrl+Alt+F5", "Video with sound on / off (MP4 through ffmpeg)" },
    { "Ctrl+F2, Ctrl+F3", "Quick save, quick load of the state" },
    { "Ctrl+F7", "Game mode of the ANK-1 keyboard (keys by position, Shift = SHIFT)" },
    { "Ctrl+F8", "Joystick from the keyboard (arrows, space = fire)" },
    { "Ctrl+F10", "Release the captured mouse (also the middle button)" },
    { "Ctrl+Shift+V", "Paste the clipboard on the SAPI keyboard (with the screen focused)" },
    { "Alt+C, Alt+V", "Copy, paste in the serial terminal" },
};

void draw_shortcuts(UiState& ui)
{
    if (!ui.show_shortcuts) return;
    float u = ImGui::GetFontSize();
    ImGui::SetNextWindowSize(ImVec2(36 * u, 24 * u), ImGuiCond_FirstUseEver);
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    if (!ImGui::Begin(tr_id("Keyboard shortcuts"), &ui.show_shortcuts)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("%s", tr("Click the screen of SAPI to type on its keyboard. Letters, digits, Enter, Esc, the "
                                "cursor keys, F1-F4, F6 and F7 go to SAPI, Ctrl+letter is a control code. The keys "
                                "below belong to the emulator and work everywhere except while typing into a text "
                                "field."));
    ImGui::Spacing();
    if (ImGui::BeginTable("keys", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("k", ImGuiTableColumnFlags_WidthFixed, 10 * u);
        ImGui::TableSetupColumn("w", ImGuiTableColumnFlags_WidthStretch);
        for (const auto& s : k_shortcuts) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(s.keys);
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", tr(s.what));
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

// ---------------------------------------------------------------- about

static void link(const char* label, const char* url)
{
    if (ImGui::SmallButton(label)) SDL_OpenURL(url);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", url);
}

void draw_about(UiState& ui)
{
    if (!ui.show_about) return;
    float u = ImGui::GetFontSize();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(42 * u, 40 * u), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(1.0f);
    if (!ImGui::Begin(tr_id("About SAPIemu"), &ui.show_about)) {
        ImGui::End();
        return;
    }
    // the picture: the rack of SAPI-1 V as the configurator draws it
    static SDL_Texture* tex = nullptr;
    static int tw = 0, th = 0;
    static bool tried = false;
    if (!tried) {
        tried = true;
        SDL_Renderer* r = ui.window ? SDL_GetRenderer(ui.window) : nullptr;
        if (SDL_Surface* s = r ? SDL_LoadPNG(data_path("docs/img/o-programu.png").c_str()) : nullptr) {
            tex = SDL_CreateTextureFromSurface(r, s);
            tw = s->w;
            th = s->h;
            SDL_DestroySurface(s);
        }
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.45f, 1.0f));
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextUnformatted("SAPIemu");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", sapiemu_version());
    ImGui::TextWrapped("%s", tr("Emulator of the Czechoslovak modular computer Tesla SAPI-1 (1983-1990) and of the "
                                "boards made for it since: the historic ZPS sets, the boards of Libor Lasota, the "
                                "instruments SAPI worked with."));
    if (tex) {
        float w = ImGui::GetContentRegionAvail().x, h = w * th / (float)tw;
        if (h > 16 * u) {
            w *= 16 * u / h;
            h = 16 * u;
        }
        ImGui::Image(ImTextureRef((ImTextureID)(intptr_t)tex), ImVec2(w, h));
    }
    ImGui::SeparatorText(tr("Author"));
    ImGui::TextUnformatted("Martin Lukášek, 8bity.cz");
    ImGui::TextWrapped("%s", tr("Free software under the GNU General Public License v3 (the file LICENSE)."));
    link("GitHub", "https://github.com/mlukasek/SAPIemu");
    ImGui::SameLine();
    link("8bity.cz", "https://www.8bity.cz");
    ImGui::SameLine();
    link("sapi.cz", "https://www.sapi.cz");
    ImGui::SeparatorText(tr("Thanks"));
    ImGui::TextWrapped("%s", tr("Petr Všijanský (EC1045, sapi.cz): dumps, documentation and disks of SAPI-1 and "
                                "answers to many questions."));
    ImGui::TextWrapped("%s", tr("Libor Lasota: the boards V, MikroMon, MIKROBIOS, his programs, demos and games."));
    ImGui::TextWrapped("%s", tr("Jiří Bíba (SAPIMAN): CP/M systems, disks and programs. R. Peřina, Tesla ELTOS, IMA, "
                                "DIZ, Elstroj and all authors of the software of the time."));
    ImGui::SeparatorText(tr("Libraries"));
    ImGui::TextWrapped("%s", "SDL 3 (zlib), Dear ImGui (MIT), cpp-httplib (MIT), nlohmann/json (MIT), chips z80.h "
                             "(zlib), i8080 (MIT), Nuked-OPL3 (LGPL 2.1+), minimp3 (CC0), DejaVu fonts.");
    ImGui::TextDisabled("%s", tr("Their licences are in the folder third_party."));
    ImGui::End();
}

void first_use_right(float w, float h, int k)
{
    (void)h;
    ImGuiIO& io = ImGui::GetIO();
    float x = io.DisplaySize.x - w - 10.0f;
    if (x < 10.0f) x = 10.0f;
    ImGui::SetNextWindowPos(ImVec2(x, 30.0f + (float)k * 28.0f), ImGuiCond_FirstUseEver);
}
