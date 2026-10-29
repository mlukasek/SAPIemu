#pragma once
// The look of the GUI: Windows 11, light and dark (as the system says or chosen). The palettes and sizes are data
// (theme_data.cpp, room for more looks); theme_apply() builds the ImGui style from them between frames. The windows
// take the semantic colours from theme() instead of fixed ones; the drawings of hardware (the rack, the panels,
// the displays, the paper) keep their own colours in every look. Plan: docs/vydani/plan-vzhled.md.
#include <stdint.h>
#include "imgui.h"

struct ImFont;
struct ImGuiIO;
struct SDL_Window;

enum ThemeVariant { THEME_WIN11 = 0, THEME_VARIANTS };   // more looks may come back (commit 1fc9a21)
enum ThemeMode { THEME_SYSTEM = 0, THEME_LIGHT, THEME_DARK };

// Colours as data: 0xRRGGBB; THEME_NONE = transparent.
#define THEME_NONE 0xFFFFFFFFu

struct ThemePalette {
    uint32_t desktop;              // behind the windows (the clear colour of the main window)
    uint32_t menubar;              // the menu bar (Windows 11: the title bar of the OS window too)
    uint32_t window, popup;        // window and popup / menu background
    uint32_t title, title_active;  // title bars
    uint32_t border;               // windows, frames, check boxes (3:1 against the window)
    uint32_t text, text_dim;       // text, secondary text (4.5:1)
    uint32_t frame, frame_hover, frame_active;      // input fields, check boxes, sliders
    uint32_t button, button_hover, button_active;   // ordinary buttons
    uint32_t accent, accent_hover, accent_active, accent_text;   // the main button of a window (Start, DEMO)
    uint32_t header, header_hover, header_active;   // selected items, collapsing headers
    uint32_t tab, tab_selected, tab_overline;
    uint32_t check, check_bg;      // tick and the background of a ticked box
    uint32_t scroll_bg, scroll_grab, scroll_hover, scroll_active;
    uint32_t separator, text_sel;  // lines, selected text
    uint32_t link, focus;          // links, the frame of the focused window
    uint32_t heading;              // titles (machine names, guides, About)
    uint32_t ok, warn, error;      // states in words
    uint32_t pending;              // "configuration changed: restart"
    uint32_t pending_bg;           // a field with a change waiting for the restart
    uint32_t pending_btn, pending_btn_active;       // the button Restart (white text)
    uint32_t pc_bg, bp_bg;         // debugger rows: the PC, a breakpoint
    uint32_t code_bg;              // code blocks of the guides
};

struct ThemeMetrics {
    ImVec2 window_pad, frame_pad, item_spacing, item_inner, cell_pad;
    float indent, scrollbar, grab_min;
    float r_window, r_child, r_popup, r_frame, r_scroll;   // rounding; grabs, tabs, menu items and images as frames
    float b_frame, b_tab, b_tabbar, sep_text;               // border sizes (windows, children, popups: 1)
    float title_align;             // 0 = left, 0.5 = centred like the label of an instrument
};

struct ThemeDef {
    const char* key;               // "win11" (settings, --ui-theme)
    const char* name_cs;
    const char* name_en;
    const ThemePalette* light;
    const ThemePalette* dark;
    const ThemeMetrics* metrics;
    int font;                      // THEME_FONT_*: the font of the user interface
};
enum { THEME_FONT_SEGOE_SEMIBOLD = 0, THEME_FONT_SEGOE_BOLD, THEME_FONT_DEJAVU };
extern const ThemeDef k_themes[THEME_VARIANTS];

// The colours of the look in use, converted for the windows.
struct ThemeColors {
    bool dark;
    ImVec4 text, text_dim, heading, link;
    ImVec4 ok, warn, error, pending, pending_bg;
    ImVec4 accent, accent_hover, accent_active, accent_text;
    ImVec4 pending_btn, pending_btn_hover, pending_btn_active;
    ImVec4 pc_bg, bp_bg, code_bg;
    ImU32 desktop, focus;
};
const ThemeColors& theme();

// --ui-theme light|dark|win11[-light|-dark]: the look of this run, not saved (like --lang).
bool theme_parse(const char* s, ThemeVariant* variant, ThemeMode* mode);
void theme_set_override(ThemeVariant variant, ThemeMode mode);
bool theme_override(ThemeVariant* variant, ThemeMode* mode);
const char* theme_mode_key(ThemeMode mode);   // "system", "light", "dark"

// The fonts of all looks, loaded once (Dear ImGui bakes the glyphs on demand): Segoe UI from the system on
// Windows (it may not be distributed), DejaVu Sans elsewhere; DejaVu Sans Mono for the terminal and the debugger;
// DejaVu Sans for the lettering of the drawn panels in every look.
struct ThemeFonts {
    ImFont* ui[THEME_VARIANTS];
    ImFont* bold[THEME_VARIANTS];
    ImFont* panel;
    ImFont* panel_bold;
    ImFont* mono;
};
void theme_fonts_load(ImGuiIO& io);
const ThemeFonts& theme_fonts();

// Builds the style of a look from scratch (ScaleAllSizes is lossy, so a change never starts from the style in
// use) and selects its font. Between frames only, before NewFrame. text_scale: View -> Settings, 1.0 = 100 %.
void theme_apply(ThemeVariant variant, bool dark, float dpi, float text_scale);
ThemeVariant theme_variant();

// The title bar of the OS window follows the look (Windows; elsewhere nothing): light or dark, on Windows 11 also in
// the colour of the menu bar with the text of the look, so that the title bar and the menu bar read as one band
// (an inactive window: the secondary text, as Windows dims it). Called after every theme_apply() and when the window
// gains or loses the focus: SDL sets the dark mode by the system at every change of the colours of Windows.
void theme_titlebar(SDL_Window* window);

// After all windows, before Render: the frame of the focused window (where the keyboard goes).
void theme_focus_frame();

// The main button of a window in the accent colour; small: without the vertical padding (as SmallButton).
bool theme_accent_button(const char* label, const ImVec2& size = ImVec2(0, 0), bool small = false);
// A bar at the left edge of the last item: a selected list item is not marked by its colour only.
void theme_selected_bar();
// The text of a title in the bold font and the heading colour (wrapped).
void theme_heading(const char* text, float size = 0.0f);
// A section title: the separator line with its text in the bold font (Settings, About, the guides).
void theme_section(const char* text);

// The lettering of the drawn hardware (the rack, the panels, the instruments): DejaVu Sans in every look, as the
// drawings were made for it, at the size of the text times `scale`.
ImFont* panel_font(bool bold = false);
void panel_text(ImDrawList* dl, const ImVec2& pos, ImU32 col, const char* text, float scale = 1.0f);
ImVec2 panel_text_size(const char* text, float scale = 1.0f);
// A lamp of the drawn hardware at `c` with the radius `r` (pixels) under the mouse: a tooltip with its state, so that
// the state is not told by the colour alone (WCAG 1.4.1). name: the lamp ("RDY"), nullptr: the state alone.
void lamp_tooltip(const ImVec2& c, float r, const char* name, bool on);

// The width of a combo box for its longest item in the UI language (the text, the padding and the arrow).
float combo_width(const char* const* items, int count);

// The sRGB contrast ratio of WCAG 2.2 (1..21) of two 0xRRGGBB colours.
double theme_contrast(uint32_t a, uint32_t b);
