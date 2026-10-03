#pragma once
// MCP (Model Context Protocol) server: JSON-RPC 2.0 over Streamable HTTP
// (POST /mcp, plain JSON responses, no SSE). Listens on 127.0.0.1 only.
// Tool calls are executed on the emulation thread through EmuHost::call().
#include <memory>
#include <string>
#include <thread>

class EmuHost;
namespace httplib { class Server; }

static const int MCP_DEFAULT_PORT = 8580;

// Request filter (exposed for tests): Host must be a loopback name, an Origin (if
// any, nullptr = header missing) exactly a loopback origin, Content-Type JSON.
bool mcp_request_allowed(const char* origin, const char* host, const char* content_type);

class McpServer {
public:
    explicit McpServer(EmuHost& host);
    ~McpServer();

    bool start(int port, std::string& err);
    void stop(void);
    bool running(void) const { return active; }
    int port(void) const { return listen_port; }
    std::string url(void) const;

    // Handle one JSON-RPC request body; returns the response body ("" for notifications).
    // Exposed for tests.
    std::string handle_body(const std::string& body, int& http_status);

private:
    EmuHost& host;
    std::unique_ptr<httplib::Server> svr;
    std::thread th;
    bool active = false;
    int listen_port = 0;
};
