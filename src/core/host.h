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
#include "core/log.h"
#include "core/machine.h"

// The machine that ran, after a state that did not load (EmuHost::load_state): what the user is told.
enum StateBack {
    STATE_BACK_SAME = 0,    // as it was: not touched (the header refused the state) or brought back from its own state
                            // saved first; switched off before: off again
    STATE_BACK_RESTARTED,   // its own state could not be saved first: built again from its description, memory lost
    STATE_BACK_RESET,       // it did not come back (the Log says why): reset
};

class EmuHost {
public:
    EmuHost();
    ~EmuHost();

    // Load a machine description; the old machine is kept on error.
    bool load(const std::string& file, std::string& err);
    // Build a machine from a description (configurator); `file` is where it was
    // loaded from / saved to (empty for an unsaved one).
    // The old machine is switched off only after the new one has built (UX review 5.5): a trial build first, while the
    // old one runs on untouched; a description that does not build here (a card of a newer version, a missing ROM or
    // medium, a wrong serial connection) leaves it exactly as it was (not switched off: an APS-5 sees no power
    // failure, its host connections stay). Then, as switching machines is a power cycle, the old one is switched off
    // (host connections released, the power fail warning, media and battery memories saved) and the new one is built
    // again from the files as they are now. Exclusive host resources (TCP ports, COM ports) are taken only then.
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
    // The machine gone without saving anything (the GUI quit at the warning about another SAPIemu over the same work
    // folder): no power fail warning, no media, RAM disks or battery memories written (Machine::discard), the host
    // connections closed. Its files stay as the other window has them.
    void drop(void);
    // Off and on at once (GUI Machine -> Switch off and on, Ctrl+F12; MCP power cycle): the memory is lost, the
    // machine comes back as it ran; on error the old one is kept.
    bool power_cycle(std::string& err);
    const std::string& machine_file(void) const { return file; }
    uint64_t machine_generation(void) const { return generation; }   // changes on every load
    // The running machine is described by another file now (the configurator saved it as a copy in My machines,
    // core/machine_index.h): the file, and the name and "based_on" of the copy in its description (no hardware); nothing
    // is built again, the generation stays.
    void set_file(const std::string& f, const std::string& name, const std::string& based_on);

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

    // Save state (docs/stav.md): into memory or a file. Loading builds the machine of the state, keeps breakpoints,
    // watchpoints and the pause. On error the old machine stays: a state of a format not read here is refused before
    // it is touched; a state that does not fit (a card of another version, a description that does not build here) is
    // found by the trial build with the state loaded into it (load_config) while the old machine runs on untouched.
    // Should the real build fail after a good trial, the old machine comes back from its own state saved in memory
    // first; a switched off machine stays off. back_out (when given): how the machine that ran is after an error
    // (StateBack).
    bool save_state(std::vector<uint8_t>& out, std::string& err);
    bool load_state(const std::vector<uint8_t>& in, std::string& err, int* back_out = nullptr);
    bool save_state_file(const std::string& path, std::string& err);
    bool load_state_file(const std::string& path, std::string& err, int* back_out = nullptr);
    // the quick slot of the machine: work/state/<machine file name>.sapistate (a copy of My machines: mine_<name>, not
    // the slot of its original)
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
    // load_config, load_state: the trial build (with the state `st` loaded when given; its messages into `tlog`, logged
    // there only when it fails) and the real one (the old machine switched off first; `tlog` logged instead of the
    // messages of this build when given; on error the old machine stays, its bridges started again).
    bool trial(const nlohmann::json& cfg, const std::string& file, const std::vector<uint8_t>* st,
               std::vector<LogHeld>& tlog, std::string& err);
    bool build(const nlohmann::json& cfg, const std::string& file, const std::vector<uint8_t>* st,
               const std::vector<LogHeld>* tlog, std::string& err);

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
