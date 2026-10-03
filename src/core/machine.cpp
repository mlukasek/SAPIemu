#include "core/machine.h"
#include "core/card.h"
#include "core/charset.h"
#include "core/ihex.h"
#include "core/log.h"
#include "core/util.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>

using nlohmann::json;

Machine::Machine()
{
    memset(bp_exec, 0, sizeof(bp_exec));
}

Machine::~Machine()
{
    flush();
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

    int slots = 8;
    bool slot1_cpu_only = true;
    if (cfg.contains("bus")) {
        const json& b = cfg["bus"];
        long long n;
        if (!cfg_number(b, "slots", n, 8, err)) return false;
        slots = (int)n;
        std::string jp3;
        if (!cfg_string(b, "jp3", jp3, "2-3", err)) return false;
        // JP3 1-2: address buffers of slot 1 disabled (PKT-1 / SAPI-1.5 style),
        // the CPU card may then sit in any slot.
        slot1_cpu_only = (jp3 != "1-2");
    }

    if (!cfg.contains("cards") || !cfg["cards"].is_array()) {
        err = "machine has no 'cards' array";
        return false;
    }

    std::vector<bool> used(slots + 1, false);
    for (const json& cc : cfg["cards"]) {
        std::string type = cc.value("type", std::string());
        Card* card = card_create(type);
        if (!card) {
            err = "unknown card type '" + type + "'";
            return false;
        }
        cards.emplace_back(card);
        card->bus = &bus;
        card->name = cc.value("name", type);
        long long slot;
        if (!cfg_number(cc, "slot", slot, 0, err)) return false;
        if (slot < 1 || slot > slots) {
            err = card->name + ": slot must be 1.." + std::to_string(slots);
            return false;
        }
        if (used[slot]) {
            err = card->name + ": slot " + std::to_string(slot) + " is already occupied";
            return false;
        }
        used[slot] = true;
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
                err = card->name + ": CPU card must be in slot 1 (ARB-1C JP3 = 2-3)";
                return false;
            }
            cpu = c;
        }
        DisplayDevice* d = dynamic_cast<DisplayDevice*>(card);
        if (d && !display) display = d;
        for (int i = 0; SerialChannel* s = card->serial_channel(i); i++) serials.push_back(s);
    }
    if (!cpu) {
        err = "machine has no CPU card";
        return false;
    }

    std::vector<Card*> sorted;
    for (auto& c : cards) sorted.push_back(c.get());
    std::stable_sort(sorted.begin(), sorted.end(), [](Card* a, Card* b) { return a->slot < b->slot; });
    bus.cards = sorted;
    bus.cpu_hz = cpu->clock_hz();
    cpu->bp_exec = bp_exec;

    LOGI("machine '%s' loaded: %d cards, CPU %.3f MHz", name.c_str(), (int)cards.size(), bus.cpu_hz / 1e6);
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
    }
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
    return z80_disasm(addr, disasm_peek, this);
}

static void ihex_poke(void* user, uint32_t addr, uint8_t value)
{
    void** ctx = (void**)user;
    ((Machine*)ctx[0])->poke((uint16_t)addr, value);
    (*(int*)ctx[1])++;
}

bool Machine::load_ihex_text(const std::string& text, std::string& err, int* bytes)
{
    int count = 0;
    void* ctx[2] = { this, &count };
    bool ok = ihex_parse(text, ihex_poke, ctx, err);
    if (bytes) *bytes = count;
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
    std::string codes = sapi_codes_from_utf8(utf8);
    for (char c : codes) k->push_code((uint8_t)c);
    return true;
}

bool Machine::type_code(uint8_t code)
{
    KeyboardDevice* k = keyboard();
    if (!k) return false;
    k->push_code(code);
    return true;
}

void Machine::flush(void)
{
    for (auto& c : cards) c->flush();
}

Card* Machine::find_card(const std::string& n)
{
    for (auto& c : cards)
        if (c->name == n) return c.get();
    return nullptr;
}
