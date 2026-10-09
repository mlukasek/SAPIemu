#include "core/machine.h"
#include "core/state.h"
#include "cards/catalog.h"
#include "cards/placeholder.h"
#include "core/card.h"
#include "core/charset.h"
#include "core/ihex.h"
#include "core/log.h"
#include "core/machine_cfg.h"
#include "core/util.h"
#include "input/keyboards.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <chrono>

using nlohmann::json;

Machine::Machine()
{
    memset(bp_exec, 0, sizeof(bp_exec));
}

Machine::~Machine()
{
    stop_bridges();   // bridge threads use channels owned by the cards
    flush();
}

bool Machine::start_bridges(std::string& err)
{
    stop_bridges();
    for (SerialChannel* ch : serials) {
        std::vector<SerialBridge*> created;
        bool ok = serial_bridges_from_config(ch, ch->bridge_cfg, ch->baud, created, err);
        for (SerialBridge* b : created) bridges.emplace_back(b);
        if (!ok) return false;
    }
    return true;
}

void Machine::stop_bridges(void)
{
    for (auto& b : bridges) b->stop();
    bridges.clear();
}

void Machine::remove_bridges(SerialChannel* ch, const char* kind)
{
    for (size_t i = 0; i < bridges.size();) {
        if (bridges[i]->channel == ch && strcmp(bridges[i]->kind(), kind) == 0) {
            bridges[i]->stop();
            bridges.erase(bridges.begin() + i);
        } else {
            i++;
        }
    }
}

bool Machine::load_file(const std::string& path, std::string& err)
{
    std::string full = data_path(path);
    std::string text;
    if (!file_read_text(full, text)) {
        err = "cannot read machine file " + full;
        return false;
    }
    json cfg = json::parse(text, nullptr, false, true /* ignore comments */);
    if (cfg.is_discarded()) {
        err = "syntax error in machine file " + full;
        return false;
    }
    config_path = full;
    return load_json(cfg, err);
}

bool Machine::load_json(const json& cfg, std::string& err)
{
    config = cfg;
    name = cfg.value("name", std::string("SAPI-1"));

    // memories after power on: zeros (default) or random bits like the real chips
    std::string pom;
    if (!cfg_string(cfg, "power_on_memory", pom, "zero", err)) return false;
    if (pom != "zero" && pom != "random") {
        err = "power_on_memory must be zero or random";
        return false;
    }
    bus.power_on_random = pom == "random";
    uint64_t seed = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
    bus.power_on_rng = (uint32_t)(seed ^ (seed >> 32)) | 1;

    // bus: number of positions from its type, ARB-1C JP3 decides where the CPU may sit
    int slots = 8;
    bool slot1_cpu_only = true;
    if (cfg.contains("bus")) {
        const json& b = cfg["bus"];
        std::string btype;
        if (!cfg_string(b, "type", btype, "ARB-1C-8", err)) return false;
        const BusInfo* bi = bus_info(btype);
        if (!bi) {
            err = "unknown bus type '" + btype + "'";
            return false;
        }
        long long n;
        if (!cfg_number(b, "slots", n, bi->slots, err)) return false;
        if (n < 1 || n > 32) {
            err = "bus slots must be 1..32";
            return false;
        }
        slots = (int)n;
        std::string jp3;
        if (!cfg_string(b, "jp3", jp3, "2-3", err)) return false;
        // JP3 1-2: address buffers of slot 1 disabled (PKT-1 / SAPI-1.5 style),
        // the CPU card may then sit in any slot.
        slot1_cpu_only = !(bi->has_jp3 && jp3 == "1-2");
    }

    if (!cfg.contains("cards") || !cfg["cards"].is_array()) {
        err = "machine has no 'cards' array";
        return false;
    }

    std::vector<bool> used(slots + 1, false);
    for (const json& cc : cfg["cards"]) {
        std::string type = cc.value("type", std::string());
        const CardInfo* info = card_info(type);
        Card* card = card_create(type);
        if (!card) {
            if (!info) {
                err = "unknown card type '" + type + "'";
                return false;
            }
            if (info->cls == CC_CPU) {
                err = type + ": this CPU card is not emulated yet";
                return false;
            }
            LOGW("%s is not emulated yet, the slot stays passive", type.c_str());
            card = new PlaceholderCard(type);
        }
        cards.emplace_back(card);
        card->bus = &bus;
        card->name = cc.value("name", type);
        for (auto& other : cards)   // two cards of a type: tell them apart by the slot
            if (other.get() != card && other->name == card->name && !cc.contains("name"))
                card->name = type + " X" + std::to_string(cc.value("slot", 0));
        long long slot;
        if (!cfg_number(cc, "slot", slot, 0, err)) return false;
        int width = info ? info->width : 1;
        if (slot < 1 || slot + width - 1 > slots) {
            err = card->name + ": slot must be 1.." + std::to_string(slots - width + 1);
            return false;
        }
        for (int i = 0; i < width; i++) {
            if (used[slot + i]) {
                err = card->name + ": slot " + std::to_string(slot + i) + " is already occupied";
                return false;
            }
            used[slot + i] = true;
        }
        card->slot = (int)slot;
        std::string cerr;
        if (!card->configure(cc, cerr)) {
            err = card->name + ": " + cerr;
            return false;
        }
        CpuCard* c = dynamic_cast<CpuCard*>(card);
        if (c) {
            if (cpu) { err = "more than one CPU card"; return false; }
            if (slot1_cpu_only && slot != 1) {
                err = card->name + ": CPU card must be in slot 1 (X1)";
                return false;
            }
            cpu = c;
        } else if (slot == 1 && slot1_cpu_only) {
            err = card->name + ": slot 1 (X1) is reserved for the CPU card";
            return false;
        }
        DisplayDevice* d = dynamic_cast<DisplayDevice*>(card);
        if (d) {
            displays.push_back(d);
            if (!display && d->text_display()) display = d;
        }
        for (int i = 0; SerialChannel* s = card->serial_channel(i); i++) serials.push_back(s);
    }
    if (!cpu) {
        err = "machine has no CPU card";
        return false;
    }

    if (cfg.contains("plant") && !cfg["plant"].is_null()) {   // the furnaces of APS-5
        std::string perr;
        Plant* p = plant_create(cfg["plant"], perr);
        if (!p) {
            err = "plant: " + perr;
            return false;
        }
        plant.reset(p);
        p->bus = &bus;
        bus.plant = p;
    }

    std::vector<Card*> sorted;
    for (auto& c : cards) sorted.push_back(c.get());
    std::stable_sort(sorted.begin(), sorted.end(), [](Card* a, Card* b) { return a->slot < b->slot; });
    bus.cards = sorted;
    bus.cpu_hz = cpu->clock_hz();
    bus.phi2_hz = cpu->phi2_hz();
    cpu->bp_exec = bp_exec;

    LOGI("machine '%s' loaded: %d cards, CPU %.3f MHz", name.c_str(), (int)cards.size(), cpu->cpu_display_hz() / 1e6);
    reset();
    return true;
}

void Machine::reset(void)
{
    bus.reset();
    bus.break_request = false;
    clear_break();
}

// Translate a CPU stop into a StopReason.
static StopReason classify_stop(Bus& bus, CpuCard* cpu, const uint8_t* bp, std::string& msg)
{
    char buf[64];
    if (bus.break_request) {
        bus.break_request = false;
        msg = bus.break_reason;
        return STOP_WATCHPOINT;
    }
    if (bp[cpu->instr_pc]) {
        snprintf(buf, sizeof(buf), "breakpoint %04Xh", cpu->instr_pc);
        msg = buf;
        return STOP_BREAKPOINT;
    }
    msg = "step";
    return STOP_STEP;
}

uint64_t Machine::run_cycles(uint64_t cycles)
{
    if (!cpu) return 0;
    uint64_t done = 0;
    clear_break();
    cpu->step_mode = false;
    while (done < cycles) {
        uint64_t chunk = cycles - done;
        if (chunk > 100000) chunk = 100000;
        done += (uint64_t)cpu->run((int)chunk);
        if (cpu->stop_hit) {
            stop = classify_stop(bus, cpu, bp_exec, stop_msg);
            break;
        }
        // mix as we go: the sources add at most 4 s ahead of the mixed part (Bus::audio_add) and
        // a card catches up at most 1 s at once, so a long run (MCP run_for 40000) lost its sound
        if (done < cycles) bus.audio_collect();
    }
    bus.audio_collect();
    return done;
}

uint64_t Machine::run_seconds(double seconds)
{
    return run_cycles((uint64_t)(seconds * bus.cpu_hz));
}

bool Machine::step(void)
{
    if (!cpu) return false;
    clear_break();
    cpu->step_mode = true;
    // Stopped right after a fetch: the next boundary ends the step. Otherwise the
    // first boundary is the start of the instruction to execute.
    cpu->step_skip = cpu->at_boundary ? 0 : 1;
    for (int guard = 0; guard < 100000; guard++) {
        cpu->run(64);
        if (cpu->stop_hit) break;
    }
    cpu->step_mode = false;
    // a watchpoint may fire inside the stepped instruction
    if (bus.break_request) stop = classify_stop(bus, cpu, bp_exec, stop_msg);
    else { stop = STOP_STEP; stop_msg = "step"; }
    return true;
}

bool Machine::step_over(double max_seconds)
{
    if (!cpu) return false;
    DisasmResult d = disassemble(cpu->instr_pc);
    if (!d.call_like) return step();
    uint16_t ret_addr = (uint16_t)(cpu->instr_pc + d.length);
    bool had_bp = bp_exec[ret_addr] != 0;
    bp_exec[ret_addr] = 1;
    step();   // leave the current instruction (it may be a breakpoint itself)
    if (stop == STOP_STEP && cpu->instr_pc != ret_addr) run_seconds(max_seconds);
    if (!had_bp) bp_exec[ret_addr] = 0;
    if (cpu->instr_pc == ret_addr && stop != STOP_WATCHPOINT) {
        stop = STOP_STEP;
        stop_msg = "step";
    }
    return true;
}

void Machine::set_breakpoint(uint16_t addr, bool enabled)
{
    bp_exec[addr] = enabled ? 1 : 0;
}

void Machine::clear_breakpoints(void)
{
    memset(bp_exec, 0, sizeof(bp_exec));
}

std::vector<uint16_t> Machine::breakpoints(void) const
{
    std::vector<uint16_t> out;
    for (int a = 0; a < 65536; a++)
        if (bp_exec[a]) out.push_back((uint16_t)a);
    return out;
}

uint8_t Machine::peek(uint16_t addr)
{
    return cpu ? cpu->peek(addr) : bus.mem_peek(addr);
}

void Machine::poke(uint16_t addr, uint8_t value)
{
    int saved_wait = bus.wait;
    bool saved_watch = bus.any_mem_watch;
    bus.any_mem_watch = false;   // debugger writes do not trigger watchpoints
    if (cpu) cpu->poke_local(addr, value);
    bus.mem_write(addr, value);
    bus.any_mem_watch = saved_watch;
    bus.wait = saved_wait;
}

static uint8_t disasm_peek(void* user, uint16_t addr)
{
    return ((Machine*)user)->peek(addr);
}

DisasmResult Machine::disassemble(uint16_t addr)
{
    int isa = cpu ? cpu->cpu_isa() : CpuCard::ISA_Z80;
    if (isa != CpuCard::ISA_Z80) return i8080_disasm(addr, disasm_peek, this, isa == CpuCard::ISA_8085);
    return z80_disasm(addr, disasm_peek, this);
}

struct IhexLoad {
    Machine* m;
    int count;
    uint16_t lo, hi;
};

static void ihex_poke(void* user, uint32_t addr, uint8_t value)
{
    IhexLoad* l = (IhexLoad*)user;
    uint16_t a = (uint16_t)addr;
    l->m->poke(a, value);
    if (!l->count || a < l->lo) l->lo = a;
    if (!l->count || a > l->hi) l->hi = a;
    l->count++;
}

bool Machine::load_ihex_text(const std::string& text, std::string& err, int* bytes, uint16_t* lo, uint16_t* hi)
{
    IhexLoad l = { this, 0, 0, 0 };
    bool ok = ihex_parse(text, ihex_poke, &l, err);
    if (bytes) *bytes = l.count;
    if (lo) *lo = l.lo;
    if (hi) *hi = l.hi;
    return ok;
}

bool Machine::load_binary(const std::string& path, uint16_t addr, std::string& err, int* bytes)
{
    std::vector<uint8_t> data;
    if (!file_read_all(data_path(path), data)) {
        err = "cannot read " + data_path(path);
        return false;
    }
    if (data.size() > 65536 - (size_t)addr) {
        err = "file does not fit into memory";
        return false;
    }
    for (size_t i = 0; i < data.size(); i++) poke((uint16_t)(addr + i), data[i]);
    if (bytes) *bytes = (int)data.size();
    return true;
}

bool Machine::type_text(const std::string& utf8)
{
    KeyboardDevice* k = keyboard();
    if (!k) return false;
    size_t pos = 0;
    while (pos < utf8.size()) {
        int32_t cp = utf8_next(utf8, pos);
        if (cp >= 0) k->type_char((uint32_t)cp);
    }
    return true;
}

bool Machine::host_key(int key, int mods)
{
    KeyboardDevice* k = keyboard();
    return k ? k->host_key(key, mods) : false;
}

bool Machine::set_keyboard(const std::string& preset, std::string& err)
{
    if (!cpu) { err = "no CPU card"; return false; }
    KeyboardDevice* k = keyboard_from_preset(preset, bus.cpu_hz, err);
    if (!k) return false;
    cpu->set_keyboard(k);
    if (cpu->keyboard() != k) { err = "this CPU card has no keyboard connector"; return false; }
    LOGI("keyboard: %s", k->type_name());
    return true;
}

void Machine::flush(void)
{
    for (auto& c : cards) c->flush();
}

void Machine::power_down(void)
{
    KeyboardDevice* k = keyboard();
    double s = k ? k->power_fail_warning() : 0;
    if (s > 0 && cpu) {   // the supply warns the program, which saves what it needs
        k->power_fail();
        std::vector<uint8_t> saved(bp_exec, bp_exec + sizeof(bp_exec));   // no breakpoint stops it
        memset(bp_exec, 0, sizeof(bp_exec));
        run_seconds(s);
        memcpy(bp_exec, saved.data(), sizeof(bp_exec));
    }
    flush();
}

DisplayDevice* Machine::find_display(const std::string& n)
{
    if (n.empty()) return display;
    for (auto& c : cards)
        if (c->name == n || c->type == n)
            if (DisplayDevice* d = dynamic_cast<DisplayDevice*>(c.get())) return d;
    return nullptr;
}

// The entry of `card` in the machine description (cards are told apart by the slot).
static nlohmann::json* card_config(nlohmann::json& cfg, const Card* card)
{
    if (!cfg.contains("cards") || !cfg["cards"].is_array()) return nullptr;
    for (nlohmann::json& cc : cfg["cards"]) {
        long long slot;
        std::string e;
        if (cc.is_object() && cfg_number(cc, "slot", slot, 0, e) && slot == card->slot) return &cc;
    }
    return nullptr;
}

bool Machine::set_display_ink(DisplayDevice* d, const std::string& value, std::string& err)
{
    Card* c = dynamic_cast<Card*>(d);
    const char* key = d ? d->ink_param() : nullptr;
    if (!c || !key) { err = "the display has no ink colour"; return false; }
    return set_live_param(c, key, value, err);
}

nlohmann::json* Machine::card_config(Card* c)
{
    return c ? ::card_config(config, c) : nullptr;
}

bool Machine::move_card(int from, int to)
{
    Card* a = nullptr;
    Card* b = nullptr;
    for (auto& c : cards) {
        if (c->slot == from) a = c.get();
        if (c->slot == to) b = c.get();
    }
    if (!a || from == to) return false;
    std::vector<std::pair<Card*, int>> v = { { a, to } };
    if (b) v.push_back({ b, from });
    set_card_slots(v);
    return true;
}

void Machine::set_card_slots(const std::vector<std::pair<Card*, int>>& to)
{
    std::vector<nlohmann::json*> entries;   // found by the old slots before any changes
    for (const auto& p : to) entries.push_back(card_config(p.first));
    for (size_t i = 0; i < to.size(); i++) {
        if (entries[i]) (*entries[i])["slot"] = to[i].second;
        to[i].first->slot = to[i].second;
    }
    std::stable_sort(bus.cards.begin(), bus.cards.end(), [](Card* x, Card* y) { return x->slot < y->slot; });
    if (config.contains("cards") && config["cards"].is_array()) {   // the description in slot order
        std::vector<nlohmann::json> v(config["cards"].begin(), config["cards"].end());
        auto slot_of = [](const nlohmann::json& c) {
            return c.is_object() && c.contains("slot") && c["slot"].is_number_integer() ? c["slot"].get<int>() : 0;
        };
        std::stable_sort(v.begin(), v.end(), [&](const nlohmann::json& x, const nlohmann::json& y) {
            return slot_of(x) < slot_of(y);
        });
        config["cards"] = v;
    }
}

bool Machine::live_param(Card* c, const char* path)
{
    DisplayDevice* d = dynamic_cast<DisplayDevice*>(c);
    if (d && d->ink_param() && !strcmp(path, d->ink_param())) return true;
    return c && c->live_param(path);
}

bool Machine::set_live_param(Card* c, const char* path, const nlohmann::json& v, std::string& err)
{
    if (!c) { err = "no card"; return false; }
    DisplayDevice* d = dynamic_cast<DisplayDevice*>(c);
    bool ok;
    if (d && d->ink_param() && !strcmp(path, d->ink_param())) {
        ok = v.is_string() && d->set_ink(v.get<std::string>(), err);
        if (!v.is_string()) err = std::string(path) + " must be a text";
    } else {
        ok = c->set_live_param(path, v, err);
    }
    if (!ok) return false;
    nlohmann::json* cc = ::card_config(config, c);
    const CardInfo* ci = card_info(c->type);
    for (const CardParam* p = ci ? ci->params : nullptr; cc && p && p->path; p++)
        if (!strcmp(p->path, path)) card_set_param(*cc, ci, p, v);
    return true;
}

Card* Machine::find_card(const std::string& n)
{
    for (auto& c : cards)
        if (c->name == n) return c.get();
    return nullptr;
}

// ---------------------------------------------------------------- save state

static const char STATE_MAGIC[8] = { 'S', 'A', 'P', 'I', 'S', 'T', 'A', 'T' };
static const uint32_t STATE_VERSION = 1;

static bool state_head(StateIO& io, std::string& cfg_text, std::string& file, std::string& err)
{
    char magic[8];
    memcpy(magic, STATE_MAGIC, 8);
    io.raw(magic, 8);
    uint32_t ver = STATE_VERSION;
    io.v(ver);
    io.v(cfg_text);
    io.v(file);
    if (!io.ok()) { err = io.error; return false; }
    if (io.loading && memcmp(magic, STATE_MAGIC, 8) != 0) { err = "not a SAPIemu state file"; return false; }
    if (io.loading && ver != STATE_VERSION) {
        err = "state version " + std::to_string(ver) + ", this emulator reads " + std::to_string(STATE_VERSION);
        return false;
    }
    return true;
}

bool Machine::state_header(const std::vector<uint8_t>& in, json& cfg, std::string& file, std::string& err)
{
    StateIO io(true);
    io.data = in;
    std::string text;
    if (!state_head(io, text, file, err)) return false;
    cfg = json::parse(text, nullptr, false);
    if (cfg.is_discarded()) { err = "the machine description in the state is damaged"; return false; }
    return true;
}

// Everything after the header, in the order of the slots.
static void state_body(StateIO& io, Machine& m)
{
    if (io.begin("bus")) {
        m.bus.state(io);
        io.end();
    }
    for (Card* c : m.bus.cards) {
        std::string nm = "card X" + std::to_string(c->slot) + " " + c->type;
        if (!io.begin(nm.c_str())) return;
        c->state(io);
        io.end();
    }
    if (io.begin("keyboard")) {
        KeyboardDevice* k = m.keyboard();
        std::string type = k ? k->type_name() : "";
        io.v(type);
        if (io.loading && io.ok() && type != (k ? k->type_name() : "")) {
            std::string err;   // plugged in while running: the same keyboard again
            if (type.empty() || !m.set_keyboard(type, err)) io.fail("keyboard " + type + ": " + err);
            k = m.keyboard();
        }
        if (k && io.ok()) k->state(io);
        io.end();
    }
    for (size_t i = 0; i < m.serials.size(); i++) {
        std::string nm = "serial " + std::to_string(i);
        if (!io.begin(nm.c_str())) return;
        m.serials[i]->state(io);
        io.end();
    }
    if (m.plant) {
        if (!io.begin("plant")) return;
        m.plant->state(io);
        io.end();
    }
}

bool Machine::save_state(std::vector<uint8_t>& out, const std::string& file, std::string& err)
{
    StateIO io(false);
    std::string text = config.dump(), f = file;
    if (!state_head(io, text, f, err)) return false;
    state_body(io, *this);
    if (!io.ok()) { err = io.error; return false; }
    out.swap(io.data);
    return true;
}

bool Machine::load_state(const std::vector<uint8_t>& in, std::string& err, bool rebuild)
{
    json cfg;
    std::string file;
    if (!state_header(in, cfg, file, err)) return false;
    if (cfg != config) {
        if (!rebuild) { err = "the state belongs to another machine description"; return false; }
        // built again from the description of the state (like EmuHost::load_config)
        stop_bridges();
        flush();
        bus.cards.clear();
        cards.clear();
        bus.plant = nullptr;
        plant.reset();
        cpu = nullptr;
        display = nullptr;
        displays.clear();
        serials.clear();
        if (!load_json(cfg, err)) return false;
    }
    StateIO io(true);
    io.data = in;
    std::string t, f;
    if (!state_head(io, t, f, err)) return false;
    state_body(io, *this);
    if (io.ok() && io.pos != io.data.size()) io.fail("data after the end of the state");
    if (!io.ok()) {
        err = io.error;
        return false;
    }
    clear_break();
    if (cpu) {
        cpu->step_mode = false;
        cpu->step_skip = 0;
        cpu->stop_hit = false;
    }
    bus.break_request = false;
    bus.audio_restart();
    return true;
}
