// Video of a display output with the sound (right click on a picture, View, Ctrl+Alt+F5): MP4 (H.264 +
// AAC) made by ffmpeg. The frames are taken at 50 per second of EMULATED time (EmuHost::tick_hook) and
// piped to ffmpeg as raw BGRA at the exact dots of the card, enlarged there by EVEN factors (4 lines per
// dot, 2 for VGA cards; the width by the TV aspect): H.264 in yuv420p keeps one colour per 2 x 2 block,
// with an odd factor the colour edges between dots get smeared. The sound of the bus is recorded meanwhile
// (Bus::audio_record); at the end a second ffmpeg run puts the two together. Picture and sound keep
// in step at any emulation speed; a pause pauses the video.
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <thread>
#include <SDL3/SDL.h>
#include "core/devices.h"
#include "core/host.h"
#include "core/log.h"
#include "core/machine.h"
#include "core/util.h"
#include "core/wav.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

const int FPS = 50;   // PAL

std::filesystem::path fspath(const std::string& u)
{
    return std::filesystem::path(std::u8string(u.begin(), u.end()));
}

std::string u8str(const std::filesystem::path& p)
{
    std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

// ---------------------------------------------------------------- a child process with stdin

struct Proc {
#ifdef _WIN32
    HANDLE process = nullptr, in = nullptr;
#else
    FILE* in = nullptr;
#endif
};

#ifdef _WIN32
std::wstring wide(const std::string& s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w((size_t)(n > 0 ? n : 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    w.resize(wcslen(w.c_str()));
    return w;
}
#endif

// Start `cmd` (UTF-8) without a console window, stdout and stderr appended to `log`; with_stdin:
// a pipe to its standard input.
bool proc_start(Proc& p, const std::string& cmd, bool with_stdin, const std::string& log, std::string& err)
{
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (with_stdin) {
        if (!CreatePipe(&rd, &wr, &sa, 1 << 20)) { err = "CreatePipe"; return false; }
        SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);
    }
    HANDLE lg = CreateFileW(wide(log).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = with_stdin ? rd : nullptr;
    si.hStdOutput = lg != INVALID_HANDLE_VALUE ? lg : nullptr;
    si.hStdError = si.hStdOutput;
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    std::wstring w = wide(cmd);
    BOOL ok = CreateProcessW(nullptr, &w[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (rd) CloseHandle(rd);
    if (lg != INVALID_HANDLE_VALUE) CloseHandle(lg);
    if (!ok) {
        if (wr) CloseHandle(wr);
        err = "cannot start ffmpeg";
        return false;
    }
    CloseHandle(pi.hThread);
    p.process = pi.hProcess;
    p.in = wr;
    return true;
#else
    std::string c = cmd + " >>\"" + log + "\" 2>&1";
    if (!with_stdin) {
        p.in = nullptr;
        return true;   // run by proc_wait through system()
    }
    p.in = popen(c.c_str(), "w");
    if (!p.in) { err = "cannot start ffmpeg"; return false; }
    return true;
#endif
}

bool proc_write(Proc& p, const void* data, size_t n)
{
#ifdef _WIN32
    const char* d = (const char*)data;
    while (n) {
        DWORD done = 0;
        if (!WriteFile(p.in, d, (DWORD)(n > 0x40000000 ? 0x40000000 : n), &done, nullptr) || !done) return false;
        d += done;
        n -= done;
    }
    return true;
#else
    return fwrite(data, 1, n, p.in) == n;
#endif
}

// Close the input (end of the stream) and wait for the end; true = exit code 0.
bool proc_finish(Proc& p)
{
#ifdef _WIN32
    if (p.in) CloseHandle(p.in);
    p.in = nullptr;
    if (!p.process) return false;
    WaitForSingleObject(p.process, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(p.process, &code);
    CloseHandle(p.process);
    p.process = nullptr;
    return code == 0;
#else
    int rc = p.in ? pclose(p.in) : -1;
    p.in = nullptr;
    return rc == 0;
#endif
}

// Run a command to its end (the mux); POSIX through the shell.
bool proc_run(const std::string& cmd, const std::string& log)
{
#ifdef _WIN32
    Proc p;
    std::string err;
    if (!proc_start(p, cmd, false, log, err)) return false;
    return proc_finish(p);
#else
    return system((cmd + " >>\"" + log + "\" 2>&1").c_str()) == 0;
#endif
}

// ---------------------------------------------------------------- the recording

struct Recording {
    bool active = false;
    UiState* ui = nullptr;
    DisplayDevice* d = nullptr;
    uint64_t gen = 0;
    int w = 0, h = 0;
    std::vector<uint32_t> frame, render;
    Proc ff;
    uint64_t frames = 0;
    bool failed = false;
    std::string out, tmp_video, tmp_wav, log;
};
Recording s_rec;

// the mux of the last recording, in a thread: 0 idle, 1 running, 2 done, 3 failed
std::atomic<int> s_mux{ 0 };
std::thread s_mux_thread;
std::string s_mux_out;

std::string dq(const std::string& s)
{
    return "\"" + s + "\"";
}

std::string native(std::string p)
{
#ifdef _WIN32
    for (char& c : p) if (c == '/') c = '\\';
#endif
    return p;
}

void notice(UiState& ui, bool ok, const std::string& text)
{
    ui.notice = text;
    ui.notice_error = !ok;
    ui.notice_ns = SDL_GetTicksNS();
}

// One frame: the picture now, in the size the recording started with (a change of the raster is cut
// or padded with black).
void capture(uint64_t)
{
    Recording& r = s_rec;
    if (!r.active || r.failed) return;
    if (r.ui->host->machine_generation() != r.gen) {   // another machine: the display is gone
        r.failed = true;
        return;
    }
    int w = r.d->screen_width(), h = r.d->screen_height();
    r.render.assign((size_t)w * h, 0xFF000000u);
    r.d->render(r.render.data(), 0xFF000000u);
    std::fill(r.frame.begin(), r.frame.end(), 0xFF000000u);
    for (int y = 0; y < h && y < r.h; y++)
        memcpy(&r.frame[(size_t)y * r.w], &r.render[(size_t)y * w], sizeof(uint32_t) * (size_t)(w < r.w ? w : r.w));
    if (!proc_write(r.ff, r.frame.data(), r.frame.size() * 4)) {
        r.failed = true;
        return;
    }
    r.frames++;
}

void mux_job(std::string ffmpeg, std::string video, std::string wav, std::string out, std::string log, bool sound)
{
    bool ok;
    if (sound) {
        std::string cmd = dq(ffmpeg) + " -hide_banner -loglevel error -y -i " + dq(native(video)) + " -i " +
                          dq(native(wav)) + " -map 0:v -map 1:a -c:v copy -c:a aac -b:a 160k -movflags +faststart " +
                          dq(native(out));
        ok = proc_run(cmd, log);
    } else {
        std::error_code ec;
        std::filesystem::rename(fspath(video), fspath(out), ec);
        ok = !ec;
    }
    std::error_code ec;
    std::filesystem::remove(fspath(video), ec);
    std::filesystem::remove(fspath(wav), ec);
    s_mux = ok ? 2 : 3;
}

}   // namespace

// ffmpeg: the path of the settings, else found on PATH; empty if none
std::string video_ffmpeg(UiState& ui)
{
    std::error_code ec;
    if (!ui.ffmpeg_path.empty()) return std::filesystem::exists(fspath(ui.ffmpeg_path), ec) ? ui.ffmpeg_path : std::string();
    const char* path = getenv("PATH");
#ifdef _WIN32
    const char sep = ';';
    const char* exe = "ffmpeg.exe";
#else
    const char sep = ':';
    const char* exe = "ffmpeg";
#endif
    std::string all = path ? path : "";
    size_t a = 0;
    while (a <= all.size()) {
        size_t b = all.find(sep, a);
        if (b == std::string::npos) b = all.size();
        std::string dir = all.substr(a, b - a);
        if (!dir.empty()) {
            std::filesystem::path p = std::filesystem::path(dir) / exe;
            if (std::filesystem::exists(p, ec)) return u8str(p);
        }
        a = b + 1;
    }
    return std::string();
}

bool video_recording(UiState&)
{
    return s_rec.active;
}

bool video_busy(UiState&)
{
    return s_rec.active || s_mux != 0;
}

double video_seconds(UiState&)
{
    return s_rec.active ? (double)s_rec.frames / FPS : 0.0;
}

void video_start(UiState& ui, DisplayDevice* d, const char* name, float px_aspect)
{
    Machine* m = ui.host->machine();
    if (!d || !m || s_rec.active) return;
    if (s_mux == 1) {
        notice(ui, false, tr("The previous video is still being finished."));
        return;
    }
    if (s_mux_thread.joinable()) s_mux_thread.join();
    std::string ffmpeg = video_ffmpeg(ui);
    if (ffmpeg.empty()) {
        notice(ui, false, tr("Video needs ffmpeg: install it (winget install ffmpeg) or put its path into the settings "
                             "file (\"video\": { \"ffmpeg\": \"C:/.../ffmpeg.exe\" })."));
        return;
    }
    Recording& r = s_rec;
    r = Recording();
    r.ui = &ui;
    r.d = d;
    r.gen = ui.host->machine_generation();
    r.w = d->screen_width();
    r.h = d->screen_height();
    r.frame.assign((size_t)r.w * r.h, 0xFF000000u);
    // even factors (each dot fills whole 2 x 2 colour blocks of yuv420p), at most 1440 lines: 4 lines per
    // dot, 2 for VGA cards (480 lines); the width by the aspect rounded to an even factor
    int sy = r.h * 4 <= 1440 ? 4 : r.h * 2 <= 1440 ? 2 : 1;
    int sx = sy >= 2 ? 2 * (int)lroundf((float)sy * px_aspect / 2.0f) : (int)lroundf((float)sy * px_aspect);
    if (sx < 1) sx = 1;
    // work/videa/<card>_<YYYYMMDD-HHMMSS>.mp4
    std::string n;
    for (const char* p = name; p && *p; p++) {
        char c = *p;
        n += ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-') ? c : '_';
    }
    SDL_Time t = 0;
    SDL_DateTime dt;
    char stamp[32] = "";
    if (SDL_GetCurrentTime(&t) && SDL_TimeToDateTime(t, &dt, true))
        snprintf(stamp, sizeof(stamp), "%04d%02d%02d-%02d%02d%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    std::string dir = data_path("work/videa");
    make_dirs(dir);
    std::string base = path_join(dir, (n.empty() ? std::string("screen") : n) + "_" + stamp);
    r.out = base + ".mp4";
    r.tmp_video = base + "_video.mp4";
    r.tmp_wav = base + "_zvuk.wav";
    r.log = path_join(dir, "ffmpeg.log");
    char geo[96];
    snprintf(geo, sizeof(geo), "-s %dx%d -framerate %d", r.w, r.h, FPS);
    char vf[160];
    snprintf(vf, sizeof(vf), "scale=iw*%d:ih*%d:flags=neighbor,pad=ceil(iw/2)*2:ceil(ih/2)*2,setsar=1,format=yuv420p", sx, sy);
    std::string cmd = dq(ffmpeg) + " -hide_banner -loglevel error -y -f rawvideo -pix_fmt bgra " + geo +
                      " -i - -vf " + vf + " -c:v libx264 -preset veryfast -crf 14 " + dq(native(r.tmp_video));
    std::string err;
    if (!proc_start(r.ff, cmd, true, r.log, err)) {
        notice(ui, false, std::string(tr("Cannot start ffmpeg")) + ": " + ffmpeg);
        return;
    }
    r.active = true;
    s_mux_out = r.out;
    m->bus.audio_record(true);   // the sound from now on, as the frames
    capture(m->bus.now);         // frame 0 at once
    ui.host->tick_period = m->bus.cpu_hz / FPS;
    ui.host->tick_next = m->bus.now + ui.host->tick_period;
    ui.host->tick_hook = capture;
    LOGI("video: recording %s (%dx%d, %d fps)", r.out.c_str(), r.w, r.h, FPS);
}

void video_stop(UiState& ui)
{
    Recording& r = s_rec;
    if (!r.active) return;
    ui.host->tick_hook = nullptr;
    ui.host->tick_period = 0;
    r.active = false;
    bool encoded = proc_finish(r.ff);
    bool sound = false;
    Machine* m = ui.host->machine();
    if (m && ui.host->machine_generation() == r.gen) {
        m->bus.audio_collect();
        const std::vector<int16_t>& s = m->bus.audio_rec;
        // the sound of exactly the recorded frames
        size_t want = (size_t)((double)r.frames * m->bus.audio_rate / FPS);
        std::vector<int16_t> a(s.begin(), s.begin() + (std::ptrdiff_t)(s.size() < want ? s.size() : want));
        a.resize(want, 0);
        sound = wav_write_mono16(r.tmp_wav, a.data(), a.size(), m->bus.audio_rate);
        m->bus.audio_record(false);
    }
    if (!encoded || r.frames == 0) {
        notice(ui, false, std::string(tr("The video was not recorded (ffmpeg failed)")) + ":\n" + r.log);
        std::error_code ec;
        std::filesystem::remove(fspath(r.tmp_video), ec);
        std::filesystem::remove(fspath(r.tmp_wav), ec);
        return;
    }
    s_mux = 1;
    s_mux_thread = std::thread(mux_job, video_ffmpeg(ui), r.tmp_video, r.tmp_wav, r.out, r.log, sound);
    notice(ui, true, std::string(tr("Finishing the video")) + ": " + r.out);
}

void video_poll(UiState& ui)
{
    if (s_rec.active && (s_rec.failed || ui.host->machine_generation() != s_rec.gen)) {
        bool broken = s_rec.failed && ui.host->machine_generation() == s_rec.gen;
        video_stop(ui);
        if (broken) notice(ui, false, std::string(tr("The video was not recorded (ffmpeg failed)")) + ":\n" + s_rec.log);
    }
    int st = s_mux.load();
    if (st == 2 || st == 3) {
        if (s_mux_thread.joinable()) s_mux_thread.join();
        s_mux = 0;
        if (st == 2) notice(ui, true, std::string(tr("Video saved")) + ": " + s_mux_out);
        else notice(ui, false, std::string(tr("The video was not recorded (ffmpeg failed)")) + ":\n" + s_rec.log);
    }
}

void video_shutdown(UiState& ui)
{
    video_stop(ui);
    if (s_mux_thread.joinable()) s_mux_thread.join();
}
