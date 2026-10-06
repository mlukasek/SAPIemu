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
    std::string state;       // a saved state loaded first (pictures for the documentation)
    std::vector<std::string> set;   // "SLOT:KEY=VALUE": a change in the configurator, not applied
                                    // (VALUE as JSON, else a string)
    int w = 0, h = 0;        // window size in points (0 = the default)
};

// Runs the SDL3/ImGui frontend; returns the process exit code. An empty
// machine_file starts the machine used last (settings), else DEFAULT_MACHINE_FILE.
// settings_file: settings to use instead of sapiemu-settings.json next to the exe.
int app_run(const std::string& machine_file, int mcp_port, const UiSnapshot* snapshot = nullptr,
            const std::string& settings_file = std::string());   // mcp_port 0 = no MCP server
