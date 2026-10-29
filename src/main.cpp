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
#include "core/plotter_ctl.h"
#include "core/util.h"
#include "core/version.h"
#include "mcp/mcp_server.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <signal.h>
#include <thread>

#include "frontend/app.h"
#ifdef SAPIEMU_GUI
#include <SDL3/SDL_main.h>
#include "frontend/i18n.h"
#include "frontend/theme.h"
#endif
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

static const char* DEFAULT_MACHINE = DEFAULT_MACHINE_FILE;

static void print_help(void)
{
    printf("SAPIemu %s - Tesla SAPI-1 emulator\n\n"
           "usage: sapiemu [options] [--headless] [actions...]\n"
           "       sapiemu-cli [options] [actions...]\n\n"
           "options:\n"
           "  --machine FILE     machine description (default %s)\n"
           "  --data-root DIR    directory with roms/, media/, machines/\n"
           "  --mcp-port N       MCP server port (default %d, 127.0.0.1 only); GUI: start it whatever the settings say\n"
           "  --no-mcp           GUI: do not start the MCP server (else as View -> Settings says)\n"
           "  --settings FILE    GUI: settings file instead of sapiemu-settings.json next to the exe\n"
           "  --lang cs|en       GUI: language of the user interface for this run (not saved)\n"
           "  --ui-theme MODE    GUI: light or dark for this run (not saved; also win11-light, win11-dark);\n"
           "                     without it as the system says\n"
           "  --ui-layout MODE   GUI: floating or docked windows for this run (not saved; other than the saved\n"
           "                     layout: its default places, imgui.ini neither read nor written)\n"
           "  --ui-snapshot FILE GUI: render into a hidden window, save it as PNG and exit\n"
           "                     (--ui-open MACHINE: in the configurator, --ui-rack SLOT: select a slot,\n"
           "                     --ui-map: the port and memory map, --ui-media, --ui-printer, --ui-diag, --ui-aps5, --ui-plotter, --ui-ims2: those windows (--ui-aps5-demo: and its DEMO),\n"
           "                     --ui-browser: Choose a machine, --ui-guide: the guide of the machine,\n"
           "                     --ui-about, --ui-settings, --ui-shortcuts: About, Settings, Keyboard shortcuts,\n"
           "                     --ui-crop KEY: only the window with the id ###KEY (Media, Minigraf...),\n"
           "                     --ui-picture: with --ui-crop only the drawn hardware in it (the panel, the mechanism,\n"
           "                     the paper) without the controls around it (previews of the machines),\n"
           "                     --ui-terminal: the serial terminal, --ui-term-text TEXT: text in it (\\n = new line),\n"
           "                     --ui-term-sel L0,C0-L1,C1: a selection in it (line indexes, cell boundaries),\n"
           "                     --ui-drag FROM:TO[:drop]: drag a card in the configurator (and drop it),\n"
           "                     --ui-screen-menu: the right click menu of the screen, --ui-shot: a screenshot (Ctrl+F5),\n"
           "                     --ui-copy: the screenshot to the clipboard (View -> Copy screenshot),\n"
           "                     --ui-zoom PCT: the zoom from View -> Screen zoom (the display used last),\n"
           "                     --ui-reset-layout: View -> Default window layout,\n"
           "                     --ui-arrange: View -> Arrange open windows,\n"
           "                     --ui-window-item KEY: the item of the window ###KEY in View or Debug clicked (Media,\n"
           "                     Debugger, rack...; repeated: one after another), --ui-front KEY: View -> Windows, that\n"
           "                     window (with --ui-window-item in the order given),\n"
           "                     --ui-layout-switch: View -> Settings -> Windows, the other layout (its own places),\n"
           "                     --ui-fullscreen: Alt+Enter, the display used last alone in the window (the right click\n"
           "                     menu with --ui-screen-menu; given twice: on and off again),\n"
           "                     --ui-pause: pause the machine (F5) just before the picture, --ui-log: the Log window,\n"
           "                     --ui-reset: F12 just before the picture (APS-5: the question first),\n"
           "                     --ui-report: Help -> Report a bug, --ui-debugger: the Debugger window,\n"
           "                     --ui-menu NAME: the menu NAME open (File, Machine, Debug, View, Help; View/Windows:\n"
           "                     with its submenu Windows),\n"
           "                     --ui-key KEY: a key pressed before the picture (F9, Ctrl+Shift+C, Enter, Esc...;\n"
           "                     repeated: one after another), --ui-mouse X,Y: the mouse there (points),\n"
           "                     --ui-video SEC: a video of SEC seconds (Ctrl+Alt+F5), the picture when it is saved,\n"
           "                     --ui-state FILE: load a saved state first, --ui-size WxH: the window size,\n"
           "                     --ui-scale F: the display scale (1.25 = 125 %%) instead of the one of the system,\n"
           "                     --ui-set SLOT:KEY=VALUE or SLOT:remove: a change in the configurator, not applied,\n"
           "                     --ui-exit: File -> Exit before the picture (the question about unsaved changes;\n"
           "                     without changes the program ends with no picture))\n"
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
           "  --joystick SPEC    joystick held: [PORT:]down,up,right,left,fire1,fire2 (\"-\" = released)\n"
           "  --break ADDR       set an execution breakpoint (hex)\n"
           "  --print-regs       print CPU registers\n"
           "  --power-cycle      switch off and on (Machine -> Switch off and on): the memory is lost, the media and\n"
           "                     battery memories are saved and read again\n"
           "  --load-hex FILE    load an Intel HEX file into memory\n"
           "  --load-bin FILE ADDR  load a binary file into memory at ADDR (hex)\n"
           "  --save-state FILE  save the state of the machine\n"
           "  --load-state FILE  load a state (builds the machine of the state)\n"
           "  --display CARD     --screenshot takes this display output (e.g. DGD-1)\n"
           "  --screenshot FILE  save the screen as PNG (2x)\n"
           "  --audio-wav FILE   record the sound card from here on, saved as WAV at the end\n"
           "  --tape FILE        cassette into the DSM-1 recorder and PLAY: WAV, MP3, or a program\n"
           "                     (.hex, .bin with --tape-addr) made into a tape\n"
           "  --tape-export FILE the tape in the recorder into a WAV file\n"
           "  --make-tape IN OUT a program (.hex, .bin) into a tape WAV (to play into a real SAPI)\n"
           "  --tape-format F    for --tape / --make-tape: mikos (default; .K_L, .J) or mikrobasic (LOAD)\n"
           "  --tape-name NAME   name on the tape (default from the file name)\n"
           "  --tape-addr ADDR   load address of a .bin (hex)\n"
           "  --tape-entry ADDR  start address (hex, default the lowest address)\n"
           "  --plotter ACTION [ARG]  a button of the plotter: pen (XY 4130 PEN), new-sheet, and of the\n"
           "                     plotter of AR 1990 ready, test, stop, whole-test (TEST held through a reset),\n"
           "                     demo (media/ar1990/ukazka.txt), send TEXT (only \\xNN = a byte, as the line of\n"
           "                     the window; CR appended), send-file FILE (relative to the data root)\n"
           "  --print-plotter    print the plotter: lamps, bytes waiting and taken, pen, strokes, extent, steps\n"
           "  --plotter-png FILE save the drawing (with 5 mm around) as PNG, 8 px/mm\n"
           "  --plotter-svg FILE save the drawing (with 5 mm around) as SVG\n",
           sapiemu_version(), DEFAULT_MACHINE, MCP_DEFAULT_PORT);
}

static bool action_has_arg(const std::string& a)
{
    return a == "--run-ms" || a == "--type" || a == "--keyboard" || a == "--wait-text" || a == "--timeout-ms" ||
           a == "--serial-send" || a == "--joystick" || a == "--break" || a == "--screenshot" || a == "--display" ||
           a == "--load-hex" || a == "--audio-wav" || a == "--save-state" || a == "--load-state" || a == "--tape" || a == "--tape-export" ||
           a == "--tape-format" || a == "--tape-name" || a == "--tape-addr" || a == "--tape-entry" ||
           a == "--plotter-png" || a == "--plotter-svg";
}

// `next` = the argument after the action (--plotter send TEXT: the action decides)
static int action_arg_count(const std::string& a, const std::string& next)
{
    if (a == "--disasm" || a == "--load-bin" || a == "--make-tape") return 2;
    if (a == "--plotter") return plotter_action_has_arg(next) ? 2 : 1;
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
    if (host.quit_requested)   // MCP quit: let its answer reach the client first
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    host.shutdown();
    mcp.stop();
    return 0;
}

int main(int argc_ansi, char** argv_ansi)
{
    std::vector<std::string> argv = utf8_args(argc_ansi, argv_ansi);
    int argc = (int)argv.size();
    std::string machine_file;   // empty: the GUI starts the last used machine
#ifdef SAPIEMU_GUI
    bool headless = false;
#else
    bool headless = true;
#endif
    std::vector<std::string> actions;
    int mcp_port = MCP_DEFAULT_PORT;
    bool mcp_enabled = true;
    bool mcp_explicit = false;   // --mcp-port: on whatever the settings say
    bool mcp_serve = false;
    UiSnapshot snap;
    std::string settings_file;

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
        else if (a == "--mcp-port" && i + 1 < argc) { mcp_port = atoi(argv[++i].c_str()); mcp_explicit = true; }
        else if (a == "--no-mcp") mcp_enabled = false;
        else if (a == "--ui-snapshot" && i + 1 < argc) { snap.path = argv[++i]; mcp_enabled = false; }
        else if (a == "--ui-rack" && i + 1 < argc) { snap.rack = true; snap.rack_slot = atoi(argv[++i].c_str()); }
        else if (a == "--ui-open" && i + 1 < argc) snap.open = argv[++i];
        else if (a == "--ui-map") { snap.map = true; snap.rack = true; }
        else if (a == "--ui-media") snap.media = true;
        else if (a == "--ui-zoom" && i + 1 < argc) snap.zoom = atoi(argv[++i].c_str());
        else if (a == "--ui-reset-layout") snap.reset_layout = true;
        else if (a == "--ui-arrange") snap.arrange = true;
        else if (a == "--ui-window-item" && i + 1 < argc) snap.window_clicks.push_back(argv[++i]);
        else if (a == "--ui-front" && i + 1 < argc) snap.window_clicks.push_back("front:" + argv[++i]);
        else if (a == "--ui-layout-switch") snap.layout_switch = true;
        else if (a == "--ui-fullscreen") snap.fullscreen++;
        else if (a == "--ui-pause") snap.pause = true;
        else if (a == "--ui-log") snap.log = true;
        else if (a == "--ui-reset") snap.reset = true;
        else if (a == "--ui-set" && i + 1 < argc) snap.set.push_back(argv[++i]);
        else if (a == "--ui-printer") snap.printer = true;
        else if (a == "--ui-diag") snap.diag = true;
        else if (a == "--ui-aps5") snap.aps5 = true;
        else if (a == "--ui-plotter") snap.plotter = true;
        else if (a == "--ui-browser") snap.browser = true;
        else if (a == "--ui-guide") snap.guide = true;
        else if (a == "--ui-about") snap.about = true;
        else if (a == "--ui-settings") snap.settings = true;
        else if (a == "--ui-shortcuts") snap.shortcuts = true;
        else if (a == "--ui-report") snap.report = true;
        else if (a == "--ui-debugger") snap.debugger = true;
        else if (a == "--ui-exit") snap.exit = true;
        else if (a == "--ui-menu" && i + 1 < argc) snap.menu = argv[++i];
        else if (a == "--ui-key" && i + 1 < argc) snap.keys.push_back(argv[++i]);
        else if (a == "--ui-mouse" && i + 1 < argc) sscanf(argv[++i].c_str(), "%f,%f", &snap.mouse_x, &snap.mouse_y);
        else if (a == "--ui-crop" && i + 1 < argc) snap.crop = argv[++i];
        else if (a == "--ui-picture") snap.picture = true;
        else if (a == "--ui-ims2") snap.ims2 = true;
        else if (a == "--ui-aps5-demo") snap.aps5_demo = true;
        else if (a == "--ui-drag" && i + 1 < argc) {
            char drop[8] = "";
            if (sscanf(argv[++i].c_str(), "%d:%d:%7s", &snap.drag_from, &snap.drag_to, drop) < 2) {
                fprintf(stderr, "--ui-drag FROM:TO[:drop]\n");
                return 2;
            }
            snap.drag_drop = strcmp(drop, "drop") == 0;
            snap.rack = true;
        }
        else if (a == "--ui-terminal") snap.terminal = true;
        else if (a == "--ui-screen-menu") snap.screen_menu = true;
        else if (a == "--ui-shot") snap.shot = true;
        else if (a == "--ui-copy") snap.copy = true;
        else if (a == "--ui-video" && i + 1 < argc) snap.video = atof(argv[++i].c_str());
        else if (a == "--ui-term-text" && i + 1 < argc) { snap.terminal = true; snap.term_text = argv[++i]; }
        else if (a == "--ui-term-sel" && i + 1 < argc) { snap.terminal = true; snap.term_sel = argv[++i]; }
        else if (a == "--ui-state" && i + 1 < argc) snap.state = argv[++i];
        else if (a == "--ui-size" && i + 1 < argc) sscanf(argv[++i].c_str(), "%dx%d", &snap.w, &snap.h);
        else if (a == "--ui-scale" && i + 1 < argc) snap.scale = (float)atof(argv[++i].c_str());
        else if (a == "--ui-theme" && i + 1 < argc) {
            std::string look = argv[++i];
#ifdef SAPIEMU_GUI
            ThemeVariant v;
            ThemeMode md;
            if (!theme_parse(look.c_str(), &v, &md)) {
                fprintf(stderr, "--ui-theme: light, dark or system (win11-light, win11-dark)\n");
                return 2;
            }
            theme_set_override(v, md);
#endif
        }
        else if (a == "--ui-layout" && i + 1 < argc) {
            std::string layout = argv[++i];
            if (layout != "floating" && layout != "docked") {
                fprintf(stderr, "--ui-layout: floating or docked\n");
                return 2;
            }
#ifdef SAPIEMU_GUI
            app_layout_override(layout == "docked");
#endif
        }
        else if (a == "--settings" && i + 1 < argc) settings_file = argv[++i];
        else if (a == "--lang" && i + 1 < argc) {
            std::string code = argv[++i];
#ifdef SAPIEMU_GUI
            UiLanguage lang;
            if (!i18n_parse(code.c_str(), &lang)) {
                fprintf(stderr, "--lang: cs or en\n");
                return 2;
            }
            i18n_set_override(lang);
#endif
        }
        else if (a == "--mcp") { mcp_serve = true; headless = true; }
        else {
            actions.push_back(a);
            for (int k = action_arg_count(a, i + 1 < argc ? argv[i + 1] : std::string()); k > 0 && i + 1 < argc; k--)
                actions.push_back(argv[++i]);
        }
    }

    data_root_init();
    LOGD("data root: %s", data_root().c_str());

    if (machine_file.empty() && (mcp_serve || headless)) machine_file = DEFAULT_MACHINE;
    if (mcp_serve) return serve_mcp(machine_file, mcp_port);

    if (headless) {
        std::unique_ptr<Machine> m(new Machine());   // ~200 KB of debug tables, keep off the stack
        std::string err;
        if (!m->load_file(machine_file, err)) {
            LOGE("%s", err.c_str());
            return 1;
        }
        int rc = headless_run(*m, actions);
        m->power_down();   // APS-5: the power fail warning before the batteries take over
        fflush(stdout);
        return rc;
    }

#ifdef SAPIEMU_GUI
    if (!actions.empty()) {
        fprintf(stderr, "unknown option '%s' (see --help)\n", actions[0].c_str());
        return 1;
    }
    // GUI: the MCP server as the settings say (View -> Settings), unless --mcp-port or --no-mcp
    return app_run(machine_file, !mcp_enabled ? 0 : mcp_explicit ? mcp_port : -1, snap.path.empty() ? nullptr : &snap,
                   settings_file);
#else
    (void)mcp_enabled;   // GUI options are parsed (and ignored) by sapiemu-cli too
    (void)mcp_explicit;
    return 0;
#endif
}
