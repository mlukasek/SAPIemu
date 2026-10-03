#pragma once
// EmuHost owns the running machine and its run state (paused, turbo). It is
// driven by one "emulation thread" (the GUI main loop or the headless loop);
// other threads (MCP server) post commands with call(), which are executed on
// the emulation thread in poll().
#include <stdint.h>
#include <atomic>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include "json.hpp"
#include "core/machine.h"

class EmuHost {
public:
    EmuHost();
    ~EmuHost();

    // Load a machine description; the old machine is kept on error.
    bool load(const std::string& file, std::string& err);
    // Build a machine from a description (configurator); `file` is where it was
    // loaded from / saved to (empty for an unsaved one).
    bool load_config(const nlohmann::json& cfg, const std::string& file, std::string& err);
    Machine* machine(void) { return m.get(); }
    const std::string& machine_file(void) const { return file; }
    uint64_t machine_generation(void) const { return generation; }   // changes on every load

    // Advance emulation by `elapsed_ns` of wall clock time (real time or turbo).
    void frame(uint64_t elapsed_ns);
    // Execute commands posted by other threads. Call from the emulation thread.
    void poll(void);
    // Post a command and wait for its result (from any thread except the emulation thread).
    nlohmann::json call(std::function<nlohmann::json(EmuHost&)> fn, int timeout_ms = 120000);

    bool paused = false;
    bool turbo = false;
    std::string status;         // last stop reason ("breakpoint F123h"...)
    double speed_percent = 0;

    // Headless real-time loop (used by sapiemu-cli --mcp) until `quit` is set.
    void run_loop(std::atomic<bool>& quit);
    // Fail pending and future call()s; call before stopping the MCP server.
    void shutdown(void);

private:
    struct Task {
        std::function<nlohmann::json(EmuHost&)> fn;
        std::promise<nlohmann::json> result;
    };
    void handle_stop(void);
    void update_speed(void);

    std::unique_ptr<Machine> m;
    std::string file;
    uint64_t generation = 0;
    std::mutex task_mtx;
    std::deque<std::shared_ptr<Task>> tasks;
    bool closing = false;   // guarded by task_mtx
    uint64_t speed_t0 = 0, speed_c0 = 0;
};

uint64_t host_time_ns(void);
