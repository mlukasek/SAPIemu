// SAPIemu entry point. sapiemu (GUI build) opens the window unless --headless is
// given; sapiemu-cli always runs headless.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "core/headless.h"
#include "core/host.h"
#include "core/log.h"
#include "core/machine.h"
#include "core/util.h"
#include "core/version.h"
#include "mcp/mcp_server.h"

#include <atomic>
#include <memory>
#include <signal.h>

#include "frontend/app.h"
#ifdef SAPIEMU_GUI
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
           "  --mcp-port N       MCP server port (default %d, 127.0.0.1 only)\n"
           "  --no-mcp           GUI: do not start the MCP server\n"
           "  --ui-snapshot FILE GUI: render into a hidden window, save it as PNG and exit\n"
           "                     (--ui-open MACHINE: in the configurator, --ui-rack SLOT: select a slot)\n"
           "  --mcp              headless: run in real time and serve MCP until Ctrl+C\n"
           "  --verbose          debug log on stderr\n"
           "  --version, --help\n\n"
           "headless actions (executed in order):\n"
           "  --run-ms N         run N ms of emulated time\n"
           "  --keyboard NAME    plug another keyboard (EKL-1, \"Consul 262.3\", ANK-1...)\n"
           "  --type TEXT        type on the keyboard (escapes \\r \\e \\t \\xNN)\n"
           "  --wait-text TEXT   run until TEXT is on screen (see --timeout-ms)\n"
           "  --timeout-ms N     timeout for --wait-text (emulated ms, default 5000)\n"
           "  --print-screen     print the screen text\n"
           "  --serial-send TEXT send to the first serial channel\n"
           "  --print-serial     print data received on the first serial channel\n"
           "  --break ADDR       set an execution breakpoint (hex)\n"
           "  --print-regs       print CPU registers\n"
           "  --screenshot FILE  save the screen as PNG (2x)\n",
           sapiemu_version(), DEFAULT_MACHINE, MCP_DEFAULT_PORT);
}

static bool action_has_arg(const std::string& a)
{
    return a == "--run-ms" || a == "--type" || a == "--keyboard" || a == "--wait-text" || a == "--timeout-ms" ||
           a == "--serial-send" || a == "--break" || a == "--screenshot";
}

static int action_arg_count(const std::string& a)
{
    if (a == "--disasm") return 2;
    return action_has_arg(a) ? 1 : 0;
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

static std::atomic<bool> g_quit(false);

static void on_signal(int)
{
    g_quit = true;
}

// Headless MCP server: real-time emulation without a window.
static int serve_mcp(const std::string& machine_file, int port)
{
    EmuHost host;
    std::string err;
    if (!host.load(machine_file, err)) {
        LOGE("%s", err.c_str());
        return 1;
    }
    McpServer mcp(host);
    if (!mcp.start(port, err)) {
        LOGE("%s", err.c_str());
        return 1;
    }
    printf("MCP server: %s  (Ctrl+C to quit)\n", mcp.url().c_str());
    fflush(stdout);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    host.run_loop(g_quit);
    host.shutdown();
    mcp.stop();
    return 0;
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
    int mcp_port = MCP_DEFAULT_PORT;
    bool mcp_enabled = true;
    bool mcp_serve = false;
    UiSnapshot snap;

#if defined(SAPIEMU_GUI) && defined(_WIN32)
    for (int i = 1; i < argc; i++)
        if (argv[i] == "--headless" || argv[i] == "--help" || argv[i] == "--version" || argv[i] == "--mcp") {
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
        else if (a == "--mcp-port" && i + 1 < argc) mcp_port = atoi(argv[++i].c_str());
        else if (a == "--no-mcp") mcp_enabled = false;
        else if (a == "--ui-snapshot" && i + 1 < argc) { snap.path = argv[++i]; mcp_enabled = false; }
        else if (a == "--ui-rack" && i + 1 < argc) { snap.rack = true; snap.rack_slot = atoi(argv[++i].c_str()); }
        else if (a == "--ui-open" && i + 1 < argc) snap.open = argv[++i];
        else if (a == "--mcp") { mcp_serve = true; headless = true; }
        else {
            actions.push_back(a);
            for (int k = action_arg_count(a); k > 0 && i + 1 < argc; k--) actions.push_back(argv[++i]);
        }
    }

    data_root_init();
    LOGD("data root: %s", data_root().c_str());

    if (mcp_serve) return serve_mcp(machine_file, mcp_port);

    if (headless) {
        std::unique_ptr<Machine> m(new Machine());   // ~200 KB of debug tables, keep off the stack
        std::string err;
        if (!m->load_file(machine_file, err)) {
            LOGE("%s", err.c_str());
            return 1;
        }
        int rc = headless_run(*m, actions);
        fflush(stdout);
        return rc;
    }

#ifdef SAPIEMU_GUI
    if (!actions.empty()) {
        fprintf(stderr, "unknown option '%s' (see --help)\n", actions[0].c_str());
        return 1;
    }
    return app_run(machine_file, mcp_enabled ? mcp_port : 0, snap.path.empty() ? nullptr : &snap);
#else
    return 0;
#endif
}
