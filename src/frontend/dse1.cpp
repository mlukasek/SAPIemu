// Window "EPROM programmer DSE-1" (###dse1): the box at the end of the programming cable X2 of the DSE-1 card drawn
// from above, with its green zero insertion force socket (in the manner of a 3M TEXTOOL), the EPROM in it and the LEDs
// PROG and MAP of the cable, and the UV lamp beside it (Erase, an emulator comfort). Under the drawing the medium in
// the socket (Insert..., New 2716 / 2732, Eject, Restore from the original), the simulator RAM into a file (an emulator
// help) and the state of the card (Vpp, ON LINE, the last programming pulse: emulator information, the box has only
// the LEDs).
// The look of the Tesla box is not known (no photo): the drawing is a deliberate deviation like the rack of the
// configurator (docs/desky/DSE-1.md). The lever of the socket is drawn open with the socket empty and closed with an
// EPROM in it; it is not clicked. A programmed EPROM (a byte other than FFh) has a paper label over its window with
// the name of its file, as people stuck one on a programmed EPROM (not while it is being programmed: the label goes on
// when the programming has ended); a blank one shows its window and the chip in it. More DSE-1 cards in a machine: the
// window shows the one chosen in its list.
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_internal.h"   // FindHoveredWindowEx: the window under a dropped file
#include "cards/dse1.h"
#include "core/host.h"
#include "core/machine.h"
#include "core/util.h"

static const float K_PI = 3.14159265f;
static const float SCENE_W = 106.0f, SCENE_H = 60.0f;   // the drawing in millimetres
static const double ERASE_S = 1.5;                      // the UV lamp shines this long (real time)

// the colours of the hardware: the same in every look
static const ImU32 COL_BOX = IM_COL32(178, 180, 176, 255);
static const ImU32 COL_BOX_HI = IM_COL32(212, 214, 210, 255);
static const ImU32 COL_BOX_SIDE = IM_COL32(126, 128, 124, 255);
static const ImU32 COL_BOX_LINE = IM_COL32(92, 94, 90, 255);
static const ImU32 COL_PRINT = IM_COL32(52, 54, 52, 255);     // the lettering on the box
static const ImU32 COL_ZIF = IM_COL32(38, 140, 88, 255);
static const ImU32 COL_ZIF_HI = IM_COL32(92, 194, 136, 255);
static const ImU32 COL_ZIF_DARK = IM_COL32(28, 108, 68, 255);
static const ImU32 COL_ZIF_LINE = IM_COL32(18, 72, 44, 255);
static const ImU32 COL_HOLE = IM_COL32(14, 34, 24, 255);
static const ImU32 COL_METAL = IM_COL32(172, 176, 182, 255);
static const ImU32 COL_METAL_HI = IM_COL32(232, 234, 238, 255);
static const ImU32 COL_METAL_LINE = IM_COL32(88, 90, 96, 255);
static const ImU32 COL_CERAMIC = IM_COL32(64, 66, 72, 255);
static const ImU32 COL_CERAMIC_EDGE = IM_COL32(96, 98, 106, 255);
static const ImU32 COL_CHIP_PRINT = IM_COL32(232, 232, 228, 255);
static const ImU32 COL_PAPER = IM_COL32(249, 247, 239, 255);
static const ImU32 COL_INK = IM_COL32(28, 48, 150, 255);
static const ImU32 COL_UV = IM_COL32(150, 70, 255, 255);

// ---------------------------------------------------------------- drawing helpers

struct Scene {   // millimetres of the drawing to the screen
    ImDrawList* dl;
    ImVec2 o;    // the top left corner of the drawing
    float s;     // pixels per millimetre
    ImVec2 p(float x, float y) const { return ImVec2(o.x + x * s, o.y + y * s); }
    float k(float mm) const { return mm * s; }
};

static ImU32 with_alpha(ImU32 c, int a)
{
    return (c & 0x00FFFFFFu) | ((ImU32)(a < 0 ? 0 : a > 255 ? 255 : a) << 24);
}

static ImU32 mix(ImU32 a, ImU32 b, float t)
{
    int r = (int)(((a >> 0) & 255) + (((int)((b >> 0) & 255) - (int)((a >> 0) & 255)) * t));
    int g = (int)(((a >> 8) & 255) + (((int)((b >> 8) & 255) - (int)((a >> 8) & 255)) * t));
    int bl = (int)(((a >> 16) & 255) + (((int)((b >> 16) & 255) - (int)((a >> 16) & 255)) * t));
    return IM_COL32(r, g, bl, (a >> 24) & 255);
}

// Text of the size `mm` (the font height) at x, y (the middle of its line): align 0 centred, -1 from x, 1 to x; made
// smaller to fit `max_w` millimetres (0 = any width).
static void text_mm(const Scene& S, float x, float y, float mm, ImU32 col, const char* t, int align = 0, bool bold = false,
                    float max_w = 0.0f)
{
    ImFont* f = panel_font(bold);
    float px = S.k(mm);
    ImVec2 ts = f->CalcTextSizeA(px, FLT_MAX, 0.0f, t);
    if (max_w > 0 && ts.x > S.k(max_w)) {
        px *= S.k(max_w) / ts.x;
        ts = f->CalcTextSizeA(px, FLT_MAX, 0.0f, t);
    }
    if (px < 1.0f) return;
    ImVec2 c = S.p(x, y);
    float left = align == 0 ? c.x - ts.x * 0.5f : align < 0 ? c.x : c.x - ts.x;
    S.dl->AddText(f, px, ImVec2(left, c.y - ts.y * 0.5f), col, t);
}

static float text_w_mm(const Scene& S, float mm, const char* t, bool bold = false)
{
    return panel_font(bold)->CalcTextSizeA(S.k(mm), FLT_MAX, 0.0f, t).x / S.s;
}

static void screw(const Scene& S, float x, float y)
{
    ImVec2 c = S.p(x, y);
    float r = S.k(1.6f);
    S.dl->AddCircleFilled(ImVec2(c.x + S.k(0.25f), c.y + S.k(0.3f)), r, IM_COL32(0, 0, 0, 60));
    S.dl->AddCircleFilled(c, r, IM_COL32(196, 198, 202, 255));
    S.dl->AddCircle(c, r, COL_METAL_LINE, 0, 1.0f);
    float d = r * 0.68f;   // the slot, a little askew
    S.dl->AddLine(ImVec2(c.x - d, c.y + d * 0.35f), ImVec2(c.x + d, c.y - d * 0.35f), IM_COL32(96, 98, 104, 255),
                  S.k(0.45f) > 1.0f ? S.k(0.45f) : 1.0f);
}

// A round LED in its black bezel: the lens dark when off, lit with a glow
static void led(const Scene& S, float x, float y, bool on, ImU32 col, const char* name)
{
    ImDrawList* dl = S.dl;
    ImVec2 c = S.p(x, y);
    if (on) {
        dl->AddCircleFilled(c, S.k(5.2f), with_alpha(col, 0x22), 32);
        dl->AddCircleFilled(c, S.k(3.6f), with_alpha(col, 0x40), 32);
    }
    dl->AddCircleFilled(ImVec2(c.x, c.y + S.k(0.3f)), S.k(2.7f), IM_COL32(0, 0, 0, 70), 32);
    dl->AddCircleFilled(c, S.k(2.6f), IM_COL32(40, 42, 44, 255), 32);
    dl->AddCircle(c, S.k(2.6f), IM_COL32(20, 20, 22, 255), 32, 1.0f);
    ImU32 lens = on ? col : mix(col, IM_COL32(26, 24, 24, 255), 0.72f);
    dl->AddCircleFilled(c, S.k(2.0f), lens, 32);
    if (on) dl->AddCircleFilled(c, S.k(1.1f), mix(col, IM_COL32(255, 255, 255, 255), 0.45f), 24);
    dl->AddCircleFilled(ImVec2(c.x - S.k(0.65f), c.y - S.k(0.7f)), S.k(0.55f), IM_COL32(255, 255, 255, on ? 200 : 90), 16);
    lamp_tooltip(c, S.k(2.8f), name, on);
}

// ---------------------------------------------------------------- the socket and the EPROM

// The socket: the green body, the block of the cam at the left, the two rows of contact holes, the recess in the
// middle; x0..x1, y0..y1 its body; the rows of contacts at cy -+ 7.62 mm, the pins from px0 2.54 mm apart.
static void zif_socket(const Scene& S, float x0, float y0, float x1, float y1, float cy, float px0, bool empty)
{
    ImDrawList* dl = S.dl;
    float r = S.k(1.2f);
    dl->AddRectFilled(S.p(x0 + 0.5f, y0 + 0.7f), S.p(x1 + 0.7f, y1 + 0.9f), IM_COL32(0, 0, 0, 70), r);   // its shadow
    dl->AddRectFilled(S.p(x0, y0), S.p(x1, y1), COL_ZIF, r);
    dl->AddRectFilled(S.p(x0 + 0.5f, y0 + 0.35f), S.p(x1 - 0.5f, y0 + 0.9f), with_alpha(COL_ZIF_HI, 150), S.k(0.4f));
    dl->AddRectFilled(S.p(x0 + 0.5f, y1 - 0.9f), S.p(x1 - 0.5f, y1 - 0.3f), with_alpha(COL_ZIF_DARK, 200), S.k(0.4f));
    // the block of the cam (a little higher) and its edge
    float bx = x0 + 7.0f;
    dl->AddRectFilled(S.p(x0 + 0.4f, y0 + 0.4f), S.p(bx, y1 - 0.4f), mix(COL_ZIF, COL_ZIF_HI, 0.25f), S.k(0.8f));
    dl->AddLine(S.p(bx, y0 + 0.6f), S.p(bx, y1 - 0.6f), COL_ZIF_LINE, 1.0f);
    dl->AddCircleFilled(S.p(x0 + 3.5f, cy), S.k(1.5f), COL_ZIF_DARK, 20);   // the rivet of the cam
    dl->AddCircle(S.p(x0 + 3.5f, cy), S.k(1.5f), COL_ZIF_LINE, 20, 1.0f);
    // the recess in the middle with its two round holes (under the chip)
    if (empty) {
        dl->AddRectFilled(S.p(bx + 4.0f, cy - 4.4f), S.p(x1 - 3.5f, cy + 4.4f), COL_ZIF_DARK, S.k(1.0f));
        dl->AddRect(S.p(bx + 4.0f, cy - 4.4f), S.p(x1 - 3.5f, cy + 4.4f), COL_ZIF_LINE, S.k(1.0f), 0, 1.0f);
        float mx = (bx + 4.0f + x1 - 3.5f) * 0.5f;
        for (int i = -1; i <= 1; i += 2) {
            ImVec2 c = S.p(mx + i * 7.0f, cy);
            dl->AddCircleFilled(c, S.k(2.1f), mix(COL_ZIF_DARK, COL_ZIF, 0.5f), 24);
            dl->AddCircleFilled(c, S.k(1.2f), COL_HOLE, 24);
        }
        text_mm(S, px0, cy + 7.62f + 2.25f, 2.1f, COL_ZIF_LINE, "1", 0, true);
        text_mm(S, px0, cy - 7.62f - 2.25f, 2.1f, COL_ZIF_LINE, "24", 0, true);
    }
    // the contact holes, a gold spring in each
    for (int row = -1; row <= 1; row += 2)
        for (int i = 0; i < 12; i++) {
            float x = px0 + i * 2.54f, y = cy + row * 7.62f;
            dl->AddRectFilled(S.p(x - 0.7f, y - 1.1f), S.p(x + 0.7f, y + 1.1f), COL_HOLE, S.k(0.25f));
            dl->AddLine(S.p(x, y - 0.7f), S.p(x, y + 0.7f), IM_COL32(196, 156, 70, 255), S.k(0.35f) > 1.0f ? S.k(0.35f) : 1.0f);
        }
}

// The lever: the metal rod from its pivot, the round knob at its end. Closed it lies along the socket; open it stands
// up (seen from above it points away from the socket, shorter, its knob nearer to the eye: larger).
static void zif_lever(const Scene& S, ImVec2 pivot, float len, bool open)
{
    ImDrawList* dl = S.dl;
    float a = open ? -98.0f * K_PI / 180.0f : 0.0f;
    float l = open ? 16.5f : len;
    float kr = open ? 2.9f : 2.4f;
    ImVec2 dir(cosf(a), sinf(a));
    ImVec2 end(pivot.x + dir.x * l, pivot.y + dir.y * l);
    ImVec2 knob(pivot.x + dir.x * (l + kr * 0.8f), pivot.y + dir.y * (l + kr * 0.8f));
    float sh = open ? 1.6f : 0.6f;   // its shadow on the box (an open lever is higher)
    float w = S.k(1.5f) > 2.0f ? S.k(1.5f) : 2.0f;
    dl->AddLine(S.p(pivot.x + sh, pivot.y + sh), S.p(end.x + sh, end.y + sh), IM_COL32(0, 0, 0, 60), w);
    dl->AddCircleFilled(S.p(knob.x + sh, knob.y + sh), S.k(kr), IM_COL32(0, 0, 0, 60), 24);
    dl->AddLine(S.p(pivot.x, pivot.y), S.p(end.x, end.y), COL_METAL_LINE, w + 2.0f);
    dl->AddLine(S.p(pivot.x, pivot.y), S.p(end.x, end.y), COL_METAL, w);
    ImVec2 n(-dir.y * 0.35f, dir.x * 0.35f);   // the light along its upper side
    dl->AddLine(S.p(pivot.x - n.x, pivot.y - n.y), S.p(end.x - n.x, end.y - n.y), COL_METAL_HI, w * 0.35f);
    // the pivot on the block of the cam
    dl->AddCircleFilled(S.p(pivot.x, pivot.y), S.k(1.7f), COL_METAL, 20);
    dl->AddCircle(S.p(pivot.x, pivot.y), S.k(1.7f), COL_METAL_LINE, 20, 1.0f);
    dl->AddCircleFilled(S.p(pivot.x, pivot.y), S.k(0.6f), COL_METAL_LINE, 12);
    // the knob
    ImVec2 kc = S.p(knob.x, knob.y);
    dl->AddCircleFilled(kc, S.k(kr), COL_ZIF_DARK, 28);
    dl->AddCircle(kc, S.k(kr), COL_ZIF_LINE, 28, 1.0f);
    dl->AddCircleFilled(ImVec2(kc.x - S.k(kr * 0.3f), kc.y - S.k(kr * 0.3f)), S.k(kr * 0.45f), with_alpha(COL_ZIF_HI, 170), 20);
}

// The end of the UTF-8 character that begins at i.
static size_t utf8_next(const std::string& s, size_t i)
{
    i++;
    while (i < s.size() && ((unsigned char)s[i] & 0xC0) == 0x80) i++;
    return i;
}

// `n` in lines at most `w` millimetres wide at the size `mm`: broken after '_', '-', '.' or ' ' where it can, else
// inside a word.
static std::vector<std::string> wrap_mm(const Scene& S, const std::string& n, float mm, float w)
{
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    while (i < n.size()) {
        size_t j = i;   // the next word, with the separator after it
        while (j < n.size() && !strchr("_-. ", n[j])) j = utf8_next(n, j);
        if (j < n.size()) j++;
        std::string word = n.substr(i, j - i);
        i = j;
        if (text_w_mm(S, mm, (cur + word).c_str()) <= w) {
            cur += word;
            continue;
        }
        if (!cur.empty()) lines.push_back(cur);
        cur.clear();
        for (size_t k = 0; k < word.size();) {   // on a line of its own, by characters when wider than one
            size_t e = utf8_next(word, k);
            std::string ch = word.substr(k, e - k);
            if (!cur.empty() && text_w_mm(S, mm, (cur + ch).c_str()) > w) {
                lines.push_back(cur);
                cur.clear();
            }
            cur += ch;
            k = e;
        }
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

// The name of the file on the label, as written on it by hand: without its folders and the suffix .bin, on up to three
// lines, smaller when long, the end cut with an ellipsis when it still does not fit the label `w` millimetres wide.
static void label_text(const Scene& S, float cx, float cy, float w, const std::string& path)
{
    std::string n = path;
    size_t sl = n.find_last_of("/\\");
    if (sl != std::string::npos) n = n.substr(sl + 1);
    if (n.size() > 4 && (n.compare(n.size() - 4, 4, ".bin") == 0 || n.compare(n.size() - 4, 4, ".BIN") == 0))
        n.resize(n.size() - 4);
    float mm = 2.5f;
    std::vector<std::string> lines = wrap_mm(S, n, mm, w);
    while (mm > 1.55f && lines.size() > (mm > 2.05f ? 2u : 3u)) {
        mm -= 0.1f;
        lines = wrap_mm(S, n, mm, w);
    }
    if (lines.size() > 3) {   // too long: the third line ends with "..."
        lines.resize(3);
        std::string& l = lines[2];
        l += "\xE2\x80\xA6";
        while (l.size() > 3 && text_w_mm(S, mm, l.c_str()) > w) {
            size_t k = l.size() - 3, prev = 0;   // the character before the ellipsis
            for (size_t i = 0; i < k; i = utf8_next(l, i)) prev = i;
            l.erase(prev, k - prev);
        }
    }
    float lh = mm * 1.18f, y = cy - lh * (float)(lines.size() - 1) * 0.5f;
    for (const std::string& l : lines) {
        text_mm(S, cx, y, mm, COL_INK, l.c_str());
        y += lh;
    }
}

// The EPROM: a ceramic DIP of 24 pins with the quartz window over the chip, the print of its maker (TESLA MHB2716C;
// Tesla did not make the 2732: Intel D2732), a date code. labelled: a paper label over the window (programmed).
// glow 0..1: the UV light in the window (Erase).
static void eprom(const Scene& S, float cx, float cy, int type, bool labelled, const std::string& path, float glow)
{
    ImDrawList* dl = S.dl;
    const float hl = 15.9f, hw = 6.8f;   // half the length and the width of the body
    float x0 = cx - hl, x1 = cx + hl, y0 = cy - hw, y1 = cy + hw;
    dl->AddRectFilled(S.p(x0 + 0.5f, y0 + 0.7f), S.p(x1 + 0.7f, y1 + 1.0f), IM_COL32(0, 0, 0, 80), S.k(0.6f));
    // the pins in the holes of the socket
    for (int row = -1; row <= 1; row += 2)
        for (int i = 0; i < 12; i++) {
            float x = cx - 13.97f + i * 2.54f;
            float ya = row < 0 ? y0 - 1.2f : y1 - 0.2f, yb = row < 0 ? y0 + 0.2f : y1 + 1.2f;
            dl->AddRectFilled(S.p(x - 0.65f, ya), S.p(x + 0.65f, yb), IM_COL32(200, 202, 206, 255));
            dl->AddRect(S.p(x - 0.65f, ya), S.p(x + 0.65f, yb), IM_COL32(110, 112, 118, 255), 0, 0, 1.0f);
        }
    // the body: the upper ceramic half, the edge of the lower one around it, the notch at pin 1
    dl->AddRectFilled(S.p(x0, y0), S.p(x1, y1), COL_CERAMIC_EDGE, S.k(0.6f));
    dl->AddRectFilled(S.p(x0 + 0.4f, y0 + 0.4f), S.p(x1 - 0.4f, y1 - 0.4f), COL_CERAMIC, S.k(0.5f));
    dl->PathArcTo(S.p(x0, cy), S.k(1.5f), -K_PI * 0.5f, K_PI * 0.5f, 16);
    dl->PathFillConvex(IM_COL32(34, 36, 40, 255));
    // the window: the frame, the quartz, the chip with its bond wires
    const float fw = 4.8f, qw = 3.9f;
    dl->AddRectFilled(S.p(cx - fw, cy - fw), S.p(cx + fw, cy + fw), IM_COL32(146, 148, 152, 255), S.k(1.6f));
    dl->AddRect(S.p(cx - fw, cy - fw), S.p(cx + fw, cy + fw), IM_COL32(70, 72, 78, 255), S.k(1.6f), 0, 1.0f);
    dl->AddRectFilled(S.p(cx - qw, cy - qw), S.p(cx + qw, cy + qw), IM_COL32(26, 30, 42, 255), S.k(1.2f));
    const float dw = 2.4f, dh = 1.9f;   // half the chip
    for (int i = 0; i < 6; i++) {   // the bond wires from the pads to the frame
        float t = -dw + 0.4f + i * (2.0f * dw - 0.8f) / 5.0f;
        dl->AddLine(S.p(cx + t, cy - dh + 0.25f), S.p(cx + t * 1.25f, cy - qw + 0.15f), IM_COL32(214, 182, 96, 220), 1.0f);
        dl->AddLine(S.p(cx + t, cy + dh - 0.25f), S.p(cx + t * 1.25f, cy + qw - 0.15f), IM_COL32(214, 182, 96, 220), 1.0f);
    }
    dl->AddRectFilled(S.p(cx - dw, cy - dh), S.p(cx + dw, cy + dh), IM_COL32(150, 144, 120, 255));
    dl->AddRectFilled(S.p(cx - dw + 0.45f, cy - dh + 0.45f), S.p(cx - 0.15f, cy + dh - 0.45f), IM_COL32(104, 102, 92, 255));
    dl->AddRectFilled(S.p(cx + 0.15f, cy - dh + 0.45f), S.p(cx + dw - 0.45f, cy + dh - 0.45f), IM_COL32(104, 102, 92, 255));
    for (float gy = cy - dh + 0.75f; gy < cy + dh - 0.5f; gy += 0.42f)   // the rows of the memory cells
        dl->AddLine(S.p(cx - dw + 0.5f, gy), S.p(cx + dw - 0.5f, gy), IM_COL32(126, 122, 104, 255), 1.0f);
    for (int i = 0; i < 7; i++) {   // the pads
        float t = -dw + 0.25f + i * (2.0f * dw - 0.5f) / 6.0f;
        dl->AddRectFilled(S.p(t + cx - 0.12f, cy - dh + 0.05f), S.p(t + cx + 0.12f, cy - dh + 0.3f), IM_COL32(226, 220, 196, 255));
        dl->AddRectFilled(S.p(t + cx - 0.12f, cy + dh - 0.3f), S.p(t + cx + 0.12f, cy + dh - 0.05f), IM_COL32(226, 220, 196, 255));
    }
    if (glow > 0) {   // the UV light: the quartz glows violet, a halo around the window
        dl->AddRectFilled(S.p(cx - qw, cy - qw), S.p(cx + qw, cy + qw), with_alpha(COL_UV, (int)(60 + 150 * glow)), S.k(1.2f));
        dl->AddRectFilled(S.p(cx - qw * 0.55f, cy - qw * 0.55f), S.p(cx + qw * 0.55f, cy + qw * 0.55f),
                          with_alpha(IM_COL32(236, 210, 255, 255), (int)(110 * glow)), S.k(0.8f));
        dl->AddCircleFilled(S.p(cx, cy), S.k(8.0f), with_alpha(COL_UV, (int)(40 * glow)), 40);
        dl->AddCircleFilled(S.p(cx, cy), S.k(11.5f), with_alpha(COL_UV, (int)(22 * glow)), 40);
    }
    // the light on the quartz
    dl->AddQuadFilled(S.p(cx - qw + 0.6f, cy - qw + 2.4f), S.p(cx - qw + 2.4f, cy - qw + 0.6f), S.p(cx - qw + 3.6f, cy - qw + 0.6f),
                      S.p(cx - qw + 0.6f, cy - qw + 3.6f), IM_COL32(255, 255, 255, 34));
    // the print on both sides of the window, a millimetre clear of a label over it (the notch at the left)
    const float lw = 5.4f, lh = 5.7f;   // half the label
    float l0 = x0 + 1.8f, l1 = cx - lw - 1.0f, r0 = cx + lw + 1.0f, r1 = x1 - 0.6f;
    float lx = (l0 + l1) * 0.5f, rx = (r0 + r1) * 0.5f;
    bool t2732 = type == Eprom27::T2732;
    text_mm(S, lx, cy - 1.6f, 2.6f, COL_CHIP_PRINT, t2732 ? "intel" : "TESLA", 0, true, l1 - l0);
    text_mm(S, lx, cy + 2.2f, 1.7f, with_alpha(COL_CHIP_PRINT, 200), t2732 ? "8317" : "8541", 0, false, l1 - l0);
    text_mm(S, rx, cy - 1.6f, 2.4f, COL_CHIP_PRINT, t2732 ? "D2732" : "MHB2716C", 0, false, r1 - r0);
    text_mm(S, rx, cy + 2.2f, 1.7f, with_alpha(COL_CHIP_PRINT, 200), t2732 ? "4K x 8" : "2K x 8", 0, false, r1 - r0);
    if (labelled) {   // the paper label over the window, a little askew
        const float a = -2.0f * K_PI / 180.0f;
        ImVec2 q[4];
        const float cs = cosf(a), sn = sinf(a);
        const float px[4] = { -lw, lw, lw, -lw }, py[4] = { -lh, -lh, lh, lh };
        for (int i = 0; i < 4; i++) q[i] = S.p(cx + 0.2f + px[i] * cs - py[i] * sn, cy + px[i] * sn + py[i] * cs);
        ImVec2 off(S.k(0.35f), S.k(0.45f));
        dl->AddQuadFilled(ImVec2(q[0].x + off.x, q[0].y + off.y), ImVec2(q[1].x + off.x, q[1].y + off.y),
                          ImVec2(q[2].x + off.x, q[2].y + off.y), ImVec2(q[3].x + off.x, q[3].y + off.y), IM_COL32(0, 0, 0, 90));
        dl->AddQuadFilled(q[0], q[1], q[2], q[3], COL_PAPER);
        dl->AddQuad(q[0], q[1], q[2], q[3], IM_COL32(206, 202, 190, 255), 1.0f);
        label_text(S, cx + 0.2f, cy, 2.0f * lw - 1.4f, path);
    }
}

// ---------------------------------------------------------------- the UV lamp (Erase)

// The lamp ("horské sluníčko") seen from the side: the hood of its reflector on a stand, turned towards the window of
// the EPROM at tx, ty; the bulb shows in its opening. lit 0..1: it shines (the bulb violet, its light on the window);
// hover: under the mouse (faint rays: it can be clicked); dim: it cannot be used now (no EPROM). caption: on its base.
static void uv_lamp(const Scene& S, float cx, float cy, float tx, float ty, float lit, bool hover, bool dim,
                    const char* caption)
{
    float aim = atan2f(ty - cy, tx - cx);
    ImDrawList* dl = S.dl;
    int al = dim ? 120 : 255;
    const float R = 6.6f;   // the hood
    ImVec2 f(cosf(aim), sinf(aim)), n(-f.y, f.x);   // where it shines, across it
    // the stand from the joint under the hood down to the base
    float jx = cx + R * 0.42f, jy = cy + R * 0.86f, by = cy + 28.5f;
    const float bw = 9.0f;   // half the base
    dl->AddRectFilled(S.p(jx - bw + 0.4f, by + 0.6f), S.p(jx + bw + 0.6f, by + 6.1f), IM_COL32(0, 0, 0, 60), S.k(2.0f));
    dl->AddRectFilled(S.p(jx - bw, by), S.p(jx + bw, by + 5.5f), with_alpha(IM_COL32(58, 60, 66, 255), al), S.k(2.0f));
    dl->AddRectFilled(S.p(jx - bw + 0.6f, by + 0.4f), S.p(jx + bw - 0.6f, by + 1.2f), with_alpha(IM_COL32(112, 114, 122, 255), al),
                      S.k(0.4f));
    text_mm(S, jx, by + 3.3f, 2.5f, with_alpha(IM_COL32(228, 214, 255, 255), al), caption, 0, true, 2.0f * bw - 1.6f);
    dl->AddRectFilled(S.p(jx - 0.8f, jy), S.p(jx + 0.8f, by), with_alpha(COL_METAL_LINE, al));
    dl->AddRectFilled(S.p(jx - 0.45f, jy), S.p(jx - 0.05f, by), with_alpha(COL_METAL, al));
    // the light: a beam from the opening onto the window, rays
    if (lit > 0)
        for (int k = 0; k < 2; k++) {
            float h = k ? 3.6f : 6.0f, back = k ? 0.6f : 0.85f;   // half the beam at the window, at the opening
            ImVec2 a0(cx + n.x * R * back, cy + n.y * R * back), a1(cx - n.x * R * back, cy - n.y * R * back);
            dl->AddQuadFilled(S.p(a0.x, a0.y), S.p(tx, ty - h), S.p(tx, ty + h), S.p(a1.x, a1.y),
                              with_alpha(COL_UV, (int)((k ? 30 : 22) * lit)));
        }
    if (lit > 0 || (hover && !dim))   // switched off: no rays (under the mouse faint ones, it can be clicked)
        for (int i = -2; i <= 2; i++) {
            float a = aim + i * 0.32f;
            ImVec2 st(cx + f.x * 1.2f + n.x * i * 1.4f, cy + f.y * 1.2f + n.y * i * 1.4f);
            float r0 = 2.2f, r1 = lit > 0 ? 7.5f : 4.6f;
            ImU32 rc = lit > 0 ? with_alpha(COL_UV, (int)(150 + 105 * lit)) : with_alpha(IM_COL32(150, 116, 210, 255), 170);
            dl->AddLine(S.p(st.x + cosf(a) * r0, st.y + sinf(a) * r0), S.p(st.x + cosf(a) * r1, st.y + sinf(a) * r1), rc,
                        S.k(0.6f) > 1.5f ? S.k(0.6f) : 1.5f);
        }
    // the hood: a half bowl, its back away from the socket, a light along it
    ImVec2 c = S.p(cx, cy);
    dl->PathArcTo(ImVec2(c.x + S.k(0.5f), c.y + S.k(0.8f)), S.k(R), aim + K_PI * 0.5f, aim + K_PI * 1.5f, 32);
    dl->PathFillConvex(IM_COL32(0, 0, 0, 60));
    dl->PathArcTo(c, S.k(R), aim + K_PI * 0.5f, aim + K_PI * 1.5f, 32);
    dl->PathFillConvex(with_alpha(IM_COL32(126, 130, 140, 255), al));
    dl->PathArcTo(c, S.k(R * 0.78f), aim + K_PI * 0.75f, aim + K_PI * 1.15f, 16);
    dl->PathStroke(with_alpha(IM_COL32(196, 200, 210, 255), al), 0, S.k(0.7f));
    dl->PathArcTo(c, S.k(R), aim + K_PI * 0.5f, aim + K_PI * 1.5f, 32);
    dl->PathStroke(with_alpha(COL_METAL_LINE, al), 0, 1.2f);
    // the joint under it
    dl->AddCircleFilled(S.p(jx, jy), S.k(1.5f), with_alpha(IM_COL32(70, 72, 78, 255), al), 20);
    dl->AddCircleFilled(S.p(jx - 0.4f, jy - 0.4f), S.k(0.5f), with_alpha(COL_METAL_HI, al), 12);
    // the opening seen at an angle: the bright inside of the reflector, the bulb
    float rot = aim;   // the short axis of the ellipse along `aim`
    ImU32 inside = lit > 0 ? mix(IM_COL32(214, 218, 228, 255), IM_COL32(226, 200, 255, 255), lit) : IM_COL32(206, 210, 220, 255);
    dl->AddEllipseFilled(c, ImVec2(S.k(R * 0.24f), S.k(R)), with_alpha(inside, al), rot, 40);
    dl->AddEllipse(c, ImVec2(S.k(R * 0.24f), S.k(R)), with_alpha(COL_METAL_LINE, al), rot, 40, 1.2f);
    ImU32 bulb = lit > 0 ? mix(IM_COL32(176, 128, 255, 255), IM_COL32(250, 240, 255, 255), lit)
                         : hover && !dim ? IM_COL32(220, 200, 250, 255) : IM_COL32(194, 182, 226, 255);
    ImVec2 bc = S.p(cx + f.x * 0.9f, cy + f.y * 0.9f);
    if (lit > 0) dl->AddCircleFilled(bc, S.k(4.6f), with_alpha(COL_UV, (int)(80 * lit)), 32);
    dl->AddCircleFilled(bc, S.k(2.3f), with_alpha(bulb, al), 32);
    dl->AddCircle(bc, S.k(2.3f), with_alpha(IM_COL32(120, 100, 160, 255), al), 32, 1.0f);
    dl->AddCircleFilled(ImVec2(bc.x - S.k(0.7f), bc.y - S.k(0.8f)), S.k(0.6f), IM_COL32(255, 255, 255, dim ? 80 : 210), 16);
}

// ---------------------------------------------------------------- Erase

// The EPROM is erased at once (atomic with the check of Vpp, so nothing the program does meanwhile is lost or erased
// later); the lamp then shines on its window for ERASE_S: the picture only.
struct EraseRun {
    std::string card;    // the DSE-1 whose EPROM is under the lamp, "" = none
    std::string path;    // that EPROM (another one put in meanwhile does not glow)
    uint64_t gen = 0;    // of this machine (after a switch of the machine or a loaded state nothing glows)
    uint64_t t0 = 0;     // the lamp went on (SDL_GetTicksNS)
};
static EraseRun s_erase;
static std::string s_ask_card;   // Erase a programmed EPROM? asked in a modal window
static bool s_ask_open = false;
static std::string s_card;       // the DSE-1 chosen in the list of the window (more than one in the machine)

// The DSE-1 the window shows: the one chosen in its list, else the first one of the machine; count = how many.
static Dse1* dse1_pick(UiState& ui, int* count)
{
    Machine* m = ui.host->machine();
    Dse1 *first = nullptr, *chosen = nullptr;
    int n = 0;
    if (m)
        for (auto& c : m->cards)
            if (Dse1* d = dynamic_cast<Dse1*>(c.get())) {
                if (!first) first = d;
                if (d->name == s_card) chosen = d;
                n++;
            }
    if (count) *count = n;
    return chosen ? chosen : first;
}

Card* dse1_card(UiState& ui)
{
    return dse1_pick(ui, nullptr);
}

static bool eprom_blank(const Eprom27& e)
{
    for (uint8_t b : e.data)
        if (b != 0xFF) return false;
    return true;
}

static float erase_glow(void)
{
    if (s_erase.card.empty()) return 0.0f;
    double t = (double)(SDL_GetTicksNS() - s_erase.t0) / 1e9;
    if (t >= ERASE_S) return 1.0f;
    float up = (float)(t / 0.25);   // on in a quarter of a second, flickering a little
    float g = (up < 1.0f ? up : 1.0f) * (0.85f + 0.15f * sinf((float)t * 31.0f));
    return g;
}

// Erase refused: under voltage (Vpp other than 0 V, as changing the medium under voltage), else the reason of the card.
static void erase_refused(UiState& ui, Dse1* d, const std::string& err)
{
    int v = d->vpp_volts();
    std::string why = tr_core(err);
    if (v != 0) {
        char b[256];
        snprintf(b, sizeof(b), tr("The socket is under voltage (Vpp %d V): the EPROM cannot be taken out to erase it. "
                                  "Wait until the program has finished."), v);
        why = b;
    }
    ui_notice(ui, false, d->name + ": " + tr("Cannot erase the EPROM") + ":\n" + why);
}

// Erase now (the card checks Vpp again: the question may have stood open while the program raised it), the lamp
// shines on the window.
static void erase_start(UiState& ui, Dse1* d)
{
    std::string err;
    if (!d->erase(err)) {
        erase_refused(ui, d, err);
        return;
    }
    ui_notice(ui, true, d->name + ": " + tr("The EPROM is erased (all bytes FFh)."));
    s_erase.card = d->name;
    s_erase.path = d->eprom().path;
    s_erase.gen = ui.host->machine_generation();
    s_erase.t0 = SDL_GetTicksNS();
}

// The lamp clicked: not under voltage; a programmed EPROM asks first (its content is lost), a blank one goes at once.
static void erase_click(UiState& ui, Dse1* d)
{
    if (d->vpp_volts() != 0) erase_refused(ui, d, std::string());
    else if (eprom_blank(d->eprom())) erase_start(ui, d);
    else {
        s_ask_card = d->name;
        s_ask_open = true;
    }
}

// Every frame (also with the window closed): the lamp has shone long enough.
static void erase_poll(void)
{
    if (!s_erase.card.empty() && (double)(SDL_GetTicksNS() - s_erase.t0) / 1e9 >= ERASE_S) s_erase = EraseRun();
}

// The lamp shines on the EPROM in the socket of `d` now: the same machine, the same EPROM.
static bool erasing_now(UiState& ui, Dse1* d)
{
    return !s_erase.card.empty() && s_erase.card == d->name && s_erase.gen == ui.host->machine_generation() &&
           d->eprom().loaded() && s_erase.path == d->eprom().path;
}

static void erase_question(UiState& ui)
{
    if (s_ask_open) {
        ImGui::OpenPopup(tr("Erase the EPROM"));
        s_ask_open = false;
    }
    if (!ImGui::BeginPopupModal(tr("Erase the EPROM"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    Machine* m = ui.host->machine();
    Dse1* d = m ? dynamic_cast<Dse1*>(m->find_card(s_ask_card)) : nullptr;
    bool can = d && d->eprom().loaded();
    // the lines broken at about 34 characters' width: a long path does not make the window wider than the screen
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 34.0f * ImGui::GetFontSize());
    if (can) {
        ImGui::TextWrapped("%s", tr("The EPROM in the socket goes under the UV lamp. All its content is lost: the file"));
        std::string p = data_relative(d->eprom().path);
#ifdef _WIN32
        for (char& c : p) if (c == '/') c = '\\';
#endif
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", p.c_str());
        ImGui::PopStyleColor();
        ImGui::TextWrapped("%s", tr("becomes all FFh, as a new blank EPROM."));
    } else ImGui::TextUnformatted(tr("The socket is empty."));
    ImGui::PopTextWrapPos();
    ImGui::BeginDisabled(!can);
    if (ImGui::Button(tr("Erase"))) {
        erase_start(ui, d);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    // Esc = Cancel; Enter does nothing: Erase throws the content away. The keyboard navigation starts at Cancel too.
    bool cancel = ImGui::Button(tr("Cancel"));
    ImGui::SetItemDefaultFocus();
    if (cancel || popup_escape()) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ---------------------------------------------------------------- the drawing

// The box of the cable X2 with the socket, the EPROM and the LEDs, the UV lamp beside it; the picture of the machine
// previews. Returns the bottom of the drawing (screen y).
static float draw_box(UiState& ui, Dse1* d)
{
    float u = ImGui::GetFontSize();
    float avail = ImGui::GetContentRegionAvail().x;
    // as wide as the window, smaller when the controls under it would not fit its height
    float below = 3.6f * ImGui::GetFrameHeightWithSpacing() + 0.3f * u;
    float s = avail / SCENE_W, sh = (ImGui::GetContentRegionAvail().y - below) / SCENE_H;
    if (s > sh) s = sh;
    if (s > 0.62f * u) s = 0.62f * u;
    if (s < 0.25f * u) s = 0.25f * u;
    ImVec2 o = ImGui::GetCursorScreenPos();
    if (avail > SCENE_W * s) o.x += (avail - SCENE_W * s) * 0.5f;
    Scene S = { ImGui::GetWindowDrawList(), o, s };
    ImDrawList* dl = S.dl;
    const Eprom27& e = d->eprom();
    bool has = e.loaded();
    bool erasing = erasing_now(ui, d);
    float glow = erasing ? erase_glow() : 0.0f;
    // the label goes on a programmed EPROM when the programming has ended (not during a run, not at 25 V)
    bool programming = d->run().active || d->vpp_volts() == 25;
    bool blank = has && eprom_blank(e);

    // the cable X2 from the card: a grey ribbon with its red wire 1 coming from the left, fading out
    const float bx0 = 10.0f, by0 = 8.0f, bx1 = 84.0f, by1 = 56.0f;   // the box
    const float ry0 = 29.0f, ry1 = 39.0f;
    dl->AddRectFilled(S.p(0, ry0 + 0.5f), S.p(bx0 + 1.0f, ry1 + 0.8f), IM_COL32(0, 0, 0, 50));
    dl->AddRectFilled(S.p(0, ry0), S.p(bx0 + 1.0f, ry1), IM_COL32(176, 178, 182, 255));
    for (float y = ry0 + 0.6f; y < ry1; y += 0.65f) dl->AddLine(S.p(0, y), S.p(bx0 + 1.0f, y), IM_COL32(150, 152, 158, 255), 1.0f);
    dl->AddRectFilled(S.p(0, ry0), S.p(bx0 + 1.0f, ry0 + 0.6f), IM_COL32(186, 52, 44, 255));
    ImU32 bg = ImGui::GetColorU32(ImGuiCol_WindowBg), clear = bg & 0x00FFFFFFu;
    dl->AddRectFilledMultiColor(S.p(-0.1f, ry0 - 0.2f), S.p(6.0f, ry1 + 1.0f), bg, clear, clear, bg);
    if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(S.p(0, ry0), S.p(bx0, ry1)))
        tip(tr("The programming cable X2 from the DSE-1 card (connector FRB-30)."));

    // the box: its shadow, the side, the top
    dl->AddRectFilled(S.p(bx0 + 1.0f, by0 + 1.5f), S.p(bx1 + 1.4f, by1 + 3.6f), IM_COL32(0, 0, 0, 55), S.k(3.0f));
    dl->AddRectFilled(S.p(bx0, by0), S.p(bx1, by1 + 2.4f), COL_BOX_SIDE, S.k(3.0f));
    dl->AddRectFilled(S.p(bx0, by0), S.p(bx1, by1), COL_BOX, S.k(3.0f));
    dl->AddRectFilled(S.p(bx0 + 1.0f, by0 + 0.4f), S.p(bx1 - 1.0f, by0 + 1.0f), COL_BOX_HI, S.k(0.3f));
    dl->AddRect(S.p(bx0, by0), S.p(bx1, by1 + 2.4f), COL_BOX_LINE, S.k(3.0f), 0, 1.0f);
    dl->AddLine(S.p(bx0 + 1.5f, by1), S.p(bx1 - 1.5f, by1), with_alpha(COL_BOX_LINE, 160), 1.0f);
    // the edge of the lid: a groove a little inside, lit on its lower side
    float px1 = 1.0f / s;   // one pixel
    dl->AddRect(S.p(bx0 + 1.6f + px1, by0 + 1.6f + px1), S.p(bx1 - 1.6f + px1, by1 - 1.6f + px1), with_alpha(COL_BOX_HI, 170),
                S.k(2.0f), 0, 1.0f);
    dl->AddRect(S.p(bx0 + 1.6f, by0 + 1.6f), S.p(bx1 - 1.6f, by1 - 1.6f), with_alpha(COL_BOX_SIDE, 190), S.k(2.0f), 0, 1.0f);
    dl->AddRectFilled(S.p(bx0 - 0.8f, ry0 - 1.2f), S.p(bx0 + 2.2f, ry1 + 1.2f), IM_COL32(48, 50, 54, 255), S.k(0.8f));   // the grommet
    screw(S, bx0 + 3.5f, by0 + 3.5f);
    screw(S, bx1 - 3.5f, by0 + 3.5f);
    screw(S, bx0 + 3.5f, by1 - 3.5f);
    screw(S, bx1 - 3.5f, by1 - 3.5f);
    text_mm(S, 17.0f, 50.4f, 3.2f, COL_PRINT, "DSE-1", -1, true);
    text_mm(S, 17.0f + text_w_mm(S, 3.2f, "DSE-1", true) + 1.6f, 50.4f, 2.6f, COL_PRINT, "X2", -1);

    // the socket, its lever, the EPROM
    const float zx0 = 16.0f, zx1 = 62.0f, zy0 = 23.0f, zy1 = 45.0f, cx = 42.5f, cy = 34.0f;
    zif_socket(S, zx0, zy0, zx1, zy1, cy, cx - 13.97f, !has);
    if (has) eprom(S, cx, cy, e.type, !erasing && !blank && !programming, e.path, glow);
    zif_lever(S, ImVec2(zx0 + 3.5f, zy0 - 1.2f), zx1 - zx0 - 4.5f, !has);
    if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(S.p(zx0, zy0), S.p(zx1, zy1))) {
        int c = d->cable();
        const char* wiring = c == Dse1::CABLE_DUAL ? tr("two sockets (replica): the EPROM sits in the socket of its type")
                           : c == Dse1::CABLE_2732 ? tr("wired for a 2732 (modified after DSE.DOC)")
                                                   : tr("wired for a 2716 (as supplied)");
        std::string t = std::string(tr("Socket of the programming cable X2")) + ", " + wiring + ".";
        if (has) {
            char b[64];
            snprintf(b, sizeof(b), "\nEPROM %d: ", e.type);
            std::string n = e.path;
            size_t sl = n.find_last_of("/\\");
            t += b + (sl == std::string::npos ? n : n.substr(sl + 1));
            if (blank) t += std::string("\n") + tr("Blank (all FFh): the chip is seen through its quartz window.");
            else if (programming)
                t += std::string("\n") + tr("Being programmed: the label with the name of its file goes on its window when "
                                            "the programming has ended.");
            else t += std::string("\n") + tr("Programmed: the label on its window has the name of its file.");
        } else t += std::string("\n") + tr("Empty: Insert... or New 2716 / New 2732 below, or drop a .bin file here.");
        tip(t.c_str());
    }

    // the LEDs of the cable
    const float lx = 69.5f;
    led(S, lx, 27.0f, d->led1(), IM_COL32(255, 48, 34, 255), tr("LED1 PROG (\"indikace progr.\", register bit D4)"));
    led(S, lx, 39.0f, d->led2(), IM_COL32(70, 232, 86, 255),
        tr("LED2 MAP (\"indikace map.\", register bit D5, with JP1 also MAP1)"));
    text_mm(S, lx + 4.0f, 27.0f, 2.7f, COL_PRINT, "PROG", -1);
    text_mm(S, lx + 4.0f, 39.0f, 2.7f, COL_PRINT, "MAP", -1);

    // the UV lamp: a button
    const float ux = 93.0f, uy = 22.0f;
    ImVec2 b0 = S.p(ux - 10.0f, uy - 11.0f), b1 = S.p(ux + 12.5f, uy + 34.0f);
    ImGui::SetCursorScreenPos(b0);
    bool usable = has && !erasing;
    ImGui::BeginDisabled(!usable);
    bool clicked = ImGui::InvisibleButton("##uv", ImVec2(b1.x - b0.x, b1.y - b0.y));
    ImGui::EndDisabled();
    bool hover = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    std::string cap = std::string("UV \xC2\xB7 ") + tr("ERASE");
    uv_lamp(S, ux, uy, cx, cy, glow, hover && usable, !usable && !erasing, cap.c_str());
    if (usable && hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (hover)
        tip(erasing ? tr("The UV lamp shines on the window of the EPROM...")
            : has   ? tr("Erase: the EPROM under the UV lamp, then all its bytes are FFh. An emulator comfort: a real EPROM goes "
                         "out of the socket under a UV lamp for 15-20 minutes. Not while the socket is under voltage (Vpp).")
                    : tr("Erase with UV light: there is no EPROM in the socket."));
    if (clicked) erase_click(ui, d);

    float bottom = S.p(0, SCENE_H).y;
    if (picture_only()) picture_mark(o.x - 0.25f * u, o.y - 0.25f * u, o.x + SCENE_W * s + 0.25f * u, bottom + 0.25f * u);
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x, bottom));
    ImGui::Dummy(ImVec2(avail, 0.3f * u));
    return bottom;
}

// ---------------------------------------------------------------- the controls

// New 2716 / New 2732: a blank EPROM (all FFh) in work/dse-1/ goes into the socket
static void dse_new(UiState& ui, Dse1* d, int type)
{
    std::string rel, err;
    if (Dse1::new_blank(type, "work/dse-1", rel, err)) media_set(ui, d, 0, rel);
    else ui_notice(ui, false, tr_core(err));
}

// On this line after the last item, or on the next one when the window is too narrow for an item `w` wide.
static void flow(float w)
{
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < w) ImGui::NewLine();
}

static float button_w(const char* label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x + 2.0f * ImGui::GetStyle().FramePadding.x;
}

// A tooltip of the state line: what it says, and that the box itself shows only its LEDs.
static void info_tip(const char* what)
{
    if (ImGui::IsItemHovered())
        tipf("%s\n%s", what, tr("Emulator information: the box of the cable has only the LEDs PROG and MAP."));
}

static void draw_controls(UiState& ui, Dse1* d)
{
    const Eprom27& e = d->eprom();
    bool has = e.loaded();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr("EPROM in the socket:"));
    if (!has) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", tr("(socket empty)"));
    } else {   // a long name on its own line, the type after it where it fits
        size_t sl = e.path.find_last_of("/\\");
        flow(ImGui::CalcTextSize(e.path.c_str() + (sl == std::string::npos ? 0 : sl + 1)).x);
        file_name_text(e.path);
        char t[16];
        snprintf(t, sizeof(t), "(%d)", e.type);
        flow(ImGui::CalcTextSize(t).x);
        ImGui::TextDisabled("%s", t);
    }
    int wired = d->socket_wiring() == Dse1::CABLE_2732 ? Eprom27::T2732 : Eprom27::T2716;
    if (has && e.type != wired) {
        char b[256];
        snprintf(b, sizeof(b), tr("A %d in the socket wired for a %d (cable X2): it does not read or program as it should."),
                 e.type, wired);
        ImGui::PushStyleColor(ImGuiCol_Text, theme().warn);
        ImGui::TextWrapped("%s", b);
        ImGui::PopStyleColor();
    }
    if (ImGui::Button(tr("Insert..."))) tape_dialog(ui, 16, d->name);
    if (ImGui::IsItemHovered()) tip(tr("An EPROM image .bin into the socket: 2 KB = 2716, 4 KB = 2732. A file can also be dropped on this window."));
    const char* blank = tr("A new blank EPROM (all bits 1, as erased by UV light) in work/dse-1/ goes into the socket.");
    flow(button_w(tr("New 2716")));
    if (ImGui::Button(tr("New 2716"))) dse_new(ui, d, Eprom27::T2716);
    if (ImGui::IsItemHovered()) tip(blank);
    flow(button_w(tr("New 2732")));
    if (ImGui::Button(tr("New 2732"))) dse_new(ui, d, Eprom27::T2732);
    if (ImGui::IsItemHovered()) tip(blank);
    flow(button_w(tr("Eject")) + button_w("...") + ImGui::GetStyle().ItemSpacing.x);
    ImGui::BeginDisabled(!has);
    if (ImGui::Button(tr("Eject"))) media_set(ui, d, 0, "");
    ImGui::EndDisabled();
    media_more(ui, d, 0);
    flow(button_w(tr("Save simulator RAM...")));
    if (ImGui::Button(tr("Save simulator RAM..."))) tape_dialog(ui, 17, d->name);
    if (ImGui::IsItemHovered())
        tip(tr("Emulator help, not on the real board: the 4 KB of the simulator RAM into a .bin file."));

    // the state of the card
    float gap = 2.0f * ImGui::GetStyle().ItemSpacing.x;
    int v = d->vpp_volts();
    char t[96];
    snprintf(t, sizeof(t), "Vpp %d V", v);
    if (v == 25) ImGui::TextColored(theme().error, "%s", t);
    else ImGui::TextUnformatted(t);
    info_tip(tr("The voltage on the Vpp pin of the socket: 0 V, 5 V for reading, 25 V for programming."));
    const char* line = d->onla() && d->onld() ? "ON LINE" : d->onla() ? "ON LINE (ONLA)" : d->onld() ? "ON LINE (ONLD)" : "OFF LINE";
    ImGui::SameLine(0, gap);
    if (d->onla() || d->onld()) ImGui::TextColored(theme().ok, "%s", line);
    else ImGui::TextDisabled("%s", line);
    info_tip(tr("ON LINE: the simulator RAM is given to the probe X3 (in the emulator it is not plugged in anywhere); "
                "OFF LINE: the computer writes and reads it."));
    if (d->last_pulse_ms() > 0) snprintf(t, sizeof(t), "%s %.2f ms", tr("Last programming pulse:"), d->last_pulse_ms());
    else snprintf(t, sizeof(t), "%s", tr("No programming pulse yet"));
    ImGui::SameLine(0, gap);
    if (ImGui::GetContentRegionAvail().x < ImGui::CalcTextSize(t).x) ImGui::NewLine();
    if (d->last_pulse_ms() > 0) ImGui::TextUnformatted(t);
    else ImGui::TextDisabled("%s", t);
    info_tip(tr("The length of the last programming pulse, measured in the time of the machine (the data sheet asks for "
                "50 ms; 1 ms or longer programs in the emulator, an assumption)."));
    if (d->pulse_on()) {
        ImGui::SameLine(0, gap);
        ImGui::TextColored(theme().error, "%s", tr("pulse on"));
    }
    const Dse1::Run& r = d->run();
    if (r.active) {
        snprintf(t, sizeof(t), "%s %u", tr("pulses while programming:"), r.pulses);
        ImGui::SameLine(0, gap);
        if (ImGui::GetContentRegionAvail().x < ImGui::CalcTextSize(t).x) ImGui::NewLine();
        ImGui::TextDisabled("%s", t);
        info_tip(tr("The programming pulses of 1 ms or longer since the programming began."));
    }
}

// ---------------------------------------------------------------- the window

bool dse1_in_machine(UiState& ui)
{
    Machine* m = ui.host->machine();
    if (m)
        for (auto& c : m->cards)
            if (dynamic_cast<Dse1*>(c.get())) return true;
    return false;
}

bool dse1_drop_target(float x, float y)
{
    if (x < 0 || y < 0) return false;
    ImGuiWindow* w = ImGui::FindWindowByName("###dse1");
    if (!w || !w->WasActive || w->Hidden) return false;
    ImGuiWindow *hovered = nullptr, *under = nullptr;
    ImGui::FindHoveredWindowEx(ImVec2(x, y), true, &hovered, &under);
    return hovered && hovered->RootWindow == w;
}

// More DSE-1 cards in the machine: the list of them, the window (and a file dropped on it) is for the chosen one.
static void card_choice(UiState& ui, Dse1* d)
{
    Machine* m = ui.host->machine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr("Card"));
    ImGui::SameLine();
    float w = 0;
    for (auto& c : m->cards)
        if (dynamic_cast<Dse1*>(c.get())) {
            float cw = ImGui::CalcTextSize(c->name.c_str()).x;
            if (cw > w) w = cw;
        }
    ImGui::SetNextItemWidth(w + ImGui::GetFrameHeight() + 2.0f * ImGui::GetStyle().FramePadding.x);
    if (ImGui::BeginCombo("##card", d->name.c_str())) {
        for (auto& c : m->cards)
            if (dynamic_cast<Dse1*>(c.get())) {
                ImGui::PushID(c.get());
                if (ImGui::Selectable(c->name.c_str(), c.get() == d)) s_card = c->name;
                ImGui::PopID();
            }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        tip(tr("This machine has more DSE-1 cards: the window shows the programming cable of the one chosen here (a "
               ".bin dropped on the window goes into its socket too)."));
}

void draw_dse1(UiState& ui)
{
    erase_poll();
    erase_question(ui);
    Machine* m = ui.host->machine();
    int cards = 0;
    Dse1* d = dse1_pick(ui, &cards);
    if (m && ui.dse1_gen != ui.host->machine_generation()) {   // a machine with the programmer opens it
        ui.dse1_gen = ui.host->machine_generation();
        if (d) ui.show_dse1 = true;
    }
    if (!ui.show_dse1) return;
    layout_window(LW_DSE1);   // on the right: the screen of SAPI with the program DSE stays in sight
    // the title in the UI language, the id stays (the saved layout)
    std::string title = std::string(tr("EPROM programmer DSE-1")) + "###dse1";
    if (!ImGui::Begin(title.c_str(), &ui.show_dse1) || device_window_off(ui)) {
        ImGui::End();
        return;
    }
    if (!d) {
        ImGui::TextWrapped("%s", device_missing(ui, "dse1").c_str());
        ImGui::End();
        return;
    }
    ImGui::PushID(d);
    if (cards > 1) card_choice(ui, d);
    draw_box(ui, d);
    draw_controls(ui, d);
    ImGui::PopID();
    ImGui::End();
}
