#pragma once
// EmuHost owns the running machine and its run state (paused, turbo). It is
// driven by one "emulation thread" (the GUI main loop or the headless loop);
// other threads (MCP server) post commands with call(), which are executed on
// the emulation thread in poll().
#include <stdint.h>
#include <atomic>
#include <deque>
#include <map>
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
    // Power switch: off = the machine is gone (battery backed memories and RAM
    // disks saved, host connections closed); on = built again from the last
    // description, like switching the real computer off and on.
    // The description is the machine as it ran (Machine::config: the media put in, the switches changed), as the
    // real machine keeps its tape in the recorder.
    bool powered(void) const { return m != nullptr; }
    void power_off(void);
    bool power_on(std::string& err);
    // Off and on at once (GUI Machine -> Switch off and on, Ctrl+F12; MCP power cycle): the memory is lost, the
    // machine comes back as it ran; on error the old one is kept.
    bool power_cycle(std::string& err);
    const std::string& machine_file(void) const { return file; }
    uint64_t machine_generation(void) const { return generation; }   // changes on every load

    // Advance emulation by `elapsed_ns` of wall clock time (real time or turbo).
    void frame(uint64_t elapsed_ns);
    // Video recording: tick_hook(now) every tick_period cycles of emulated time (from tick_next on);
    // the emulation then runs in pieces that end on these boundaries, so the frames of a video are
    // taken at their exact emulated time whatever the speed.
    std::function<void(uint64_t)> tick_hook;
    uint64_t tick_period = 0, tick_next = 0;
    // Autosave of the media (Card::unsaved; docs/rozhodnuti.md 2026-10-29): the disks, tapes and RAM disks written
    // while running go to their files a few seconds after the last write (at least once a minute while the writes go
    // on), when the emulation pauses (frame) and when the program window loses the focus (the GUI calls
    // save_media_new). A medium that could not be saved is tried again after a new write or a minute later, not on
    // every pause or focus loss (the Log would fill with the same error).
    // frame() calls autosave with host_time_ns(); a test gives its own times.
    void autosave(uint64_t now_ns);
    // The media with unsaved writes saved now; returns how many cards saved.
    int save_media(void);
    // The same, unless nothing was written since the last try (a pause, the focus lost).
    int save_media_new(void);
    // Execute commands posted by other threads. Call from the emulation thread.
    void poll(void);
    // Post a command and wait for its result (from any thread except the emulation thread).
    nlohmann::json call(std::function<nlohmann::json(EmuHost&)> fn, int timeout_ms = 120000);

    bool paused = false;
    // Speed of the emulation (the whole machine against real time, not the CPU clock of the
    // card): pace_percent of real time, or as fast as possible.
    bool max_speed = false;
    int pace_percent = 100;     // 100, 200, 400
    // F8: 100 % -> 200 % -> 400 % -> maximum -> 100 %.
    void cycle_speed(void);
    // Emulated cycles per real second (0 = maximum).
    uint32_t target_hz(void) const;
    std::string status;         // last stop reason ("breakpoint F123h"...)
    double speed_percent = 0;

    // Save state (docs/stav.md): into memory or a file. Loading builds the machine of the state
    // (the old one stays on error), keeps breakpoints, watchpoints and the pause.
    bool save_state(std::vector<uint8_t>& out, std::string& err);
    bool load_state(const std::vector<uint8_t>& in, std::string& err);
    bool save_state_file(const std::string& path, std::string& err);
    bool load_state_file(const std::string& path, std::string& err);
    // the quick slot of the machine: work/state/<machine file name>.sapistate
    std::string quick_state_path(void) const;
    // named states in memory (MCP save_state / load_state without a file)
    std::map<std::string, std::vector<uint8_t>> snapshots;

    // Headless real-time loop (used by sapiemu-cli --mcp) until `quit` or quit_requested is set.
    void run_loop(std::atomic<bool>& quit);
    // MCP quit: the loop of the program (headless or GUI) ends as if closed by the user
    std::atomic<bool> quit_requested{ false };
    // Fail pending and future call()s; call before stopping the MCP server.
    void shutdown(void);

private:
    struct Task {
        std::function<nlohmann::json(EmuHost&)> fn;
        std::promise<nlohmann::json> result;
    };
    void handle_stop(void);
    void run_ticked(uint64_t cycles);
    void update_speed(void);

    std::unique_ptr<Machine> m;
    std::string file;
    nlohmann::json last_cfg;   // description of the machine, for power_on()
    uint64_t generation = 0;
    std::mutex task_mtx;
    std::deque<std::shared_ptr<Task>> tasks;
    bool closing = false;   // guarded by task_mtx
    uint64_t speed_t0 = 0, speed_c0 = 0;
    // autosave: the last look, media_writes() seen then and since when it has not changed, the first unsaved write
    // seen (0 = nothing unsaved), media_writes() at the last save (after a failed one only a new write or a minute
    // tries again), the pause already saved
    uint64_t as_look = 0, as_writes = 0, as_quiet = 0, as_first = 0, as_tried = UINT64_MAX;
    bool as_paused = false;
};

uint64_t host_time_ns(void);
