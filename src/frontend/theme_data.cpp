// The looks of the GUI as data: palettes (light and dark) and sizes of the three variants. The contrast of the
// pairs that must be readable is checked by tests/test_theme.cpp (WCAG 2.2: text 4.5:1, controls 3:1).
// The values are the proposal of docs/vydani/plan-vzhled.md (appendix A, B), tuned in the gallery.
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

// ---------------------------------------------------------------- Windows classic (10): flat, square, compact

static const ThemePalette k_classic_light = {
    .desktop = 0x808080, .menubar = 0xF0F0F0, .window = 0xF0F0F0, .popup = 0xF2F2F2,
    .title = 0xE3E3E3, .title_active = 0x99C9EF, .border = 0x7A7A7A, .text = 0x000000, .text_dim = 0x666666,
    .frame = 0xFFFFFF, .frame_hover = 0xF5FAFE, .frame_active = 0xCCE4F7,
    .button = 0xE1E1E1, .button_hover = 0xE5F1FB, .button_active = 0xCCE4F7,
    .accent = 0x0078D4, .accent_hover = 0x006CBE, .accent_active = 0x005A9E, .accent_text = 0xFFFFFF,
    .header = 0xCCE8FF, .header_hover = 0xE5F3FF, .header_active = 0x99D1FF,
    .tab = 0xDADADA, .tab_selected = 0xF0F0F0, .tab_overline = 0x0078D4,
    .check = 0x000000, .check_bg = 0xFFFFFF,
    .scroll_bg = 0xE8E8E8, .scroll_grab = 0x7A7A7A, .scroll_hover = 0x5F5F5F, .scroll_active = 0x4D4D4D,
    .separator = 0xA0A0A0, .text_sel = 0x99C9EF, .link = 0x0066CC, .focus = 0x0078D4, .heading = 0x000000,
    .ok = 0x107C10, .warn = 0x8A6100, .error = 0xC50F1F,
    .pending = 0xB84A00, .pending_bg = 0xFFE3CC, .pending_btn = 0xB84A00, .pending_btn_active = 0x9E4000,
    .pc_bg = 0xFFF29D, .bp_bg = 0xF7D3D6, .code_bg = 0xFFFFFF,
};

static const ThemePalette k_classic_dark = {
    .desktop = 0x141414, .menubar = 0x2B2B2B, .window = 0x202020, .popup = 0x2B2B2B,
    .title = 0x2B2B2B, .title_active = 0x1E4F7A, .border = 0x8C8C8C, .text = 0xFFFFFF, .text_dim = 0xA6A6A6,
    .frame = 0x121212, .frame_hover = 0x1C1C1C, .frame_active = 0x2A2A2A,
    .button = 0x333333, .button_hover = 0x454545, .button_active = 0x555555,
    .accent = 0x0078D4, .accent_hover = 0x006CBE, .accent_active = 0x005A9E, .accent_text = 0xFFFFFF,
    .header = 0x0063B1, .header_hover = 0x3E3E3E, .header_active = 0x005A9E,
    .tab = 0x2B2B2B, .tab_selected = 0x202020, .tab_overline = 0x4CA0E0,
    .check = 0xFFFFFF, .check_bg = 0x0063B1,
    .scroll_bg = 0x171717, .scroll_grab = 0x7A7A7A, .scroll_hover = 0x9A9A9A, .scroll_active = 0x4CA0E0,
    .separator = 0x4A4A4A, .text_sel = 0x264F78, .link = 0x66B3FF, .focus = 0x4CA0E0, .heading = 0xFFFFFF,
    .ok = 0x6CCB5F, .warn = 0xF2C94C, .error = 0xFF7B72,
    .pending = 0xFF9F45, .pending_bg = 0x4A2A0A, .pending_btn = 0xA85400, .pending_btn_active = 0x954A00,
    .pc_bg = 0x4D4400, .bp_bg = 0x4A1F1F, .code_bg = 0x121212,
};

static const ThemeMetrics k_classic_metrics = {
    .window_pad = ImVec2(8, 8), .frame_pad = ImVec2(6, 3), .item_spacing = ImVec2(6, 4),
    .item_inner = ImVec2(4, 4), .cell_pad = ImVec2(4, 2),
    .indent = 16, .scrollbar = 16, .grab_min = 10,
    .r_window = 0, .r_child = 0, .r_popup = 0, .r_frame = 0, .r_scroll = 0,
    .b_frame = 1, .b_tab = 1, .b_tabbar = 1, .sep_text = 1,
    .title_align = 0.0f,
};

// ---------------------------------------------------------------- SAPI industrial: the steel of the rack and
// the aluminium of the panels (src/frontend/rack.cpp), the dark one a control room (the review, palette 5.2)

static const ThemePalette k_sapi_light = {
    .desktop = 0x708498, .menubar = 0xD6DAE2, .window = 0xECEEF1, .popup = 0xF5F6F8,
    .title = 0xD6DAE2, .title_active = 0xB0C4D6, .border = 0x6E7C8A, .text = 0x14191E, .text_dim = 0x57606B,
    .frame = 0xFFFFFF, .frame_hover = 0xF2F5F8, .frame_active = 0xE2E9F0,
    .button = 0xDDE2E8, .button_hover = 0xCFD8E1, .button_active = 0xBCC9D6,
    .accent = 0x2F5F8F, .accent_hover = 0x3A6C9E, .accent_active = 0x264E76, .accent_text = 0xFFFFFF,
    .header = 0xB0C4D6, .header_hover = 0xD0DCE7, .header_active = 0x9DB4C9,
    .tab = 0xD6DAE2, .tab_selected = 0xECEEF1, .tab_overline = 0x2F5F8F,
    .check = 0x2F5F8F, .check_bg = 0xD9E4EE,
    .scroll_bg = THEME_NONE, .scroll_grab = 0x7D8996, .scroll_hover = 0x66727F, .scroll_active = 0x2F5F8F,
    .separator = 0xB4BCC6, .text_sel = 0xB0C4D6, .link = 0x245A8C, .focus = 0x2F5F8F, .heading = 0x2F5F8F,
    .ok = 0x256D29, .warn = 0x7A5A00, .error = 0xB3261E,
    .pending = 0x9A3F00, .pending_bg = 0xFFE3CC, .pending_btn = 0xB04C00, .pending_btn_active = 0x963F00,
    .pc_bg = 0xFFF1B8, .bp_bg = 0xF9DEDC, .code_bg = 0xF7F8FA,
};

static const ThemePalette k_sapi_dark = {
    .desktop = 0x121417, .menubar = 0x16181C, .window = 0x1B1E22, .popup = 0x23272D,
    .title = 0x1B1E22, .title_active = 0x3D6A99, .border = 0x7A838F, .text = 0xE6E9ED, .text_dim = 0x9AA3AE,
    .frame = 0x2A2F36, .frame_hover = 0x333A43, .frame_active = 0x3B444F,
    .button = 0x2F353D, .button_hover = 0x3A424C, .button_active = 0x46505C,
    .accent = 0x3D6A99, .accent_hover = 0x44739F, .accent_active = 0x335C85, .accent_text = 0xFFFFFF,
    .header = 0x3D6A99, .header_hover = 0x34536F, .header_active = 0x335C85,
    .tab = 0x23272D, .tab_selected = 0x2C4763, .tab_overline = 0x8FB8E0,
    .check = 0x8FB8E0, .check_bg = 0x2C4763,
    .scroll_bg = THEME_NONE, .scroll_grab = 0x646C77, .scroll_hover = 0x7A838F, .scroll_active = 0x8FB8E0,
    .separator = 0x3A4048, .text_sel = 0x2C4763, .link = 0x8FB8E0, .focus = 0x8FB8E0, .heading = 0x8FB8E0,
    .ok = 0x4CC38A, .warn = 0xE3B341, .error = 0xF47A70,
    .pending = 0xF0883E, .pending_bg = 0x4A2A0A, .pending_btn = 0xA85400, .pending_btn_active = 0x954A00,
    .pc_bg = 0x3A3418, .bp_bg = 0x4A2424, .code_bg = 0x15171A,
};

static const ThemeMetrics k_sapi_metrics = {
    .window_pad = ImVec2(10, 10), .frame_pad = ImVec2(8, 4), .item_spacing = ImVec2(8, 6),
    .item_inner = ImVec2(6, 4), .cell_pad = ImVec2(6, 3),
    .indent = 18, .scrollbar = 12, .grab_min = 10,
    .r_window = 4, .r_child = 3, .r_popup = 4, .r_frame = 2, .r_scroll = 2,
    .b_frame = 1, .b_tab = 0, .b_tabbar = 1, .sep_text = 2,
    .title_align = 0.5f,
};

const ThemeDef k_themes[THEME_VARIANTS] = {
    { "win11", "Windows 11", "Windows 11", &k_win11_light, &k_win11_dark, &k_win11_metrics, THEME_FONT_SEGOE_SEMIBOLD },
    { "classic", "Windows klasický", "Windows classic", &k_classic_light, &k_classic_dark, &k_classic_metrics,
      THEME_FONT_SEGOE_BOLD },
    { "sapi", "SAPI průmyslový", "SAPI industrial", &k_sapi_light, &k_sapi_dark, &k_sapi_metrics, THEME_FONT_DEJAVU },
};

// ---------------------------------------------------------------- the names of the looks

const char* theme_mode_key(ThemeMode mode)
{
    return mode == THEME_LIGHT ? "light" : mode == THEME_DARK ? "dark" : "system";
}

bool theme_parse(const char* s, ThemeVariant* variant, ThemeMode* mode)
{
    std::string t = s ? s : "";
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
