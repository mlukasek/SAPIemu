#pragma once
#include <string>

// Runs the SDL3/ImGui frontend; returns the process exit code.
int app_run(const std::string& machine_file, int mcp_port);   // mcp_port 0 = no MCP server
