// EmuHost switches machines as the power switch does, but the old machine goes only after the new one has built (UX
// review 5.5): a machine file that cannot be read, a description that does not build here (a card of a newer version,
// a missing ROM, a wrong serial connection, a value of a wrong type) and a state that does not fit leave the running
// machine exactly as it was: the same machine, its memory, breakpoints, pause and serial connections, nothing run (an
// APS-5 sees no power failure). The trial build writes no file (a state loaded into it would otherwise go into the
// RAM disk file of the running machine). Its messages are logged once. Also Machine -> Keyboard through the
// description, the lock of the GUI (file_lock_try) and a machine dropped without saving (Quit at its warning).
#include <stdio.h>
#include <string.h>
#include <chrono>
#include "cards/hrk1.h"
#include "core/headless.h"
#include "core/host.h"
#include "core/log.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static json machine_cfg(const std::string& file)
{
    std::string text;
    CHECK(file_read_text(data_path(file), text));
    json cfg = json::parse(text, nullptr, false, true);
    CHECK(!cfg.is_discarded());
    return cfg;
}

// The info lines of the log that say a machine was built ("machine '...' loaded").
static int loaded_lines(void)
{
    std::vector<std::string> lines;
    log_recent(lines, 1000);
    int n = 0;
    for (const std::string& l : lines)
        if (l.find("info: machine '") == 0 && l.find("' loaded: ") != std::string::npos) n++;
    return n;
}

// MikroMon with a TCP server on its serial line (port 0: any free one)
static json mikromon_cfg(void)
{
    json cfg = machine_cfg("machines/mikromon.sapi");
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "DMR-1") c["serial"] = { { "tcp", { { "port", 0 } } } };
    return cfg;
}

static void broken_keeps_machine(void)
{
    const char* file = "machines/mikromon.sapi";
    const uint16_t A = 0x8123;   // RAM of DMR-1
    EmuHost h;
    std::string err;
    json cfg = mikromon_cfg();
    CHECK(h.load_config(cfg, file, err));
    Machine* m = h.machine();
    CHECK(m != nullptr);
    if (!m) return;
    m->run_seconds(1.0);
    m->poke(A, 0xA5);
    m->set_breakpoint(0x1234, true);
    h.paused = true;
    uint64_t gen = h.machine_generation(), now = m->bus.now;
    std::string screen = screen_text(*m);
    size_t bridges = m->bridges.size();
    SerialBridge* bridge = bridges ? m->bridges[0].get() : nullptr;
    CHECK_EQ(bridges, 1);

    std::vector<std::pair<const char*, json>> bad;
    json c = cfg;
    c["cards"].push_back({ { "slot", 5 }, { "type", "XYZ-9" } });
    bad.push_back({ "a card of a newer version", c });
    c = cfg;
    c["cards"][0]["rom"] = "roms/none/missing.txt";
    bad.push_back({ "a missing ROM", c });
    c = cfg;
    c["cards"][1]["serial"] = { { "com", { { "baud", 9600 } } } };
    bad.push_back({ "a COM port without its name", c });
    c = cfg;
    c["cards"][2]["name"] = 5;   // json: type error (an exception inside the build)
    bad.push_back({ "a value of a wrong type", c });
    c = cfg;
    c["cards"].push_back({ { "slot", 3 }, { "type", "RAM-1" } });
    bad.push_back({ "two cards in a slot", c });
    for (auto& b : bad) {
        err.clear();
        CHECK(!h.load_config(b.second, file, err));
        printf("%-30s %s\n", b.first, err.c_str());
        CHECK(!err.empty());
    }
    CHECK(!h.load("machines/none-such.sapi", err));
    printf("%-30s %s\n", "a missing machine file", err.c_str());
    // exactly as it was: nothing ran, nothing was switched off
    CHECK(h.machine() == m);
    CHECK_EQ(h.machine_generation(), gen);
    CHECK_EQ(m->bus.now, now);
    CHECK_EQ(m->peek(A), 0xA5);
    CHECK(m->has_breakpoint(0x1234));
    CHECK(h.paused);
    CHECK(h.machine_file() == file);
    CHECK(screen_text(*m) == screen);
    CHECK_EQ(m->bridges.size(), bridges);
    CHECK(bridges && m->bridges[0].get() == bridge);   // the TCP server of the line was not closed
    // and it runs on
    h.paused = false;
    m->clear_breakpoints();
    m->run_seconds(0.5);
    CHECK(m->bus.now > now);

    // a good description after all: built once, logged once
    int before = loaded_lines();
    CHECK(h.load_config(cfg, file, err));
    CHECK(h.machine_generation() != gen);
    CHECK_EQ(loaded_lines(), before + 1);
    CHECK(h.power_cycle(err));
    CHECK_EQ(loaded_lines(), before + 2);
}

// A state that does not fit (cut short in its last block): the machine of the state is built and the state loaded
// as a trial; the running machine stays as it was, and the RAM disk file it uses keeps its content although the
// state's RAM disk was loaded into the trial before the cut.
static void state_trial_writes_nothing(void)
{
    const char* file = "machines/mikromon.sapi";
    const char* img = "work/test/host_rmd1.bin";
    std::string err;
    file_remove(data_path(img));
    json cfg = machine_cfg(file);
    cfg["cards"].push_back({ { "slot", 4 }, { "type", "RMD-1" }, { "image", img } });
    std::vector<uint8_t> st;
    {   // the state: an RMD-1 with the content of the DRAM after power on (no file yet)
        Machine a;
        CHECK(a.load_json(cfg, err));
        a.run_seconds(0.2);
        CHECK(a.save_state(st, file, err));
        a.discard();
    }
    CHECK(!file_exists(data_path(img)));
    std::vector<uint8_t> content(256 * 1024, 0x11);
    CHECK(file_write_all(data_path(img), content.data(), content.size()));
    EmuHost h;
    CHECK(h.load_config(cfg, file, err));
    Machine* m = h.machine();
    uint64_t gen = h.machine_generation();
    if (!m) return;
    m->run_seconds(0.2);
    std::vector<uint8_t> cut = st;
    cut.resize(cut.size() - 4);   // the last block (serial 0) cut short: RMD-1 (X4) loads before it
    int back = -1;
    CHECK(!h.load_state(cut, err, &back));
    printf("%-30s %s\n", "a state cut short", err.c_str());
    CHECK_EQ(back, (int)STATE_BACK_SAME);
    CHECK(h.machine() == m);
    CHECK_EQ(h.machine_generation(), gen);
    std::vector<uint8_t> now;
    CHECK(file_read_all(data_path(img), now));
    CHECK(now == content);
    // the whole state loads (and its RAM disk goes into the file when the machine saves it)
    CHECK(h.load_state(st, err));
    CHECK(h.machine_generation() != gen);
    h.save_media();
    CHECK(file_read_all(data_path(img), now));
    CHECK(now != content);
    file_remove(data_path(img));
}

// Quit at the warning about another SAPIemu over the same work folder (EmuHost::drop): the machine goes without saving
// anything, its RAM disk written since the last save stays unsaved and the file keeps what the other window wrote.
static void drop_saves_nothing(void)
{
    const char* file = "machines/mikromon.sapi";
    const char* img = "work/test/host_drop_rmd1.bin";
    std::string err;
    file_remove(data_path(img));
    json cfg = machine_cfg(file);
    cfg["cards"].push_back({ { "slot", 4 }, { "type", "RMD-1" }, { "image", img } });
    std::vector<uint8_t> st;
    {   // a state with the RAM disk of another content than the file
        Machine a;
        CHECK(a.load_json(cfg, err));
        a.run_seconds(0.2);
        CHECK(a.save_state(st, file, err));
        a.discard();
    }
    std::vector<uint8_t> content(256 * 1024, 0x22);
    CHECK(file_write_all(data_path(img), content.data(), content.size()));
    {
        EmuHost h;
        CHECK(h.load_config(cfg, file, err));
        CHECK(h.load_state(st, err));   // its RAM disk is not saved yet
        Machine* m = h.machine();
        CHECK(m && m->unsaved());
        uint64_t gen = h.machine_generation();
        h.drop();
        CHECK(!h.machine());
        CHECK(h.machine_generation() != gen);
    }   // ~EmuHost: no machine to switch off
    std::vector<uint8_t> now;
    CHECK(file_read_all(data_path(img), now));
    CHECK(now == content);
    file_remove(data_path(img));
}

// APS-5: a description that does not build leaves the unit running; before, the old machine got the power fail
// warning first (HRK-1: an outage until it is switched off and on).
static void aps5_no_power_failure(void)
{
    const char* file = "machines/aps5.sapi";
    EmuHost h;
    std::string err;
    json cfg = machine_cfg(file);
    CHECK(h.load_config(cfg, file, err));
    Machine* m = h.machine();
    if (!m) return;
    m->run_seconds(2.0);
    Hrk1* panel = dynamic_cast<Hrk1*>(m->keyboard());
    CHECK(panel != nullptr);
    if (!panel) return;
    json bad = cfg;
    bad["cards"].push_back({ { "slot", 8 }, { "type", "XYZ-9" } });
    CHECK(!h.load_config(bad, file, err));
    CHECK(h.machine() == m);
    m->run_seconds(0.5);
    CHECK(panel->powered());
    CHECK(panel->outage_left() == 0);
}

// How long a switch takes now that the new machine is built twice (the trial and the real one).
static void timing(void)
{
    for (const char* file : { "machines/mikromon.sapi", "machines/aps5.sapi", "machines/cpm_zrmd_fdc1.sapi",
                              "machines/rkd1_jb_cm5400.sapi" }) {
        EmuHost h;
        std::string err;
        json cfg = machine_cfg(file);
        auto t0 = std::chrono::steady_clock::now();
        bool ok = h.load_config(cfg, file, err);
        auto t1 = std::chrono::steady_clock::now();
        ok = ok && h.power_cycle(err);
        auto t2 = std::chrono::steady_clock::now();
        CHECK(ok);
        if (!ok) fprintf(stderr, "%s: %s\n", file, err.c_str());
        printf("%-34s first start %4d ms, switched off and on %4d ms\n", file,
               (int)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count(),
               (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
    }
}

// Machine -> Keyboard goes through the description (set_live_param of the CPU card's "keyboard"): the keyboard is
// plugged in at once and stays after the machine is switched off and on.
static void keyboard_in_description(void)
{
    const char* file = "machines/mikromon.sapi";
    EmuHost h;
    std::string err;
    CHECK(h.load_config(machine_cfg(file), file, err));
    Machine* m = h.machine();
    if (!m || !m->cpu) return;
    CHECK(m->keyboard() && !strcmp(m->keyboard()->type_name(), "EKL-1"));
    json k = { { "type", "ANK-1" } };
    CHECK(m->set_live_param(m->cpu, "keyboard", k, err));
    CHECK(m->keyboard() && !strcmp(m->keyboard()->type_name(), "ANK-1"));
    CHECK(m->config["cards"][0]["keyboard"] == k);
    CHECK(!m->set_live_param(m->cpu, "keyboard", json{ { "type", "XYZ" } }, err));   // unknown: the old one stays
    CHECK(m->keyboard() && !strcmp(m->keyboard()->type_name(), "ANK-1"));
    CHECK(h.power_cycle(err));
    m = h.machine();
    CHECK(m && m->keyboard() && !strcmp(m->keyboard()->type_name(), "ANK-1"));
}

// The lock a GUI holds over its settings and work folder: a second one finds it taken (and is warned).
static void instance_lock(void)
{
    std::string p = data_path("work/test/host_gui.lock");
    CHECK_EQ(file_lock_try(p), 1);
    CHECK_EQ(file_lock_try(p), 0);
    CHECK_EQ(file_lock_try(data_path("work/test/no-such-folder/x.lock")), -1);
}

int main(void)
{
    test_init();
    instance_lock();
    broken_keeps_machine();
    state_trial_writes_nothing();
    drop_saves_nothing();
    aps5_no_power_failure();
    keyboard_in_description();
    timing();
    return TEST_RESULT();
}
