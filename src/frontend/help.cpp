// Help: the windows "About SAPIemu" and "Keyboard shortcuts" (one table for the whole program), the user guides
// of docs/ opened in the program the system has for them.
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/theme.h"

#include <stdio.h>
#include <vector>
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
    { "Ctrl+F12", "Switch off and on (the memory is lost, as with the power switch)" },
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

// The logo SAPI 1 (tools/make_icon.py): the mask logo-alpha.png (white on transparent) in the colour of the text,
// dark letters on the light window and white ones on the dark window (author 2026-10-28). The mask is shrunk on the
// CPU to the size it is drawn at, every pixel the mean of the area it covers: shrunk three times by the GPU (a few
// texels per pixel) its edges were rough. Without the mask the old logo.png with its black band.
struct AboutLogo {
    bool tried = false;
    SDL_Surface* mask = nullptr;    // logo-alpha.png as RGBA32, kept for other sizes
    SDL_Texture* tex = nullptr;     // the mask at the size drawn last, or logo.png
    int tex_w = 0, tex_h = 0;
    bool tinted = false;            // the mask (else logo.png)
};

// Area average of the alpha of `src` (RGBA32) into w x h (not larger than src): the strokes keep their weight.
static void shrink_alpha(const SDL_Surface* src, int w, int h, std::vector<uint32_t>& out)
{
    int sw = src->w, sh = src->h;
    float sx = (float)sw / (float)w, sy = (float)sh / (float)h;
    std::vector<float> rows((size_t)w * sh);   // horizontally shrunk, all source rows
    for (int y = 0; y < sh; y++) {
        const uint8_t* line = (const uint8_t*)src->pixels + (size_t)y * src->pitch;
        for (int x = 0; x < w; x++) {
            float x0 = x * sx, x1 = x0 + sx, sum = 0;
            for (int i = (int)x0; i < sw && (float)i < x1; i++) {
                float a = x0 > (float)i ? x0 : (float)i, b = x1 < (float)(i + 1) ? x1 : (float)(i + 1);
                if (b > a) sum += line[(size_t)i * 4 + 3] * (b - a);
            }
            rows[(size_t)y * w + x] = sum / sx;
        }
    }
    out.assign((size_t)w * h, 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float y0 = y * sy, y1 = y0 + sy, sum = 0;
            for (int j = (int)y0; j < sh && (float)j < y1; j++) {
                float a = y0 > (float)j ? y0 : (float)j, b = y1 < (float)(j + 1) ? y1 : (float)(j + 1);
                if (b > a) sum += rows[(size_t)j * w + x] * (b - a);
            }
            uint32_t al = (uint32_t)(sum / sy + 0.5f);
            out[(size_t)y * w + x] = (al > 255 ? 255u : al) << 24 | 0x00FFFFFFu;   // ARGB, white
        }
}

// The texture of the logo `h` pixels high (the mask shrunk to it; logo.png as it is).
static SDL_Texture* about_logo(UiState& ui, AboutLogo& lg, int h)
{
    SDL_Renderer* r = ui.window ? SDL_GetRenderer(ui.window) : nullptr;
    if (!r) return nullptr;
    if (!lg.tried) {
        lg.tried = true;
        if (SDL_Surface* s = SDL_LoadPNG(data_path("docs/img/logo-alpha.png").c_str())) {
            lg.mask = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(s);
        }
        lg.tinted = lg.mask != nullptr;
        if (!lg.mask)
            if (SDL_Surface* s = SDL_LoadPNG(data_path("docs/img/logo.png").c_str())) {
                lg.tex = SDL_CreateTextureFromSurface(r, s);
                lg.tex_w = s->w;
                lg.tex_h = s->h;
                SDL_DestroySurface(s);
            }
    }
    if (!lg.mask) return lg.tex;
    if (h > lg.mask->h) h = lg.mask->h;   // larger: the whole mask, the GPU enlarges it smoothly
    if (h < 8) h = 8;
    if (lg.tex && lg.tex_h == h) return lg.tex;
    int w = (int)((float)h * (float)lg.mask->w / (float)lg.mask->h + 0.5f);
    std::vector<uint32_t> px;
    shrink_alpha(lg.mask, w, h, px);
    if (lg.tex) SDL_DestroyTexture(lg.tex);
    lg.tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    if (lg.tex) {
        SDL_UpdateTexture(lg.tex, nullptr, px.data(), w * 4);
        SDL_SetTextureBlendMode(lg.tex, SDL_BLENDMODE_BLEND);
    }
    lg.tex_w = w;
    lg.tex_h = h;
    return lg.tex;
}

static void url_link(const char* label, const char* url)
{
    ImGui::TextLinkOpenURL(label, url);   // a link as in a browser; the backend opens it through SDL
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", url);
}

void draw_about(UiState& ui)
{
    if (!ui.show_about) return;
    float u = ImGui::GetFontSize();
    ImGuiIO& io = ImGui::GetIO();
    // the width fixed, the height that of the content (whatever imgui.ini says): no scroll bar for a few pixels; at most
    // the height of the main window
    ImGui::SetNextWindowSizeConstraints(ImVec2(42 * u, 0), ImVec2(42 * u, io.DisplaySize.y * 0.95f));
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    if (!ImGui::Begin(tr_id("About SAPIemu"), &ui.show_about, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    // the logo on the left, the name and the version in the top right corner next to it
    static AboutLogo s_logo;
    float fb = io.DisplayFramebufferScale.y > 0 ? io.DisplayFramebufferScale.y : 1.0f;
    ImVec2 top = ImGui::GetCursorPos();
    float right = top.x + ImGui::GetContentRegionAvail().x, left = top.x, bottom = top.y;
    if (SDL_Texture* logo = about_logo(ui, s_logo, (int)(3.2f * u * fb + 0.5f))) {
        // pixels of the texture 1:1 on the pixel grid of the screen (no second resampling)
        float h = (float)s_logo.tex_h / fb, w = (float)s_logo.tex_w / fb;
        if (!s_logo.tinted || s_logo.tex_h != (int)(3.2f * u * fb + 0.5f)) {   // logo.png or a larger size
            h = 3.2f * u;
            w = h * (float)s_logo.tex_w / (float)s_logo.tex_h;
        }
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2((float)(int)(p.x * fb + 0.5f) / fb, (float)(int)(p.y * fb + 0.5f) / fb));
        if (s_logo.tinted)
            ImGui::ImageWithBg(ImTextureRef((ImTextureID)(intptr_t)logo), ImVec2(w, h), ImVec2(0, 0), ImVec2(1, 1),
                               ImVec4(0, 0, 0, 0), theme().text);
        else ImGui::Image(ImTextureRef((ImTextureID)(intptr_t)logo), ImVec2(w, h));
        left += w + u;
        bottom += h;
    }
    const char* ver = sapiemu_version();
    ImFont* bold = theme_fonts().bold[theme_variant()];
    ImGui::PushFont(bold, 32.0f);
    float nw = ImGui::CalcTextSize("SAPIemu").x;
    ImGui::SetCursorPos(ImVec2(right - nw > left ? right - nw : left, top.y));
    ImGui::PushStyleColor(ImGuiCol_Text, theme().heading);
    ImGui::TextUnformatted("SAPIemu");
    ImGui::PopStyleColor();
    ImGui::PopFont();
    float vw = ImGui::CalcTextSize(ver).x;
    ImGui::SetCursorPosX(right - vw > left ? right - vw : left);
    ImGui::TextDisabled("%s", ver);
    if (ImGui::GetCursorPosY() < bottom) ImGui::SetCursorPosY(bottom);
    ImGui::Spacing();
    ImGui::TextWrapped("%s", tr("Emulator of the Czechoslovak modular 8-bit microcomputer Tesla SAPI-1 (1983-1990), "
                                "designed by Ing. Eduard Smutný and made by Tesla Liberec. SAPI-1 was an industrial "
                                "control computer with its cards in a rack on the ARB-1 bus. The emulator offers the "
                                "original boards and the historic ZPS sets as well as the boards made for it later, "
                                "e.g. the boards of Libor Lasota and Petr Všijanský, and also the devices and "
                                "instruments SAPI worked with."));
    theme_section(tr("Author"));
    ImGui::TextUnformatted("Copyright © 2026 Martin Lukášek, 8bity.cz");
    ImGui::TextWrapped("%s", tr("Free software under the GNU General Public License v3 (the file LICENSE)."));
    url_link("GitHub", "https://github.com/mlukasek/SAPIemu");
    ImGui::SameLine();
    url_link("8bity.cz", "https://www.8bity.cz");
    ImGui::SameLine();
    url_link("sapi.cz", "https://www.sapi.cz");
    ImGui::SameLine();
    url_link("pmd85.cz", "https://pmd85.cz");
    theme_section(tr("Thanks"));
    ImGui::TextWrapped("%s", tr("Petr Všijanský (EC1045, sapi.cz): dumps, documentation and disks of SAPI-1 and "
                                "answers to many questions."));
    ImGui::TextWrapped("%s", tr("Libor Lasota (pmd85.cz): the boards V, MikroMon, MIKROBIOS, his programs, demos and games."));
    ImGui::TextWrapped("%s", tr("Jiří Bíba (SAPIMAN): CP/M systems, disks and programs. R. Peřina, Tesla ELTOS, IMA, "
                                "DIZ, Elstroj and all authors of the software of the time."));
    theme_section(tr("Libraries"));
    ImGui::TextWrapped("%s", "SDL 3 (zlib), Dear ImGui (MIT), cpp-httplib (MIT), nlohmann/json (MIT), chips z80.h "
                             "(zlib), i8080 (MIT), Nuked-OPL3 (LGPL 2.1+), minimp3 (CC0), DejaVu fonts.");
    ImGui::TextDisabled("%s", tr("Their licences are in the file THIRD-PARTY-NOTICES.txt."));
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
