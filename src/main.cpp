// SAPIemu entry point. sapiemu (GUI build) opens the window unless --headless is
// given; sapiemu-cli always runs headless.
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "core/headless.h"
#include "core/log.h"
#include "core/machine.h"
#include "core/util.h"
#include "core/version.h"

#ifdef SAPIEMU_GUI
#include "frontend/app.h"
#include <SDL3/SDL_main.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

static const char* DEFAULT_MACHINE = "machines/mikromon.jsonc";

static void print_help(void)
{
    printf("SAPIemu %s - Tesla SAPI-1 emulator\n\n"
           "usage: sapiemu [options] [--headless] [actions...]\n"
           "       sapiemu-cli [options] [actions...]\n\n"
           "options:\n"
           "  --machine FILE     machine description (default %s)\n"
           "  --data-root DIR    directory with roms/, media/, machines/\n"
           "  --verbose          debug log on stderr\n"
           "  --version, --help\n\n"
           "headless actions (executed in order):\n"
           "  --run-ms N         run N ms of emulated time\n"
           "  --type TEXT        type on the keyboard (escapes \\r \\e \\t \\xNN)\n"
           "  --wait-text TEXT   run until TEXT is on screen (see --timeout-ms)\n"
           "  --timeout-ms N     timeout for --wait-text (emulated ms, default 5000)\n"
           "  --print-screen     print the screen text\n"
           "  --serial-send TEXT send to the first serial channel\n"
           "  --print-serial     print data received on the first serial channel\n"
           "  --break ADDR       set an execution breakpoint (hex)\n"
           "  --print-regs       print CPU registers\n"
           "  --screenshot FILE  save the screen as PNG (2x)\n",
           sapiemu_version(), DEFAULT_MACHINE);
}

static bool action_has_arg(const std::string& a)
{
    return a == "--run-ms" || a == "--type" || a == "--wait-text" || a == "--timeout-ms" ||
           a == "--serial-send" || a == "--break" || a == "--screenshot";
}

#if defined(SAPIEMU_GUI) && defined(_WIN32)
static void attach_parent_console(void)
{
    // GUI subsystem: reuse the console of the shell that started us, if any
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
}
#endif

// Command line arguments as UTF-8 (argv on Windows is in the ANSI code page).
static std::vector<std::string> utf8_args(int argc, char** argv)
{
    std::vector<std::string> out;
#ifdef _WIN32
    int n = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &n);
    if (wargv) {
        for (int i = 0; i < n; i++) {
            int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
            std::string s(len > 0 ? len - 1 : 0, '\0');
            if (len > 1) WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &s[0], len, NULL, NULL);
            out.push_back(s);
        }
        LocalFree(wargv);
        return out;
    }
#endif
    for (int i = 0; i < argc; i++) out.push_back(argv[i]);
    return out;
}

int main(int argc_ansi, char** argv_ansi)
{
    std::vector<std::string> argv = utf8_args(argc_ansi, argv_ansi);
    int argc = (int)argv.size();
    std::string machine_file = DEFAULT_MACHINE;
#ifdef SAPIEMU_GUI
    bool headless = false;
#else
    bool headless = true;
#endif
    std::vector<std::string> actions;

#if defined(SAPIEMU_GUI) && defined(_WIN32)
    for (int i = 1; i < argc; i++)
        if (argv[i] == "--headless" || argv[i] == "--help" || argv[i] == "--version") {
            attach_parent_console();
            break;
        }
#endif
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   // screen dumps contain Czech letters and box drawing
#endif

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--version") { printf("SAPIemu %s\n", sapiemu_version()); return 0; }
        else if (a == "--help" || a == "-h") { print_help(); return 0; }
        else if (a == "--machine" && i + 1 < argc) machine_file = argv[++i];
        else if (a == "--data-root" && i + 1 < argc) data_root_set(argv[++i]);
        else if (a == "--verbose") log_set_level(LOG_DEBUG);
        else if (a == "--headless") headless = true;
        else {
            actions.push_back(a);
            if (action_has_arg(a) && i + 1 < argc) actions.push_back(argv[++i]);
        }
    }

    data_root_init();
    LOGD("data root: %s", data_root().c_str());

    if (headless) {
        Machine m;
        std::string err;
        if (!m.load_file(machine_file, err)) {
            LOGE("%s", err.c_str());
            return 1;
        }
        int rc = headless_run(m, actions);
        fflush(stdout);
        return rc;
    }

#ifdef SAPIEMU_GUI
    if (!actions.empty()) {
        fprintf(stderr, "unknown option '%s' (see --help)\n", actions[0].c_str());
        return 1;
    }
    return app_run(machine_file);
#else
    return 0;
#endif
}
