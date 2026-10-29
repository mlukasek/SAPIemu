// Help: the window "Help" (First steps and the Glossary from docs/, drawn in the program), the windows "About
// SAPIemu", "Keyboard shortcuts" (one table for the whole program) and "Report a bug", the user guides of docs/
// opened in the program the system has for them.
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/markdown.h"
#include "frontend/theme.h"

#include <stdio.h>
#include <string.h>
#include <vector>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "core/host.h"
#include "core/log.h"
#include "core/util.h"
#include "core/version.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>   // RtlGetVersion: the version of Windows for Report a bug
#else
#include <sys/utsname.h>
#endif

// A file or a folder in the program the system has for it (a folder: Explorer).
static void open_in_system(const std::string& path)
{
    std::string p = path;
    for (char& c : p)
        if (c == '\\') c = '/';
    SDL_OpenURL(("file:///" + p).c_str());
}

void open_doc(const char* rel)
{
    open_in_system(data_path(rel));
}

void open_folder(const std::string& dir)
{
    make_dirs(dir);
    open_in_system(dir);
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

// ---------------------------------------------------------------- the window Help: first steps, glossary

// The pages of the window Help (HelpPage): a document of docs/ in the language of the user interface (English: the
// Czech one when the English one is missing), or its copy at the top of the release zip (tools/package.py GUIDES)
// when docs/ lacks it. package.py checks that these files are in the zip.
static const struct {
    const char* title;   // tr() key: the tab and the item of the menu Help
    const char* cs;
    const char* en;
    const char* top_cs;
    const char* top_en;
    bool all_open;       // all its sections open at first (one page to look things up in), else the first one
} k_help[HELP_PAGES] = {
    { "First steps", "docs/prvni-kroky.md", "docs/getting-started.md", "PRVNI-KROKY.md", "GETTING-STARTED.md", false },
    { "Glossary", "docs/slovnicek.md", "docs/glossary.md", nullptr, nullptr, true },
};

// The file of a page now (from the data root), "" = none there.
static std::string help_file(int page)
{
    const char* in_docs[2] = { k_help[page].cs, k_help[page].en };
    const char* at_top[2] = { k_help[page].top_cs, k_help[page].top_en };
    for (int k = i18n_language() == LANG_EN ? 1 : 0; k >= 0; k--) {
        if (file_exists(data_path(in_docs[k]))) return in_docs[k];
        if (at_top[k] && file_exists(data_path(at_top[k]))) return at_top[k];
    }
    return std::string();
}

void help_open(UiState& ui, int page)
{
    ui.show_help = true;
    ui.help_page = page;
    ui.help_select = true;
    ui.help_focus = true;
}

// A link to the document of another page (the glossary from the first steps): that page instead of the file outside.
static bool help_link(UiState& ui, const std::string& path)
{
    for (int p = 0; p < HELP_PAGES; p++)
        if (path == help_file(p)) {
            ui.help_page = p;
            ui.help_select = true;
            return true;
        }
    return false;
}

struct HelpDoc {
    bool read = false;
    UiLanguage lang = LANG_CS;   // read for this language of the user interface
    std::string file;            // the file read (another one after a change of the language)
    std::vector<MdBlock> blocks;
    bool sections = false;       // it has ## sections (Expand all, Collapse all)
};

static void help_page(UiState& ui, int page, HelpDoc& d, int set_open)
{
    bool fresh = false;   // another document (the language changed): from the top
    if (!d.read || d.lang != i18n_language()) {
        std::string file = help_file(page);
        fresh = d.read && file != d.file;
        d.read = true;
        d.lang = i18n_language();
        d.file = file;
        d.blocks.clear();
        std::string text;
        if (!file.empty() && file_read_text(data_path(file), text)) {
            md_parse(text, d.blocks);
            md_resolve(d.blocks, file);
        }
        d.sections = false;
        for (const MdBlock& b : d.blocks)
            if (b.kind == MD_HEADING && b.level == 2) d.sections = true;
    }
    float u = ImGui::GetFontSize();
    if (d.sections) {
        if (ImGui::SmallButton(tr("Expand all"))) set_open = 1;
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Collapse all"))) set_open = -1;
        ImGui::SameLine();
    }
    if (!d.file.empty()) {   // the file itself (to print it, to search in it), at the right edge
        const char* name = d.file.c_str();
        float w = ImGui::CalcTextSize(name).x;
        float x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - w;
        if (x > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(x);
        if (ImGui::TextLink(name)) open_doc(name);
        if (ImGui::IsItemHovered()) tip(tr("Opens the file in the program the system has for .md files."));
    } else {
        ImGui::NewLine();
    }
    ImGui::Separator();   // the text scrolls under it
    ImGui::BeginChild("text", ImVec2(0, 0), ImGuiChildFlags_None);
    if (d.blocks.empty()) {
        std::string want = i18n_language() == LANG_EN ? k_help[page].en : k_help[page].cs;
        ImGui::TextDisabled(tr("The file %s is missing."), want.c_str());
    } else {
        MdView v;
        v.fold = true;
        v.all_open = k_help[page].all_open;
        v.set_open = set_open;
        v.own_link = help_link;
        ImGui::Indent(0.4f * u);
        md_draw(ui, d.blocks, v);
        ImGui::Unindent(0.4f * u);
        ImGui::Dummy(ImVec2(0, 0.5f * u));   // the end of the text clear of the edge
        if (fresh) ImGui::SetScrollY(0);
        if (ui.help_scroll >= 0) ImGui::SetScrollY(ui.help_scroll * ImGui::GetScrollMaxY());
    }
    ImGui::EndChild();
}

void draw_help(UiState& ui)
{
    if (!ui.show_help) return;
    layout_window(LW_HELP);
    if (ui.help_focus) {   // asked for from the menu: in front of the windows that hide it
        ImGui::SetNextWindowFocus();
        ui.help_focus = false;
    }
    bool visible = ImGui::Begin(tr_id("Help"), &ui.show_help);
    if (!ui.show_help) ui.focus_display = 2;   // closed: the keyboard back to the display
    if (!visible) {
        ImGui::End();
        return;
    }
    static HelpDoc s_docs[HELP_PAGES];   // each tab keeps its own scrolling and open sections
    bool select = ui.help_select;        // a link may ask for another page while drawing: the next frame
    ui.help_select = false;
    if (ImGui::BeginTabBar("pages")) {
        for (int p = 0; p < HELP_PAGES; p++) {
            char label[96];
            snprintf(label, sizeof(label), "%s###page%d", tr(k_help[p].title), p);
            ImGuiTabItemFlags flags = select && ui.help_page == p ? ImGuiTabItemFlags_SetSelected : 0;
            if (!ImGui::BeginTabItem(label, nullptr, flags)) continue;
            if (!select) ui.help_page = p;
            int set_open = 0;
            if (ui.help_set_open && p == ui.help_page && !select) {
                set_open = ui.help_set_open;
                ui.help_set_open = 0;
            }
            help_page(ui, p, s_docs[p], set_open);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void help_docs_menu(UiState& ui)
{
    // in the window Help: Markdown drawn as on GitHub (tables, sections that fold), not a raw .md outside
    if (ImGui::MenuItem(tr("First steps"))) help_open(ui, HELP_FIRST_STEPS);
    if (ImGui::MenuItem(tr("Glossary"))) help_open(ui, HELP_GLOSSARY);
    if (ImGui::IsItemHovered())
        tip(tr("The words of SAPI and of SAPIemu: machine, configurator, card, slot, jumper..."));
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
    const char* keys;   // tr() key too: the names of keys stay, a word ("arrows") is translated
    const char* what;   // tr() key
} k_shortcuts[] = {
    // the debugging keys as in Visual Studio (F5, F9, F10, F11; decision 2026-10-29)
    { "F5", "Pause / run" },
    { "F8", "Emulation speed 100 / 200 / 400 % / maximum" },
    { "F12", "Reset of the machine (the memory stays)" },
    { "Ctrl+F12", "Switch off and on (the memory is lost, as with the power switch)" },
    { "F11, F10", "Step, step over (when paused)" },
    { "F9", "Breakpoint on / off in the debugger: at the disassembly row under the mouse, else at the PC" },
    { "Ctrl+F5", "Screenshot of the picture focused last" },
    { "Ctrl+Alt+F5", "Video with sound on / off (MP4 through ffmpeg)" },
    { "Ctrl+Shift+C", "Copy the text of the screen to the clipboard" },
    { "Alt+Enter", "Full screen of the display used last, Alt+Enter again leaves it (Esc goes to SAPI)" },
    { "Ctrl+F2, Ctrl+F3", "Quick save, quick load of the state" },
    { "Ctrl+F7", "Game mode of the ANK-1 keyboard (keys by position, Shift = SHIFT)" },
    { "Ctrl+F8", "Joystick from the keyboard (arrows, space = fire)" },
    { "Ctrl+F10", "Release the captured mouse (also the middle button)" },
    { "Ctrl+Shift+V", "Paste the clipboard on the SAPI keyboard (with the screen focused)" },
    { "Alt+C, Alt+V", "Copy, paste in the serial terminal (also Ctrl+Shift+C, Ctrl+Shift+V there)" },
    { "Enter, Esc", "In a question window: Enter = the highlighted button, Esc = Cancel or Close" },
    { "Tab, arrows", "In the windows of the emulator (not on the screen of SAPI, not in the terminal): Tab shows the "
                     "frame on a control and moves it to the next one, the arrows move it too; Space or Enter "
                     "presses the one in the frame, a click of the mouse hides the frame" },
    { "Ctrl+Tab", "Switch between the windows (all the open windows also in View -> Windows)" },
};

void draw_shortcuts(UiState& ui)
{
    if (!ui.show_shortcuts) return;
    float u = ImGui::GetFontSize();
    layout_window(LW_SHORTCUTS);
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
            ImGui::TextUnformatted(tr(s.keys));
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
    if (ImGui::IsItemHovered()) tip(url);
}

void draw_about(UiState& ui)
{
    if (!ui.show_about) return;
    float u = ImGui::GetFontSize();
    ImGuiIO& io = ImGui::GetIO();
    // the width fixed, the height that of the content (whatever imgui.ini says): no scroll bar for a few pixels; at most
    // the height of the work area (not over the menu bar)
    float most = ImGui::GetMainViewport()->WorkSize.y - u;
    ImGui::SetNextWindowSizeConstraints(ImVec2(42 * u, 0), ImVec2(42 * u, most > 10 * u ? most : 10 * u));
    layout_window(LW_ABOUT);
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

// ---------------------------------------------------------------- report a bug

static const char* k_issues_url = "https://github.com/mlukasek/SAPIemu/issues";
// The repository is private until the publication (plan-vzhled.md, stage 9): the address as plain text, a link
// would end on a GitHub 404. At the publication: true, and the texts "until SAPIemu is public" go.
static const bool k_issues_public = false;
static const int k_report_log_lines = 200;

// The values of the report: English in the copied text (like the log), translated in the window.
typedef const char* (*ReportText)(const char*);
static const char* report_en(const char* s) { return s; }

// The system and its version. Windows: RtlGetVersion (GetVersionEx answers 6.2 to a program without a manifest
// for the newer versions); Windows 11 still says 10.0, its builds start at 22000.
static std::string os_text(ReportText tx)
{
    char t[160];
#ifdef _WIN32
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*);
    OSVERSIONINFOW v;
    memset(&v, 0, sizeof(v));
    v.dwOSVersionInfoSize = sizeof(v);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn get = nt ? (RtlGetVersionFn)(void*)GetProcAddress(nt, "RtlGetVersion") : nullptr;
    if (get && get(&v) == 0) {
        const char* name = "Windows";
        if (v.dwMajorVersion == 10) name = v.dwBuildNumber >= 22000 ? "Windows 11" : "Windows 10";
        snprintf(t, sizeof(t), "%s (%lu.%lu.%lu)", name, (unsigned long)v.dwMajorVersion,
                 (unsigned long)v.dwMinorVersion, (unsigned long)v.dwBuildNumber);
    } else {
        snprintf(t, sizeof(t), "Windows");
    }
#else
    struct utsname u;
    if (uname(&u) == 0) snprintf(t, sizeof(t), "%s %s %s", u.sysname, u.release, u.machine);
    else snprintf(t, sizeof(t), "%s", SDL_GetPlatform());
#endif
    std::string s = t;
    snprintf(t, sizeof(t), tx(", %d-bit program, %d CPU threads, RAM %d MB"), (int)sizeof(void*) * 8,
             SDL_GetNumLogicalCPUCores(), SDL_GetSystemRAM());
    return s + t;
}

struct ReportRow {
    const char* key;     // in the copied text (English, like the log)
    const char* label;   // tr() key of the window
    std::string value;
};

// What the report says about this run: the same rows in the window (tx = tr) and in the copied text (report_en).
static void report_rows(UiState& ui, std::vector<ReportRow>& rows, ReportText tx)
{
    EmuHost& h = *ui.host;
    Machine* m = h.machine();
    std::string ver = sapiemu_version();
#ifndef NDEBUG
    ver += std::string(" ") + tx("(debug build)");
#endif
    rows.push_back({ "version", "Version", ver });
    std::string file = h.machine_file().empty() ? std::string("-") : h.machine_file();
    std::string mach = m ? m->name + " (" + file + ")" : std::string(tx("switched off")) + " (" + file + ")";
    if (rack_runs_kept(ui)) mach += std::string(", ") + tx("with changes kept from the configurator, not in its file");
    rows.push_back({ "machine", "Running machine", mach });
    if (m) {
        std::string cards;
        for (auto& c : m->cards) {
            if (!cards.empty()) cards += ", ";
            if (c->slot > 0) cards += "X" + std::to_string(c->slot) + " ";
            cards += c->type;
            if (c->name != c->type) cards += " (" + c->name + ")";
        }
        rows.push_back({ "cards", "Cards", cards });
        char st[96];
        const char* run = h.paused ? tx("paused") : tx("running");
        if (h.max_speed) snprintf(st, sizeof(st), tx("%s, speed maximum"), run);
        else snprintf(st, sizeof(st), tx("%s, speed %d %%"), run, h.pace_percent);
        rows.push_back({ "state", "State", st });
    }
    rows.push_back({ "data", "Data folder", data_root() });
    rows.push_back({ "work", "Work folder", work_root() });
    rows.push_back({ "settings", "Settings file", ui.settings_file.empty() ? std::string("-") : ui.settings_file });
    rows.push_back({ "log", "Log file", ui.log_file.empty() ? std::string("-") : ui.log_file });
    if (!ui.log_prev_file.empty()) rows.push_back({ "previous_log", "Previous log", ui.log_prev_file });
    rows.push_back({ "system", "System", os_text(tx) });
    SDL_Renderer* r = ui.window ? SDL_GetRenderer(ui.window) : nullptr;
    const char* rname = r ? SDL_GetRendererName(r) : nullptr;
    int w = 0, hh = 0;
    if (ui.window) SDL_GetWindowSize(ui.window, &w, &hh);
    int sdl = SDL_GetVersion();
    char d[200];
    snprintf(d, sizeof(d), tx("%s, window %dx%d, scale %.2f, language %s, SDL %d.%d.%d, Dear ImGui %s"),
             rname ? rname : "-", w, hh, ui.window ? SDL_GetWindowDisplayScale(ui.window) : 1.0f,
             i18n_code(i18n_language()), SDL_VERSIONNUM_MAJOR(sdl), SDL_VERSIONNUM_MINOR(sdl), SDL_VERSIONNUM_MICRO(sdl),
             ImGui::GetVersion());
    rows.push_back({ "display", "Display", d });
}

std::string report_diagnostics(UiState& ui, int log_lines)
{
    std::vector<ReportRow> rows;
    report_rows(ui, rows, report_en);
    std::string s = "SAPIemu diagnostics\n";
    for (const ReportRow& r : rows) s += std::string(r.key) + ": " + r.value + "\n";
    if (log_lines > 0) {
        std::vector<std::string> lines;
        log_recent(lines, log_lines);
        s += "--- log, the last " + std::to_string(lines.size()) + " lines ---\n";
        for (const std::string& l : lines) s += l + "\n";
    }
    return s;
}

// Help -> Report a bug: what to describe, where to send it, the diagnostics of this run to copy into the report.
// No contact of the author here: the beta testers send it the way they got the program, after the publication it
// goes to GitHub Issues.
void draw_report(UiState& ui)
{
    if (!ui.show_report) return;
    float u = ImGui::GetFontSize();
    float most = ImGui::GetMainViewport()->WorkSize.y - u;   // as About: the width fixed, the height of the content
    ImGui::SetNextWindowSizeConstraints(ImVec2(44 * u, 0), ImVec2(44 * u, most > 10 * u ? most : 10 * u));
    layout_window(LW_REPORT);
    if (!ImGui::Begin(tr_id("Report a bug"), &ui.show_report, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("%s", tr("Describe what you did (the machine, the steps), what you expected and what happened "
                                "instead. A screenshot (Ctrl+F5), a state saved just before the bug (File -> Save "
                                "state...) and, if you changed the machine, its file help a lot."));
    ImGui::Spacing();
    if (k_issues_public) {
        ImGui::TextWrapped("%s", tr("Send the report to the author through GitHub Issues:"));
        ImGui::TextLinkOpenURL("github.com/mlukasek/SAPIemu/issues", k_issues_url);
        if (ImGui::IsItemHovered()) tip(k_issues_url);
    } else {
        ImGui::TextWrapped("%s", tr("Send the report to the author: until SAPIemu is public, reply the way you got the "
                                    "program. After that, use GitHub Issues (github.com/mlukasek/SAPIemu/issues)."));
    }
    theme_section(tr("Diagnostics"));
    std::vector<ReportRow> rows;
    report_rows(ui, rows, tr);
    if (ImGui::BeginTable("diag", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("k", ImGuiTableColumnFlags_WidthFixed, 9 * u);
        ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch);
        for (const ReportRow& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(tr(r.label));
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", r.value.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    if (theme_accent_button(tr("Copy diagnostics"))) {
        SDL_SetClipboardText(report_diagnostics(ui, k_report_log_lines).c_str());
        ui.report_status =
            tr("Copied to the clipboard: the diagnostics and the end of the log. Paste them into the report.");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(ui.log_file.empty());
    if (ImGui::Button(tr("Open the log folder"))) open_folder(path_dir(ui.log_file));
    ImGui::EndDisabled();
    if (!ui.report_status.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme().ok);
        ImGui::TextWrapped("%s", ui.report_status.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PushStyleColor(ImGuiCol_Text, theme().text_dim);
    ImGui::TextWrapped("%s", tr("Copy diagnostics: the rows above and the last 200 lines of the log. They include paths "
                                "on this PC, which may contain your user name; check them before sending."));
    // the start that opens this window began a new log: the one of a run that crashed is the previous log
    if (!ui.log_prev_file.empty())
        ImGui::TextWrapped("%s", tr("After a crash: send the previous log too (the row above, in the log folder). It is "
                                    "the log of the run before this one; the next start replaces it."));
    ImGui::PopStyleColor();
    ImGui::End();
}
