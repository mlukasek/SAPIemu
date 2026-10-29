// Save state is complete: for every machine in machines/, the state saved at A, run on,
// saved at B, is the same as the state of a new machine loaded with A and run for the same
// time. A variable a card forgets to save shows up as a difference (the block is named).
// Also: the file round trip through EmuHost, a state of another description, damaged data, the formats (a newer or
// a too old one refused before the running machine is touched, format 1 of the alphas still read).
#include <stdio.h>
#include <string.h>
#include "core/headless.h"
#include "core/host.h"
#include "core/machine.h"
#include "core/state.h"
#include "core/util.h"
#include "core/version.h"
#include "test_util.h"

using nlohmann::json;

static json machine_cfg(const std::string& file)
{
    std::string text;
    CHECK(file_read_text(data_path(file), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "IDE-1") c.erase("ide");   // no 128 MB image here
    return cfg;
}

// The blocks of a state: name -> content (after the header).
static void blocks(const std::vector<uint8_t>& d, std::vector<std::string>& names, std::vector<std::vector<uint8_t>>& data)
{
    StateIO io(true);
    io.data = d;
    char magic[8];
    io.raw(magic, 8);
    uint32_t ver;
    io.v(ver);
    std::string app, cfg, file;
    if (ver >= 2) io.v(app);
    io.v(cfg);
    io.v(file);
    while (io.ok() && io.pos < io.data.size()) {
        std::string nm;
        io.v(nm);
        uint32_t len = 0;
        io.v(len);
        if (!io.ok() || io.pos + len > io.data.size()) break;
        names.push_back(nm);
        data.emplace_back(io.data.begin() + (ptrdiff_t)io.pos, io.data.begin() + (ptrdiff_t)(io.pos + len));
        io.pos += len;
    }
}

static bool same_states(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, const std::string& what)
{
    if (a == b) return true;
    std::vector<std::string> na, nb;
    std::vector<std::vector<uint8_t>> da, db;
    blocks(a, na, da);
    blocks(b, nb, db);
    for (size_t i = 0; i < na.size() && i < nb.size(); i++) {
        if (na[i] != nb[i]) {
            fprintf(stderr, "%s: block %s / %s\n", what.c_str(), na[i].c_str(), nb[i].c_str());
            return false;
        }
        if (da[i] != db[i]) {
            size_t at = 0;
            while (at < da[i].size() && at < db[i].size() && da[i][at] == db[i][at]) at++;
            fprintf(stderr, "%s: block '%s' differs at byte %u (%u / %u bytes)\n", what.c_str(), na[i].c_str(),
                    (unsigned)at, (unsigned)da[i].size(), (unsigned)db[i].size());
            return false;
        }
    }
    fprintf(stderr, "%s: the states differ (%u / %u blocks)\n", what.c_str(), (unsigned)na.size(), (unsigned)nb.size());
    return false;
}

static void roundtrip(const std::string& file)
{
    json cfg = machine_cfg(file);
    Machine m1;
    std::string err;
    if (!m1.load_json(cfg, err)) {
        fprintf(stderr, "%s: %s\n", file.c_str(), err.c_str());
        CHECK(false);
        return;
    }
    m1.run_seconds(1.5);
    m1.type_text("1\r");   // something for the keyboard (ECMON: a boot choice)
    m1.run_seconds(0.02);
    std::vector<uint8_t> a, b1, b2;
    CHECK(m1.save_state(a, file, err));
    m1.run_seconds(0.7);
    CHECK(m1.save_state(b1, file, err));

    Machine m2;
    CHECK(m2.load_json(cfg, err));
    m2.run_seconds(0.3);   // something else first: the state replaces it
    if (!m2.load_state(a, err)) {
        fprintf(stderr, "%s: load_state: %s\n", file.c_str(), err.c_str());
        CHECK(false);
        return;
    }
    m2.run_seconds(0.7);
    CHECK(m2.save_state(b2, file, err));
    bool same = same_states(b1, b2, file);
    CHECK(same);
    CHECK(screen_text(m1) == screen_text(m2));
    printf("%-40s %6u KB  %s\n", file.c_str(), (unsigned)(a.size() / 1024), same ? "same" : "DIFFERENT");
}

// MPH-1V busy: a YM3812 tone, the 82C54 counting with F2 interrupts, the mouse moving
static void roundtrip_mph(void)
{
    json cfg = machine_cfg("machines/sapi1v.sapi");
    Machine m1;
    std::string err;
    CHECK(m1.load_json(cfg, err));
    m1.run_seconds(0.5);
    const uint8_t P = 0x50;
    auto ym = [&](uint8_t r, uint8_t v) { m1.bus.io_write(P + 6, r); m1.bus.io_write(P + 7, v); };
    ym(0x20, 0x01); ym(0x23, 0x01); ym(0x40, 0x10); ym(0x43, 0x00); ym(0x60, 0xF4); ym(0x63, 0xF2);
    ym(0x80, 0x24); ym(0x83, 0x35); ym(0xC0, 0x06); ym(0x01, 0x20); ym(0xE3, 0x02);
    ym(0xA0, 0x44); ym(0xB0, 0x32);
    ym(0x02, 0x80); ym(0x04, 0x01);              // timer 1 running
    m1.bus.io_write(P + 3, 0xB4);                // counter 2, mode 2
    m1.bus.io_write(P + 2, 0x39);
    m1.bus.io_write(P + 2, 0x01);
    m1.bus.io_write(P + 3, 0x36);                // counter 0, mode 3
    m1.bus.io_write(P + 0, 0x10);
    m1.bus.io_write(P + 0, 0x00);
    m1.bus.io_write(P + 4, 0x28);                // GATE2, GATE0
    m1.bus.mouse_dx = 120;
    m1.bus.mouse_dy = -77;
    m1.run_seconds(0.0123);
    std::vector<uint8_t> a, b1, b2;
    CHECK(m1.save_state(a, "machines/sapi1v.sapi", err));
    int mdx = m1.bus.mouse_dx, mdy = m1.bus.mouse_dy;   // the host side: not in the state
    m1.run_seconds(0.5);
    CHECK(m1.save_state(b1, "", err));
    Machine m2;
    CHECK(m2.load_json(cfg, err));
    CHECK(m2.load_state(a, err));
    m2.bus.mouse_dx = mdx;
    m2.bus.mouse_dy = mdy;
    m2.run_seconds(0.5);
    CHECK(m2.save_state(b2, "", err));
    bool same = same_states(b1, b2, "MPH-1V busy");
    CHECK(same);
    printf("%-40s %6u KB  %s\n", "MPH-1V busy (tone, 82C54, mouse)", (unsigned)(a.size() / 1024), same ? "same" : "DIFFERENT");
}

// EmuHost: the file round trip builds the machine of the state and keeps breakpoints
static void host_file(void)
{
    EmuHost h;
    std::string err;
    json cfg = machine_cfg("machines/sapi1v.sapi");
    CHECK(h.load_config(cfg, "machines/sapi1v.sapi", err));
    CHECK(run_until_text(*h.machine(), "Zadej 0-3", 3.0));
    std::string path = data_path("work/test-state/sapi1v.sapistate");
    CHECK(h.save_state_file(path, err));
    std::string before = screen_text(*h.machine());
    h.machine()->type_text("1");
    h.machine()->run_seconds(2.0);
    CHECK(screen_text(*h.machine()) != before);
    h.machine()->set_breakpoint(0x1234, true);
    h.paused = true;
    uint64_t gen = h.machine_generation();
    if (!h.load_state_file(path, err)) fprintf(stderr, "load_state_file: %s\n", err.c_str());
    CHECK(h.machine_generation() != gen);
    CHECK(screen_text(*h.machine()) == before);
    CHECK(h.machine()->has_breakpoint(0x1234));
    CHECK(h.paused);
    CHECK(h.quick_state_path().find("sapi1v.sapistate") != std::string::npos);
    // in place: a machine of another description refuses it, with rebuild it builds itself
    std::vector<uint8_t> st;
    CHECK(h.save_state(st, err));
    Machine other;
    CHECK(other.load_json(machine_cfg("machines/mikromon.sapi"), err));
    CHECK(!other.load_state(st, err));
    CHECK(other.load_state(st, err, true));
    CHECK(other.name == h.machine()->name);
    // damaged: the old machine stays (the trial build, state_formats)
    std::vector<uint8_t> bad = st;
    bad.resize(bad.size() / 2);
    CHECK(!h.load_state(bad, err));
    printf("damaged state: %s\n", err.c_str());
    CHECK(h.machine() != nullptr);
    std::vector<uint8_t> junk(100, 0x55);
    CHECK(!h.load_state(junk, err));
    file_remove(path);
}

// The state with another start of the header: format `fmt`, with the version of the program `app` when fmt >= 2.
static std::vector<uint8_t> with_format(const std::vector<uint8_t>& st, uint32_t fmt, const std::string& app)
{
    StateIO in(true);
    in.data = st;
    char magic[8];
    in.raw(magic, 8);
    uint32_t f = 0;
    in.v(f);
    std::string a;
    if (f >= 2) in.v(a);
    CHECK(in.ok());
    StateIO out(false);
    out.raw(magic, 8);
    out.v(fmt);
    std::string a2 = app;
    if (fmt >= 2) out.v(a2);
    out.data.insert(out.data.end(), st.begin() + (ptrdiff_t)in.pos, st.end());
    return out.data;
}

// The format of the state: the version of SAPIemu in the header (format 2); a format this SAPIemu does not read (a
// newer or a too old version) is refused before the running machine is touched; format 1 of the alphas still loads;
// a state that does not fit after all (a card block cut short) leaves the running machine as it was (the trial build
// of EmuHost, test_host).
static void state_formats(void)
{
    const char* file = "machines/sapi1v.sapi";
    const uint16_t A = 0x7123;   // RAM-1V, nobody uses it at the ECMON prompt
    EmuHost h;
    std::string err;
    CHECK(h.load_config(machine_cfg(file), file, err));
    CHECK(run_until_text(*h.machine(), "Zadej 0-3", 3.0));
    std::vector<uint8_t> st;
    CHECK(h.save_state(st, err));
    std::string screen_st = screen_text(*h.machine());
    StateOrigin o;
    CHECK(Machine::state_origin(st, o));
    CHECK_EQ((int)o.format, (int)STATE_FORMAT);
    CHECK(o.app == sapiemu_version());
    // the running machine moves on: a load would replace its screen and its memory
    h.machine()->type_text("1");
    h.machine()->run_seconds(2.0);
    h.machine()->poke(A, 0xA5);
    std::string screen = screen_text(*h.machine());
    CHECK(screen != screen_st);
    h.machine()->set_breakpoint(0x2345, true);
    Machine* mp = h.machine();
    uint64_t gen = h.machine_generation();
    // a newer format (with its version of the program) and a too old one: refused, the same machine untouched
    std::vector<uint8_t> newer = with_format(st, STATE_FORMAT + 1, "9.9.9-test");
    StateOrigin on;
    CHECK(Machine::state_origin(newer, on));
    CHECK_EQ((int)on.format, (int)STATE_FORMAT + 1);
    CHECK(on.app == "9.9.9-test");
    int back = -1;
    CHECK(!h.load_state(newer, err, &back));
    CHECK_EQ(back, (int)STATE_BACK_SAME);
    printf("newer format: %s\n", err.c_str());
    CHECK(err.find("newer SAPIemu (9.9.9-test") != std::string::npos);
    CHECK(err.find(sapiemu_version()) != std::string::npos);
    std::vector<uint8_t> old = with_format(st, STATE_FORMAT_MIN - 1, "");
    CHECK(!h.load_state(old, err));
    printf("too old format: %s\n", err.c_str());
    CHECK(err.find("old SAPIemu") != std::string::npos);
    CHECK(h.machine() == mp);
    CHECK_EQ(h.machine_generation(), gen);
    CHECK(screen_text(*h.machine()) == screen);
    CHECK_EQ(h.machine()->peek(A), 0xA5);
    // format 1 (the alphas before format 2): no version of the program, loads as before
    std::vector<uint8_t> v1 = with_format(st, 1, "");
    StateOrigin o1;
    CHECK(Machine::state_origin(v1, o1));
    CHECK_EQ((int)o1.format, 1);
    CHECK(o1.app.empty());
    json cfg1;
    std::string f1;
    CHECK(Machine::state_header(v1, cfg1, f1, err));
    CHECK(f1 == file);
    if (!h.load_state(v1, err)) fprintf(stderr, "format 1: %s\n", err.c_str());
    CHECK(h.machine_generation() != gen);
    CHECK(screen_text(*h.machine()) == screen_st);
    CHECK(h.machine()->peek(A) != 0xA5);
    // does not fit after all (the header is good, a card block is cut short): the trial build finds it, the machine
    // runs on as it was (not built again)
    h.machine()->type_text("1");
    h.machine()->run_seconds(2.0);
    h.machine()->poke(A, 0x5A);
    screen = screen_text(*h.machine());
    h.paused = true;
    gen = h.machine_generation();
    mp = h.machine();
    std::vector<uint8_t> cut = st;
    cut.resize(cut.size() / 2);
    back = -1;
    CHECK(!h.load_state(cut, err, &back));
    CHECK_EQ(back, (int)STATE_BACK_SAME);   // the GUI says "runs on as it was"
    printf("a state that does not fit: %s\n", err.c_str());
    CHECK(h.machine() == mp);
    CHECK_EQ(h.machine_generation(), gen);
    CHECK(h.machine_file() == file);
    CHECK(screen_text(*h.machine()) == screen);
    CHECK_EQ(h.machine()->peek(A), 0x5A);
    CHECK(h.machine()->has_breakpoint(0x2345));
    CHECK(h.paused);
    // switched off: it stays off
    h.power_off();
    CHECK(!h.load_state(cut, err));
    CHECK(h.machine() == nullptr);
    CHECK(h.machine_file() == file);
    CHECK(h.power_on(err));
}

// Cards moved while running (the configurator drags them): the slots of the bus are equal, so the
// machine runs on exactly as one without the move, and its state loads into the moved description.
static void moved_cards(void)
{
    const char* file = "machines/cpm_zrmd_fdc1.sapi";
    json cfg = machine_cfg(file);
    Machine m0, m1;
    std::string err;
    CHECK(m0.load_json(cfg, err) && m1.load_json(cfg, err));
    m0.run_seconds(1.0);
    m1.run_seconds(1.0);
    CHECK(m1.move_card(3, 7));    // AND-1Z-89 to an empty slot
    CHECK(m1.move_card(2, 4));    // DMR-1 and ZRMD-1 swap
    CHECK(!m1.move_card(8, 6));   // nothing in X8
    int and_slot = 0, dmr_slot = 0, zrmd_slot = 0;
    for (auto& c : m1.cards) {
        if (c->type == "AND-1Z-89") and_slot = c->slot;
        if (c->type == "DMR-1") dmr_slot = c->slot;
        if (c->type == "ZRMD-1") zrmd_slot = c->slot;
    }
    CHECK_EQ(and_slot, 7);
    CHECK_EQ(dmr_slot, 4);
    CHECK_EQ(zrmd_slot, 2);
    int last = 0;   // the description follows, in slot order
    for (const json& c : m1.config["cards"]) {
        int s = c.value("slot", 0);
        CHECK(s >= last);
        last = s;
        if (c.value("type", std::string()) == "AND-1Z-89") CHECK_EQ(s, 7);
    }
    for (Machine* m : { &m0, &m1 }) {
        m->type_text("1\r");
        m->run_seconds(3.0);
    }
    CHECK(screen_text(m0) == screen_text(m1));
    std::vector<uint8_t> st;
    CHECK(m1.save_state(st, file, err));
    Machine m2;
    CHECK(m2.load_json(m1.config, err));
    if (!m2.load_state(st, err)) {
        fprintf(stderr, "moved cards: load_state: %s\n", err.c_str());
        CHECK(false);
    }
    CHECK(screen_text(m2) == screen_text(m1));
    printf("moved cards: %s\n", screen_text(m1) == screen_text(m0) ? "runs as before" : "DIFFERENT");
}

// EmuHost::set_file: the running machine described by its copy in My machines (the configurator's Save of a supplied
// machine in a release build): the file, the name and "based_on" in its description, its own quick slot; it is not
// built again, a power cycle keeps the copy.
static void host_copy(void)
{
    EmuHost h;
    std::string err;
    CHECK(h.load("machines/mikromon.sapi", err));
    uint64_t gen = h.machine_generation();
    std::string slot = h.quick_state_path();
    h.set_file("work/machines/mikromon.sapi", "MikroMon (moje)", "machines/mikromon.sapi");
    CHECK(h.machine_generation() == gen);
    CHECK(h.machine_file() == "work/machines/mikromon.sapi");
    CHECK(h.machine()->name == "MikroMon (moje)");
    CHECK(h.machine()->config.value("based_on", std::string()) == "machines/mikromon.sapi");
    CHECK(h.quick_state_path() != slot && h.quick_state_path().find("mine_mikromon.sapistate") != std::string::npos);
    CHECK(h.power_cycle(err));
    CHECK(h.machine_file() == "work/machines/mikromon.sapi" && h.machine()->name == "MikroMon (moje)");
}

int main(int argc, char** argv)
{
    test_init();
    host_time_fix(1790000000);   // the RTCs read a fixed host clock
    if (argc > 1) {
        roundtrip(argv[1]);
        return TEST_RESULT();
    }
    for (const std::string& f : list_machine_files()) roundtrip(f);
    roundtrip_mph();
    moved_cards();
    host_file();
    host_copy();
    state_formats();
    return TEST_RESULT();
}
