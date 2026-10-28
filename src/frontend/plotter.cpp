// Window "Minigraf" (the plotter): the sheet of the plotter ARITMA Minigraf 0507 (DPP-1) or XY 4130 (JPR-1 X3,
// DPP-1), the drawing as it grows, the carriage on its rail with the pen, and the sheet saved as PNG or SVG
// (work/minigraf). The plotter of AR 1990 (a JPR-1 driving a Minigraf) has its panel and the cable from the
// host computer here too, the XY 4130 its lamps and the PEN button.
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/theme.h"

#include <stdio.h>
#include <mutex>
#include <vector>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "chips/ar1990.h"
#include "chips/minigraf.h"
#include "chips/xy4130.h"
#include "core/host.h"
#include "core/machine.h"
#include "core/util.h"

static void plot_notice(UiState& ui, bool ok, const std::string& text)
{
    ui.notice = text;
    ui.notice_error = !ok;
    ui.notice_ns = SDL_GetTicksNS();
}

// work/minigraf/kresba_<YYYYMMDD-HHMMSS>.<ext>
static std::string plot_path(const char* ext)
{
    SDL_Time t = 0;
    SDL_DateTime dt;
    char stamp[32] = "";
    if (SDL_GetCurrentTime(&t) && SDL_TimeToDateTime(t, &dt, true))
        snprintf(stamp, sizeof(stamp), "%04d%02d%02d-%02d%02d%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    std::string dir = data_path("work/minigraf");
    make_dirs(dir);
    std::string base = path_join(dir, std::string("kresba_") + stamp);
    std::string path = base + ext;
    for (int i = 2; file_exists(path) && i < 100; i++) path = base + "-" + std::to_string(i) + ext;
    return path;
}

// The paper (a strip as wide as A4) around the drawing and the pen, at least as high as A4, on the
// table; the rail goes over the travel of the carriage. The table fits the width of the window, a long roll
// scrolls; while the pen moves, the view follows it when it would leave the view.
static void draw_sheet(Plotter* g)
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.y < 100) avail.y = 100;
    ImGui::BeginChild("sheet", avail, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    float vw = ImGui::GetContentRegionAvail().x, vh = ImGui::GetWindowHeight();
    const float spm = (float)g->spm;
    int32_t x0, y0, x1, y1;
    if (!g->extent(x0, y0, x1, y1)) y0 = y1 = g->y();
    if (g->y() < y0) y0 = g->y();
    if (g->y() > y1) y1 = g->y();
    float top = y1 / spm + 10, bottom = y0 / spm - 10;   // mm of the paper shown
    if (top - bottom < 297) {
        float c = (top + bottom) * 0.5f;
        top = c + 148.5f;
        bottom = c - 148.5f;
    }
    const float mw = 230.0f, mh = top - bottom;   // mm shown
    float k = vw / mw;                             // pixels per mm
    float ch = mh * k;                             // height of the table, shorter ones in the middle
    ImVec2 o = ImGui::GetCursorScreenPos();
    float ox = o.x, oy = o.y + (ch < vh ? (vh - ch) * 0.5f : 0.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(ox, oy), ImVec2(ox + mw * k, oy + mh * k), IM_COL32(70, 74, 80, 255), 4.0f);
    // paper (x, y in steps, y up) to the screen
    const float sx0 = ox + 10 * k;
    auto P = [&](int32_t x, int32_t y) { return ImVec2(sx0 + x / spm * k, oy + (top - y / spm) * k); };
    dl->PushClipRect(ImVec2(ox, oy), ImVec2(ox + mw * k, oy + mh * k), true);
    ImVec2 pa(sx0, oy), pb(sx0 + 210 * k, oy + mh * k);
    dl->AddRectFilled(ImVec2(pa.x + 3, pa.y), ImVec2(pb.x + 3, pb.y), IM_COL32(30, 30, 34, 160));
    dl->AddRectFilled(pa, pb, IM_COL32(250, 250, 246, 255));
    ImU32 ink = IM_COL32(20, 40, 120, 255);
    float th = k * 0.3f < 1.0f ? 1.0f : k * 0.3f;   // a 0.3 mm pen
    std::vector<ImVec2> pts;
    for (const std::vector<int32_t>& st : g->strokes) {
        if (st.size() == 2) {
            dl->AddCircleFilled(P(st[0], st[1]), th * 0.6f, ink);
            continue;
        }
        pts.clear();
        for (size_t i = 0; i + 1 < st.size(); i += 2) pts.push_back(P(st[i], st[i + 1]));
        dl->AddPolyline(pts.data(), (int)pts.size(), ink, ImDrawFlags_None, th);
    }
    // the rail over the travel, the left stop, the carriage with the pen
    ImVec2 pen = P(g->x(), g->y());
    float r0 = P(g->stop_x, 0).x, r1 = P(g->stop_x + g->travel, 0).x;
    dl->AddRectFilled(ImVec2(r0 - 2 * k, pen.y - 1.2f * k), ImVec2(r1 + 2 * k, pen.y + 1.2f * k), IM_COL32(150, 155, 162, 200), 1.0f);
    dl->AddRectFilled(ImVec2(r0 - 3 * k, pen.y - 3 * k), ImVec2(r0 - 1 * k, pen.y + 3 * k), IM_COL32(110, 110, 116, 230));
    dl->AddRect(ImVec2(pen.x - 4 * k, pen.y - 4 * k), ImVec2(pen.x + 4 * k, pen.y + 4 * k), IM_COL32(60, 62, 70, 230), 1.5f, 0, 2.0f);
    dl->AddCircleFilled(pen, 1.6f * k, g->pen_down() ? IM_COL32(200, 40, 40, 255) : IM_COL32(120, 120, 128, 220));
    dl->PopClipRect();
    ImGui::Dummy(ImVec2(vw, ch > vh ? ch : vh - 1.0f));
    static uint64_t s_steps = ~0ull;   // the pen moved: keep it in sight
    if (g->steps != s_steps) {
        s_steps = g->steps;
        float py = pen.y - o.y, sy = ImGui::GetScrollY();
        if (ch > vh && (py < sy + 0.15f * vh || py > sy + 0.85f * vh)) ImGui::SetScrollY(py - 0.5f * vh);
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------- the plotter of AR 1990

// a file for the plotter of AR 1990 (the dialog answers from another thread)
static std::mutex s_ar_mtx;
static std::string s_ar_file;
static bool s_ar_open = false;

static void SDLCALL ar_file_done(void*, const char* const* files, int)
{
    std::lock_guard<std::mutex> lk(s_ar_mtx);
    s_ar_file = (files && files[0]) ? files[0] : "";
    s_ar_open = false;
}

static void ar_lamp(const char* name, bool on, ImU32 col)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float u = ImGui::GetFontSize();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 c(p.x + 0.45f * u, p.y + 0.55f * u);
    dl->AddCircleFilled(c, 0.38f * u, on ? col : IM_COL32(70, 70, 70, 255), 16);
    dl->AddCircle(c, 0.38f * u, IM_COL32(30, 30, 30, 255), 16, 1.0f);
    ImGui::Dummy(ImVec2(1.0f * u, 1.1f * u));
    ImGui::SameLine(0, 0.2f * u);
    ImGui::TextUnformatted(name);
}

// The front panel and the back switches of the plotter, the cable from the host computer.
static void draw_ar1990(UiState& ui, Machine* m, Ar1990Plotter* p)
{
    static char line[256] = "";
    {
        std::string file;
        {
            std::lock_guard<std::mutex> lk(s_ar_mtx);
            file.swap(s_ar_file);
        }
        if (!file.empty()) {
            std::vector<uint8_t> d;
            if (file_read_all(file, d)) {
                p->send(d.data(), d.size());
                plot_notice(ui, true, std::string(tr("Sent to the plotter")) + ": " + file);
            } else plot_notice(ui, false, std::string(tr("Cannot read")) + " " + file);
        }
    }
    theme_section(tr("Plotter of AR 1990 (JPR-1, commands of Sharp MZ-1P16)"));
    ar_lamp("PWR", true, IM_COL32(240, 60, 50, 255));
    ImGui::SameLine(0, 1.0f * ImGui::GetFontSize());
    ar_lamp("RDY", p->led_ready(), IM_COL32(80, 220, 90, 255));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Lit: the plotter waits for the READY button."));
    ImGui::SameLine(0, 1.0f * ImGui::GetFontSize());
    ar_lamp("BUFF", p->led_buff(), IM_COL32(240, 200, 60, 255));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Lit all the time: the buffer is empty."));
    ImGui::SameLine(0, 1.5f * ImGui::GetFontSize());
    if (ImGui::Button("STOP")) p->press(Ar1990Plotter::BTN_STOP);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Stops the plotter; it beeps and waits for READY."));
    ImGui::SameLine();
    if (ImGui::Button("TEST")) p->press(Ar1990Plotter::BTN_TEST);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("The test of the pens (as code 04H). Held at power on: the whole character set."));
    ImGui::SameLine();
    if (ImGui::Button("READY")) p->press(Ar1990Plotter::BTN_READY);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Goes on after a beep: after power on, STOP, a pen change or a stop."));
    ImGui::SameLine();
    if (ImGui::Button(tr("Whole test"))) {   // TEST held while the JPR-1 starts
        p->press(Ar1990Plotter::BTN_TEST);
        m->reset();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Reset with TEST held: the whole character set (then READY)."));
    bool mz = !p->ascii, roll = !p->a4, crlf = !p->cr_only, lfcr = !p->lf_only;
    ImGui::Checkbox("COLOR", &p->color);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Colour commands stop the plotter and beep for a pen change."));
    ImGui::SameLine();
    if (ImGui::Checkbox("MZ800", &mz)) p->ascii = !mz;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Codes of the Sharp MZ-800 converted to ASCII (switch ASCII/MZ800)."));
    ImGui::SameLine();
    if (ImGui::Checkbox(tr("Roll"), &roll)) p->a4 = !roll;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("A4/ROLE: a new page skips a few lines (roll) or goes to a new sheet (A4)."));
    ImGui::SameLine();
    if (ImGui::Checkbox("CR+LF", &crlf)) p->cr_only = !crlf;
    ImGui::SameLine();
    if (ImGui::Checkbox("LF+CR", &lfcr)) p->lf_only = !lfcr;
    // the host: a file or a line of commands
    ImGui::BeginDisabled(s_ar_open);
    if (ImGui::Button(tr("Send file..."))) {
        static const SDL_DialogFileFilter flt[] = { { "Text (TXT, PLT)", "txt;plt" }, { "*", "*" } };
        std::string where = dialog_location(data_path("media/ar1990"));
        s_ar_open = true;
        SDL_ShowOpenFileDialog(ar_file_done, nullptr, ui.window, flt, 2, where.c_str(), false);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("Demo"))) {
        std::vector<uint8_t> d;
        if (file_read_all(data_path("media/ar1990/ukazka.txt"), d)) p->send(d.data(), d.size());
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Sends media/ar1990/ukazka.txt (graphics and text mode)."));
    ImGui::SetNextItemWidth(-8.0f * ImGui::GetFontSize());
    bool go = ImGui::InputTextWithHint("##arline", tr("commands, e.g. D400,0"), line, sizeof(line),
                                       ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button(tr("Send")) || go) {
        std::string s;   // \xNN = a control code
        for (const char* c = line; *c; c++) {
            unsigned v;
            if (c[0] == '\\' && (c[1] == 'x' || c[1] == 'X') && sscanf(c + 2, "%2x", &v) == 1 && c[2] && c[3]) {
                s += (char)v;
                c += 3;
            } else s += *c;
        }
        s += '\r';   // the end of the parameters
        p->send((const uint8_t*)s.data(), s.size());
        line[0] = 0;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Sends the line and CR. Codes: 02H graphics mode (type \\x02), 01H text mode."));
    ImGui::TextDisabled("%s %zu B, %s %llu B", tr("waiting"), p->pending(), tr("taken"), (unsigned long long)p->sent);
    ImGui::Separator();
}

// The front panel of the XY 4130: the lamps POWER, READY and PEN, the button PEN.
static void draw_xy4130(Xy4130* p)
{
    theme_section(tr("Plotter XY 4130"));
    ar_lamp("POWER", true, IM_COL32(80, 220, 90, 255));
    ImGui::SameLine(0, 1.0f * ImGui::GetFontSize());
    ar_lamp("READY", p->ready(), IM_COL32(80, 220, 90, 255));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Lit after the carriage has reached the left stop (the driver does it); off after a new sheet."));
    ImGui::SameLine(0, 1.0f * ImGui::GetFontSize());
    ar_lamp("PEN", p->pen_enabled(), IM_COL32(240, 200, 60, 255));
    ImGui::SameLine(0, 1.5f * ImGui::GetFontSize());
    if (ImGui::Button("PEN")) p->pen_button();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Ready to write: the pen goes down only with the PEN lamp lit (off after power on)."));
    if (!p->pen_enabled() || !p->ready()) {
        ImGui::SameLine();
        ImGui::TextColored(theme().warn, "%s", !p->pen_enabled() ? tr("press PEN") : tr("the pen waits for READY"));
    }
    ImGui::Separator();
}

void draw_plotter(UiState& ui)
{
    Machine* m = ui.host->machine();
    // a machine with the plotter opens the window when it starts (once for each machine)
    if (m && ui.plotter_gen != ui.host->machine_generation()) {
        ui.plotter_gen = ui.host->machine_generation();
        for (auto& c : m->cards)
            if (c->plotter()) ui.show_plotter = true;
    }
    if (!ui.show_plotter) return;
    float u = ImGui::GetFontSize();   // on the right: the screen of SAPI stays in sight
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(30 * u, 42 * u), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 30.5f * u, 2.0f * u), ImGuiCond_FirstUseEver);
    Plotter* g = nullptr;
    if (m)
        for (auto& c : m->cards)
            if (c->plotter()) g = c->plotter();
    Xy4130* xy = dynamic_cast<Xy4130*>(g);
    // the title names the plotter, the ID stays (the saved layout)
    std::string title = std::string(xy ? tr("Plotter XY 4130") : tr("Minigraf")) + "###Minigraf";
    if (!ImGui::Begin(title.c_str(), &ui.show_plotter) || !m) {
        ImGui::End();
        return;
    }
    if (!g) {
        ImGui::TextDisabled("%s", tr("No plotter. Configurator: DPP-1 (Minigraf 0507, XY 4130) or JPR-1 (XY 4130 on X3)."));
        ImGui::End();
        return;
    }
    if (Ar1990Plotter* ar = m->cpu ? dynamic_cast<Ar1990Plotter*>(m->cpu->keyboard()) : nullptr) draw_ar1990(ui, m, ar);
    if (xy) draw_xy4130(xy);
    if (ImGui::Button(tr("New sheet"))) g->new_sheet();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", xy ? tr("Clean paper is put in under the pen (the drawing is cleared); READY goes off.")
                                   : tr("Clean paper is put in under the pen (the drawing is cleared)."));
    ImGui::SameLine();
    std::string err;
    if (ImGui::Button(tr("Save PNG"))) {
        std::string p = plot_path(".png");
        if (g->save_png(p, 8, false, err)) plot_notice(ui, true, std::string(tr("Drawing saved")) + ": " + p);
        else plot_notice(ui, false, err);
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Save SVG"))) {
        std::string p = plot_path(".svg");
        if (g->save_svg(p, false, err)) plot_notice(ui, true, std::string(tr("Drawing saved")) + ": " + p);
        else plot_notice(ui, false, err);
    }
    ImGui::Text("X %.1f  Y %.1f mm   %s   %s %llu", g->x() / (double)g->spm, g->y() / (double)g->spm, g->pen_down() ? tr("pen down") : tr("pen up"), tr("steps"),
                (unsigned long long)g->steps);
    if (g->stopped) {
        ImGui::SameLine();
        ImGui::TextDisabled("  %s %llu", tr("at the stop"), (unsigned long long)g->stopped);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr("Steps of the carriage against its stop: the driver sets its origin so."));
    }
    if (g->lost) {
        ImGui::SameLine();
        ImGui::TextColored(theme().warn, "  %s %llu", tr("lost"), (unsigned long long)g->lost);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr("The code skipped a phase: the motor may have lost steps."));
    }
    draw_sheet(g);
    ImGui::End();
}
