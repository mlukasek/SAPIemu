// Window "Minigraf": the sheet of the plotter ARITMA Minigraf 0507 (DPP-1), the drawing as it grows,
// the carriage on its rail with the pen, and the sheet saved as PNG or SVG (work/minigraf).
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <stdio.h>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "chips/minigraf.h"
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
// table; the rail goes over the travel of the carriage.
static void draw_sheet(Minigraf* g)
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.y < 100) avail.y = 100;
    const float spm = (float)Minigraf::STEPS_PER_MM;
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
    float k = avail.x / mw < avail.y / mh ? avail.x / mw : avail.y / mh;   // pixels per mm
    ImVec2 o = ImGui::GetCursorScreenPos();
    float ox = o.x + (avail.x - mw * k) * 0.5f, oy = o.y + (avail.y - mh * k) * 0.5f;
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
    float r0 = P(Minigraf::STOP_X, 0).x, r1 = P(Minigraf::STOP_X + Minigraf::TRAVEL, 0).x;
    dl->AddRectFilled(ImVec2(r0 - 2 * k, pen.y - 1.2f * k), ImVec2(r1 + 2 * k, pen.y + 1.2f * k), IM_COL32(150, 155, 162, 200), 1.0f);
    dl->AddRectFilled(ImVec2(r0 - 3 * k, pen.y - 3 * k), ImVec2(r0 - 1 * k, pen.y + 3 * k), IM_COL32(110, 110, 116, 230));
    dl->AddRect(ImVec2(pen.x - 4 * k, pen.y - 4 * k), ImVec2(pen.x + 4 * k, pen.y + 4 * k), IM_COL32(60, 62, 70, 230), 1.5f, 0, 2.0f);
    dl->AddCircleFilled(pen, 1.6f * k, g->pen_down() ? IM_COL32(200, 40, 40, 255) : IM_COL32(120, 120, 128, 220));
    dl->PopClipRect();
    ImGui::Dummy(avail);
}

void draw_plotter(UiState& ui)
{
    Machine* m = ui.host->machine();
    // a machine with the plotter opens the window when it starts (once for each machine)
    if (m && ui.plotter_gen != ui.host->machine_generation()) {
        ui.plotter_gen = ui.host->machine_generation();
        for (auto& c : m->cards)
            if (c->minigraf()) ui.show_plotter = true;
    }
    if (!ui.show_plotter) return;
    float u = ImGui::GetFontSize();   // on the right: the screen of SAPI stays in sight
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(30 * u, 42 * u), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 30.5f * u, 2.0f * u), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(tr_id("Minigraf"), &ui.show_plotter) || !m) {
        ImGui::End();
        return;
    }
    Minigraf* g = nullptr;
    for (auto& c : m->cards)
        if (c->minigraf()) g = c->minigraf();
    if (!g) {
        ImGui::TextDisabled("%s", tr("No plotter. Configurator: DPP-1, the plotter ARITMA Minigraf 0507."));
        ImGui::End();
        return;
    }
    if (ImGui::Button(tr("New sheet"))) g->new_sheet();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Clean paper is put in under the pen (the drawing is cleared)."));
    ImGui::SameLine();
    std::string err;
    if (ImGui::Button(tr("Save PNG"))) {
        std::string p = plot_path(".png");
        if (g->save_png(p, 8, err)) plot_notice(ui, true, std::string(tr("Drawing saved")) + ": " + p);
        else plot_notice(ui, false, err);
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Save SVG"))) {
        std::string p = plot_path(".svg");
        if (g->save_svg(p, err)) plot_notice(ui, true, std::string(tr("Drawing saved")) + ": " + p);
        else plot_notice(ui, false, err);
    }
    ImGui::Text("X %.1f  Y %.1f mm   %s   %s %llu", g->x() / (double)Minigraf::STEPS_PER_MM,
                g->y() / (double)Minigraf::STEPS_PER_MM, g->pen_down() ? tr("pen down") : tr("pen up"), tr("steps"),
                (unsigned long long)g->steps);
    if (g->stopped) {
        ImGui::SameLine();
        ImGui::TextDisabled("  %s %llu", tr("at the stop"), (unsigned long long)g->stopped);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr("Steps of the carriage against its stop: the driver sets its origin so."));
    }
    if (g->lost) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "  %s %llu", tr("lost"), (unsigned long long)g->lost);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr("The code skipped a phase: the motor may have lost steps."));
    }
    draw_sheet(g);
    ImGui::End();
}
