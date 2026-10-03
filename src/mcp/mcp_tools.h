#pragma once
// MCP tool definitions and implementations. mcp_tool_call() must run on the
// emulation thread (McpServer wraps it in EmuHost::call()).
#include <string>
#include "json.hpp"

class EmuHost;

nlohmann::json mcp_tool_list(void);
// Returns an MCP CallToolResult: { "content": [...], "isError": bool }
nlohmann::json mcp_tool_call(EmuHost& host, const std::string& name, const nlohmann::json& args);
