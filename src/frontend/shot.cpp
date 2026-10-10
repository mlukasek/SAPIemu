// Screenshots of the display outputs (right click on a picture, View -> Screenshot, Ctrl+F5): the
// exact dots of the emulated picture as PNG, or enlarged 3x with the TV aspect; automatically named in
// work/snimky, into a chosen file, or to the clipboard as PNG and BMP (Windows: CF_DIB for every program).
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <math.h>
#include <stdio.h>
#include <SDL3/SDL.h>
#include "core/devices.h"
#include "core/png.h"
#include "core/util.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static const uint32_t k_shot_bg = 0xFF000000u;   // a monochrome picture's background

static void shot_notice(UiState& ui, bool ok, const std::string& text)
{
    ui.notice = text;
    ui.notice_error = !ok;
    ui.notice_ns = SDL_GetTicksNS();
}

// The picture and the repeat of its dots: 1x1, or 3 lines per dot and the width by the TV aspect
static void shot_pixels(UiState& ui, DisplayDevice* d, float px_aspect, std::vector<uint32_t>& px, int& w, int& h,
                        int& sx, int& sy)
{
    w = d->screen_width();
    h = d->screen_height();
    px.assign((size_t)w * h, k_shot_bg);
    d->render(px.data(), k_shot_bg);
    sx = sy = 1;
    if (ui.shot_enlarged) {
        sy = 3;
        sx = (int)lroundf(3.0f * px_aspect);
        if (sx < 1) sx = 1;
    }
}

// work/snimky/<card>_<YYYYMMDD-HHMMSS>.png (letters, digits, - and _ of the card name)
static std::string shot_auto_path(const char* name)
{
    std::string n;
    for (const char* p = name; p && *p; p++) {
        char c = *p;
        n += ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-') ? c : '_';
    }
    if (n.empty()) n = "screen";
    SDL_Time t = 0;
    SDL_DateTime dt;
    char stamp[32] = "";
    if (SDL_GetCurrentTime(&t) && SDL_TimeToDateTime(t, &dt, true))
        snprintf(stamp, sizeof(stamp), "%04d%02d%02d-%02d%02d%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    std::string dir = data_path("work/snimky");
    make_dirs(dir);
    std::string base = path_join(dir, n + "_" + stamp);
    std::string path = base + ".png";
    for (int i = 2; file_exists(path) && i < 100; i++) path = base + "-" + std::to_string(i) + ".png";
    return path;
}

static void shot_write(UiState& ui, const std::string& path, const std::vector<uint32_t>& px, int w, int h, int sx, int sy)
{
    if (png_write_argb(path, px.data(), w, h, sx, sy))
        shot_notice(ui, true, std::string(tr("Screenshot saved")) + ": " + path);
    else
        shot_notice(ui, false, std::string(tr("Cannot save the screenshot")) + ":\n" + path);
}

void shot_save(UiState& ui, DisplayDevice* d, const char* name, float px_aspect)
{
    if (!d) return;
    std::vector<uint32_t> px;
    int w, h, sx, sy;
    shot_pixels(ui, d, px_aspect, px, w, h, sx, sy);
    shot_write(ui, shot_auto_path(name), px, w, h, sx, sy);
}

static void SDLCALL shot_dialog_done(void* userdata, const char* const* files, int)
{
    UiState::ShotDialog* s = (UiState::ShotDialog*)userdata;
    std::lock_guard<std::mutex> lk(s->mtx);
    s->result = (files && files[0]) ? files[0] : "";
    s->done = true;
}

void shot_save_as(UiState& ui, DisplayDevice* d, const char* name, float px_aspect)
{
    UiState::ShotDialog& s = ui.shot_dlg;
    if (!d) return;
    {
        std::lock_guard<std::mutex> lk(s.mtx);
        if (s.active) return;
        s.active = true;
        s.done = false;
    }
    shot_pixels(ui, d, px_aspect, s.px, s.w, s.h, s.sx, s.sy);   // the picture of now, not of the moment the file is chosen
    static const SDL_DialogFileFilter filter[] = { { "PNG", "png" } };
    std::string where = dialog_location(shot_auto_path(name));   // the name offered in its folder
    SDL_ShowSaveFileDialog(shot_dialog_done, &s, ui.window, filter, 1, where.c_str());
}

void shot_poll(UiState& ui)
{
    UiState::ShotDialog& s = ui.shot_dlg;
    std::string path;
    {
        std::lock_guard<std::mutex> lk(s.mtx);
        if (!s.active || !s.done) return;
        s.active = false;
        path = s.result;
    }
    if (path.empty()) return;   // cancelled
    size_t slash = path.find_last_of("/\\");
    if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".png";
    shot_write(ui, path, s.px, s.w, s.h, s.sx, s.sy);
    s.px.clear();
}

// The clipboard keeps the encoded pictures until another program takes it over.
static std::vector<uint8_t> s_clip_png, s_clip_bmp;

static const void* SDLCALL shot_clip_data(void*, const char* mime, size_t* size)
{
    const std::vector<uint8_t>& v = SDL_strcmp(mime, "image/png") == 0 ? s_clip_png : s_clip_bmp;
    *size = v.size();
    return v.data();
}

#ifdef _WIN32
// SDL 3 puts only the first picture format on the Windows clipboard (WIN_SetClipboardData): Word reads
// "PNG", most programs (file managers, terminals, painting programs) only CF_DIB. Both are written here.
static bool clip_put(UINT format, const uint8_t* data, size_t size)
{
    HGLOBAL hm = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hm) return false;
    void* p = GlobalLock(hm);
    if (!p) {
        GlobalFree(hm);
        return false;
    }
    memcpy(p, data, size);
    GlobalUnlock(hm);
    if (SetClipboardData(format, hm)) return true;   // the clipboard owns it now
    GlobalFree(hm);
    return false;
}

static bool clip_windows(UiState& ui)
{
    HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(ui.window), SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                                             nullptr);
    bool open = false;
    for (int i = 0; i < 10 && !(open = OpenClipboard(hwnd) != 0); i++) SDL_Delay(10);   // another program may hold it
    if (!open) return false;
    EmptyClipboard();
    const size_t file_header = 14;   // BITMAPFILEHEADER: CF_DIB is the BMP without it
    bool ok = s_clip_bmp.size() > file_header &&
              clip_put(CF_DIB, s_clip_bmp.data() + file_header, s_clip_bmp.size() - file_header);
    ok = clip_put(RegisterClipboardFormatA("PNG"), s_clip_png.data(), s_clip_png.size()) && ok;
    CloseClipboard();
    return ok;
}
#endif

void shot_copy(UiState& ui, DisplayDevice* d, float px_aspect)
{
    if (!d) return;
    std::vector<uint32_t> px;
    int w, h, sx, sy;
    shot_pixels(ui, d, px_aspect, px, w, h, sx, sy);
    s_clip_png = png_encode_argb(px.data(), w, h, sx, sy);
    s_clip_bmp = bmp_encode_argb(px.data(), w, h, sx, sy);
#ifdef _WIN32
    if (clip_windows(ui))
        shot_notice(ui, true, tr("Screenshot copied to the clipboard."));
    else
        shot_notice(ui, false, std::string(tr("Cannot copy the screenshot")) + ": Windows error " +
                                   std::to_string((unsigned long)GetLastError()));
#else
    static const char* mimes[] = { "image/png", "image/bmp" };
    if (SDL_SetClipboardData(shot_clip_data, nullptr, nullptr, mimes, 2))
        shot_notice(ui, true, tr("Screenshot copied to the clipboard."));
    else
        shot_notice(ui, false, std::string(tr("Cannot copy the screenshot")) + ":\n" + SDL_GetError());
#endif
}
