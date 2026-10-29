// Default places of the windows (docs/vydani/plan-vzhled.md, stage 5): one helper for all windows. A window takes
// its place and size at its first use (ImGuiCond_FirstUseEver), so a place saved in imgui.ini always wins; View ->
// Default window layout lets every window take its default place again. The work area is the main window without
// the menu bar and nothing goes outside it:
// - the main output (the console screen, or the display named by "main_display" in the machine description) at the
//   top left, at the largest integer zoom that fits about 65 % of the width, at most about 200 % (planned by app.cpp,
//   layout_main);
// - the other displays next to it (below it, else right of it) at the largest integer zoom up to that cap that has free
//   room, each window as large as its picture with a small border;
// - the instruments, Media and the tools in the free room, the column at the right edge first; the guide next to the
//   displays, never over an instrument or Media nor under one: beside it, below it or wherever there is room, rather
//   smaller than over the main output; Media not over the main output either when it fits elsewhere;
// - the configurator, when the layout opens it (layout_window_if_room), only in the free room at a readable size;
// - the dialogs (Choose a machine, Settings...) in the middle of the work area, never over the menu bar; Choose a
//   machine about 75 % x 80 % of it (at least 58 x 38 font heights);
// - a window more than half outside the work area (the main window got smaller, a saved place from a larger screen),
//   a dialog any part outside it, a display any part when it fits whole, back inside it at the start and after the main
//   window changes its size, smaller only when it does not fit (layout_keep_inside); a dialog partly outside also when
//   it opens (layout_window).
// Where there is no room (a small window, a big instrument), a window goes where it covers the least of what matters:
// the instruments and Media most, then the main output, the console screen, the other displays, the tools.
// The docked layout (View -> Settings -> Windows: docked, layout_dock at the end) puts the windows into the dock space
// instead; only the dialogs and the configurator take the places above.
#include "frontend/ui.h"

#include <stdio.h>
#include <string.h>
#include "imgui.h"
#include "imgui_internal.h"   // the windows of this frame and their saved settings

namespace {

enum Role { ROLE_DISPLAY, ROLE_INSTRUMENT, ROLE_MEDIA, ROLE_GUIDE, ROLE_TOOL, ROLE_DIALOG };

struct WinDef {
    const char* id;      // the window id: the part of its title from ### on
    int role;
    float w, h;          // the size it asks for in font heights (16 px at 100 %); h 0 = the height of the work area
    float min_w, min_h;  // smaller it is of no use: then it goes over other windows
    float max_w;         // wider when it has to be shorter (the text of the guide), 0 = w
    int cascade;         // its step down from the top right corner when there is no room (tool windows)
};

// in the order of LayoutWin (ui.h)
const WinDef k_wins[LW_COUNT] = {
    { "###Machine guide", ROLE_GUIDE, 30, 36, 24, 12, 45, 0 },
    { "###Media", ROLE_MEDIA, 48, 28, 32, 12, 0, 1 },
    { "###Minigraf", ROLE_INSTRUMENT, 30, 42, 24, 20, 0, 0 },
    { "###APS-5: panel and furnaces", ROLE_INSTRUMENT, 62, 50, 40, 24, 0, 0 },
    { "###IMS-2 measuring bench", ROLE_INSTRUMENT, 40, 56, 30, 24, 0, 0 },
    { "###Diagnostic board (K3)", ROLE_INSTRUMENT, 17, 16, 17, 16, 0, 1 },
    { "###Debugger", ROLE_TOOL, 35, 45, 26, 20, 0, 0 },
    { "###Memory", ROLE_TOOL, 46, 30, 30, 12, 0, 1 },   // a whole row of 16 bytes with their text (mono font)
    { "###I/O log", ROLE_TOOL, 26, 25, 20, 12, 0, 2 },
    { "###Serial terminal", ROLE_TOOL, 44, 29, 30, 14, 0, 3 },
    { "###Printer", ROLE_TOOL, 45, 30, 30, 14, 0, 4 },
    { "###Log", ROLE_TOOL, 38, 16, 26, 8, 0, 5 },
    { "###rack", ROLE_TOOL, 62, 0, 44, 26, 0, 0 },
    { "###Choose a machine", ROLE_DIALOG, 58, 38, 0, 0, 0, 0 },
    { "###Settings", ROLE_DIALOG, 40, 56, 0, 0, 0, 0 },   // its sections without scrolling where there is room
    { "###Keyboard shortcuts", ROLE_DIALOG, 36, 24, 0, 0, 0, 0 },
    { "###About SAPIemu", ROLE_DIALOG, 0, 0, 0, 0, 0, 0 },   // its own size (AlwaysAutoResize)
    { "###Report a bug", ROLE_DIALOG, 0, 0, 0, 0, 0, 0 },    // the same
    { "###Help", ROLE_DIALOG, 50, 44, 0, 0, 0, 0 },          // the documents: text to read, wide tables, pictures
};

const int MAX_DISPLAY_IDS = 5;   // ###screen, ###gfx0..3 (app.cpp)
const int MAX_OBSTACLES = 16;   // more open windows are not taken into account (the search grows with n^5)

ImGuiID s_ids[LW_COUNT];
ImGuiID s_display_ids[MAX_DISPLAY_IDS];
bool s_ids_ready = false;

// the main output planned for this frame (app.cpp)
ImGuiID s_main_id = 0;
float s_main_w = 0, s_main_h = 0;
int s_gfx_count = 0;   // the graphic display windows of the machine (###gfx0...)
bool s_hold = false;   // no first places now (layout_hold: full screen, a minimized window)
bool s_rack_low = false;   // the configurator opened by the layout: no obstacle (layout_rack_low)

// A bit of the masks of find_place beside the roles: the main output (the guide and Media keep clear of it first).
const unsigned MASK_MAIN = 1u << 8;

struct Box {
    float x0, y0, x1, y1;
};

struct Obstacle {
    Box b;
    int role;
    float weight;   // how much covering it costs (see the head of the file)
    bool main;      // the main output
};

void ids_init(void)
{
    if (s_ids_ready) return;
    for (int i = 0; i < LW_COUNT; i++) s_ids[i] = ImHashStr(k_wins[i].id);
    s_display_ids[0] = ImHashStr("###screen");
    for (int i = 1; i < MAX_DISPLAY_IDS; i++) {
        char id[16];
        snprintf(id, sizeof(id), "###gfx%d", i - 1);
        s_display_ids[i] = ImHashStr(id);
    }
    s_ids_ready = true;
}

int role_of(ImGuiID id)
{
    ids_init();
    for (int i = 0; i < LW_COUNT; i++)
        if (s_ids[i] == id) return k_wins[i].role;
    for (int i = 0; i < MAX_DISPLAY_IDS; i++)
        if (s_display_ids[i] == id) return ROLE_DISPLAY;
    return ROLE_TOOL;   // the style editor and other windows without a default place
}

float weight_of(ImGuiID id, int role)
{
    switch (role) {
    case ROLE_INSTRUMENT:
    case ROLE_MEDIA: return 16.0f;
    case ROLE_DISPLAY: return id == s_main_id ? 4.0f : (id == s_display_ids[0] ? 3.0f : 2.0f);
    case ROLE_GUIDE: return 2.0f;
    default: return 1.0f;
    }
}

// The window takes its first place in this frame: it does not exist yet and imgui.ini has no place for it, or View
// -> Default window layout let it take the default one again.
bool first_use(ImGuiID id)
{
    if (ImGuiWindow* w = ImGui::FindWindowByID(id)) return (w->SetWindowPosAllowFlags & ImGuiCond_FirstUseEver) != 0;
    return ImGui::FindWindowSettingsByID(id) == nullptr;
}

float minf(float a, float b) { return a < b ? a : b; }
float maxf(float a, float b) { return a > b ? a : b; }

// The main window below the menu bar of this frame (WorkPos is that of the last frame: none at the first one), less
// the gap at its edges.
Box work_area(float gap)
{
    ImRect r = ((ImGuiViewportP*)ImGui::GetMainViewport())->GetBuildWorkRect();
    Box b = { r.Min.x + gap, r.Min.y + gap, r.Max.x - gap, r.Max.y - gap };
    if (b.x1 < b.x0 + 1.0f) b.x1 = b.x0 + 1.0f;
    if (b.y1 < b.y0 + 1.0f) b.y1 = b.y0 + 1.0f;
    return b;
}

Box main_box(const Box& wa)
{
    return { wa.x0, wa.y0, wa.x0 + minf(s_main_w, wa.x1 - wa.x0), wa.y0 + minf(s_main_h, wa.y1 - wa.y0) };
}

// The windows on the screen now except `self`, the dialogs (they come and go), the invisible fallback window of ImGui
// and the windows that take their first place later in this frame (after View -> Default window layout). The display
// windows of the machine not drawn yet in this frame (the main output goes first): at their places, those only in
// imgui.ini at their saved places; the main output as planned.
int obstacles(ImGuiID self, const Box& wa, Obstacle* out)
{
    ImGuiContext& g = *GImGui;
    int n = 0;
    bool seen[MAX_DISPLAY_IDS] = {};
    for (ImGuiWindow* w : g.Windows) {
        if (n >= MAX_OBSTACLES - MAX_DISPLAY_IDS - 1) break;
        if (w->ID == self || w->IsFallbackWindow || !(w->Active || w->WasActive)) continue;
        if (w->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar))
            continue;   // menus, popups, the menu bar, the dock space and dock node hosts
        if ((w->Flags & ImGuiWindowFlags_ChildWindow) && !w->DockIsActive) continue;   // docked windows are children too
        if (w->SetWindowPosAllowFlags & ImGuiCond_FirstUseEver) continue;
        int role = role_of(w->ID);
        if (role == ROLE_DIALOG) continue;
        if (s_rack_low && w->ID == s_ids[LW_RACK]) continue;   // the lowest priority: the windows opening later cover it
        for (int i = 0; i < MAX_DISPLAY_IDS; i++)
            if (w->ID == s_display_ids[i]) seen[i] = true;
        out[n++] = { { w->Pos.x, w->Pos.y, w->Pos.x + w->Size.x, w->Pos.y + w->Size.y }, role, weight_of(w->ID, role),
                     w->ID == s_main_id };
    }
    ImVec2 vp = ImGui::GetMainViewport()->Pos;
    for (int i = 0; i <= s_gfx_count && i < MAX_DISPLAY_IDS; i++) {
        ImGuiID id = s_display_ids[i];
        if (seen[i] || id == self) continue;
        Box b;
        if (ImGuiWindow* w = ImGui::FindWindowByID(id)) {
            if (w->SetWindowPosAllowFlags & ImGuiCond_FirstUseEver) {   // it takes its first place later
                if (id != s_main_id || s_main_w <= 0) continue;
                b = main_box(wa);
            } else {
                b = { w->Pos.x, w->Pos.y, w->Pos.x + w->Size.x, w->Pos.y + w->Size.y };
            }
        } else if (ImGuiWindowSettings* s = ImGui::FindWindowSettingsByID(id)) {
            if (s->Size.x <= 0 || s->Size.y <= 0) continue;
            b = { vp.x + s->Pos.x, vp.y + s->Pos.y, vp.x + s->Pos.x + s->Size.x, vp.y + s->Pos.y + s->Size.y };
        } else if (id == s_main_id && s_main_w > 0) {
            b = main_box(wa);
        } else {
            continue;
        }
        out[n++] = { b, ROLE_DISPLAY, weight_of(id, ROLE_DISPLAY), id == s_main_id };
    }
    return n;
}

// the obstacle is one of the roles of `mask` (or the main output with MASK_MAIN)
bool in_mask(const Obstacle& o, unsigned mask)
{
    return (mask & (1u << o.role)) || (o.main && (mask & MASK_MAIN));
}

// a, grown by g on every side, overlaps b
bool overlaps(const Box& a, const Box& b, float g)
{
    return a.x0 - g < b.x1 && b.x0 < a.x1 + g && a.y0 - g < b.y1 && b.y0 < a.y1 + g;
}

float overlap_area(const Box& a, const Box& b)
{
    float w = minf(a.x1, b.x1) - maxf(a.x0, b.x0), h = minf(a.y1, b.y1) - maxf(a.y0, b.y0);
    return w > 0 && h > 0 ? w * h : 0.0f;
}

// Which place first of those equally good: the right, then the top (the instruments, Media, the tools); the displays the
// left, then the top (next to the main output, below it before right of it), the guide the top, then the left (next to
// the displays): the right edge stays free for the instruments, Media and the tools (the configurator).
enum { ORDER_RIGHT, ORDER_LEFT, ORDER_TOP };

struct Want {
    float w, h, min_w, min_h, max_w;
    bool size_first;   // an instrument, Media: its size before covering less (an APS-5 cut in half is no use)
    int order;         // which of the places as good: ORDER_RIGHT, ORDER_LEFT, ORDER_TOP
};

struct Pick {
    bool found = false, full = false;
    float cost = 0, area = 0, l = 0, r = 0, t = 0;
    Box b = {};
};

// Is the place c (whole: it has the size asked for) better than the best one so far? The guide and the displays
// prefer covering less of the other windows (weighted) to their size, the instruments and Media the other way round;
// then by want.order.
bool better(const Pick& p, const Want& want, bool full, float cost, float area, const Box& c)
{
    if (!p.found) return true;
    bool cheaper = cost < p.cost - 1.0f, same_cost = cost < p.cost + 1.0f;
    bool larger = (full && !p.full) || (full == p.full && area > p.area + 1.0f);
    bool same_size = full == p.full && area > p.area - 1.0f && area < p.area + 1.0f;
    if (want.size_first ? larger : cheaper) return true;
    if (!(want.size_first ? same_size : same_cost)) return false;
    if (want.size_first ? cheaper : larger) return true;
    if (!(want.size_first ? same_cost : same_size)) return false;
    if (want.order == ORDER_LEFT) return c.x0 < p.l - 0.5f || (c.x0 < p.l + 0.5f && c.y0 < p.t - 0.5f);
    if (want.order == ORDER_TOP) return c.y0 < p.t - 0.5f || (c.y0 < p.t + 0.5f && c.x0 < p.l - 0.5f);
    return c.x1 > p.r + 0.5f || (c.x1 > p.r - 0.5f && c.y0 < p.t - 0.5f);
}

// The best place in the work area clear of the obstacles whose role is in `mask` (with the gap), covering the least
// of the others: up to w x h (wider up to max_w when it has to be shorter), at least min_w x min_h. Its right edge
// is the right edge of the work area, the left edge of a window, or that of a window put against the left edge or
// right of a window; its top likewise. Each such corner with the room up to the next windows gives the largest and
// the smallest window. False: no such place.
bool find_place(const Box& wa, const Obstacle* ob, int n, unsigned mask, const Want& want, float gap, Box* out)
{
    float rs[3 * MAX_OBSTACLES + 3], ts[3 * MAX_OBSTACLES + 3], ls[MAX_OBSTACLES + 1], bs[MAX_OBSTACLES + 1];
    int nr = 0, nt = 0, nl = 0, nb = 0;
    auto add = [](float* a, int& k, float v, float lo, float hi) {
        if (v >= lo - 0.5f && v <= hi + 0.5f) a[k++] = v;
    };
    add(rs, nr, wa.x1, wa.x0, wa.x1);
    add(rs, nr, wa.x0 + want.w, wa.x0, wa.x1);
    add(rs, nr, wa.x0 + want.min_w, wa.x0, wa.x1);
    add(ts, nt, wa.y0, wa.y0, wa.y1);
    add(ts, nt, wa.y1 - want.h, wa.y0, wa.y1);
    add(ts, nt, wa.y1 - want.min_h, wa.y0, wa.y1);
    ls[nl++] = wa.x0;
    bs[nb++] = wa.y1;
    for (int i = 0; i < n; i++) {
        const Box& b = ob[i].b;
        add(rs, nr, b.x0 - gap, wa.x0 + 1.0f, wa.x1);
        add(rs, nr, b.x1 + gap + want.w, wa.x0 + 1.0f, wa.x1);
        add(rs, nr, b.x1 + gap + want.min_w, wa.x0 + 1.0f, wa.x1);
        add(ts, nt, b.y1 + gap, wa.y0, wa.y1 - 1.0f);
        add(ts, nt, b.y0 - gap - want.h, wa.y0, wa.y1 - 1.0f);
        add(ts, nt, b.y0 - gap - want.min_h, wa.y0, wa.y1 - 1.0f);
        add(ls, nl, b.x1 + gap, wa.x0, wa.x1 - 1.0f);
        add(bs, nb, b.y0 - gap, wa.y0 + 1.0f, wa.y1);
    }
    Pick best;
    float g = gap - 1.0f;   // the gap kept to the obstacles (a pixel less: the edges themselves are candidates)
    for (int ri = 0; ri < nr; ri++)
        for (int ti = 0; ti < nt; ti++)
            for (int li = 0; li < nl; li++)
                for (int bi = 0; bi < nb; bi++) {
                    float r = rs[ri], t = ts[ti], aw = r - ls[li], ah = bs[bi] - t;
                    if (aw < want.min_w - 0.5f || ah < want.min_h - 0.5f) continue;
                    float h = minf(want.h, ah), w = want.w;
                    if (h < want.h && want.max_w > want.w) w = minf(want.max_w, want.w * want.h / h);
                    w = minf(w, aw);
                    for (int v = 0; v < 2; v++) {   // the largest window, the smallest one
                        float cw = v ? want.min_w : w, ch = v ? want.min_h : h;
                        if (v && cw >= w - 0.5f && ch >= h - 0.5f) break;
                        Box c = { r - cw, t, r, t + ch };
                        bool clear = true;
                        float cost = 0;
                        for (int i = 0; i < n && clear; i++) {
                            if (in_mask(ob[i], mask)) clear = !overlaps(c, ob[i].b, g);
                            else cost += overlap_area(c, ob[i].b) * ob[i].weight;
                        }
                        if (!clear) continue;
                        bool full = cw >= want.w - 0.5f && ch >= want.h - 0.5f;
                        if (!better(best, want, full, cost, cw * ch, c)) continue;
                        best.found = true;
                        best.full = full;
                        best.cost = cost;
                        best.area = cw * ch;
                        best.l = c.x0;
                        best.r = r;
                        best.t = t;
                        best.b = c;
                    }
                }
    if (best.found) *out = best.b;
    return best.found;
}

// The roles a window keeps clear of when it cannot keep clear of all windows (it covers the others, the least of them).
unsigned hard_roles(int role)
{
    switch (role) {
    case ROLE_GUIDE: return (1u << ROLE_INSTRUMENT) | (1u << ROLE_MEDIA);
    case ROLE_MEDIA: return (1u << ROLE_INSTRUMENT) | (1u << ROLE_GUIDE);
    case ROLE_INSTRUMENT: return (1u << ROLE_INSTRUMENT) | (1u << ROLE_MEDIA);
    case ROLE_DISPLAY: return 1u << ROLE_DISPLAY;
    default: return 0;
    }
}

// The place of a window at its first use: the free room (the guide also smaller, down to 0.6 of its least size); else
// clear of the windows that matter for it, covering the least of the others (the guide and Media clear of the main
// output first, the guide also narrower and shorter rather than over an instrument; another display at least clear of
// the main output); else a tool window at the right edge: the first one at the top, the others from the middle of the
// height a step lower each (the top of the first one, the registers and buttons of the Debugger, stays in sight, and
// every title too); any other window where it covers the least. (The displays try their zooms first, layout_display.)
Box place(ImGuiID self, int role, Want want, int cascade, const Box& wa, float u)
{
    float gap = 0.5f * u, ww = wa.x1 - wa.x0, wh = wa.y1 - wa.y0;
    want.w = minf(want.w, ww);
    want.h = minf(want.h, wh);
    want.min_w = minf(want.min_w, want.w);
    want.min_h = minf(want.min_h, want.h);
    want.max_w = minf(maxf(want.max_w, want.w), ww);
    Obstacle ob[MAX_OBSTACLES];
    int n = obstacles(self, wa, ob);
    unsigned hard = hard_roles(role);
    bool guide = role == ROLE_GUIDE;
    Want small = want;
    small.min_w = 0.6f * want.min_w;
    small.min_h = 0.6f * want.min_h;
    Box b;
    if (find_place(wa, ob, n, ~0u, want, gap, &b)) return b;
    if (guide && find_place(wa, ob, n, ~0u, small, gap, &b)) return b;
    if (hard) {
        if (guide || role == ROLE_MEDIA) {
            if (find_place(wa, ob, n, hard | MASK_MAIN, want, gap, &b)) return b;
            if (guide && find_place(wa, ob, n, hard | MASK_MAIN, small, gap, &b)) return b;
        }
        if (find_place(wa, ob, n, hard, want, gap, &b)) return b;
        if (guide && find_place(wa, ob, n, hard, small, gap, &b)) return b;
        // another display over the other displays rather than over the main output (a game under its console)
        if (role == ROLE_DISPLAY && self != s_main_id && find_place(wa, ob, n, MASK_MAIN, want, gap, &b)) return b;
    }
    if (role == ROLE_TOOL) {
        float y = wa.y0 + (cascade > 0 ? 0.5f * wh + (float)(cascade - 1) * 1.75f * u : 0.0f), h = want.h;
        if (y + h > wa.y1) h = maxf(want.min_h, wa.y1 - y);
        if (y + h > wa.y1) y = maxf(wa.y0, wa.y1 - h);
        return { wa.x1 - want.w, y, wa.x1, y + h };
    }
    if (find_place(wa, ob, n, 0, want, gap, &b)) return b;
    return { wa.x1 - want.w, wa.y0, wa.x1, wa.y0 + want.h };
}

// The box moved the least into the work area wa, smaller where it does not fit.
Box inside(const Box& b, const Box& wa)
{
    float w = minf(b.x1 - b.x0, wa.x1 - wa.x0), h = minf(b.y1 - b.y0, wa.y1 - wa.y0);
    float x = maxf(wa.x0, minf(b.x0, wa.x1 - w)), y = maxf(wa.y0, minf(b.y0, wa.y1 - h));
    return { x, y, x + w, y + h };
}

// The window `id` there is to come back into the work area wa (layout_keep_inside): more than half of it outside, a
// dialog (Choose a machine, Settings...: its close button and its edges in reach) any part of it, a display any part
// when it fits whole (author 2026-10-29: no cut picture).
bool to_bring_inside(ImGuiID id, const Box& b, const Box& wa)
{
    float a = (b.x1 - b.x0) * (b.y1 - b.y0);
    if (a <= 0) return false;
    int role = role_of(id);
    bool out = b.x0 < wa.x0 - 0.5f || b.y0 < wa.y0 - 0.5f || b.x1 > wa.x1 + 0.5f || b.y1 > wa.y1 + 0.5f;
    if (role == ROLE_DIALOG) return out;
    if (role == ROLE_DISPLAY && out && b.x1 - b.x0 <= wa.x1 - wa.x0 + 0.5f && b.y1 - b.y0 <= wa.y1 - wa.y0 + 0.5f)
        return true;
    return overlap_area(b, wa) < 0.5f * a;
}

// Where such a window goes: moved the least into the work area with the gap of the layout (in), smaller only when it
// does not fit; a display that fits only without the gap into the whole work area wa (its picture keeps its zoom).
Box back_inside(ImGuiID id, const Box& b, const Box& wa, const Box& in)
{
    bool fits = b.x1 - b.x0 <= in.x1 - in.x0 + 0.5f && b.y1 - b.y0 <= in.y1 - in.y0 + 0.5f;
    return inside(b, role_of(id) == ROLE_DISPLAY && !fits ? wa : in);
}

// A floating window not taking its first place now: not docked, not the menu bar, a popup, a child, a dock node host.
bool floating_placed(ImGuiWindow* w)
{
    if (w->IsFallbackWindow || w->DockNodeAsHost || w->DockId || w->DockIsActive) return false;
    if (w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip |
                    ImGuiWindowFlags_NoTitleBar))
        return false;
    return (w->SetWindowPosAllowFlags & ImGuiCond_FirstUseEver) == 0;
}

// The window to box b now (the windows taking their first places in this frame see it: obstacles).
void window_to(ImGuiWindow* w, const Box& b)
{
    ImGui::SetWindowPos(w, ImVec2(b.x0, b.y0));
    ImGui::SetWindowSize(w, ImVec2(b.x1 - b.x0, b.y1 - b.y0));
    w->Size = w->SizeFull;
}

// The saved place of a window not created yet to box b (imgui.ini keeps it relative to the main window).
void settings_to(ImGuiWindowSettings* s, const Box& b)
{
    ImVec2 vp = ImGui::GetMainViewport()->Pos;
    s->Pos = ImVec2ih((short)(b.x0 - vp.x), (short)(b.y0 - vp.y));
    s->Size = ImVec2ih((short)(b.x1 - b.x0), (short)(b.y1 - b.y0));
    ImGui::MarkIniSettingsDirty();
}

Box settings_box(const ImGuiWindowSettings* s)
{
    ImVec2 vp = ImGui::GetMainViewport()->Pos;
    return { vp.x + s->Pos.x, vp.y + s->Pos.y, vp.x + s->Pos.x + s->Size.x, vp.y + s->Pos.y + s->Size.y };
}

// layout_keep_inside: the work area size seen in the last frames, for how many frames, and the one checked last
float s_seen_w = 0, s_seen_h = 0, s_kept_w = 0, s_kept_h = 0;
int s_seen_frames = 0;

void set_next(const Box& b)
{
    ImGui::SetNextWindowPos(ImVec2(b.x0, b.y0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(b.x1 - b.x0, b.y1 - b.y0), ImGuiCond_FirstUseEver);
}

// No first places now: layout_hold, or the main window too small to lay anything out (minimized: the ImGui backend
// gives it the size 0). A window keeps its first use and takes its place in the first normal frame.
bool held(const Box& wa, float u)
{
    return s_hold || wa.x1 - wa.x0 < 8.0f * u || wa.y1 - wa.y0 < 8.0f * u;
}

// A dialog opening now at a place partly outside the work area (author 2026-10-29: the lower edge of Choose a machine
// out of reach, it could not be resized): back inside with the gap `in` of the layout, smaller only when it does not
// fit (sized: not one that takes its own size). Not one open already (moved out by hand) or docked.
void dialog_inside(ImGuiID id, bool sized, const Box& in)
{
    Box b;
    if (ImGuiWindow* w = ImGui::FindWindowByID(id)) {
        if (w->WasActive || !floating_placed(w)) return;
        b = { w->Pos.x, w->Pos.y, w->Pos.x + w->SizeFull.x, w->Pos.y + w->SizeFull.y };
    } else {
        ImGuiWindowSettings* s = ImGui::FindWindowSettingsByID(id);
        if (!s || s->DockId || s->Size.x <= 0 || s->Size.y <= 0) return;
        b = settings_box(s);
    }
    if (!to_bring_inside(id, b, work_area(0.0f))) return;
    Box n = inside(b, in);
    ImGui::SetNextWindowPos(ImVec2(n.x0, n.y0));
    if (sized) ImGui::SetNextWindowSize(ImVec2(n.x1 - n.x0, n.y1 - n.y0));
}

} // namespace

void layout_hold(bool hold)
{
    s_hold = hold;
}

void layout_rack_low(bool low)
{
    s_rack_low = low;
}

bool layout_held(void)
{
    float u = ImGui::GetFontSize();
    return held(work_area(0.5f * u), u);
}

void layout_window(int which)
{
    if (which < 0 || which >= LW_COUNT) return;
    ids_init();
    const WinDef& d = k_wins[which];
    float u = ImGui::GetFontSize();
    Box wa = work_area(0.5f * u);
    if (held(wa, u)) return;
    if (!first_use(s_ids[which])) {
        if (d.role == ROLE_DIALOG) dialog_inside(s_ids[which], d.w > 0, wa);
        return;
    }
    float ww = wa.x1 - wa.x0, wh = wa.y1 - wa.y0;
    if (d.role == ROLE_DIALOG) {   // the middle of the work area, at most its size
        float w = d.w * u, h = d.h * u;
        if (which == LW_BROWSER) {   // Choose a machine larger in a larger main window (author 2026-10-29): no scrolling
            w = maxf(w, 0.75f * ww);
            h = maxf(h, 0.8f * wh);
        }
        if (d.w > 0) ImGui::SetNextWindowSize(ImVec2(minf(w, ww), minf(h, wh)), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2((wa.x0 + wa.x1) * 0.5f, (wa.y0 + wa.y1) * 0.5f), ImGuiCond_FirstUseEver,
                                ImVec2(0.5f, 0.5f));
        return;
    }
    Want want = { d.w * u, d.h > 0 ? d.h * u : wh, d.min_w * u, d.min_h * u, d.max_w * u,
                  d.role == ROLE_INSTRUMENT || d.role == ROLE_MEDIA, d.role == ROLE_GUIDE ? ORDER_TOP : ORDER_RIGHT };
    set_next(place(s_ids[which], d.role, want, d.cascade, wa, u));
}

bool layout_window_if_room(int which)
{
    if (which < 0 || which >= LW_COUNT) return false;
    ids_init();
    if (!first_use(s_ids[which])) return false;
    const WinDef& d = k_wins[which];
    float u = ImGui::GetFontSize(), gap = 0.5f * u;
    Box wa = work_area(gap);
    if (held(wa, u)) return false;
    float ww = wa.x1 - wa.x0, wh = wa.y1 - wa.y0;
    // the least size: the configurator its toolbar in one row, the drawing of the rack and the settings of a card under
    // it without scrolling much (44 x 44 font heights: 704 x 704 pixels at 100 %, higher than the work area of a window
    // of 1280 x 720: not on a notebook)
    float min_w = (which == LW_RACK ? 44.0f : d.min_w) * u, min_h = (which == LW_RACK ? 44.0f : d.min_h) * u;
    if (min_w > ww || min_h > wh) return false;
    Want want = { maxf(minf(d.w * u, ww), min_w), d.h > 0 ? maxf(minf(d.h * u, wh), min_h) : wh, min_w, min_h, 0, false,
                  ORDER_RIGHT };
    want.max_w = want.w;
    Obstacle ob[MAX_OBSTACLES];
    int n = obstacles(s_ids[which], wa, ob);
    Box b;
    if (!find_place(wa, ob, n, ~0u, want, gap, &b)) return false;
    set_next(b);
    return true;
}

void layout_to_back(void)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* self = ImGui::GetCurrentWindow();
    for (ImGuiWindow* w : g.Windows) {   // back to front: behind the first ordinary window (not the dock space host)
        if (w == self) return;
        if (w->RootWindow != w || !(w->Active || w->WasActive) ||
            (w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip |
                         ImGuiWindowFlags_NoBringToFrontOnFocus)))
            continue;
        ImGui::BringWindowToDisplayBehind(self, w);
        return;
    }
}

float layout_width(int which)
{
    return which >= 0 && which < LW_COUNT ? k_wins[which].w * ImGui::GetFontSize() : 0.0f;
}

void layout_work_size(float* w, float* h, float* gap)
{
    *gap = 0.5f * ImGui::GetFontSize();
    Box wa = work_area(*gap);
    *w = wa.x1 - wa.x0;
    *h = wa.y1 - wa.y0;
}

bool layout_first_use(const char* id)
{
    return first_use(ImHashStr(id));
}

void layout_main(const char* id, float w, float h, int gfx_count)
{
    ids_init();
    s_main_id = id ? ImHashStr(id) : 0;
    s_gfx_count = gfx_count;
    s_main_w = w;
    s_main_h = h;
}

void layout_display(const char* id, float zero_w, float zero_h, float step_w, float step_h, float k_max, bool exact)
{
    ImGuiID self = ImHashStr(id);
    if (!first_use(self)) return;
    ids_init();
    float u = ImGui::GetFontSize(), gap = 0.5f * u;
    Box wa = work_area(gap);
    if (held(wa, u)) return;
    float ww = wa.x1 - wa.x0, wh = wa.y1 - wa.y0;
    Obstacle ob[MAX_OBSTACLES];
    int n = obstacles(self, wa, ob);
    if (self == s_main_id) {   // the main output at the top left, unless another window is there already
        Box b = main_box(wa);
        bool clear = true;
        for (int i = 0; i < n && clear; i++)
            if (overlaps(b, ob[i].b, 0.0f)) clear = false;
        if (clear) {
            set_next(b);
            return;
        }
    }
    Box b;
    if (step_w <= 0 || step_h <= 0 || exact) {   // no picture (switched off): its size; a zoom chosen: that one
        float w = zero_w + (step_w > 0 ? k_max * step_w : 0), h = zero_h + (step_h > 0 ? k_max * step_h : 0);
        Want want = { w, h, w, h, w, false, ORDER_LEFT };
        set_next(place(self, ROLE_DISPLAY, want, 0, wa, u));
        return;
    }
    // the integer zooms from k_max down to 1:1, each as large as its picture
    for (float k = k_max; k > 0.99f; k -= 1.0f) {
        float w = zero_w + k * step_w, h = zero_h + k * step_h;
        if (w > ww + 0.5f || h > wh + 0.5f) continue;
        Want want = { w, h, w, h, w, false, ORDER_LEFT };
        if (find_place(wa, ob, n, ~0u, want, gap, &b)) {
            set_next(b);
            return;
        }
    }
    // below 1:1 (drawn smooth there, the letters keep their strokes) down to 3/4: the largest free room, the window
    // shrunk to the zoom that fits it
    float hi = minf(1.0f, maxf(k_max, 0.1f)), lo = minf(0.75f, hi);
    Want want = { zero_w + hi * step_w, zero_h + hi * step_h, zero_w + lo * step_w, zero_h + lo * step_h,
                  zero_w + hi * step_w, false, ORDER_LEFT };
    if (find_place(wa, ob, n, ~0u, want, gap, &b)) {
        float k = minf((b.x1 - b.x0 - zero_w) / step_w, (b.y1 - b.y0 - zero_h) / step_h);
        k = maxf(lo, minf(hi, k));
        set_next({ b.x0, b.y0, b.x0 + zero_w + k * step_w, b.y0 + zero_h + k * step_h });
        return;
    }
    // no free room: at 1:1 (k_max when smaller) clear of the other displays (another display at least of the main
    // output), covering the least of the rest
    want.min_w = want.w;
    want.min_h = want.h;
    set_next(place(self, ROLE_DISPLAY, want, 0, wa, u));
}

void layout_refit(const char* id, float w, float h)
{
    ImGuiID self = ImHashStr(id);
    if (first_use(self)) return;   // it takes its first place in this frame, or it was never created nor saved
    float u = ImGui::GetFontSize();
    Box wa = work_area(0.5f * u);
    if (held(wa, u)) return;
    if (ImGuiWindow* win = ImGui::FindWindowByID(self)) {
        if (!floating_placed(win)) return;
        window_to(win, inside({ win->Pos.x, win->Pos.y, win->Pos.x + w, win->Pos.y + h }, wa));
        return;
    }
    ImGuiWindowSettings* s = ImGui::FindWindowSettingsByID(self);   // saved, not created in this run yet
    if (!s || s->DockId) return;
    Box b = settings_box(s);
    settings_to(s, inside({ b.x0, b.y0, b.x0 + w, b.y0 + h }, wa));
}

void layout_keep_inside(void)
{
    float u = ImGui::GetFontSize(), gap = 0.5f * u;
    Box wa = work_area(0.0f);
    if (held(wa, u)) return;   // full screen, minimized: the size it comes back with is checked
    float ww = wa.x1 - wa.x0, wh = wa.y1 - wa.y0;
    if (ww != s_seen_w || wh != s_seen_h) {   // a new size: checked when it stays (not the steps of a resize, nor the
        s_seen_w = ww;                        // size of the first frames before the system maximizes the window)
        s_seen_h = wh;
        s_seen_frames = 0;
    }
    if (++s_seen_frames < 3 || (ww == s_kept_w && wh == s_kept_h)) return;
    s_kept_w = ww;
    s_kept_h = wh;
    Box in = work_area(gap);
    ImGuiContext& g = *GImGui;
    for (ImGuiWindow* w : g.Windows) {   // open or closed in this run
        if (!floating_placed(w)) continue;
        Box b = { w->Pos.x, w->Pos.y, w->Pos.x + w->SizeFull.x, w->Pos.y + w->SizeFull.y };
        if (to_bring_inside(w->ID, b, wa)) window_to(w, back_inside(w->ID, b, wa, in));
    }
    for (ImGuiWindowSettings* s = g.SettingsWindows.begin(); s != nullptr; s = g.SettingsWindows.next_chunk(s)) {
        // not the child windows (the resizable list of Choose a machine: its size only, no place)
        if (s->WantDelete || s->IsChild || s->DockId || s->Size.x <= 0 || s->Size.y <= 0 || ImGui::FindWindowByID(s->ID))
            continue;
        Box b = settings_box(s);
        if (to_bring_inside(s->ID, b, wa)) settings_to(s, back_inside(s->ID, b, wa, in));
    }
}

void layout_forget_closed(void)
{
    ImGuiContext& g = *GImGui;
    for (ImGuiWindowSettings* s = g.SettingsWindows.begin(); s != nullptr; s = g.SettingsWindows.next_chunk(s))
        if (!ImGui::FindWindowByID(s->ID)) s->WantDelete = true;
}

void layout_take_default_unsaved(void)
{
    s_kept_w = s_kept_h = 0;   // the places just loaded are checked against the work area too (layout_keep_inside)
    ImGuiContext& g = *GImGui;
    for (ImGuiWindow* w : g.Windows) {
        if (w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip |
                        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
            continue;   // the menu bar, the dock space host, popups
        if (w->DockNodeAsHost || ImGui::FindWindowSettingsByID(w->ID)) continue;
        w->SetWindowPosAllowFlags |= ImGuiCond_FirstUseEver;
        w->SetWindowSizeAllowFlags |= ImGuiCond_FirstUseEver;
        w->SetWindowCollapsedAllowFlags |= ImGuiCond_FirstUseEver;
    }
}

// ---------------------------------------------------------------- finding a window (View -> Windows)

namespace {

// Docked into a dock node that is drawn: a child of the node (its flags of the last frame). Not DockIsActive alone: ImGui
// clears it for the only window of a node from the update of the node until the Begin of the window, and the menus are
// drawn in between.
bool docked(ImGuiWindow* w)
{
    return w->DockIsActive || (w->DockNode && (w->Flags & ImGuiWindowFlags_ChildWindow));
}

// A window the user sees as one: open (drawn in the last frame or in this one), docked or floating; not a menu, a popup,
// a tooltip, the menu bar, the full screen, the dock space or the host of a dock node.
bool user_window(ImGuiWindow* w)
{
    if (w->IsFallbackWindow || !(w->Active || w->WasActive)) return false;
    if (w->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) return false;
    if (docked(w)) return true;   // a child of its dock node, without a title bar when the node hides its tab bar
    return !(w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoTitleBar)) && !w->DockNodeAsHost;
}

// A window drawn in front of others that can cover them: a floating window or the host of a floating dock node, open.
// Not the dock space over the main window (always at the back), the menu bar or the full screen (no title bar).
bool front_window(ImGuiWindow* o)
{
    if (o->RootWindowDockTree != o || !(o->Active || o->WasActive)) return false;   // children, docked windows
    if (o->IsFallbackWindow || o->Hidden) return false;   // ImGui's implicit "Debug" window: begun every frame, not shown
    if (o->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) return false;
    if (o->DockNodeAsHost) return !o->DockNodeAsHost->IsDockSpace();
    return !(o->Flags & ImGuiWindowFlags_NoTitleBar);
}

// Collapsed, a tab of a dock node that is not selected, or a window in front of it (g.Windows is back to front: the
// root windows after the root of this one) overlaps it by more than a border. A window docked into the dock space over
// the main window only by the first two: the dock space stays at the back (NoBringToFrontOnFocus), a floating window
// over it cannot be put behind it (FocusWindow would do nothing, and the window could never be closed from the menu).
bool covered(ImGuiWindow* w)
{
    if (w->Collapsed || (docked(w) && !w->DockTabIsVisible)) return true;
    if (w->RootWindowDockTree && (w->RootWindowDockTree->Flags & ImGuiWindowFlags_NoBringToFrontOnFocus)) return false;
    ImGuiContext& g = *GImGui;
    ImGuiWindow** root = g.Windows.find(w->RootWindowDockTree);
    if (root == g.Windows.end()) return false;
    for (ImGuiWindow** it = root + 1; it != g.Windows.end(); ++it) {
        ImGuiWindow* o = *it;
        if (!front_window(o)) continue;
        float ow = minf(w->Pos.x + w->Size.x, o->Pos.x + o->Size.x) - maxf(w->Pos.x, o->Pos.x);
        float oh = minf(w->Pos.y + w->Size.y, o->Pos.y + o->Size.y) - maxf(w->Pos.y, o->Pos.y);
        if (ow > 2.0f && oh > 2.0f) return true;
    }
    return false;
}

ImGuiWindow* open_window(const char* id)
{
    ImGuiWindow* w = id ? ImGui::FindWindowByName(id) : nullptr;
    return w && user_window(w) ? w : nullptr;
}

// The order of the list: the displays (###screen, ###gfx0...), then the windows in the order of LayoutWin, the others last.
int list_rank(ImGuiID id)
{
    ids_init();
    for (int i = 0; i < MAX_DISPLAY_IDS; i++)
        if (s_display_ids[i] == id) return i;
    for (int i = 0; i < LW_COUNT; i++)
        if (s_ids[i] == id) return MAX_DISPLAY_IDS + i;
    return MAX_DISPLAY_IDS + LW_COUNT;
}

} // namespace

bool layout_covered(const char* id)
{
    ImGuiWindow* w = open_window(id);
    return w && covered(w);
}

void layout_to_front(const char* id)
{
    ImGuiWindow* w = open_window(id);
    if (!w) return;
    if (w->Collapsed) ImGui::SetWindowCollapsed(w, false);
    bool in_dock = docked(w);
    if (in_dock && w->DockNode->TabBar) w->DockNode->TabBar->NextSelectedTabId = w->TabId;
    // a floating window partly outside the work area (the main window got smaller) comes into it with the gap of the
    // layout, its title bar first (the work area of the last frame: the menu bar of this one may not be drawn yet)
    if (!in_dock && !(w->SetWindowPosAllowFlags & ImGuiCond_FirstUseEver)) {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        float gap = 0.5f * ImGui::GetFontSize();
        ImVec2 p = w->Pos, a = vp->WorkPos, b(vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y);
        if (p.x < a.x || p.y < a.y || p.x + w->Size.x > b.x || p.y + w->Size.y > b.y) {
            p.x = maxf(a.x + gap, minf(p.x, b.x - gap - w->Size.x));
            p.y = maxf(a.y + gap, minf(p.y, b.y - gap - w->Size.y));
            ImGui::SetWindowPos(w, p);
        }
    }
    ImGui::FocusWindow(w);
}

void layout_open_windows(std::vector<LayoutOpenWindow>& out)
{
    out.clear();
    std::vector<int> rank;
    for (ImGuiWindow* w : GImGui->Windows) {
        if (!user_window(w)) continue;
        const char* name = w->Name;
        const char* hash = strstr(name, "###");
        LayoutOpenWindow e;
        e.id = hash ? hash : name;
        e.title.assign(name, ImGui::FindRenderedTextEnd(name));
        int r = list_rank(w->ID);
        if (r < MAX_DISPLAY_IDS) {   // a display: its card without the zoom ("AND-1  200 %", app.cpp display_title)
            size_t z = e.title.rfind("  ");
            if (z != std::string::npos && z > 0) e.title.erase(z);
        }
        if (e.title.empty()) e.title = hash ? hash + 3 : name;
        e.covered = covered(w);
        size_t at = out.size();   // sorted by the rank, then by the title
        while (at > 0 && (rank[at - 1] > r || (rank[at - 1] == r && out[at - 1].title > e.title))) at--;
        out.insert(out.begin() + (std::ptrdiff_t)at, e);
        rank.insert(rank.begin() + (std::ptrdiff_t)at, r);
    }
}

// ---------------------------------------------------------------- the docked layout

namespace {

// Into the dock node: a window that exists keeps no default place of its first use (SetNextWindowPos would undock
// it, ImGui PosUndock); one not created yet gets the size it has when it is dragged out of the dock.
void dock_window(const char* id, ImGuiID node, float w, float h)
{
    ImGui::DockBuilderDockWindow(id, node);
    ImGuiID wid = ImHashStr(id);
    if (ImGuiWindow* win = ImGui::FindWindowByID(wid)) {
        win->SetWindowPosAllowFlags &= ~ImGuiCond_FirstUseEver;
        win->SetWindowSizeAllowFlags &= ~ImGuiCond_FirstUseEver;
        win->SetWindowCollapsedAllowFlags &= ~ImGuiCond_FirstUseEver;
    } else if (ImGuiWindowSettings* s = ImGui::FindWindowSettingsByID(wid)) {
        if (s->Size.x <= 0 || s->Size.y <= 0) s->Size = ImVec2ih((short)w, (short)h);
    }
}

// The tabs chosen by layout_dock and the frames left to keep them selected (layout_dock_frame).
ImGuiID s_sel_node[2], s_sel_tab[2];
int s_sel_frames = 0;

// The tab of window `id` selected when the tab bar of the node is made, and for a few frames after (slot 0, 1).
void dock_select(int slot, ImGuiID node, const char* id)
{
    s_sel_node[slot] = node;
    s_sel_tab[slot] = ImHashStr("#TAB", 0, ImHashStr(id));   // ImGuiWindow::TabId
    if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(node)) n->SelectedTabId = s_sel_tab[slot];
    s_sel_frames = 4;
}

} // namespace

void layout_dock_frame(void)
{
    if (s_sel_frames <= 0) return;
    s_sel_frames--;
    ImGuiContext& g = *GImGui;
    for (int i = 0; i < 2; i++) {
        ImGuiDockNode* n = s_sel_node[i] ? ImGui::DockBuilderGetNode(s_sel_node[i]) : nullptr;
        ImGuiTabItem* t = n && n->TabBar ? ImGui::TabBarFindTabByID(n->TabBar, s_sel_tab[i]) : nullptr;
        if (!t || !t->Window) continue;
        n->TabBar->NextSelectedTabId = s_sel_tab[i];
        // a window of the node that just opened has the keyboard (its tab would come back in front): the chosen one
        // takes it (after a machine start the display takes it from there, app.cpp)
        if (g.NavWindow && g.NavWindow->RootWindow->DockNode == n && g.NavWindow->RootWindow != t->Window)
            ImGui::FocusWindow(t->Window);
    }
    if (s_sel_frames == 0) s_sel_node[0] = s_sel_node[1] = 0;
}

unsigned layout_dockspace_id(void)
{
    char label[32];
    ImFormatString(label, sizeof(label), "WindowOverViewport_%08X", ImGui::GetMainViewport()->ID);
    return ImHashStr("DockSpace", 0, ImHashStr(label));
}

// The column at the right edge: as wide as the guide and the tools ask (about a third of the main window), wider for an
// instrument or Media of the machine (up to 55 %) or its other displays at 1:1 (up to 45 %), but the main output keeps
// the width it has in the floating layout (p.main_w; the column a quarter of the width at least). Its width stays when
// the main window changes, the main output takes the rest. In the column: the other displays at the top (65 % of the
// height, 70 % for two, 35 % when the machine opens an instrument or Media; the first of them 60 % of that), under them
// the windows of the machine (60 % of the rest, 70 % when the machine opens one), the guide and the tools at the bottom
// (an open guide 12 font heights at least: its text, not only its heading). A part with no window open gives its room
// to the others: one part alone has the whole column.
void layout_dock(unsigned dockspace, const DockPlan& p)
{
    ids_init();
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float u = ImGui::GetFontSize();
    ImVec2 size = vp->WorkSize;   // of the last frame: at the first one with the menu bar, the column keeps its width
    float W = size.x;
    if (W < 2.0f || size.y < 2.0f) return;
    float col = minf(maxf(0.36f * W, 26.0f * u), 48.0f * u);
    if (p.side_w > 0) col = maxf(col, minf(p.side_w, 0.55f * W));
    if (p.display_w > 0) col = maxf(col, minf(p.display_w, 0.45f * W));
    if (p.main_w > 0) col = minf(col, W - p.main_w);
    col = maxf(minf(col, 0.6f * W), 0.25f * W);
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodePos(dockspace, vp->WorkPos);
    ImGui::DockBuilderSetNodeSize(dockspace, size);
    float disp_share = p.side_w > 0 ? 0.35f : (p.machine_displays >= 3 ? 0.7f : 0.65f);
    float side_share = p.side_w > 0 ? 0.7f : 0.6f;
    if (p.guide) {
        float rest = p.machine_displays > 1 ? size.y * (1.0f - disp_share) : size.y;
        if (rest * (1.0f - side_share) < 12.0f * u) side_share = maxf(0.3f, 1.0f - 12.0f * u / rest);
    }
    ImGuiID center = 0, right = 0, displays = 0, rest = 0, side = 0, tools = 0, d1 = 0, d2 = 0;
    ImGui::DockBuilderSplitNode(dockspace, ImGuiDir_Right, col / W, &right, &center);   // center: the central node
    ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 1.0f - disp_share, &rest, &displays);
    ImGui::DockBuilderSplitNode(rest, ImGuiDir_Down, 1.0f - side_share, &tools, &side);
    ImGui::DockBuilderSplitNode(displays, ImGuiDir_Down, 0.4f, &d2, &d1);
    float dw = minf(40.0f * u, W), dh = minf(30.0f * u, size.y);
    for (int i = 0; i < p.n_displays; i++)
        dock_window(p.displays[i], i == 0 ? center : (i % 2 ? d1 : d2), dw, dh);
    static const int k_side[] = { LW_MEDIA, LW_PLOTTER, LW_APS5, LW_IMS2, LW_DIAG, LW_TERMINAL, LW_PRINTER };
    static const int k_tools[] = { LW_GUIDE, LW_DEBUGGER, LW_MEMORY, LW_IOLOG, LW_LOG };
    for (int which : k_side)
        dock_window(k_wins[which].id, side, minf(k_wins[which].w * u, W), minf(k_wins[which].h * u, size.y));
    for (int which : k_tools)
        dock_window(k_wins[which].id, tools, minf(k_wins[which].w * u, W), minf(k_wins[which].h * u, size.y));
    s_sel_node[0] = s_sel_node[1] = 0;
    if (p.machine_tab) dock_select(0, side, p.machine_tab);
    if (p.guide) dock_select(1, tools, k_wins[LW_GUIDE].id);
    ImGui::DockBuilderFinish(dockspace);
}

namespace {

// The dock node of a window: of the window, else of its saved place (not created yet); 0 = not docked.
ImGuiID dock_of(ImGuiID id)
{
    if (ImGuiWindow* w = ImGui::FindWindowByID(id)) return w->DockId;
    if (ImGuiWindowSettings* s = ImGui::FindWindowSettingsByID(id)) return s->DockId;
    return 0;
}

// The window into dock node `node` (a tab at its end). The node it leaves stays even when it is left empty (ImGui would
// remove it; another window is going to take it).
void dock_move(ImGuiID id, ImGuiID node)
{
    if (ImGuiWindow* w = ImGui::FindWindowByID(id)) {
        if (w->DockNode) ImGui::DockContextProcessUndockWindow(GImGui, w, false);   // keeps its DockId: no removal
        ImGui::SetWindowDock(w, node, ImGuiCond_Always);
        w->DockOrder = -1;
    } else if (ImGuiWindowSettings* s = ImGui::FindWindowSettingsByID(id)) {
        s->DockId = node;
        s->DockOrder = -1;
    }
}

} // namespace

// The central node under `node` (DockBuilderGetCentralNode knows it only after the DockSpace call of a frame: not at the
// first frame, when the dock comes from imgui.ini).
static ImGuiDockNode* central_node(ImGuiDockNode* node)
{
    if (!node) return nullptr;
    if (node->IsCentralNode()) return node;
    ImGuiDockNode* c = central_node(node->ChildNodes[0]);
    return c ? c : central_node(node->ChildNodes[1]);
}

void layout_dock_main(unsigned dockspace, const char* main_id)
{
    ids_init();
    ImGuiDockNode* central = central_node(ImGui::DockBuilderGetNode(dockspace));
    if (!central) return;
    ImGuiID main = ImHashStr(main_id), from = dock_of(main);
    if (from == central->ID || from == 0) return;   // in the middle already, or dragged out by the user
    bool swap = ImGui::DockBuilderGetNode(from) != nullptr && !ImGui::DockBuilderGetNode(from)->IsSplitNode();
    for (int i = 0; i < MAX_DISPLAY_IDS; i++)   // the displays in the middle go where the main output was
        if (s_display_ids[i] != main && dock_of(s_display_ids[i]) == central->ID && swap) dock_move(s_display_ids[i], from);
    dock_move(main, central->ID);
    dock_select(0, central->ID, main_id);
    s_sel_node[1] = 0;
}
