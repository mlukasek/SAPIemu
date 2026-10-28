// The look of the GUI (theme.h): the ImGui style from the data of theme_data.cpp, the fonts, the semantic colours
// of the windows and the frame of the focused window.
#include "frontend/theme.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"   // the focused window and its rectangle (the focus frame)

#include "core/util.h"

namespace {

ThemeColors g_colors;
ThemeFonts g_fonts;
ThemeVariant g_variant = THEME_WIN11;
float g_dpi = 1.0f;
bool g_override = false;
ThemeVariant g_override_variant = THEME_WIN11;
ThemeMode g_override_mode = THEME_SYSTEM;

ImVec4 col(uint32_t rgb, float alpha = 1.0f)
{
    if (rgb == THEME_NONE) return ImVec4(0, 0, 0, 0);
    return ImVec4((float)((rgb >> 16) & 0xFF) / 255.0f, (float)((rgb >> 8) & 0xFF) / 255.0f, (float)(rgb & 0xFF) / 255.0f,
                  alpha);
}

// t = 0: a, t = 1: b
ImVec4 mix(uint32_t a, uint32_t b, float t)
{
    ImVec4 x = col(a), y = col(b);
    return ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, 1.0f);
}

ImVec4 darker(uint32_t a, float t)
{
    return mix(a, 0x000000, t);
}

// The font of a look with the fallback merged into it: Segoe UI lacks some symbols (U+26A0, U+2716...).
// Segoe UI is scaled up: ImGui sizes a font by its ascent + descent, which is 1.33 em in Segoe UI against
// 1.16 em in DejaVu Sans, so at the same size its letters would be 20 % smaller. ExtraSizeScale enlarges the
// glyphs only, the height of a line (the layout unit of the windows) stays.
ImFont* add_font(ImGuiIO& io, const std::string& path, float extra, const std::string& fallback)
{
    if (path.empty() || !file_exists(path)) return nullptr;
    ImFontConfig cfg;
    cfg.ExtraSizeScale = extra;
    ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), 16.0f, &cfg);
    if (f && !fallback.empty() && file_exists(fallback)) {
        ImFontConfig merge;
        merge.MergeMode = true;
        io.Fonts->AddFontFromFileTTF(fallback.c_str(), 16.0f, &merge);
    }
    return f;
}

std::string windows_font(const char* file)
{
#ifdef _WIN32
    const char* windir = getenv("WINDIR");
    if (!windir || !*windir) return std::string();
    return std::string(windir) + "\\Fonts\\" + file;
#else
    (void)file;
    return std::string();
#endif
}

} // namespace

const ThemeColors& theme()
{
    return g_colors;
}

ThemeVariant theme_variant()
{
    return g_variant;
}

void theme_set_override(ThemeVariant variant, ThemeMode mode)
{
    g_override = true;
    g_override_variant = variant;
    g_override_mode = mode;
}

bool theme_override(ThemeVariant* variant, ThemeMode* mode)
{
    if (!g_override) return false;
    *variant = g_override_variant;
    *mode = g_override_mode;
    return true;
}

void theme_fonts_load(ImGuiIO& io)
{
    std::string dv = data_path("third_party/fonts/DejaVuSans.ttf");
    std::string dvb = data_path("third_party/fonts/DejaVuSans-Bold.ttf");
    std::string mono = data_path("third_party/fonts/DejaVuSansMono.ttf");
    ImFont* dejavu = add_font(io, dv, 1.0f, std::string());
    ImFont* dejavu_bold = add_font(io, dvb, 1.0f, std::string());
    if (!dejavu_bold) dejavu_bold = dejavu;
    bool need_semibold = false, need_bold = false;   // the fonts the looks of k_themes use
    for (int v = 0; v < THEME_VARIANTS; v++) {
        if (k_themes[v].font == THEME_FONT_SEGOE_SEMIBOLD) need_semibold = true;
        if (k_themes[v].font == THEME_FONT_SEGOE_BOLD) need_bold = true;
    }
    ImFont* segoe = need_semibold || need_bold ? add_font(io, windows_font("segoeui.ttf"), 1.16f, dv) : nullptr;
    ImFont* segoe_semibold = need_semibold ? add_font(io, windows_font("seguisb.ttf"), 1.16f, dvb) : nullptr;
    ImFont* segoe_bold = need_bold ? add_font(io, windows_font("segoeuib.ttf"), 1.16f, dvb) : nullptr;
    if (!segoe_bold) segoe_bold = segoe_semibold;
    if (!segoe_semibold) segoe_semibold = segoe_bold;
    for (int v = 0; v < THEME_VARIANTS; v++) {
        bool system = k_themes[v].font != THEME_FONT_DEJAVU && segoe;
        g_fonts.ui[v] = system ? segoe : dejavu;
        ImFont* b = k_themes[v].font == THEME_FONT_SEGOE_SEMIBOLD ? segoe_semibold : segoe_bold;
        g_fonts.bold[v] = system && b ? b : dejavu_bold;
    }
    g_fonts.panel = dejavu;
    g_fonts.panel_bold = dejavu_bold;
    g_fonts.mono = file_exists(mono) ? io.Fonts->AddFontFromFileTTF(mono.c_str(), 15.0f) : nullptr;
}

const ThemeFonts& theme_fonts()
{
    return g_fonts;
}

void theme_apply(ThemeVariant variant, bool dark, float dpi, float text_scale)
{
    if (variant < 0 || variant >= THEME_VARIANTS) variant = THEME_WIN11;
    const ThemeDef& def = k_themes[variant];
    const ThemePalette& p = dark ? *def.dark : *def.light;
    const ThemeMetrics& m = *def.metrics;
    g_variant = variant;
    g_dpi = dpi;

    ImGuiStyle s;   // from scratch: ScaleAllSizes() truncates, a style scaled twice drifts
    s.WindowPadding = m.window_pad;
    s.FramePadding = m.frame_pad;
    s.ItemSpacing = m.item_spacing;
    s.ItemInnerSpacing = m.item_inner;
    s.CellPadding = m.cell_pad;
    s.IndentSpacing = m.indent;
    s.ScrollbarSize = m.scrollbar;
    s.GrabMinSize = m.grab_min;
    s.WindowRounding = m.r_window;
    s.ChildRounding = m.r_child;
    s.PopupRounding = m.r_popup;
    s.FrameRounding = m.r_frame;
    s.GrabRounding = m.r_frame;
    s.TabRounding = m.r_frame;
    s.MenuItemRounding = m.r_frame;
    s.ScrollbarRounding = m.r_scroll;
    s.ImageRounding = 0;    // the pictures of the displays stay square
    s.WindowBorderSize = 1;
    s.ChildBorderSize = 1;
    s.PopupBorderSize = 1;
    s.FrameBorderSize = m.b_frame;
    s.TabBorderSize = m.b_tab;
    s.TabBarBorderSize = m.b_tabbar;
    s.TabBarOverlineSize = 2;
    s.SeparatorTextBorderSize = m.sep_text;
    s.ImageBorderSize = 0;  // the previews push their own frame
    s.WindowTitleAlign = ImVec2(m.title_align, 0.5f);
    s.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = col(p.text);
    c[ImGuiCol_TextDisabled] = col(p.text_dim);
    c[ImGuiCol_WindowBg] = col(p.window);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = col(p.popup);
    c[ImGuiCol_Border] = col(p.border);
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = col(p.frame);
    c[ImGuiCol_FrameBgHovered] = col(p.frame_hover);
    c[ImGuiCol_FrameBgActive] = col(p.frame_active);
    c[ImGuiCol_TitleBg] = col(p.title);
    c[ImGuiCol_TitleBgActive] = col(p.title_active);
    c[ImGuiCol_TitleBgCollapsed] = col(p.title);
    c[ImGuiCol_MenuBarBg] = col(p.menubar);
    c[ImGuiCol_ScrollbarBg] = col(p.scroll_bg);
    c[ImGuiCol_ScrollbarGrab] = col(p.scroll_grab);
    c[ImGuiCol_ScrollbarGrabHovered] = col(p.scroll_hover);
    c[ImGuiCol_ScrollbarGrabActive] = col(p.scroll_active);
    c[ImGuiCol_CheckMark] = col(p.check);
    c[ImGuiCol_CheckboxSelectedBg] = col(p.check_bg);
    c[ImGuiCol_SliderGrab] = col(p.focus);   // the accent of a button may be too dark on a dark field
    c[ImGuiCol_SliderGrabActive] = col(p.link);
    c[ImGuiCol_Button] = col(p.button);
    c[ImGuiCol_ButtonHovered] = col(p.button_hover);
    c[ImGuiCol_ButtonActive] = col(p.button_active);
    c[ImGuiCol_Header] = col(p.header);
    c[ImGuiCol_HeaderHovered] = col(p.header_hover);
    c[ImGuiCol_HeaderActive] = col(p.header_active);
    c[ImGuiCol_Separator] = col(p.separator);
    c[ImGuiCol_SeparatorHovered] = col(p.accent_hover);
    c[ImGuiCol_SeparatorActive] = col(p.accent);
    c[ImGuiCol_ResizeGrip] = col(p.border, 0.25f);
    c[ImGuiCol_ResizeGripHovered] = col(p.accent, 0.6f);
    c[ImGuiCol_ResizeGripActive] = col(p.accent);
    c[ImGuiCol_InputTextCursor] = col(p.text);
    c[ImGuiCol_TabHovered] = col(p.button_hover);
    c[ImGuiCol_Tab] = col(p.tab);
    c[ImGuiCol_TabSelected] = col(p.tab_selected);
    c[ImGuiCol_TabSelectedOverline] = col(p.tab_overline);
    c[ImGuiCol_TabDimmed] = col(p.tab);
    c[ImGuiCol_TabDimmedSelected] = col(p.tab_selected);
    c[ImGuiCol_TabDimmedSelectedOverline] = col(p.border, 0.5f);
    c[ImGuiCol_DockingPreview] = col(p.accent, 0.4f);
    c[ImGuiCol_DockingEmptyBg] = col(p.desktop);
    c[ImGuiCol_PlotLines] = col(p.text_dim);
    c[ImGuiCol_PlotLinesHovered] = col(p.accent);
    c[ImGuiCol_PlotHistogram] = col(p.accent);
    c[ImGuiCol_PlotHistogramHovered] = col(p.accent_hover);
    c[ImGuiCol_TableHeaderBg] = mix(p.window, p.text, 0.07f);
    c[ImGuiCol_TableBorderStrong] = col(p.border);
    c[ImGuiCol_TableBorderLight] = col(p.separator);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = mix(p.window, p.text, 0.035f);
    c[ImGuiCol_TextLink] = col(p.link);
    c[ImGuiCol_TextSelectedBg] = col(p.text_sel);
    c[ImGuiCol_TreeLines] = col(p.border, 0.5f);
    c[ImGuiCol_DragDropTarget] = col(p.accent);
    c[ImGuiCol_DragDropTargetBg] = col(p.accent, 0.15f);
    c[ImGuiCol_UnsavedMarker] = col(p.text);
    c[ImGuiCol_NavCursor] = col(p.focus);
    c[ImGuiCol_NavWindowingHighlight] = col(p.focus, 0.7f);
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, dark ? 0.35f : 0.2f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, dark ? 0.45f : 0.25f);

    s.ScaleAllSizes(dpi);
    s.FontScaleDpi = dpi;
    s.FontScaleMain = text_scale;
    ImGui::GetStyle() = s;
    ImGuiIO& io = ImGui::GetIO();
    if (g_fonts.ui[variant]) io.FontDefault = g_fonts.ui[variant];

    ThemeColors& t = g_colors;
    t.dark = dark;
    t.text = col(p.text);
    t.text_dim = col(p.text_dim);
    t.heading = col(p.heading);
    t.link = col(p.link);
    t.ok = col(p.ok);
    t.warn = col(p.warn);
    t.error = col(p.error);
    t.pending = col(p.pending);
    t.pending_bg = col(p.pending_bg);
    t.accent = col(p.accent);
    t.accent_hover = col(p.accent_hover);
    t.accent_active = col(p.accent_active);
    t.accent_text = col(p.accent_text);
    t.pending_btn = col(p.pending_btn);
    t.pending_btn_hover = col(p.pending_btn_active);     // darker, the white text stays readable
    t.pending_btn_active = darker(p.pending_btn_active, 0.15f);
    t.pc_bg = col(p.pc_bg);
    t.bp_bg = col(p.bp_bg);
    t.code_bg = col(p.code_bg);
    t.desktop = ImGui::ColorConvertFloat4ToU32(col(p.desktop));
    t.focus = ImGui::ColorConvertFloat4ToU32(col(p.focus));
}

void theme_focus_frame()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* w = g.NavWindow;
    if (!w) return;
    w = w->RootWindow;
    if (!w || !w->WasActive || w->Hidden || w->Collapsed || w->DockNodeAsHost) return;
    if (w->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_ChildMenu |
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_Modal))
        return;   // menus, the menu bar, the dock space host
    float th = 2.0f * g_dpi;
    ImRect r = w->Rect();
    ImDrawList* dl = w->DrawList;   // its own list: windows in front of it stay in front of the frame
    dl->PushClipRect(r.Min, r.Max, false);
    float half = th * 0.5f;
    dl->AddRect(ImVec2(r.Min.x + half, r.Min.y + half), ImVec2(r.Max.x - half, r.Max.y - half), g_colors.focus,
                w->DockIsActive ? 0.0f : g.Style.WindowRounding, 0, th);
    dl->PopClipRect();
}

bool theme_accent_button(const char* label, const ImVec2& size, bool small)
{
    const ThemeColors& t = g_colors;
    ImGui::PushStyleColor(ImGuiCol_Button, t.accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, t.accent_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, t.accent_active);
    ImGui::PushStyleColor(ImGuiCol_Text, t.accent_text);
    ImGui::PushStyleColor(ImGuiCol_Border, t.accent_active);
    if (small) ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
    bool pressed = ImGui::Button(label, size);
    if (small) ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
    return pressed;
}

void theme_selected_bar()
{
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    float w = 3.0f * g_dpi, inset = 3.0f * g_dpi;
    if (b.y - a.y < 4 * inset) inset = (b.y - a.y) * 0.2f;
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(a.x, a.y + inset), ImVec2(a.x + w, b.y - inset), g_colors.focus,
                                              w * 0.5f);
}

void theme_heading(const char* text, float size)
{
    ImFont* bold = g_fonts.bold[g_variant];
    if (bold) ImGui::PushFont(bold, size);
    ImGui::PushStyleColor(ImGuiCol_Text, g_colors.heading);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    if (bold) ImGui::PopFont();
}

ImFont* panel_font(bool bold)
{
    ImFont* f = bold ? g_fonts.panel_bold : g_fonts.panel;
    return f ? f : ImGui::GetFont();
}

void panel_text(ImDrawList* dl, const ImVec2& pos, ImU32 col, const char* text, float scale)
{
    dl->AddText(panel_font(false), ImGui::GetFontSize() * scale, pos, col, text);
}

ImVec2 panel_text_size(const char* text, float scale)
{
    return panel_font(false)->CalcTextSizeA(ImGui::GetFontSize() * scale, FLT_MAX, 0.0f, text);
}

void theme_section(const char* text)
{
    ImFont* bold = g_fonts.bold[g_variant];
    if (bold) ImGui::PushFont(bold, 0.0f);
    ImGui::SeparatorText(text);
    if (bold) ImGui::PopFont();
}
