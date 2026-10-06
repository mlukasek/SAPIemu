// Window "Diagnostic board": the board on K3 of JPR-1S / JPR-1X (Libor Lasota, rev 2.0), two seven
// segment displays (P2-OUT left, P1-OUT right) and the buttons "-" (P2-IN7) and "+" (P2-IN0).
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include "imgui.h"
#include "core/host.h"
#include "core/machine.h"

// One digit: segments a..g = bits 0..6, dp = bit 7, 1 = lit.
static void draw_digit(ImDrawList* dl, ImVec2 o, float w, float h, uint8_t v)
{
    const ImU32 on = IM_COL32(255, 45, 30, 255), off = IM_COL32(70, 18, 16, 255);
    float t = w * 0.16f;            // segment thickness
    float k = w * 0.12f;            // slant of the digit
    float hh = h * 0.5f;
    auto sk = [&](float x, float y) { return ImVec2(o.x + x + k * (1.0f - y / h), o.y + y); };
    // horizontal segment at height y, vertical segment at x from y0 to y1
    auto hseg = [&](float y, bool lit) {
        ImVec2 p[6] = { sk(t * 0.6f, y), sk(t * 1.1f, y - t * 0.5f), sk(w - t * 1.1f, y - t * 0.5f),
                        sk(w - t * 0.6f, y), sk(w - t * 1.1f, y + t * 0.5f), sk(t * 1.1f, y + t * 0.5f) };
        dl->AddConvexPolyFilled(p, 6, lit ? on : off);
    };
    auto vseg = [&](float x, float y0, float y1, bool lit) {
        ImVec2 p[6] = { sk(x, y0 + t * 0.6f), sk(x + t * 0.5f, y0 + t * 1.1f), sk(x + t * 0.5f, y1 - t * 1.1f),
                        sk(x, y1 - t * 0.6f), sk(x - t * 0.5f, y1 - t * 1.1f), sk(x - t * 0.5f, y0 + t * 1.1f) };
        dl->AddConvexPolyFilled(p, 6, lit ? on : off);
    };
    hseg(t * 0.5f, v & 0x01);                 // a
    vseg(w - t * 0.5f, 0, hh, v & 0x02);      // b
    vseg(w - t * 0.5f, hh, h, v & 0x04);      // c
    hseg(h - t * 0.5f, v & 0x08);             // d
    vseg(t * 0.5f, hh, h, v & 0x10);          // e
    vseg(t * 0.5f, 0, hh, v & 0x20);          // f
    hseg(hh, v & 0x40);                       // g
    ImVec2 dp = sk(w + t * 0.9f, h - t * 0.5f);
    dl->AddCircleFilled(dp, t * 0.6f, (v & 0x80) ? on : off);
}

void draw_diag(UiState& ui)
{
    Machine* m = ui.host->machine();
    CpuCard* cpu = m ? m->cpu : nullptr;
    bool has = cpu && cpu->k3_diag();
    if (!ui.show_diag || !has) {   // closed or no board: no button is held
        if (has) {
            cpu->k3_button(0, false);
            cpu->k3_button(1, false);
        }
        if (!ui.show_diag) return;
    }
    float u = ImGui::GetFontSize();
    ImGui::SetNextWindowSize(ImVec2(17 * u, 16 * u), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(tr_id("Diagnostic board (K3)"), &ui.show_diag)) {
        ImGui::End();
        return;
    }
    if (!has) {
        ImGui::TextWrapped("%s", tr("No diagnostic board. Configurator: K3 of JPR-1S or JPR-1X = diagnostic board."));
        ImGui::End();
        return;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    float dw = 4.0f * u, dh = 6.5f * u, gap = 1.6f * u, pad = 0.8f * u;
    float bw = 2 * dw + gap + 2 * pad + 0.6f * u, bh = dh + 2 * pad + 1.4f * u;
    dl->AddRectFilled(o, ImVec2(o.x + bw, o.y + bh), IM_COL32(40, 8, 8, 255), 4.0f);
    ImU32 label = IM_COL32(230, 230, 230, 255);
    dl->AddText(ImVec2(o.x + pad + dw * 0.5f - 0.4f * u, o.y + 0.1f * u), label, "P2");
    dl->AddText(ImVec2(o.x + pad + dw + gap + dw * 0.5f - 0.4f * u, o.y + 0.1f * u), label, "P1");
    draw_digit(dl, ImVec2(o.x + pad, o.y + 1.3f * u), dw, dh, cpu->k3_display(0));
    draw_digit(dl, ImVec2(o.x + pad + dw + gap, o.y + 1.3f * u), dw, dh, cpu->k3_display(1));
    ImGui::Dummy(ImVec2(bw, bh));
    // the buttons are held while the mouse button is down on them
    ImVec2 sz(bw * 0.5f - ImGui::GetStyle().ItemSpacing.x * 0.5f, 2.2f * u);
    ImGui::Button("-##minus", sz);
    bool minus = ImGui::IsItemActive();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("S2 \"-\": P2-IN7 (JPR-1X: interrupt request R7)"));
    ImGui::SameLine();
    ImGui::Button("+##plus", sz);
    bool plus = ImGui::IsItemActive();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("S1 \"+\": P2-IN0 (JPR-1X: interrupt request R0 = /INT0)"));
    cpu->k3_button(0, plus);
    cpu->k3_button(1, minus);
    ImGui::TextDisabled("P2-OUT %02Xh  P1-OUT %02Xh", cpu->k3_display(0), cpu->k3_display(1));
    ImGui::End();
}
