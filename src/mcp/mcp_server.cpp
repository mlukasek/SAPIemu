#include "mcp/mcp_server.h"
#include "mcp/mcp_tools.h"
#include "core/host.h"
#include "core/log.h"
#include "core/version.h"

#include "httplib.h"
#include "json.hpp"

using nlohmann::json;

static const char* const k_versions[] = { "2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05" };

static const char* const k_instructions =
    "SAPIemu emulates the Czechoslovak 8-bit computer Tesla SAPI-1 (Z80 CPU card JPR-1ZI, ARB-1 bus, "
    "AND-1Z 64x20 text display, DMR-1 serial+RAM, EKL-1 keyboard). Use machine_info first. "
    "Addresses and ports in tool arguments are hex strings (e.g. \"F000\"); JSON numbers are decimal. "
    "Typical workflow: load_hex / write_memory, set_registers pc or type commands into the monitor "
    "(type_text), run_until / run_for, then screen_text, screenshot, read_memory, get_registers. "
    "Unless paused, the machine also runs in real time between tool calls (and stops by itself on "
    "breakpoints); call pause first for deterministic debugging. "
    "The MikroMon monitor prints 'MikroMon 2kB V1.3' at boot; its command 'M addr' dumps memory "
    "and waits for SPACE/CR/ESC after a page.";

McpServer::McpServer(EmuHost& h) : host(h) {}

McpServer::~McpServer()
{
    stop();
}

std::string McpServer::url(void) const
{
    return "http://127.0.0.1:" + std::to_string(listen_port) + "/mcp";
}

static json rpc_error(const json& id, int code, const std::string& msg)
{
    return json{ { "jsonrpc", "2.0" }, { "id", id }, { "error", { { "code", code }, { "message", msg } } } };
}

static json rpc_result(const json& id, const json& result)
{
    return json{ { "jsonrpc", "2.0" }, { "id", id }, { "result", result } };
}

std::string McpServer::handle_body(const std::string& body, int& http_status)
{
    json req = json::parse(body, nullptr, false);
    if (req.is_discarded() || !req.is_object()) {
        http_status = 400;
        return rpc_error(nullptr, -32700, "parse error").dump();
    }
    std::string method = req.value("method", std::string());
    bool is_request = req.contains("id");
    json id = is_request ? req["id"] : json(nullptr);

    if (!is_request) {
        // notifications (notifications/initialized, cancelled...) and responses: accepted, no body
        http_status = 202;
        return std::string();
    }
    http_status = 200;
    const json params = req.contains("params") ? req["params"] : json::object();

    if (method == "initialize") {
        std::string wanted = params.value("protocolVersion", std::string());
        std::string version = k_versions[1];
        for (const char* v : k_versions) if (wanted == v) version = v;
        json result = {
            { "protocolVersion", version },
            { "capabilities", { { "tools", { { "listChanged", false } } } } },
            { "serverInfo", { { "name", "sapiemu" }, { "title", "SAPIemu - Tesla SAPI-1 emulator" },
                              { "version", sapiemu_version() } } },
            { "instructions", k_instructions },
        };
        return rpc_result(id, result).dump();
    }
    if (method == "ping") return rpc_result(id, json::object()).dump();
    if (method == "tools/list") return rpc_result(id, json{ { "tools", mcp_tool_list() } }).dump();
    if (method == "tools/call") {
        std::string name = params.value("name", std::string());
        json args = params.contains("arguments") ? params["arguments"] : json::object();
        LOGD("MCP tool %s %s", name.c_str(), args.dump().c_str());
        json r = host.call([name, args](EmuHost& h) { return mcp_tool_call(h, name, args); });
        if (r.contains("error") && !r.contains("content"))
            r = json{ { "content", json::array({ json{ { "type", "text" }, { "text", r["error"] } } }) }, { "isError", true } };
        return rpc_result(id, r).dump();
    }
    return rpc_error(id, -32601, "method not found: " + method).dump();
}

// Reject browser requests from other origins (DNS rebinding protection).
static bool origin_allowed(const httplib::Request& req)
{
    if (!req.has_header("Origin")) return true;
    std::string o = req.get_header_value("Origin");
    return o.rfind("http://127.0.0.1", 0) == 0 || o.rfind("http://localhost", 0) == 0 ||
           o.rfind("https://127.0.0.1", 0) == 0 || o.rfind("https://localhost", 0) == 0;
}

bool McpServer::start(int port, std::string& err)
{
    stop();
    svr.reset(new httplib::Server());
    svr->Post("/mcp", [this](const httplib::Request& req, httplib::Response& res) {
        if (!origin_allowed(req)) { res.status = 403; return; }
        int status = 200;
        std::string body = handle_body(req.body, status);
        res.status = status;
        if (!body.empty()) res.set_content(body, "application/json");
    });
    svr->Get("/mcp", [](const httplib::Request&, httplib::Response& res) {
        res.status = 405;   // no server-initiated SSE stream
    });
    svr->Delete("/mcp", [](const httplib::Request&, httplib::Response& res) {
        res.status = 405;
    });
    if (!svr->bind_to_port("127.0.0.1", port)) {
        err = "cannot listen on 127.0.0.1:" + std::to_string(port) + " (port in use?)";
        svr.reset();
        return false;
    }
    listen_port = port;
    active = true;
    th = std::thread([this]() { svr->listen_after_bind(); });
    LOGI("MCP server listening on %s", url().c_str());
    return true;
}

void McpServer::stop(void)
{
    if (svr) svr->stop();
    if (th.joinable()) th.join();
    svr.reset();
    active = false;
}
