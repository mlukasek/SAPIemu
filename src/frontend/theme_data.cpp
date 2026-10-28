// The looks of the GUI as data: palettes (light and dark) and sizes. The contrast of the pairs that must be readable
// is checked by tests/test_theme.cpp (WCAG 2.2: text 4.5:1, controls 3:1). The values are the proposal of
// docs/vydani/plan-vzhled.md (appendix A, B). One look now (the author chose Windows 11 from the gallery,
// 2026-10-28); a new one is a palette pair, its sizes and a line of k_themes (the classic and the SAPI looks
// of the gallery are in commit 1fc9a21).
#include "frontend/theme.h"

#include <math.h>
#include <string>

// ---------------------------------------------------------------- Windows 11 (Fluent)

static const ThemePalette k_win11_light = {
    .desktop = 0xEBEBEB, .menubar = 0xF3F3F3, .window = 0xF9F9F9, .popup = 0xFCFCFC,
    .title = 0xF3F3F3, .title_active = 0xCCE4F7, .border = 0x868686, .text = 0x1B1B1B, .text_dim = 0x616161,
    .frame = 0xFFFFFF, .frame_hover = 0xF3F3F3, .frame_active = 0xEBEBEB,
    .button = 0xFDFDFD, .button_hover = 0xF3F3F3, .button_active = 0xEBEBEB,
    .accent = 0x005FB8, .accent_hover = 0x1A6FBF, .accent_active = 0x00529E, .accent_text = 0xFFFFFF,
    .header = 0xCCE4F7, .header_hover = 0xE5F1FB, .header_active = 0xB9D8F3,
    .tab = 0xE6E6E6, .tab_selected = 0xF9F9F9, .tab_overline = 0x005FB8,
    .check = 0x005FB8, .check_bg = 0xCCE4F7,
    .scroll_bg = THEME_NONE, .scroll_grab = 0x868686, .scroll_hover = 0x6B6B6B, .scroll_active = 0x505050,
    .separator = 0xD6D6D6, .text_sel = 0x99C9EF, .link = 0x005FB8, .focus = 0x005FB8, .heading = 0x1B1B1B,
    .ok = 0x0F7B0F, .warn = 0x8C6500, .error = 0xC42B1C,
    .pending = 0xB84A00, .pending_bg = 0xFFE3CC, .pending_btn = 0xB84A00, .pending_btn_active = 0x9E4000,
    .pc_bg = 0xFFF4CE, .bp_bg = 0xFDE7E9, .code_bg = 0xF0F0F0,
};

static const ThemePalette k_win11_dark = {
    .desktop = 0x1C1C1C, .menubar = 0x202020, .window = 0x2B2B2B, .popup = 0x2C2C2C,
    .title = 0x2B2B2B, .title_active = 0x2D4A60, .border = 0x808080, .text = 0xFFFFFF, .text_dim = 0xA8A8A8,
    .frame = 0x333333, .frame_hover = 0x3A3A3A, .frame_active = 0x1F1F1F,
    .button = 0x373737, .button_hover = 0x3F3F3F, .button_active = 0x313131,
    .accent = 0x4CC2FF, .accent_hover = 0x47B1E8, .accent_active = 0x42A1D2, .accent_text = 0x000000,
    .header = 0x2D4A60, .header_hover = 0x3A3A3A, .header_active = 0x36586F,
    .tab = 0x232323, .tab_selected = 0x2B2B2B, .tab_overline = 0x4CC2FF,
    .check = 0x4CC2FF, .check_bg = 0x1F3F57,
    .scroll_bg = THEME_NONE, .scroll_grab = 0x808080, .scroll_hover = 0x9A9A9A, .scroll_active = 0x4CC2FF,
    .separator = 0x404040, .text_sel = 0x2D5F80, .link = 0x4CC2FF, .focus = 0x4CC2FF, .heading = 0xFFFFFF,
    .ok = 0x6CCB5F, .warn = 0xFCE100, .error = 0xFF99A4,
    .pending = 0xFF9F45, .pending_bg = 0x4A2A0A, .pending_btn = 0xA85400, .pending_btn_active = 0x954A00,
    .pc_bg = 0x433519, .bp_bg = 0x442726, .code_bg = 0x1C1C1C,
};

static const ThemeMetrics k_win11_metrics = {
    .window_pad = ImVec2(12, 10), .frame_pad = ImVec2(10, 5), .item_spacing = ImVec2(8, 7),
    .item_inner = ImVec2(6, 5), .cell_pad = ImVec2(8, 4),
    .indent = 20, .scrollbar = 12, .grab_min = 12,
    .r_window = 8, .r_child = 6, .r_popup = 8, .r_frame = 4, .r_scroll = 6,
    .b_frame = 1, .b_tab = 0, .b_tabbar = 1, .sep_text = 1,
    .title_align = 0.0f,
};

const ThemeDef k_themes[THEME_VARIANTS] = {
    { "win11", "Windows 11", "Windows 11", &k_win11_light, &k_win11_dark, &k_win11_metrics, THEME_FONT_SEGOE_SEMIBOLD },
};

// ---------------------------------------------------------------- the names of the looks

const char* theme_mode_key(ThemeMode mode)
{
    return mode == THEME_LIGHT ? "light" : mode == THEME_DARK ? "dark" : "system";
}

bool theme_parse(const char* s, ThemeVariant* variant, ThemeMode* mode)
{
    std::string t = s ? s : "";
    if (t == "light" || t == "dark" || t == "system") t = std::string(k_themes[0].key) + "-" + t;   // the mode only
    std::string key = t, m;
    size_t dash = t.find('-');
    if (dash != std::string::npos) {
        key = t.substr(0, dash);
        m = t.substr(dash + 1);
    }
    int v = -1;
    for (int i = 0; i < THEME_VARIANTS; i++)
        if (key == k_themes[i].key) v = i;
    if (v < 0) return false;
    ThemeMode md = THEME_SYSTEM;
    if (m == "light") md = THEME_LIGHT;
    else if (m == "dark") md = THEME_DARK;
    else if (!m.empty() && m != "system") return false;
    *variant = (ThemeVariant)v;
    *mode = md;
    return true;
}

// ---------------------------------------------------------------- WCAG 2.2

static double channel(uint32_t c)
{
    double v = (double)(c & 0xFF) / 255.0;
    return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}

static double luminance(uint32_t rgb)
{
    return 0.2126 * channel(rgb >> 16) + 0.7152 * channel(rgb >> 8) + 0.0722 * channel(rgb);
}

double theme_contrast(uint32_t a, uint32_t b)
{
    double la = luminance(a), lb = luminance(b);
    if (la < lb) {
        double x = la;
        la = lb;
        lb = x;
    }
    return (la + 0.05) / (lb + 0.05);
}
