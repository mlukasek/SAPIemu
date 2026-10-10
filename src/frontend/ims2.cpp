// Window "IMS-2 measuring bench": the instruments on the IMS-2 bus of the DPS-1 card (plant "IMS-2",
// docs/navod-dps1.md). The voltmeter Metra M1T 330 drawn after photos of the real one (red display with
// mV / V / %, AUTO, FILTR, START REP / 1x, CHYBA, TEST and EXT lamps, terminals G, L, H), the power supply
// Tesla BS 525 (two outputs: meter with the volt and ampere scales, voltage and current limit knobs, the
// switch "Mereni", the protection lamp, terminals; the mains switch), the wires between them and a monitor
// of the bus: the lines and the last transfers (addressing with ATN, the texts).
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "imgui.h"
#include "cards/dps1.h"
#include "core/host.h"
#include "core/ims2.h"
#include "core/machine.h"
#include "plant/ims2_bench.h"

static const float K_PI = 3.14159265f;
static const ImU32 K_LED_RED = IM_COL32(255, 60, 40, 255);
static const ImU32 K_LED_OFF = IM_COL32(70, 30, 26, 255);
static const ImU32 K_LABEL = IM_COL32(230, 200, 120, 255);   // the yellowish print of the M1T 330 panel

// ---------------------------------------------------------------- drawing helpers

// a 7 segment digit: segments a b c d e f g (bits 0-6)
static uint8_t seg_of(char c)
{
    static const uint8_t digits[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
    if (c >= '0' && c <= '9') return digits[c - '0'];
    switch (c) {
    case '-': return 0x40;
    case 'O': return 0x3F;
    case 'L': return 0x38;
    default: return 0;
    }
}

static void draw_digit(ImDrawList* dl, ImVec2 p, float h, uint8_t seg, bool dp, ImU32 on, ImU32 off)
{
    float w = h * 0.55f, t = h * 0.11f, sl = h * 0.08f;   // width, thickness, slant
    ImVec2 a(p.x + sl, p.y), b(p.x + w + sl, p.y), m0(p.x + sl * 0.5f, p.y + h * 0.5f),
        m1(p.x + w + sl * 0.5f, p.y + h * 0.5f), c(p.x, p.y + h), d(p.x + w, p.y + h);
    auto bar = [&](ImVec2 s, ImVec2 e, int bit) {
        dl->AddLine(ImVec2(s.x + (e.x - s.x) * 0.1f, s.y + (e.y - s.y) * 0.1f),
                    ImVec2(s.x + (e.x - s.x) * 0.9f, s.y + (e.y - s.y) * 0.9f), (seg >> bit) & 1 ? on : off, t);
    };
    bar(a, b, 0);     // a
    bar(b, m1, 1);    // b
    bar(m1, d, 2);    // c
    bar(c, d, 3);     // d
    bar(m0, c, 4);    // e
    bar(a, m0, 5);    // f
    bar(m0, m1, 6);   // g
    dl->AddCircleFilled(ImVec2(d.x + t * 1.4f, d.y - t * 0.3f), t * 0.6f, dp ? on : off);
}

static void lamp(ImDrawList* dl, ImVec2 c, float r, bool lit, ImU32 col = K_LED_RED)
{
    dl->AddCircleFilled(c, r, lit ? col : K_LED_OFF, 16);
    if (lit) dl->AddCircleFilled(c, r * 2.0f, (col & 0x00FFFFFF) | 0x30000000, 16);
    dl->AddCircle(c, r, IM_COL32(20, 20, 20, 255), 16, 1.0f);
}

static void text_center(ImDrawList* dl, ImVec2 c, ImU32 col, const char* s, float scale = 1.0f)
{
    ImFont* f = ImGui::GetFont();
    float fs = ImGui::GetFontSize() * scale;
    ImVec2 sz = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, s);
    dl->AddText(f, fs, ImVec2(c.x - sz.x * 0.5f, c.y - sz.y * 0.5f), col, s);
}

// a push button of the panel; true when clicked
static bool panel_button(const char* id, ImVec2 p0, ImVec2 p1, const char* tip)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(p0);
    bool clicked = ImGui::InvisibleButton(id, ImVec2(p1.x - p0.x, p1.y - p0.y));
    bool held = ImGui::IsItemActive();
    float r = (p1.y - p0.y) * 0.35f;
    dl->AddRectFilled(ImVec2(p0.x, p0.y + 2), ImVec2(p1.x, p1.y + 2), IM_COL32(10, 10, 10, 255), r);
    dl->AddRectFilled(ImVec2(p0.x, p0.y + (held ? 2.0f : 0.0f)), ImVec2(p1.x, p1.y + (held ? 2.0f : 0.0f)),
                      ImGui::IsItemHovered() ? IM_COL32(58, 58, 60, 255) : IM_COL32(40, 40, 42, 255), r);
    if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    return clicked;
}

// a knob: drag up/right (Shift fine) or the wheel; returns true when the value changed
static bool knob(const char* id, ImVec2 c, float r, double* v, double lo, double hi, double step, const char* fmt,
                 const char* label)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetCursorScreenPos(ImVec2(c.x - r, c.y - r));
    ImGui::InvisibleButton(id, ImVec2(2 * r, 2 * r));
    bool changed = false;
    double k = io.KeyShift ? 0.05 : 1.0;
    if (ImGui::IsItemActive() && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
        *v += (io.MouseDelta.x - io.MouseDelta.y) * step * k;
        changed = true;
    }
    if (ImGui::IsItemHovered() && io.MouseWheel != 0) {
        *v += io.MouseWheel * step * 10 * k;
        changed = true;
    }
    if (*v < lo) *v = lo;
    if (*v > hi) *v = hi;
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        char s[64];
        snprintf(s, sizeof s, fmt, *v);
        ImGui::SetTooltip("%s: %s\n%s", label, s, tr("Drag or turn the wheel; Shift = fine."));
    }
    double f = (*v - lo) / (hi - lo);
    float ang = (float)((-225.0 + 270.0 * f) * K_PI / 180.0);
    dl->AddCircleFilled(ImVec2(c.x + 1, c.y + 2), r, IM_COL32(15, 15, 15, 255), 24);
    dl->AddCircleFilled(c, r, IM_COL32(35, 35, 38, 255), 24);
    dl->AddCircle(c, r * 0.82f, IM_COL32(70, 70, 74, 255), 24, 1.5f);
    dl->AddLine(ImVec2(c.x + cosf(ang) * r * 0.25f, c.y + sinf(ang) * r * 0.25f),
                ImVec2(c.x + cosf(ang) * r * 0.85f, c.y + sinf(ang) * r * 0.85f), IM_COL32(235, 235, 235, 255), 2.5f);
    for (int i = 0; i <= 10; i++) {   // the dial around the knob
        float a = (float)((-225.0 + 27.0 * i) * K_PI / 180.0);
        dl->AddLine(ImVec2(c.x + cosf(a) * r * 1.12f, c.y + sinf(a) * r * 1.12f),
                    ImVec2(c.x + cosf(a) * r * 1.25f, c.y + sinf(a) * r * 1.25f), IM_COL32(200, 200, 200, 200), 1.0f);
    }
    return changed;
}

static void terminal(ImDrawList* dl, ImVec2 c, float r, ImU32 col, const char* label, ImU32 label_col)
{
    dl->AddCircleFilled(c, r, col, 20);
    dl->AddCircleFilled(c, r * 0.45f, IM_COL32(25, 25, 25, 255), 16);
    dl->AddCircle(c, r, IM_COL32(10, 10, 10, 255), 20, 1.5f);
    if (label) text_center(dl, ImVec2(c.x, c.y - r - ImGui::GetFontSize() * 0.6f), label_col, label, 0.85f);
}

// ---------------------------------------------------------------- the voltmeter

struct M1tTerms {
    ImVec2 g, l, h;
};

static M1tTerms draw_m1t330(Ims2Bench* b, uint64_t now, ImVec2 o, float w)
{
    M1t330& v = b->m1t;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float hgt = w * 0.30f, u = ImGui::GetFontSize();
    // the case and the dark front
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + hgt), IM_COL32(205, 196, 170, 255), 10.0f);
    ImVec2 f0(o.x + w * 0.025f, o.y + hgt * 0.06f), f1(o.x + w * 0.975f, o.y + hgt * 0.94f);
    dl->AddRectFilled(f0, f1, IM_COL32(36, 36, 38, 255), 4.0f);
    float fw = f1.x - f0.x, fh = f1.y - f0.y;
    // the display: six digits, the first is the sign
    ImVec2 d0(f0.x + fw * 0.08f, f0.y + fh * 0.10f), d1(f0.x + fw * 0.47f, f0.y + fh * 0.42f);
    dl->AddRectFilled(d0, d1, IM_COL32(12, 4, 4, 255), 3.0f);
    dl->AddRect(d0, d1, IM_COL32(110, 110, 110, 255), 3.0f, 0, 1.5f);
    std::string s = v.display();
    // split into characters and decimal points
    char ch[8] = { ' ', ' ', ' ', ' ', ' ', ' ', 0 };
    bool dp[8] = {};
    {
        std::vector<char> digs;
        std::vector<bool> dps;
        for (char c : s) {
            if (c == '.') {
                if (!dps.empty()) dps.back() = true;
                else { digs.push_back(' '); dps.push_back(true); }
            } else {
                digs.push_back(c);
                dps.push_back(false);
            }
        }
        int n = (int)digs.size();
        for (int i = 0; i < n && i < 6; i++) {   // right aligned in six places
            ch[6 - n + i] = digs[i];
            dp[6 - n + i] = dps[i];
        }
    }
    float dh = (d1.y - d0.y) * 0.62f, step = (d1.x - d0.x) / 6.3f;
    for (int i = 0; i < 6; i++)
        draw_digit(dl, ImVec2(d0.x + step * 0.25f + i * step, d0.y + (d1.y - d0.y) * 0.19f), dh, seg_of(ch[i]), dp[i],
                   IM_COL32(255, 40, 30, 255), IM_COL32(45, 10, 8, 255));
    // units
    const char* units[3] = { "mV", "V", "%" };
    bool ulit[3] = { v.unit_mv(), v.unit_v(), v.unit_pct() };
    for (int i = 0; i < 3; i++) {
        ImVec2 c(d1.x + fw * 0.03f, d0.y + (d1.y - d0.y) * (0.18f + 0.32f * i));
        lamp(dl, c, u * 0.22f, ulit[i]);
        dl->AddText(ImVec2(c.x + u * 0.4f, c.y - u * 0.45f), IM_COL32(210, 210, 210, 255), units[i]);
    }
    // the badge of Metra
    ImVec2 b0(f0.x + fw * 0.60f, f0.y + fh * 0.08f), b1(f0.x + fw * 0.73f, f0.y + fh * 0.44f);
    dl->AddRect(b0, b1, IM_COL32(200, 200, 200, 255), 2.0f, 0, 1.2f);
    text_center(dl, ImVec2((b0.x + b1.x) * 0.5f, b0.y + (b1.y - b0.y) * 0.18f), IM_COL32(220, 220, 220, 255), "VOLTMETR", 0.6f);
    text_center(dl, ImVec2((b0.x + b1.x) * 0.5f, b0.y + (b1.y - b0.y) * 0.38f), IM_COL32(220, 220, 220, 255), "M1T 330", 0.7f);
    ImVec2 lc((b0.x + b1.x) * 0.5f, b0.y + (b1.y - b0.y) * 0.72f);
    dl->AddCircle(lc, (b1.y - b0.y) * 0.2f, IM_COL32(220, 220, 220, 255), 20, 1.5f);
    text_center(dl, lc, IM_COL32(220, 220, 220, 255), "M", 0.7f);
    // TEST, EXT
    ImVec2 t0(f0.x + fw * 0.85f, f0.y + fh * 0.17f);
    dl->AddText(ImVec2(t0.x - u * 2.1f, t0.y - u * 0.45f), IM_COL32(210, 210, 210, 255), "TEST");
    lamp(dl, ImVec2(t0.x + u * 0.5f, t0.y), u * 0.25f, false);
    dl->AddText(ImVec2(t0.x - u * 2.1f, t0.y + fh * 0.18f - u * 0.45f), IM_COL32(210, 210, 210, 255), "EXT.");
    lamp(dl, ImVec2(t0.x + u * 0.5f, t0.y + fh * 0.18f), u * 0.25f, v.power && v.remote);
    if (ImGui::IsMouseHoveringRect(ImVec2(t0.x - u * 2.2f, t0.y + fh * 0.1f), ImVec2(t0.x + u, t0.y + fh * 0.26f)))
        ImGui::SetTooltip("%s", tr("EXT: remote control from the IMS-2 bus (REN and addressed)."));
    // the buttons
    float by0 = f0.y + fh * 0.66f, by1 = f0.y + fh * 0.86f, bw = fw * 0.075f;
    struct Btn {
        float x;
        const char* label;
        const char* id;
    };
    float x = f0.x + fw * 0.05f;
    // SIT
    dl->AddText(ImVec2(x + bw * 0.2f, by0 - u * 1.9f), K_LABEL, "SÍŤ");
    dl->AddText(ImVec2(x, by0 - u * 1.05f), K_LABEL, "0\xe2\x96\xa1 I\xe2\x96\xa0");
    if (panel_button("##m1t_power", ImVec2(x, by0), ImVec2(x + bw, by1), tr("Mains switch"))) b->set_power_m1t(!v.power);
    x += bw * 1.35f;
    dl->AddText(ImVec2(x + bw * 0.12f, by0 - u * 1.9f), K_LABEL, "AUTO");
    lamp(dl, ImVec2(x + bw * 0.5f, by0 - u * 0.55f), u * 0.22f, v.power && v.autorange);
    if (panel_button("##m1t_auto", ImVec2(x, by0), ImVec2(x + bw, by1), tr("Autorange (R4); local only"))) v.key_auto(now);
    x += bw * 1.35f;
    dl->AddText(ImVec2(x + bw * 0.12f, by0 - u * 1.9f), K_LABEL, "FILTR");
    lamp(dl, ImVec2(x + bw * 0.5f, by0 - u * 0.55f), u * 0.22f, v.power && v.filter);
    if (panel_button("##m1t_filter", ImVec2(x, by0), ImVec2(x + bw, by1), tr("Digital filter (D1); local only")))
        v.key_filter(now);
    x += bw * 1.35f;
    dl->AddText(ImVec2(x + bw * 0.8f, by0 - u * 1.9f), K_LABEL, "START");
    dl->AddText(ImVec2(x + bw * 0.1f, by0 - u * 1.05f), K_LABEL, "REP");
    dl->AddText(ImVec2(x + bw * 1.6f, by0 - u * 1.05f), K_LABEL, "1x");
    lamp(dl, ImVec2(x + bw * 1.17f, by0 - u * 0.55f), u * 0.22f, v.power && v.repeat && !v.remote);
    if (panel_button("##m1t_rep", ImVec2(x, by0), ImVec2(x + bw, by1), tr("Measure again and again; local only")))
        v.key_rep(now);
    if (panel_button("##m1t_one", ImVec2(x + bw * 1.35f, by0), ImVec2(x + bw * 2.35f, by1), tr("One measurement; local only")))
        v.key_single(now);
    x += bw * 2.7f;
    dl->AddText(ImVec2(x + bw * 0.02f, by0 - u * 1.9f), K_LABEL, "CHYBA");
    if (panel_button("##m1t_err", ImVec2(x, by0), ImVec2(x + bw, by1), tr("The display shows the error of the value in %")))
        v.key_error();
    // the terminals
    ImVec2 g0(f0.x + fw * 0.73f, f0.y + fh * 0.55f), g1(f0.x + fw * 0.985f, f0.y + fh * 0.95f);
    dl->AddRect(g0, g1, IM_COL32(90, 90, 90, 255), 3.0f, 0, 1.2f);
    float ty = (g0.y + g1.y) * 0.5f + u * 0.3f, tr_ = u * 0.55f;
    M1tTerms t;
    t.g = ImVec2(g0.x + (g1.x - g0.x) * 0.18f, ty);
    t.l = ImVec2(g0.x + (g1.x - g0.x) * 0.50f, ty);
    t.h = ImVec2(g0.x + (g1.x - g0.x) * 0.82f, ty);
    terminal(dl, t.g, tr_, IM_COL32(30, 30, 30, 255), "G", K_LABEL);
    terminal(dl, t.l, tr_, IM_COL32(30, 30, 30, 255), "L", K_LABEL);
    terminal(dl, t.h, tr_, IM_COL32(190, 30, 30, 255), "H", K_LABEL);
    ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + hgt));
    ImGui::Dummy(ImVec2(w, u * 0.3f));
    return t;
}

// ---------------------------------------------------------------- the power supply

struct BsTerms {
    ImVec2 plus[2], minus[2];
};

static void meter(ImDrawList* dl, ImVec2 p0, ImVec2 p1, const Bs525Output& o, bool power)
{
    dl->AddRectFilled(p0, p1, IM_COL32(25, 25, 25, 255), 3.0f);
    ImVec2 f0(p0.x + 3, p0.y + 3), f1(p1.x - 3, p1.y - 3);
    dl->AddRectFilled(f0, f1, IM_COL32(240, 236, 222, 255), 2.0f);
    ImVec2 pivot((f0.x + f1.x) * 0.5f, f1.y - (f1.y - f0.y) * 0.08f);
    float r = (f1.y - f0.y) * 0.80f, u = ImGui::GetFontSize();
    // the scales: volts above (0-30), amperes below (0-1)
    for (int i = 0; i <= 30; i++) {
        float a = (float)((-135.0 + 90.0 * i / 30.0) * K_PI / 180.0);
        float l = i % 5 == 0 ? 0.12f : 0.06f;
        ImVec2 q0(pivot.x + cosf(a) * r, pivot.y + sinf(a) * r), q1(pivot.x + cosf(a) * r * (1 - l), pivot.y + sinf(a) * r * (1 - l));
        dl->AddLine(q0, q1, IM_COL32(20, 20, 20, 255), 1.0f);
        if (i % 10 == 0) {
            char s[8];
            snprintf(s, sizeof s, "%d", i);
            ImVec2 tq(pivot.x + cosf(a) * r * 1.13f, pivot.y + sinf(a) * r * 1.13f);
            text_center(dl, tq, IM_COL32(20, 20, 20, 255), s, 0.55f);
        }
    }
    for (int i = 0; i <= 10; i++) {
        float a = (float)((-135.0 + 90.0 * i / 10.0) * K_PI / 180.0);
        ImVec2 q0(pivot.x + cosf(a) * r * 0.70f, pivot.y + sinf(a) * r * 0.70f),
            q1(pivot.x + cosf(a) * r * 0.64f, pivot.y + sinf(a) * r * 0.64f);
        dl->AddLine(q0, q1, IM_COL32(150, 20, 20, 255), 1.0f);
        if (i % 5 == 0) {
            char s[8];
            snprintf(s, sizeof s, i == 10 ? "1" : "0.%d", i);
            if (i == 0) snprintf(s, sizeof s, "0");
            ImVec2 tq(pivot.x + cosf(a) * r * 0.55f, pivot.y + sinf(a) * r * 0.55f);
            text_center(dl, tq, IM_COL32(150, 20, 20, 255), s, 0.5f);
        }
    }
    text_center(dl, ImVec2(f0.x + u * 0.8f, f1.y - u * 0.5f), IM_COL32(20, 20, 20, 255), o.meter_a ? "A" : "V", 0.7f);
    double f = power ? (o.meter_a ? o.i / 1.0 : o.u / 30.0) : 0;
    if (f > 1.05) f = 1.05;
    float a = (float)((-135.0 + 90.0 * f) * K_PI / 180.0);
    dl->AddLine(pivot, ImVec2(pivot.x + cosf(a) * r * 1.02f, pivot.y + sinf(a) * r * 1.02f), IM_COL32(10, 10, 10, 255), 1.6f);
    dl->AddCircleFilled(pivot, u * 0.2f, IM_COL32(30, 30, 30, 255));
}

static BsTerms draw_bs525(Ims2Bench* b, ImVec2 o, float w, bool* changed)
{
    Bs525& bs = b->bs;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float hgt = w * 0.36f, u = ImGui::GetFontSize();
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + hgt), IM_COL32(120, 124, 128, 255), 6.0f);
    ImVec2 f0(o.x + w * 0.02f, o.y + hgt * 0.04f), f1(o.x + w * 0.98f, o.y + hgt * 0.96f);
    dl->AddRectFilled(f0, f1, IM_COL32(178, 182, 184, 255), 4.0f);
    float fw = f1.x - f0.x, fh = f1.y - f0.y;
    text_center(dl, ImVec2(f0.x + fw * 0.5f, f0.y + u * 0.8f), IM_COL32(20, 20, 20, 255), "TESLA  BS 525");
    text_center(dl, ImVec2(f0.x + fw * 0.5f, f0.y + u * 1.8f), IM_COL32(60, 60, 60, 255),
                tr("Stabilized power supply 2 x 30 V / 1 A"), 0.8f);
    BsTerms t;
    for (int k = 0; k < 2; k++) {
        Bs525Output& out = bs.out[k];
        float x0 = f0.x + fw * (k == 0 ? 0.03f : 0.56f), x1 = x0 + fw * 0.41f;
        ImGui::PushID(k);
        text_center(dl, ImVec2(x0 + fw * 0.14f, f0.y + fh * 0.17f), IM_COL32(20, 20, 20, 255), k == 0 ? "I" : "II", 1.0f);
        meter(dl, ImVec2(x0 + fw * 0.03f, f0.y + fh * 0.22f), ImVec2(x0 + fw * 0.25f, f0.y + fh * 0.56f), out, bs.power);
        // the switch "Mereni"
        ImVec2 s0(x0 + fw * 0.29f, f0.y + fh * 0.26f), s1(x0 + fw * 0.38f, f0.y + fh * 0.36f);
        dl->AddText(ImVec2(s0.x, s0.y - u * 1.0f), IM_COL32(20, 20, 20, 255), "MĚŘENÍ");
        ImGui::SetCursorScreenPos(s0);
        if (ImGui::InvisibleButton("##meas", ImVec2(s1.x - s0.x, s1.y - s0.y))) out.meter_a = !out.meter_a;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The meter shows the voltage (V) or the current (A)."));
        dl->AddRectFilled(s0, s1, IM_COL32(40, 40, 40, 255), 3.0f);
        float sx = out.meter_a ? s1.x - (s1.x - s0.x) * 0.3f : s0.x + (s1.x - s0.x) * 0.3f;
        dl->AddRectFilled(ImVec2(sx - u * 0.25f, s0.y + 2), ImVec2(sx + u * 0.25f, s1.y - 2), IM_COL32(210, 210, 210, 255), 2.0f);
        dl->AddText(ImVec2(s0.x - u * 0.1f, s1.y + 1), IM_COL32(20, 20, 20, 255), "V");
        dl->AddText(ImVec2(s1.x - u * 0.6f, s1.y + 1), IM_COL32(20, 20, 20, 255), "A");
        // the protection lamp
        ImVec2 lc(x0 + fw * 0.335f, f0.y + fh * 0.52f);
        lamp(dl, lc, u * 0.28f, bs.power && out.limiting);
        dl->AddText(ImVec2(lc.x - u * 1.9f, lc.y + u * 0.35f), IM_COL32(20, 20, 20, 255), tr("PROTECTION"));
        // the knobs
        ImVec2 ku(x0 + fw * 0.08f, f0.y + fh * 0.83f), ki(x0 + fw * 0.22f, f0.y + fh * 0.83f);
        text_center(dl, ImVec2(ku.x, ku.y - u * 1.75f), IM_COL32(20, 20, 20, 255), tr("VOLTAGE"), 0.8f);
        text_center(dl, ImVec2(ki.x, ki.y - u * 1.75f), IM_COL32(20, 20, 20, 255), tr("CURRENT"), 0.8f);
        if (knob("##u", ku, u * 0.95f, &out.u_set, 0, 30, 0.02, "%.3f V", tr("Output voltage"))) *changed = true;
        if (knob("##i", ki, u * 0.95f, &out.i_lim, 0.05, 1.1, 0.002, "%.3f A", tr("Current limit"))) *changed = true;
        // the terminals
        t.plus[k] = ImVec2(x0 + fw * 0.30f, f0.y + fh * 0.84f);
        t.minus[k] = ImVec2(x0 + fw * 0.38f, f0.y + fh * 0.84f);
        terminal(dl, t.plus[k], u * 0.5f, IM_COL32(190, 30, 30, 255), "+", IM_COL32(20, 20, 20, 255));
        terminal(dl, t.minus[k], u * 0.5f, IM_COL32(30, 30, 30, 255), "-", IM_COL32(20, 20, 20, 255));
        ImGui::PopID();
    }
    // the mains switch with the lamp E40
    ImVec2 m0(f0.x + fw * 0.465f, f0.y + fh * 0.70f), m1(f0.x + fw * 0.535f, f0.y + fh * 0.84f);
    text_center(dl, ImVec2((m0.x + m1.x) * 0.5f, m0.y - u * 1.6f), IM_COL32(20, 20, 20, 255), tr("MAINS"), 0.85f);
    lamp(dl, ImVec2((m0.x + m1.x) * 0.5f, m0.y - u * 0.6f), u * 0.25f, bs.power, IM_COL32(255, 170, 40, 255));
    if (panel_button("##bs_power", m0, m1, tr("Mains switch"))) {
        bs.power = !bs.power;
        *changed = true;
    }
    ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + hgt));
    ImGui::Dummy(ImVec2(w, u * 0.2f));
    return t;
}

// ---------------------------------------------------------------- the bus monitor

static void draw_monitor(UiState& ui, Ims2Bench* b, Dps1* card, float u)
{
    Ims2Bus& bus = b->ims;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // the lines
    static const struct { uint8_t bit; const char* name; } lines[] = {
        { IMS_ATN, "ATN" }, { IMS_EOI, "EOI" }, { IMS_IFC, "IFC" }, { IMS_REN, "REN" }, { IMS_SRQ, "SRQ" },
        { IMS_DAV, "DAV" }, { IMS_NRFD, "NRFD" }, { IMS_NDAC, "NDAC" },
    };
    for (auto& l : lines) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        lamp(dl, ImVec2(p.x + u * 0.4f, p.y + u * 0.55f), u * 0.28f, bus.asserted(l.bit), IM_COL32(80, 230, 90, 255));
        ImGui::Dummy(ImVec2(u * 0.9f, u));
        ImGui::SameLine(0, u * 0.1f);
        ImGui::TextUnformatted(l.name);
        ImGui::SameLine(0, u * 0.7f);
    }
    ImGui::Text("DIO %02Xh", bus.dio_lines);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The data lines DIO8-DIO1 (asserted = 1)."));
    if (card) {
        ImGui::TextDisabled("DPS-1: %s, %s, %s", card->pon_ff() ? tr("drivers off (PON)") : tr("drivers on"),
                            card->controller() ? tr("controller (CEN)") : tr("not the controller"),
                            card->listening() ? tr("listener") : tr("talker"));
    } else {
        ImGui::TextDisabled("%s", tr("No DPS-1 in the machine: nobody drives the bus."));
    }
    // the last transfers: commands with ATN together, the data bytes of one talker as a text
    std::vector<Ims2Event> ev;
    bus.log_copy(ev);
    std::vector<std::string> rows;
    std::string cur;
    int kind = 0;   // 0 none, 1 commands, 2 data
    std::string talker;
    auto flush = [&]() {
        if (!cur.empty()) rows.push_back(cur);
        cur.clear();
        kind = 0;
    };
    for (const Ims2Event& e : ev) {
        char t[32];
        snprintf(t, sizeof t, "%9.3f s  ", (double)e.t / bus.hz);
        if (!e.transfer) {
            flush();
            std::string s = t;
            if (e.byte & IMS_IFC) s += (e.lines & IMS_IFC) ? "IFC" : "IFC off";
            if (e.byte & IMS_REN) s += (e.lines & IMS_REN) ? "REN" : "REN off";
            rows.push_back(s);
            continue;
        }
        if (e.lines & IMS_ATN) {
            if (kind != 1) {
                flush();
                cur = std::string(t) + "ATN " + e.from + ":";
                kind = 1;
            }
            cur += " " + ims2_command_name(e.byte);
        } else {
            if (kind != 2 || talker != e.from) {
                flush();
                cur = std::string(t) + e.from + " \xc2\xbb ";
                kind = 2;
                talker = e.from;
            }
            uint8_t c = e.byte & 0x7F;
            char s[8];
            if (c == '\r') strcpy(s, "\\r");
            else if (c == '\n') strcpy(s, "\\n");
            else if (c >= 32 && c < 127) { s[0] = (char)c; s[1] = 0; }
            else snprintf(s, sizeof s, "<%02X>", c);
            cur += s;
            if (e.lines & IMS_EOI) {
                cur += "  [EOI]";
                flush();
            }
        }
    }
    flush();
    ImGui::BeginChild("monitor", ImVec2(0, u * 7.5f), ImGuiChildFlags_Borders);
    size_t from = rows.size() > 40 ? rows.size() - 40 : 0;
    if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
    for (size_t i = from; i < rows.size(); i++) ImGui::TextUnformatted(rows[i].c_str());
    if (ui.mono) ImGui::PopFont();
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - u) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    if (ImGui::SmallButton(tr("Clear"))) bus.clear_log();
    ImGui::SameLine();
    ImGui::TextDisabled("%s %llu", tr("bytes transferred:"), (unsigned long long)bus.transfers);
}

// ---------------------------------------------------------------- the window

void draw_ims2(UiState& ui)
{
    Machine* m = ui.host->machine();
    Ims2Bench* b = m ? dynamic_cast<Ims2Bench*>(m->plant.get()) : nullptr;
    if (m && ui.ims2_gen != ui.host->machine_generation()) {   // a machine with the bench opens it
        ui.ims2_gen = ui.host->machine_generation();
        if (b) ui.show_ims2 = true;
    }
    if (!ui.show_ims2) return;
    float u = ImGui::GetFontSize();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(40 * u, 56 * u), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 41 * u, 2.0f * u), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(tr_id("IMS-2 measuring bench"), &ui.show_ims2) || !m) {
        ImGui::End();
        return;
    }
    if (!b) {
        ImGui::TextDisabled("%s", tr("No measuring bench. Machine description: \"plant\": { \"type\": \"IMS-2\" } with DPS-1."));
        ImGui::End();
        return;
    }
    Dps1* card = dynamic_cast<Dps1*>(m->find_card("DPS-1"));
    uint64_t now = m->bus.now;
    // the cable
    ImGui::TextDisabled("%s", card && card->cable() ? tr("DPS-1 X2 -> IMS-2 cable -> voltmeter M1T 330")
                                                    : tr("DPS-1 X2 -> IMS-2 cable (connected at the first access of the card)"));
    ImGui::SameLine();
    ImGui::TextDisabled("  %s %d", tr("address"), b->m1t.address);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The address switches on the back of the voltmeter (machine description: plant.m1t330.address)."));
    draw_monitor(ui, b, card, u);
    ImGui::Separator();
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 o = ImGui::GetCursorScreenPos();
    M1tTerms vt = draw_m1t330(b, now, o, w);
    ImGui::Dummy(ImVec2(w, u * 2.5f));   // room for the wires
    o = ImGui::GetCursorScreenPos();
    bool changed = false;
    BsTerms st = draw_bs525(b, o, w, &changed);
    // the wires from the supply output to H and L
    if (b->input >= 0) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        int k = b->input;
        auto wire = [&](ImVec2 a, ImVec2 c, ImU32 col) {
            ImVec2 c1(a.x, a.y + (c.y - a.y) * 0.5f), c2(c.x, a.y + (c.y - a.y) * 0.5f);
            dl->AddBezierCubic(a, c1, c2, c, IM_COL32(0, 0, 0, 120), 5.0f);
            dl->AddBezierCubic(a, c1, c2, c, col, 3.0f);
        };
        wire(vt.h, st.plus[k], IM_COL32(200, 40, 30, 255));
        wire(vt.l, st.minus[k], IM_COL32(40, 40, 40, 255));
    }
    // the bench: which output feeds the voltmeter, the loads
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr("Voltmeter H, L on:"));
    ImGui::SameLine();
    if (ImGui::RadioButton(tr("output I"), b->input == 0)) { b->input = 0; changed = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton(tr("output II"), b->input == 1)) { b->input = 1; changed = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton(tr("nothing"), b->input == -1)) { b->input = -1; changed = true; }
    for (int k = 0; k < 2; k++) {
        Bs525Output& o2 = b->bs.out[k];
        ImGui::PushID(k + 10);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s %s:", tr("Load on output"), k == 0 ? "I" : "II");
        ImGui::SameLine();
        char cur[32];
        double r = BS525_LOADS[o2.load];
        if (r > 0) snprintf(cur, sizeof cur, r >= 1000 ? "%.0f k\xce\xa9" : "%.0f \xce\xa9", r >= 1000 ? r / 1000 : r);
        else snprintf(cur, sizeof cur, "%s", tr("none (only the voltmeter)"));
        ImGui::SetNextItemWidth(12 * u);
        if (ImGui::BeginCombo("##load", cur)) {
            for (int i = 0; i < BS525_LOAD_COUNT; i++) {
                char s[32];
                double ri = BS525_LOADS[i];
                if (ri > 0) snprintf(s, sizeof s, ri >= 1000 ? "%.0f k\xce\xa9" : "%.0f \xce\xa9", ri >= 1000 ? ri / 1000 : ri);
                else snprintf(s, sizeof s, "%s", tr("none (only the voltmeter)"));
                if (ImGui::Selectable(s, o2.load == i)) { o2.load = i; changed = true; }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%.3f V  %.3f A%s", b->bs.power ? o2.u : 0.0, b->bs.power ? o2.i : 0.0,
                            o2.limiting ? tr("  (current limit)") : "");
        ImGui::PopID();
    }
    if (changed) b->changed();
    ImGui::End();
}
