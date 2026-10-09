#include "core/host.h"
#include "core/state.h"
#include "core/util.h"
#include "core/log.h"

#include <chrono>
#include <thread>

using nlohmann::json;

uint64_t host_time_ns(void)
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

EmuHost::EmuHost() {}

EmuHost::~EmuHost()
{
    shutdown();
    if (m) m->power_down();
}

void EmuHost::shutdown(void)
{
    std::lock_guard<std::mutex> lock(task_mtx);
    closing = true;
    for (auto& t : tasks) t->result.set_value(json{ { "error", "emulator is shutting down" } });
    tasks.clear();
}

bool EmuHost::load(const std::string& f, std::string& err)
{
    std::string text, full = data_path(f);
    if (!file_read_text(full, text)) {
        err = "cannot read machine file " + full;
        return false;
    }
    nlohmann::json cfg = nlohmann::json::parse(text, nullptr, false, true /* ignore comments */);
    if (cfg.is_discarded()) {
        err = "syntax error in machine file " + full;
        return false;
    }
    return load_config(cfg, f, err);
}

bool EmuHost::load_config(const nlohmann::json& cfg, const std::string& f, std::string& err)
{
    // the old machine releases its TCP ports / COM ports first; switching
    // machines is a power cycle: the old one saves its RAM disks
    if (m) {
        m->stop_bridges();
        m->power_down();
    }
    std::unique_ptr<Machine> nm(new Machine());
    nm->config_path = f.empty() ? std::string() : data_path(f);
    if (!nm->load_json(cfg, err) || !nm->start_bridges(err)) {
        if (m) {
            std::string e2;
            if (!m->start_bridges(e2)) LOGW("%s", e2.c_str());
        }
        return false;
    }
    m = std::move(nm);
    file = f;
    last_cfg = cfg;
    generation++;
    paused = false;
    status.clear();
    speed_t0 = host_time_ns();
    speed_c0 = 0;
    return true;
}

void EmuHost::power_off(void)
{
    if (!m) return;
    m->stop_bridges();
    m->power_down();
    m.reset();
    generation++;
    paused = false;
    status.clear();
    speed_percent = 0;
    LOGI("machine switched off");
}

bool EmuHost::power_on(std::string& err)
{
    if (last_cfg.is_null()) {
        err = "no machine to switch on";
        return false;
    }
    nlohmann::json cfg = last_cfg;
    return load_config(cfg, file, err);
}

void EmuHost::handle_stop(void)
{
    if (!m->stopped_on_breakpoint()) return;
    paused = true;
    status = m->stop_text();
    LOGI("stopped: %s", status.c_str());
    m->clear_break();
}

void EmuHost::update_speed(void)
{
    uint64_t t = host_time_ns();
    if (t - speed_t0 < 500000000ull) return;
    double emu_s = (double)(m->bus.now - speed_c0) / m->bus.cpu_hz;
    double real_s = (double)(t - speed_t0) / 1e9;
    speed_percent = paused ? 0.0 : 100.0 * emu_s / real_s;
    speed_t0 = t;
    speed_c0 = m->bus.now;
}

// run_cycles in pieces that end on the ticks of tick_hook
void EmuHost::run_ticked(uint64_t cycles)
{
    if (!tick_hook || !tick_period) {
        m->run_cycles(cycles);
        return;
    }
    uint64_t end = m->bus.now + cycles;
    while (m && m->bus.now < end) {
        uint64_t stop = tick_next < end ? tick_next : end;
        if (stop > m->bus.now) m->run_cycles(stop - m->bus.now);
        if (m->stopped_on_breakpoint()) break;
        while (tick_hook && tick_period && m->bus.now >= tick_next) {
            tick_hook(m->bus.now);
            tick_next += tick_period;
        }
        if (!tick_hook) {   // stopped inside the hook
            if (m->bus.now < end) m->run_cycles(end - m->bus.now);
            break;
        }
    }
}

void EmuHost::frame(uint64_t elapsed_ns)
{
    if (!m) return;
    if (!paused) {
        status.clear();
        if (max_speed) {
            uint64_t start = host_time_ns();
            while (host_time_ns() - start < 14000000ull) {
                run_ticked(m->bus.cpu_hz / 200);
                if (m->stopped_on_breakpoint()) break;
            }
        } else {
            if (elapsed_ns > 100000000ull) elapsed_ns = 100000000ull;   // don't catch up after stalls
            run_ticked((uint64_t)((double)elapsed_ns * target_hz() / 1e9));
        }
        handle_stop();
    }
    update_speed();
}

uint32_t EmuHost::target_hz(void) const
{
    if (max_speed || !m) return 0;
    return (uint32_t)((uint64_t)m->bus.cpu_hz * (uint64_t)pace_percent / 100u);
}

void EmuHost::cycle_speed(void)
{
    if (max_speed) {
        max_speed = false;
        pace_percent = 100;
    } else if (pace_percent < 400) {
        pace_percent *= 2;
    } else {
        max_speed = true;
    }
}

void EmuHost::poll(void)
{
    for (;;) {
        std::shared_ptr<Task> t;
        {
            std::lock_guard<std::mutex> lock(task_mtx);
            if (tasks.empty()) return;
            t = tasks.front();
            tasks.pop_front();
        }
        json r;
        try {
            r = t->fn(*this);
        } catch (const std::exception& e) {
            r = json{ { "error", std::string("exception: ") + e.what() } };
        }
        t->result.set_value(r);
    }
}

json EmuHost::call(std::function<json(EmuHost&)> fn, int timeout_ms)
{
    auto t = std::make_shared<Task>();
    t->fn = std::move(fn);
    std::future<json> f = t->result.get_future();
    {
        std::lock_guard<std::mutex> lock(task_mtx);
        if (closing) return json{ { "error", "emulator is shutting down" } };
        tasks.push_back(t);
    }
    if (f.wait_for(std::chrono::milliseconds(timeout_ms)) != std::future_status::ready)
        return json{ { "error", "timeout waiting for the emulator" } };
    return f.get();
}

void EmuHost::run_loop(std::atomic<bool>& quit)
{
    uint64_t last = host_time_ns();
    while (!quit && !quit_requested) {
        poll();
        uint64_t now = host_time_ns();
        frame(now - last);
        last = now;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

// ---------------------------------------------------------------- save state

bool EmuHost::save_state(std::vector<uint8_t>& out, std::string& err)
{
    if (!m) { err = "the machine is switched off"; return false; }
    return m->save_state(out, file, err);
}

bool EmuHost::load_state(const std::vector<uint8_t>& in, std::string& err)
{
    nlohmann::json cfg;
    std::string f;
    if (!Machine::state_header(in, cfg, f, err)) return false;
    // debugging aids of the old machine survive
    std::vector<uint16_t> bps;
    std::vector<uint8_t> mw, iw;
    if (m) {
        bps = m->breakpoints();
        mw.assign(m->bus.mem_watch, m->bus.mem_watch + 65536);
        iw.assign(m->bus.io_watch, m->bus.io_watch + 256);
    }
    bool was_paused = paused;
    if (!load_config(cfg, f, err)) return false;
    if (!m->load_state(in, err)) {
        err = "state: " + err;
        m->reset();
        return false;
    }
    for (uint16_t a : bps) m->set_breakpoint(a, true);
    for (size_t i = 0; i < mw.size(); i++)
        if (mw[i]) m->set_mem_watch((uint16_t)i, mw[i]);
    for (size_t i = 0; i < iw.size(); i++)
        if (iw[i]) m->set_io_watch((uint8_t)i, iw[i]);
    paused = was_paused;
    speed_t0 = host_time_ns();
    speed_c0 = m->bus.now;
    return true;
}

bool EmuHost::save_state_file(const std::string& path, std::string& err)
{
    std::vector<uint8_t> d;
    if (!save_state(d, err)) return false;
    std::string dir = path_dir(path);
    if (!dir.empty()) make_dirs(dir);
    if (!file_write_all(path, d.data(), d.size())) { err = "cannot write " + path; return false; }
    LOGI("state saved: %s (%u KB)", path.c_str(), (unsigned)(d.size() / 1024));
    return true;
}

bool EmuHost::load_state_file(const std::string& path, std::string& err)
{
    std::vector<uint8_t> d;
    if (!file_read_all(path, d)) { err = "cannot read " + path; return false; }
    if (!load_state(d, err)) return false;
    LOGI("state loaded: %s", path.c_str());
    return true;
}

std::string EmuHost::quick_state_path(void) const
{
    std::string base = file.empty() ? std::string("unsaved") : file;
    size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot > 0) base.resize(dot);
    return data_path("work/state/" + base + ".sapistate");
}
