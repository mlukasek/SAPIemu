// The looks of the GUI (src/frontend/theme_data.cpp): every pair a user must read or see keeps the contrast of
// WCAG 2.2 in every palette: text 4.5:1, the edges of controls, ticks and grabs 3:1. Also --ui-theme parses.
#include <stdio.h>
#include <string.h>
#include "frontend/theme.h"
#include "test_util.h"

static int g_bad = 0;

static void need(const char* look, const char* what, uint32_t fg, uint32_t bg, double min)
{
    if (fg == THEME_NONE || bg == THEME_NONE) return;
    double c = theme_contrast(fg, bg);
    if (c + 1e-9 < min) {
        fprintf(stderr, "%s: %s %06X on %06X has %.2f:1, needs %.1f:1\n", look, what, (unsigned)fg, (unsigned)bg, c, min);
        g_bad++;
    }
}

static void check_palette(const char* look, const ThemePalette& p)
{
    const double TEXT = 4.5, UI = 3.0;
    // text on everything it is written on
    need(look, "text / window", p.text, p.window, TEXT);
    need(look, "text / popup", p.text, p.popup, TEXT);
    need(look, "text / menu bar", p.text, p.menubar, TEXT);
    need(look, "text / title", p.text, p.title, TEXT);
    need(look, "text / active title", p.text, p.title_active, TEXT);
    need(look, "text / field", p.text, p.frame, TEXT);
    need(look, "text / field hovered", p.text, p.frame_hover, TEXT);
    need(look, "text / button", p.text, p.button, TEXT);
    need(look, "text / button hovered", p.text, p.button_hover, TEXT);
    need(look, "text / button pressed", p.text, p.button_active, TEXT);
    need(look, "text / selection", p.text, p.header, TEXT);
    need(look, "text / selection hovered", p.text, p.header_hover, TEXT);
    need(look, "text / tab", p.text, p.tab, TEXT);
    need(look, "text / selected tab", p.text, p.tab_selected, TEXT);
    need(look, "text / selected text", p.text, p.text_sel, TEXT);
    need(look, "text / field with a change", p.text, p.pending_bg, TEXT);
    need(look, "text / PC row", p.text, p.pc_bg, TEXT);
    need(look, "text / breakpoint row", p.text, p.bp_bg, TEXT);
    need(look, "text / code block", p.text, p.code_bg, TEXT);
    need(look, "secondary text / window", p.text_dim, p.window, TEXT);
    need(look, "secondary text / field", p.text_dim, p.frame, TEXT);
    need(look, "secondary text / popup", p.text_dim, p.popup, TEXT);
    need(look, "heading / window", p.heading, p.window, TEXT);
    need(look, "link / window", p.link, p.window, TEXT);
    need(look, "ok / window", p.ok, p.window, TEXT);
    need(look, "warn / window", p.warn, p.window, TEXT);
    need(look, "error / window", p.error, p.window, TEXT);
    need(look, "pending / window", p.pending, p.window, TEXT);
    need(look, "pending / menu bar", p.pending, p.menubar, TEXT);
    // the main button and the button Restart
    need(look, "accent text / accent", p.accent_text, p.accent, TEXT);
    need(look, "accent text / accent hovered", p.accent_text, p.accent_hover, TEXT);
    need(look, "accent text / accent pressed", p.accent_text, p.accent_active, TEXT);
    need(look, "white / Restart", 0xFFFFFF, p.pending_btn, TEXT);
    need(look, "white / Restart pressed", 0xFFFFFF, p.pending_btn_active, TEXT);
    // what must be seen: edges, ticks, grabs, the focus
    need(look, "border / window", p.border, p.window, UI);
    need(look, "border / field", p.border, p.frame, UI);
    need(look, "tick / ticked box", p.check, p.check_bg, UI);
    need(look, "radio dot / field", p.check, p.frame, UI);
    need(look, "scroll grab / track", p.scroll_grab, p.scroll_bg == THEME_NONE ? p.window : p.scroll_bg, UI);
    need(look, "slider grab / field", p.focus, p.frame, UI);
    need(look, "slider grab pressed / field", p.link, p.frame, UI);
    need(look, "focus / window", p.focus, p.window, UI);
}

int main(void)
{
    // the formula: black on white 21:1, a colour on itself 1:1, the order does not matter
    CHECK(theme_contrast(0x000000, 0xFFFFFF) > 20.99 && theme_contrast(0x000000, 0xFFFFFF) < 21.01);
    CHECK(theme_contrast(0x777777, 0x777777) == 1.0);
    CHECK(theme_contrast(0x005FB8, 0xFFFFFF) == theme_contrast(0xFFFFFF, 0x005FB8));

    for (int v = 0; v < THEME_VARIANTS; v++) {
        const ThemeDef& d = k_themes[v];
        CHECK(d.key && d.name_cs && d.name_en && d.light && d.dark && d.metrics);
        char look[48];
        snprintf(look, sizeof(look), "%s-light", d.key);
        check_palette(look, *d.light);
        snprintf(look, sizeof(look), "%s-dark", d.key);
        check_palette(look, *d.dark);
        // the dark palette is dark, the light one light
        CHECK(theme_contrast(d.light->window, 0xFFFFFF) < theme_contrast(d.dark->window, 0xFFFFFF));
    }
    CHECK(g_bad == 0);

    ThemeVariant v = THEME_WIN11;
    ThemeMode m = THEME_SYSTEM;
    CHECK(theme_parse("sapi-dark", &v, &m) && v == THEME_SAPI && m == THEME_DARK);
    CHECK(theme_parse("classic", &v, &m) && v == THEME_CLASSIC && m == THEME_SYSTEM);
    CHECK(theme_parse("win11-light", &v, &m) && v == THEME_WIN11 && m == THEME_LIGHT);
    CHECK(theme_parse("win11-system", &v, &m) && m == THEME_SYSTEM);
    CHECK(!theme_parse("win11-blue", &v, &m));
    CHECK(!theme_parse("amiga", &v, &m));
    CHECK(!theme_parse(nullptr, &v, &m));
    CHECK(strcmp(theme_mode_key(THEME_DARK), "dark") == 0);
    return TEST_RESULT();
}
