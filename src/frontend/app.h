#pragma once
#include <string>

// Self-check of the GUI layout: render into a hidden window, save the window
// content as PNG and exit (sapiemu --ui-snapshot FILE [--ui-rack SLOT] [--ui-open MACHINE]).
struct UiSnapshot {
    std::string path;
    bool rack = false;       // show the configurator
    int rack_slot = 0;       // slot selected in it
    std::string open;        // machine description opened in the configurator (not started)
};

// Runs the SDL3/ImGui frontend; returns the process exit code.
int app_run(const std::string& machine_file, int mcp_port, const UiSnapshot* snapshot = nullptr);   // mcp_port 0 = no MCP server
