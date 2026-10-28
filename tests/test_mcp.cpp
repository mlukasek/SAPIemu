// MCP JSON-RPC handling and tools, without network: requests go through
// McpServer::handle_body while a second thread plays the emulation loop.
#include <atomic>
#include <thread>
#include "core/host.h"
#include "core/util.h"
#include "mcp/mcp_server.h"
#include "test_util.h"

using nlohmann::json;

static json rpc(McpServer& s, const json& req, int expect_status = 200)
{
    int status = 0;
    std::string body = s.handle_body(req.dump(), status);
    CHECK_EQ(status, expect_status);
    return body.empty() ? json() : json::parse(body);
}

static json tool(McpServer& s, const char* name, const json& args)
{
    json r = rpc(s, { { "jsonrpc", "2.0" }, { "id", 7 }, { "method", "tools/call" },
                      { "params", { { "name", name }, { "arguments", args } } } });
    return r["result"];
}

static std::string text_of(const json& result)
{
    return result["content"][0].value("text", std::string());
}

static void test_request_filter(void)
{
    const char* json = "application/json";
    // Claude Code / curl: loopback Host, no Origin, JSON
    CHECK(mcp_request_allowed(nullptr, "127.0.0.1:8580", json));
    CHECK(mcp_request_allowed(nullptr, "localhost:8580", "application/json; charset=utf-8"));
    CHECK(mcp_request_allowed(nullptr, "[::1]:8580", json));
    CHECK(mcp_request_allowed("http://localhost:3000", "127.0.0.1:8580", json));   // local web tool
    // DNS rebinding: foreign Host name
    CHECK(!mcp_request_allowed(nullptr, "localhost.attacker.tld:8580", json));
    CHECK(!mcp_request_allowed(nullptr, "evil.example:8580", json));
    // CSRF from other sites, also with look-alike names
    CHECK(!mcp_request_allowed("http://localhost.attacker.tld:8580", "127.0.0.1:8580", json));
    CHECK(!mcp_request_allowed("http://127.0.0.1.attacker.tld", "127.0.0.1:8580", json));
    CHECK(!mcp_request_allowed("https://evil.example", "127.0.0.1:8580", json));
    CHECK(!mcp_request_allowed("null", "127.0.0.1:8580", json));
    // "simple" cross-site requests without JSON content type
    CHECK(!mcp_request_allowed(nullptr, "127.0.0.1:8580", "text/plain"));
    CHECK(!mcp_request_allowed(nullptr, "127.0.0.1:8580", nullptr));
}

int main(void)
{
    test_init();
    test_request_filter();
    EmuHost host;
    std::string err;
    CHECK(host.load("machines/mikromon.sapi", err));
    host.paused = true;   // deterministic: only tool calls run the machine

    std::atomic<bool> quit(false);
    std::thread emu([&]() {
        while (!quit) {
            host.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    McpServer s(host);
    json init = rpc(s, { { "jsonrpc", "2.0" }, { "id", 1 }, { "method", "initialize" },
                         { "params", { { "protocolVersion", "2025-06-18" }, { "capabilities", json::object() },
                                       { "clientInfo", { { "name", "test" }, { "version", "1" } } } } } });
    CHECK(init["result"]["protocolVersion"] == "2025-06-18");
    CHECK(init["result"]["serverInfo"]["name"] == "sapiemu");

    rpc(s, { { "jsonrpc", "2.0" }, { "method", "notifications/initialized" } }, 202);

    json list = rpc(s, { { "jsonrpc", "2.0" }, { "id", 2 }, { "method", "tools/list" } });
    CHECK(list["result"]["tools"].size() >= 20);

    json unknown = rpc(s, { { "jsonrpc", "2.0" }, { "id", 3 }, { "method", "foo/bar" } });
    CHECK_EQ(unknown["error"]["code"].get<int>(), -32601);

    int status = 0;
    s.handle_body("{not json", status);
    CHECK_EQ(status, 400);

    // boot MikroMon and talk to it
    json r = tool(s, "run_until", { { "screen_text", "MikroMon 2kB V1.3" }, { "timeout_ms", 2000 } });
    CHECK(r["isError"] == false);
    CHECK(text_of(r).find("\"success\": true") != std::string::npos);

    r = tool(s, "type_text", { { "text", "M F000\r" }, { "run_ms", 500 } });
    r = tool(s, "screen_text", json::object());
    CHECK(text_of(r).find(">F000: F301 0008") != std::string::npos);

    // another keyboard, keys without text
    r = tool(s, "set_keyboard", { { "name", "Consul 262.3" } });
    CHECK(r["isError"] == false);
    r = tool(s, "machine_info", json::object());
    CHECK(text_of(r).find("\"keyboard\": \"Consul 262.3\"") != std::string::npos);
    r = tool(s, "press_key", { { "key", "esc" }, { "run_ms", 300 } });
    CHECK(r["isError"] == false);
    r = tool(s, "press_key", { { "key", "pause" } });
    CHECK(r["isError"] == true);   // only Consul 259.11 has CLEAR
    r = tool(s, "set_keyboard", { { "name", "Consul 999" } });
    CHECK(r["isError"] == true);
    r = tool(s, "set_keyboard", { { "name", "EKL-1" } });

    r = tool(s, "disassemble", { { "address", "F000" }, { "count", 3 } });
    CHECK(text_of(r).find("LD BC,0800h") != std::string::npos);

    r = tool(s, "write_memory", { { "address", "8000" }, { "data", "3E 41 76" } });
    r = tool(s, "read_memory", { { "address", "8000" }, { "length", 3 }, { "format", "hex" } });
    CHECK(text_of(r) == "3E4176");

    // save state: a checkpoint in memory, changed memory, back to the checkpoint; then a file
    r = tool(s, "save_state", { { "name", "x" } });
    CHECK(r["isError"] == false);
    r = tool(s, "write_memory", { { "address", "8000" }, { "data", "00 00 00" } });
    r = tool(s, "load_state", { { "name", "x" } });
    CHECK(r["isError"] == false);
    CHECK(text_of(r).find("\"memory_states\"") != std::string::npos);
    r = tool(s, "read_memory", { { "address", "8000" }, { "length", 3 }, { "format", "hex" } });
    CHECK(text_of(r) == "3E4176");
    r = tool(s, "load_state", { { "name", "none" } });
    CHECK(r["isError"] == true);
    r = tool(s, "save_state", { { "file", "work/test-state/mcp.sapistate" } });
    CHECK(r["isError"] == false);
    r = tool(s, "write_memory", { { "address", "8000" }, { "data", "11" } });
    r = tool(s, "load_state", { { "file", "work/test-state/mcp.sapistate" } });
    CHECK(r["isError"] == false);
    r = tool(s, "read_memory", { { "address", "8000" }, { "length", 1 }, { "format", "hex" } });
    CHECK(text_of(r) == "3E");
    file_remove(data_path("work/test-state/mcp.sapistate"));

    // breakpoint + run_until address
    r = tool(s, "set_registers", { { "pc", "8000" } });
    r = tool(s, "step", { { "count", 1 } });
    r = tool(s, "get_registers", json::object());
    CHECK(text_of(r).find("\"af\": \"41") != std::string::npos);

    r = tool(s, "screenshot", { { "scale", 1 } });
    CHECK(r["content"][0]["type"] == "image");
    CHECK(r["content"][0]["data"].get<std::string>().rfind("iVBOR", 0) == 0);

    r = tool(s, "read_memory", { { "address", "zz" } });
    CHECK(r["isError"] == true);

    // speed of the emulation: percent of real time, or maximum
    r = tool(s, "resume", { { "speed_percent", 200 } });
    CHECK(json::parse(text_of(r))["speed_hz"] == 4000000);
    r = tool(s, "resume", { { "max_speed", true } });
    CHECK(json::parse(text_of(r))["speed_hz"] == 0);
    r = tool(s, "resume", { { "speed_percent", 100 } });
    CHECK(json::parse(text_of(r))["speed_hz"] == 2000000);
    r = tool(s, "resume", { { "speed_percent", 5 } });
    CHECK(r["isError"] == true);
    r = tool(s, "pause", json::object());

    // power switch: off = no machine, on = the same description again, RAM cleared
    r = tool(s, "write_memory", { { "address", "8000" }, { "data", "AA" } });
    r = tool(s, "power", { { "state", "off" } });
    CHECK(r["isError"] == false);
    r = tool(s, "get_registers", json::object());
    CHECK(r["isError"] == true);
    r = tool(s, "power", { { "state", "on" } });
    CHECK(r["isError"] == false);
    r = tool(s, "read_memory", { { "address", "8000" }, { "length", 1 }, { "format", "hex" } });
    CHECK(text_of(r) == "00");   // tests run without the battery file
    r = tool(s, "run_until", { { "screen_text", "MikroMon 2kB V1.3" }, { "timeout_ms", 2000 } });
    CHECK(text_of(r).find("\"success\": true") != std::string::npos);
    r = tool(s, "pause", json::object());

    // machine configuration
    r = tool(s, "card_catalog", { { "type", "DMR-1" } });
    CHECK(text_of(r).find("ram_config") != std::string::npos);
    r = tool(s, "get_machine_config", json::object());
    json mc = json::parse(text_of(r));
    CHECK(mc["runnable"] == true);
    json cfg = mc["config"];
    cfg["cards"].push_back({ { "slot", 4 }, { "type", "DSM-1" }, { "base", "14h" } });
    r = tool(s, "set_machine_config", { { "config", cfg }, { "save_as", "work/test/test_mcp_machine.sapi" } });
    json sr = json::parse(text_of(r));
    CHECK(sr["started"] == true);
    CHECK(sr["problems"].empty());   // DSM-1 on 14h, emulated
    CHECK(file_exists(data_path("work/test/test_mcp_machine.sapi")));
    r = tool(s, "get_machine_config", { { "path", "machines/zps/ZPS-4.sapi" } });
    json zps = json::parse(text_of(r));
    CHECK(zps["runnable"] == true);
    zps["config"]["cards"].erase(0);   // without a CPU card the machine cannot run
    r = tool(s, "set_machine_config", { { "config", zps["config"] } });
    CHECK(json::parse(text_of(r))["started"] == false);
    r = tool(s, "set_machine_config", { { "config", cfg }, { "save_as", "work/test/x.txt" }, { "start", false } });
    CHECK(r["isError"] == true);

    // cassette recorder of the DSM-1 (ZPS-1)
    r = tool(s, "load_machine", { { "path", "machines/zps/ZPS-1.sapi" } });
    CHECK(r["isError"] == false);
    r = tool(s, "tape_insert", { { "path", "work/test/mcp_tape.wav" }, { "new", true } });
    CHECK(r["isError"] == false);
    r = tool(s, "tape_control", { { "action", "record" } });
    CHECK(json::parse(text_of(r))["tape"]["mode"] == "record");
    r = tool(s, "tape_status", { { "card", "10h" } });
    CHECK(json::parse(text_of(r))["tape"]["write_protect"] == false);
    r = tool(s, "tape_control", { { "action", "protect" } });
    r = tool(s, "tape_control", { { "action", "record" } });
    CHECK(r["isError"] == true);   // write protected
    r = tool(s, "tape_control", { { "action", "eject" } });
    CHECK(json::parse(text_of(r))["tape"]["loaded"] == false);
    r = tool(s, "plotter", json::object());
    CHECK(r["isError"] == true);   // no plotter in ZPS-1

    // the plotter of AR 1990: READY (the origin), TEST (the test of the pens), a line of commands
    r = tool(s, "load_machine", { { "path", "machines/plotter_ar1990.sapi" } });
    CHECK(r["isError"] == false);
    r = tool(s, "run_for", { { "ms", 1000 } });   // it beeps and waits for READY
    r = tool(s, "plotter", json::object());
    CHECK(json::parse(text_of(r))["lamp_rdy"] == true);
    r = tool(s, "plotter", { { "ar_button", "ready" } });
    r = tool(s, "run_for", { { "ms", 6000 } });
    r = tool(s, "plotter", { { "ar_button", "test" } });
    CHECK(r["isError"] == false);
    r = tool(s, "run_for", { { "ms", 20000 } });
    r = tool(s, "plotter", json::object());
    json pl = json::parse(text_of(r));
    CHECK(pl["device"] == "AR 1990");
    CHECK(pl["strokes"] == 28);
    CHECK(pl["lamp_rdy"] == false && pl["lamp_buff"] == true);
    CHECK(pl["bytes_waiting"] == 0);
    r = tool(s, "plotter", { { "send", "\\x02M0,0" } });
    CHECK(json::parse(text_of(r))["bytes_waiting"] == 6);   // 02h, M0,0 and CR
    r = tool(s, "plotter", { { "ar_button", "jump" } });
    CHECK(r["isError"] == true);
    r = tool(s, "plotter", { { "pen_button", true } });
    CHECK(r["isError"] == true);   // the XY 4130 only

    // quit: the machine is switched off, the loop of the program is asked to end
    CHECK(!host.quit_requested);
    r = tool(s, "quit", json::object());
    CHECK(r["isError"] == false);
    CHECK(host.quit_requested && !host.powered());

    // a port another server holds is refused, not shared (Windows lets SO_REUSEADDR bind it twice: another
    // session's emulator got the requests of a second cli on the same port, 2026-10-16)
    {
        McpServer a(host), b(host);
        std::string err;
        int port = 0;
        for (int p = 8650; p < 8700 && !port; p++)
            if (a.start(p, err)) port = p;
        CHECK(port != 0);
        if (port) {
            CHECK(!b.start(port, err));
            CHECK(err.find("port in use") != std::string::npos);
        }
        a.stop();
        if (port) CHECK(b.start(port, err));   // free again
        b.stop();
    }

    quit = true;
    emu.join();
    return TEST_RESULT();
}
