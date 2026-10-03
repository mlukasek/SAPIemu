#include "core/host.h"
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
    if (m) m->flush();
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
    // the old machine releases its TCP ports / COM ports first
    if (m) m->stop_bridges();
    std::unique_ptr<Machine> nm(new Machine());
    if (!nm->load_file(f, err) || !nm->start_bridges(err)) {
        if (m) {
            std::string e2;
            if (!m->start_bridges(e2)) LOGW("%s", e2.c_str());
        }
        return false;
    }
    if (m) m->flush();
    m = std::move(nm);
    file = f;
    generation++;
    paused = false;
    status.clear();
    speed_t0 = host_time_ns();
    speed_c0 = 0;
    return true;
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

void EmuHost::frame(uint64_t elapsed_ns)
{
    if (!m) return;
    if (!paused) {
        status.clear();
        if (turbo) {
            uint64_t start = host_time_ns();
            while (host_time_ns() - start < 14000000ull) {
                m->run_cycles(m->bus.cpu_hz / 200);
                if (m->stopped_on_breakpoint()) break;
            }
        } else {
            if (elapsed_ns > 100000000ull) elapsed_ns = 100000000ull;   // don't catch up after stalls
            m->run_cycles((uint64_t)((double)elapsed_ns * m->bus.cpu_hz / 1e9));
        }
        handle_stop();
    }
    update_speed();
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
    while (!quit) {
        poll();
        uint64_t now = host_time_ns();
        frame(now - last);
        last = now;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
