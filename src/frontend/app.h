#pragma once
#include <string>
#include <vector>

// Machine started when neither --machine nor the settings name one.
#define DEFAULT_MACHINE_FILE "machines/mikromon.sapi"

// Self-check of the GUI layout: render into a hidden window, save the window
// content as PNG and exit (sapiemu --ui-snapshot FILE [--ui-rack SLOT] [--ui-open MACHINE]).
struct UiSnapshot {
    std::string path;
    bool rack = false;       // show the configurator
    int rack_slot = 0;       // slot selected in it
    std::string open;        // machine description opened in the configurator (not started)
    bool map = false;        // show the port and memory map tab
    bool media = false;      // show the Media window
    bool printer = false;    // show the Printer window
    bool diag = false;       // show the Diagnostic board window
    bool aps5 = false;       // show the APS-5 window
    bool plotter = false;    // show the Minigraf window
    bool ims2 = false;       // show the IMS-2 measuring bench window
    bool aps5_demo = false;  // and start its DEMO
    int drag_from = 0, drag_to = 0;   // drag the card in slot drag_from to drag_to in the configurator
    bool drag_drop = false;           // and drop it (else the picture shows it still dragged)
    bool terminal = false;   // show the Serial terminal window
    bool screen_menu = false;   // the right click menu of the console screen open
    bool shot = false;          // take a screenshot (Ctrl+F5) before the picture
    bool copy = false;          // copy the screenshot to the clipboard (View -> Copy screenshot)
    double video = 0;           // record a video of so many seconds (Ctrl+Alt+F5) before the picture
    std::string term_text;   // text received on the first channel ("\n" written as two characters = a new line)
    std::string term_sel;    // "L0,C0-L1,C1": a selection in it (line indexes, cell boundaries)
    std::string state;       // a saved state loaded first (pictures for the documentation)
    std::vector<std::string> set;   // "SLOT:KEY=VALUE": a change in the configurator, not applied
                                    // (VALUE as JSON, else a string)
    int w = 0, h = 0;        // window size in points (0 = the default)
    int zoom = 0;            // the zoom of the screen chosen from the menu (100, 200...; 0 = none)
    bool reset_layout = false;   // View -> Default window layout
};

// Runs the SDL3/ImGui frontend; returns the process exit code. An empty
// machine_file starts the machine used last (settings), else DEFAULT_MACHINE_FILE.
// settings_file: settings to use instead of sapiemu-settings.json next to the exe.
int app_run(const std::string& machine_file, int mcp_port, const UiSnapshot* snapshot = nullptr,
            const std::string& settings_file = std::string());   // mcp_port 0 = no MCP server
