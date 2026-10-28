// Window "APS-5": the front panel of the unit (HRK-1: keys, LEDs "porucha" and "napájení", the key
// switch), the mains (an outage of the given length) and the plant: eight furnaces glowing by their
// temperature with their heaters, thermocouples, logic inputs and outputs, and a plot of the
// temperatures (docs/desky/APS-5.md).
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/theme.h"

#include "imgui.h"
#include "cards/hrk1.h"
#include "core/host.h"
#include "core/machine.h"
#include "plant/furnaces.h"
#include "plant/thermocouple.h"

#include <math.h>
#include <stdio.h>

static ImU32 lerp_col(ImU32 a, ImU32 b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int r = (int)(((a >> IM_COL32_R_SHIFT) & 255) * (1 - t) + ((b >> IM_COL32_R_SHIFT) & 255) * t);
    int g = (int)(((a >> IM_COL32_G_SHIFT) & 255) * (1 - t) + ((b >> IM_COL32_G_SHIFT) & 255) * t);
    int bl = (int)(((a >> IM_COL32_B_SHIFT) & 255) * (1 - t) + ((b >> IM_COL32_B_SHIFT) & 255) * t);
    return IM_COL32(r, g, bl, 255);
}

// The glow of hot steel and ceramics (Pouillet's colours): dark below 450 degC, then dark red,
// cherry, orange, yellow, white.
static ImU32 glow(double c)
{
    static const struct { double t; ImU32 col; } k[] = {
        { 20, IM_COL32(52, 48, 46, 255) },    { 450, IM_COL32(62, 40, 34, 255) },
        { 550, IM_COL32(110, 18, 8, 255) },   { 700, IM_COL32(170, 30, 10, 255) },
        { 850, IM_COL32(220, 70, 18, 255) },  { 1000, IM_COL32(250, 130, 30, 255) },
        { 1150, IM_COL32(255, 190, 70, 255) }, { 1300, IM_COL32(255, 230, 150, 255) },
        { 1500, IM_COL32(255, 250, 225, 255) },
    };
    const int n = (int)(sizeof(k) / sizeof(k[0]));
    if (c <= k[0].t) return k[0].col;
    for (int i = 1; i < n; i++)
        if (c < k[i].t) return lerp_col(k[i - 1].col, k[i].col, (float)((c - k[i - 1].t) / (k[i].t - k[i - 1].t)));
    return k[n - 1].col;
}

static const ImU32 k_plot_col[8] = {
    IM_COL32(255, 99, 71, 255),  IM_COL32(255, 200, 40, 255), IM_COL32(110, 200, 90, 255),
    IM_COL32(80, 170, 255, 255), IM_COL32(200, 120, 255, 255), IM_COL32(60, 220, 210, 255),
    IM_COL32(255, 140, 200, 255), IM_COL32(230, 230, 230, 255),
};

static void lamp(ImDrawList* dl, ImVec2 c, float r, bool on, ImU32 col_on)
{
    ImU32 off = lerp_col(col_on, IM_COL32(30, 30, 30, 255), 0.78f);
    if (on) dl->AddCircleFilled(c, r * 1.9f, (col_on & 0x00FFFFFFu) | 0x30000000u);
    dl->AddCircleFilled(c, r, on ? col_on : off);
    dl->AddCircle(c, r, IM_COL32(20, 20, 20, 255), 0, 1.0f);
}

// The front panel: LEDs, the membrane keyboard HRK-1A, the key switch.
static void draw_panel(Hrk1* p, float u)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    float kw = 2.4f * u, gap = 0.55f * u, pad = 0.9f * u;
    float w = 3 * kw + 2 * gap + 2 * pad, h = 25.0f * u;
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), IM_COL32(176, 178, 172, 255), 4.0f);
    dl->AddRect(o, ImVec2(o.x + w, o.y + h), IM_COL32(90, 92, 88, 255), 4.0f, 0, 1.5f);
    ImU32 ink = IM_COL32(30, 30, 30, 255);
    float y = o.y + 0.5f * u;
    panel_text(dl, ImVec2(o.x + pad, y), ink, "porucha");
    panel_text(dl, ImVec2(o.x + w - pad - panel_text_size("napájení").x, y), ink, "napájení");
    lamp(dl, ImVec2(o.x + pad + 1.4f * u, y + 1.9f * u), 0.45f * u, p->powered() && p->led_fault(), IM_COL32(255, 40, 30, 255));
    lamp(dl, ImVec2(o.x + w - pad - 1.6f * u, y + 1.9f * u), 0.45f * u, p->powered(), IM_COL32(60, 230, 60, 255));
    // keys: 7 8 9 / 4 5 6 / 1 2 3 / 0 / <- M -> / R
    static const int rows[6][3] = { { 7, 8, 9 }, { 4, 5, 6 }, { 1, 2, 3 }, { 0, -1, -1 },
                                    { Hrk1::KEY_BACK, Hrk1::KEY_M, Hrk1::KEY_ENTER }, { -1, Hrk1::KEY_R, -1 } };
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(236, 236, 230, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(250, 250, 245, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(190, 190, 184, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(20, 20, 20, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(20, 20, 20, 255));
    float ky = o.y + 3.6f * u;
    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 3; c++) {
            int code = rows[r][c];
            if (code < 0) continue;
            char lbl[24];
            if (code < 10) snprintf(lbl, sizeof(lbl), "%d##k%d", code, code);
            else snprintf(lbl, sizeof(lbl), "%s##k%d", code == Hrk1::KEY_BACK ? "<-" : code == Hrk1::KEY_ENTER ? "->"
                                                        : code == Hrk1::KEY_M ? "M" : "R", code);
            ImGui::SetCursorScreenPos(ImVec2(o.x + pad + c * (kw + gap), ky + r * (kw * 0.86f + gap) + (r >= 4 ? 0.5f * u : 0)));
            if (ImGui::Button(lbl, ImVec2(kw, kw * 0.86f))) p->press((uint8_t)code);
            if (code == Hrk1::KEY_R && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tr("R: reserved commands, only with the key in position 2"));
        }
    }
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar();
    // the key switch: click turns it
    ImVec2 kc(o.x + w * 0.5f, o.y + h - 2.0f * u);
    float kr = 1.15f * u;
    dl->AddCircleFilled(kc, kr, IM_COL32(150, 152, 148, 255));
    dl->AddCircle(kc, kr, IM_COL32(40, 40, 40, 255), 0, 2.0f);
    float a = p->lock == 1 ? -2.2f : -0.9f;
    ImVec2 d(cosf(a) * kr * 0.8f, sinf(a) * kr * 0.8f);
    dl->AddLine(ImVec2(kc.x - d.x, kc.y - d.y), ImVec2(kc.x + d.x, kc.y + d.y), IM_COL32(30, 30, 30, 255), 0.35f * u);
    panel_text(dl, ImVec2(kc.x - kr - 0.9f * u, kc.y - kr - 0.4f * u), ink, "1");
    panel_text(dl, ImVec2(kc.x + kr + 0.4f * u, kc.y - kr - 0.4f * u), ink, "2");
    ImGui::SetCursorScreenPos(ImVec2(kc.x - kr, kc.y - kr));
    if (ImGui::InvisibleButton("##lock", ImVec2(2 * kr, 2 * kr))) p->lock = p->lock == 1 ? 2 : 1;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Key switch: 1 = operator (view, start, stop), 2 = programming (R commands). Click to turn."));
    ImGui::SetCursorScreenPos(o);
    ImGui::Dummy(ImVec2(w, h));
}

// One furnace: a brick box, the chamber glowing by its temperature, the heating coil, the
// thermocouple; its numbers and switches below.
static void draw_furnace(FurnacePlant* pl, Hrk1* panel, int i, float u, float tw)
{
    Furnace& f = pl->furnaces[(size_t)i];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    float bh = 6.2f * u;
    bool heat = ((pl->heaters >> i) & 1) != 0;
    // brick casing and the door
    dl->AddRectFilled(o, ImVec2(o.x + tw, o.y + bh), IM_COL32(120, 64, 46, 255), 3.0f);
    for (float yy = o.y + 0.6f * u; yy < o.y + bh; yy += 0.6f * u)
        dl->AddLine(ImVec2(o.x, yy), ImVec2(o.x + tw, yy), IM_COL32(96, 50, 36, 255), 1.0f);
    float m = 0.8f * u;
    ImVec2 c0(o.x + m, o.y + m * 1.3f), c1(o.x + tw - m, o.y + bh - m);
    dl->AddRectFilled(ImVec2(c0.x - 3, c0.y - 3), ImVec2(c1.x + 3, c1.y + 3), IM_COL32(205, 200, 185, 255), 2.0f);
    dl->AddRectFilled(c0, c1, glow(f.tch), 2.0f);
    // heating coils on both walls: they glow by the element temperature, red edge while switched on
    for (int side = 0; side < 2; side++) {
        float x = side ? c1.x - 0.45f * u : c0.x + 0.45f * u;
        ImU32 col = f.heater_broken ? IM_COL32(60, 60, 60, 255) : glow(f.th > f.tch ? f.th : f.tch);
        for (int k = 0; k < 9; k++) {
            float y0 = c0.y + 0.3f * u + k * (c1.y - c0.y - 0.6f * u) / 9, y1 = y0 + (c1.y - c0.y - 0.6f * u) / 9;
            if (f.heater_broken && k == 4) continue;   // the gap of a blown element
            dl->AddLine(ImVec2(x - 0.25f * u, y0), ImVec2(x + 0.25f * u, y1), col, 0.18f * u);
        }
    }
    // the thermocouple through the roof
    float tx = (c0.x + c1.x) * 0.5f;
    ImU32 tcol = IM_COL32(170, 170, 175, 255);
    if (!f.connected) {
        tcol = IM_COL32(80, 80, 80, 255);
    }
    dl->AddLine(ImVec2(tx, o.y - 0.2f * u), ImVec2(tx, c0.y + 1.8f * u), tcol, 0.16f * u);
    if (f.tc_broken) dl->AddLine(ImVec2(tx - 0.4f * u, o.y + 0.3f * u), ImVec2(tx + 0.4f * u, o.y + 0.7f * u), IM_COL32(255, 60, 40, 255), 2.5f);
    // the temperature on the chamber
    char b[64];
    snprintf(b, sizeof(b), "%.0f °C", f.tch);
    ImFont* font = panel_font();
    float fs = ImGui::GetFontSize() * 1.45f;
    ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0, b);
    ImVec2 tp(tx - ts.x * 0.5f, (c0.y + c1.y) * 0.5f - ts.y * 0.1f);
    dl->AddRectFilled(ImVec2(tp.x - 4, tp.y - 2), ImVec2(tp.x + ts.x + 4, tp.y + ts.y + 2), IM_COL32(0, 0, 0, 150), 3.0f);
    dl->AddText(font, fs, tp, IM_COL32(255, 255, 255, 255), b);
    // name and the lamps (heater = DRV-1A contact, LO1, LO2) on the casing
    panel_text(dl, ImVec2(o.x + 0.3f * u, o.y + 0.05f * u), IM_COL32(255, 235, 210, 255), f.name.c_str());
    lamp(dl, ImVec2(o.x + tw - 0.6f * u, o.y + 0.55f * u), 0.32f * u, heat && !f.heater_broken, IM_COL32(255, 150, 20, 255));
    ImGui::Dummy(ImVec2(tw, bh));
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s: %s %.1f °C, %s %.1f °C", f.name.c_str(), tr("chamber"), f.tch, tr("heater"), f.th);
        ImGui::Text("%s %s, %.0f W, %s %.0f °C, %s %.0f min", tr("thermocouple"), tc_name(f.tc), f.power_w, tr("full power"),
                    f.t_max, tr("time constant"), f.tau_s / 60);
        ImGui::Text("%s: %s", tr("Heating (DRV-1A contact)"), heat ? tr("on") : tr("off"));
        ImGui::EndTooltip();
    }
    // logic outputs of the firmware and the switches of the furnace
    bool lo1 = ((pl->lo[0] >> i) & 1) != 0, lo2 = ((pl->lo[1] >> i) & 1) != 0;
    if (panel) {   // the latches on JPR-1 X2/X3 (the plant gets them only through HRK-1)
        lo1 = ((panel->lo[0] >> i) & 1) != 0;
        lo2 = ((panel->lo[1] >> i) & 1) != 0;
    }
    ImVec2 lp = ImGui::GetCursorScreenPos();
    lamp(dl, ImVec2(lp.x + 0.45f * u, lp.y + 0.55f * u), 0.3f * u, lo1, IM_COL32(255, 60, 40, 255));
    dl->AddText(ImVec2(lp.x + 0.95f * u, lp.y), ImGui::GetColorU32(ImGuiCol_Text), "LO1");
    lamp(dl, ImVec2(lp.x + 3.4f * u, lp.y + 0.55f * u), 0.3f * u, lo2, IM_COL32(255, 60, 40, 255));
    dl->AddText(ImVec2(lp.x + 3.9f * u, lp.y), ImGui::GetColorU32(ImGuiCol_Text), "LO2");
    ImGui::Dummy(ImVec2(tw, 1.1f * u));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Logic outputs LO n1 (fault of the measurement), LO n2 (held at L)"));
    bool la = !f.li[0], lb = !f.li[1];
    if (ImGui::Checkbox("LI a##a", &la)) f.li[0] = !la;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Logic input closed = L (open = H, as the cold start expects)"));
    ImGui::SameLine();
    if (ImGui::Checkbox("LI b##b", &lb)) f.li[1] = !lb;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Logic input closed = L (open = H, as the cold start expects)"));
    ImGui::Checkbox(tr("TC broken"), &f.tc_broken);
    ImGui::Checkbox(tr("Heater broken"), &f.heater_broken);
}

// The temperatures of the chambers over the chosen span.
static void draw_plot(FurnacePlant* pl, UiState& ui, float u)
{
    static const int spans[3] = { 600, 3600, 14400 };
    static const char* const names[3] = { "10 min", "1 h", "4 h" };
    ImGui::TextUnformatted(tr("Temperatures"));
    for (int s = 0; s < 3; s++) {
        ImGui::SameLine();
        ImGui::RadioButton(names[s], &ui.aps5_span, s);
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x, h = 11.0f * u;
    if (w < 10 * u) w = 10 * u;
    float lm = 3.6f * u, bm = 1.4f * u;
    ImVec2 p0(o.x + lm, o.y + 0.4f * u), p1(o.x + w - 0.4f * u, o.y + h - bm);
    dl->AddRectFilled(p0, p1, IM_COL32(18, 20, 24, 255));
    int n = spans[ui.aps5_span] / FurnacePlant::HIST_STEP;
    int have = pl->hist_count < n ? pl->hist_count : n;
    float tmax = 200;
    for (size_t i = 0; i < pl->furnaces.size() && i < 8; i++) {
        if (pl->furnaces[i].tch > tmax) tmax = (float)pl->furnaces[i].tch;
        for (int k = 0; k < have; k++) {
            int idx = (pl->hist_pos - 1 - k + FurnacePlant::HIST_LEN) % FurnacePlant::HIST_LEN;
            if (pl->hist[i][idx] > tmax) tmax = pl->hist[i][idx];
        }
    }
    tmax = ceilf(tmax / 200.0f) * 200.0f;
    ImU32 grid = IM_COL32(60, 64, 70, 255), txt = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    for (float t = 0; t <= tmax + 1; t += (tmax > 1000 ? 400.0f : 200.0f)) {
        float y = p1.y - (p1.y - p0.y) * t / tmax;
        dl->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), grid);
        char b[16];
        snprintf(b, sizeof(b), "%.0f", t);
        dl->AddText(ImVec2(o.x + lm - ImGui::CalcTextSize(b).x - 0.3f * u, y - 0.5f * u), txt, b);
    }
    for (int q = 0; q <= 4; q++) {   // time marks: minutes ago
        float x = p0.x + (p1.x - p0.x) * q / 4;
        dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), grid);
        char b[24];
        int ago = spans[ui.aps5_span] * (4 - q) / 4 / 60;
        if (q == 4) snprintf(b, sizeof(b), "%s", tr("now"));
        else snprintf(b, sizeof(b), "-%d min", ago);
        float tx = x - ImGui::CalcTextSize(b).x * (q == 4 ? 1.0f : 0.5f);
        dl->AddText(ImVec2(tx, p1.y + 0.15f * u), txt, b);
    }
    dl->PushClipRect(p0, p1, true);
    for (size_t i = 0; i < pl->furnaces.size() && i < 8; i++) {
        ImVec2 prev;
        bool first = true;
        for (int k = have - 1; k >= -1; k--) {   // oldest to newest, then the value now
            float v = k >= 0 ? pl->hist[i][(pl->hist_pos - 1 - k + FurnacePlant::HIST_LEN) % FurnacePlant::HIST_LEN]
                             : (float)pl->furnaces[i].tch;
            float x = p1.x - (p1.x - p0.x) * (float)((k + 1) * FurnacePlant::HIST_STEP) / spans[ui.aps5_span];
            ImVec2 pt(x, p1.y - (p1.y - p0.y) * v / tmax);
            if (!first) dl->AddLine(prev, pt, k_plot_col[i], 1.6f);
            prev = pt;
            first = false;
        }
    }
    dl->PopClipRect();
    ImGui::Dummy(ImVec2(w, h));
    for (size_t i = 0; i < pl->furnaces.size() && i < 8; i++) {   // legend: the stroke of the line, the name
        if (i) ImGui::SameLine(0, ImGui::GetFontSize());
        ImVec2 p = ImGui::GetCursorScreenPos();
        float u = ImGui::GetFontSize(), y = p.y + ImGui::GetTextLineHeight() * 0.5f;
        dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + 1.2f * u, y), k_plot_col[i], 0.25f * u);
        ImGui::Dummy(ImVec2(1.4f * u, ImGui::GetTextLineHeight()));
        ImGui::SameLine(0, 0);
        ImGui::TextUnformatted(pl->furnaces[i].name.c_str());
    }
}

// The emulation back to the speed before the demo.
static void demo_speed_back(UiState& ui)
{
    if (!ui.aps5_demo_fast) return;
    ui.host->max_speed = ui.aps5_demo_was_max;
    ui.aps5_demo_fast = false;
}

// The demo goes on every frame (also with the window closed); it stops with its machine.
static void demo_poll(UiState& ui, Machine* m)
{
    Aps5Demo& d = ui.aps5_demo;
    if (!d.running()) {
        demo_speed_back(ui);
        return;
    }
    if (!m || ui.aps5_demo_gen != ui.host->machine_generation()) {
        d.stop();
        demo_speed_back(ui);
        return;
    }
    d.poll(*m);
    if (!d.running()) demo_speed_back(ui);
}

static void demo_start(UiState& ui, Machine* m)
{
    std::string err;
    if (!m || !ui.aps5_demo.start(*m, err)) return;
    ui.aps5_demo_gen = ui.host->machine_generation();
    if (!ui.aps5_demo_fast) {
        ui.aps5_demo_was_max = ui.host->max_speed;
        ui.aps5_demo_fast = true;
    }
    ui.host->max_speed = true;
}

static void demo_controls(UiState& ui, Machine* m, float u)
{
    if (ui.aps5_demo_autostart && m && m->display) {   // --ui-aps5-demo
        ui.aps5_demo_autostart = false;
        demo_start(ui, m);
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 10.1f * u);   // as wide as the panel
    Aps5Demo& d = ui.aps5_demo;
    if (!d.running()) {
        if (theme_accent_button(tr("DEMO"), ImVec2(5.5f * u, 0))) ImGui::OpenPopup("##aps5demo");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr("Types the example of docs/aps5-ukazka.md on the panel: calibration, program 1, "
                                       "furnaces 1, 2, 5 regulated to 700 degC, 4 and 6 by hand."));
        if (d.phase == Aps5Demo::DONE) {
            ImGui::SameLine();
            ImGui::TextColored(theme().ok, "%s", tr("Demo done"));
        } else if (d.phase == Aps5Demo::FAILED) {
            ImGui::TextColored(theme().error, "%s %s", tr("Demo stopped:"), tr(d.error.c_str()));
        }
    } else {
        if (ImGui::Button(tr("Stop the demo"))) {
            d.stop();
            demo_speed_back(ui);
        }
        ImVec4 c = theme().warn;
        if (d.phase == Aps5Demo::TYPING)
            ImGui::TextColored(c, "%s %d/%d: %s", tr("DEMO"), d.step + 1, d.steps, tr(d.label));
        else
            ImGui::TextColored(c, "%s %.0f min", tr("DEMO: the furnaces heat up,"), ceil(d.heat_left / 60));
        if (ui.aps5_demo_fast) ImGui::TextColored(c, "%s", tr("Emulation at full speed (F8 changes it)"));
    }
    ImGui::PopTextWrapPos();
    if (ImGui::BeginPopup("##aps5demo")) {
        ImGui::TextUnformatted(tr("The demo types on the panel like an operator:"));
        ImGui::BulletText("%s", tr("calibration, program 1 (20 min to 700 degC, 20 min hold, 20 min to 300 degC)"));
        ImGui::BulletText("%s", tr("furnaces 1, 2 and 5 regulated by it, furnaces 4 and 6 by hand (70 %, 50 %)"));
        ImGui::BulletText("%s", tr("then 25 minutes of heating"));
        ImGui::TextUnformatted(tr("It overwrites program 1 and these furnaces and runs the emulation at full speed."));
        if (ImGui::Button(tr("Start"))) {
            demo_start(ui, m);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr("Cancel"))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void draw_aps5(UiState& ui)
{
    Machine* m = ui.host->machine();
    demo_poll(ui, m);
    FurnacePlant* pl = m ? dynamic_cast<FurnacePlant*>(m->plant.get()) : nullptr;
    Hrk1* panel = m ? dynamic_cast<Hrk1*>(m->keyboard()) : nullptr;
    // a machine with the plant opens the window when it starts (once for each machine)
    if (pl && ui.aps5_gen != ui.host->machine_generation()) {
        ui.aps5_gen = ui.host->machine_generation();
        ui.show_aps5 = true;
    }
    if (!ui.show_aps5) return;
    float u = ImGui::GetFontSize();
    ImGui::SetNextWindowSize(ImVec2(62 * u, 44 * u), ImGuiCond_FirstUseEver);
    ImGuiIO& io = ImGui::GetIO();   // on the right: the screen of the unit (AND-1) stays in sight
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 62 * u - 0.5f * u, 2.0f * u), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(tr_id("APS-5: panel and furnaces"), &ui.show_aps5)) {
        ImGui::End();
        return;
    }
    if (!pl && !panel) {
        ImGui::TextWrapped("%s", tr("No APS-5. Open machines/aps5.sapi (the plant of the machine and the HRK-1 panel)."));
        ImGui::End();
        return;
    }
    if (panel) {
        ImGui::BeginGroup();
        draw_panel(panel, u);
        ImGui::PushItemWidth(5.5f * u);
        double left = panel->outage_left();
        if (left > 0) {
            ImGui::TextColored(theme().warn, "%s %.0f s", tr("No power,"), left);
        } else {
            if (ImGui::Button(tr("Mains failure"))) panel->outage(ui.aps5_outage_s);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tr("The supply warns the program (/PFAIL), the unit is without power for the given time, "
                                           "then starts again; the batteries keep CRM-1 and RTC-1."));
            ImGui::SameLine();
            ImGui::DragFloat("##outage", &ui.aps5_outage_s, 1.0f, 1.0f, 36000.0f, "%.0f s");
        }
        demo_controls(ui, m, u);
        if (pl) {
            float bat = (float)pl->battery;
            if (ImGui::SliderFloat(tr("Battery"), &bat, 3.0f, 5.0f, "%.2f V")) pl->battery = bat;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Flat battery 4.5 V in the front panel; below 4.1 V the unit warns"));
            float amb = (float)pl->ambient, blk = (float)pl->block_c;
            if (ImGui::DragFloat(tr("Surroundings"), &amb, 0.2f, -20.0f, 50.0f, "%.1f °C")) pl->ambient = amb;
            if (ImGui::DragFloat(tr("Terminal block"), &blk, 0.2f, 0.0f, 50.0f, "%.1f °C")) pl->block_c = blk;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The isothermal terminal block (cold junctions, the thermometer of DMT-1)"));
            if (ImGui::Button(tr("Cool all furnaces"))) pl->reset_process();
        }
        ImGui::PopItemWidth();
        ImGui::EndGroup();
        ImGui::SameLine();
    }
    if (pl) {
        ImGui::BeginGroup();
        float avail = ImGui::GetContentRegionAvail().x;
        float sp = ImGui::GetStyle().ItemSpacing.x;
        int cols = pl->furnaces.size() > 4 ? 4 : (int)pl->furnaces.size();
        float tw = (avail - sp * (cols - 1)) / cols;
        if (tw < 8 * u) tw = 8 * u;
        for (size_t i = 0; i < pl->furnaces.size(); i++) {
            if (i % 4) ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::PushID((int)i);
            ImGui::PushItemWidth(tw);
            draw_furnace(pl, panel, (int)i, u, tw);
            ImGui::PopItemWidth();
            ImGui::PopID();
            ImGui::EndGroup();
        }
        draw_plot(pl, ui, u);
        ImGui::EndGroup();
    }
    ImGui::End();
}
