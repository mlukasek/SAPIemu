#include "mcp/mcp_tools.h"
#include "cards/catalog.h"
#include "cards/dsm1.h"
#include "cards/hrk1.h"
#include "cards/ide1.h"
#include "chips/kzd1.h"
#include "chips/minigraf.h"
#include "core/tapegen.h"
#include "chips/printer.h"
#include "core/charset.h"
#include "core/headless.h"
#include "core/host.h"
#include "core/log.h"
#include "core/machine_cfg.h"
#include "core/machine_map.h"
#include "core/png.h"
#include "core/util.h"
#include "core/version.h"
#include "core/wav.h"
#include "input/keyboards.h"
#include "plant/furnaces.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <filesystem>

using nlohmann::json;

namespace {

// ---------------------------------------------------------------- helpers

struct ToolError {
    std::string msg;
};

json text_result(const std::string& text)
{
    return json{ { "content", json::array({ json{ { "type", "text" }, { "text", text } } }) }, { "isError", false } };
}

json json_result(const json& j)
{
    return text_result(j.dump(2));
}

json error_result(const std::string& msg)
{
    return json{ { "content", json::array({ json{ { "type", "text" }, { "text", msg } } }) }, { "isError", true } };
}

std::string base64(const std::vector<uint8_t>& d)
{
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < d.size(); i += 3) {
        uint32_t v = (d[i] << 16) | (d[i + 1] << 8) | d[i + 2];
        out += t[v >> 18]; out += t[(v >> 12) & 63]; out += t[(v >> 6) & 63]; out += t[v & 63];
    }
    if (i + 1 == d.size()) {
        uint32_t v = d[i] << 16;
        out += t[v >> 18]; out += t[(v >> 12) & 63]; out += "==";
    } else if (i + 2 == d.size()) {
        uint32_t v = (d[i] << 16) | (d[i + 1] << 8);
        out += t[v >> 18]; out += t[(v >> 12) & 63]; out += t[(v >> 6) & 63]; out += '=';
    }
    return out;
}

std::string h4(unsigned v) { char b[8]; snprintf(b, sizeof(b), "%04X", v & 0xFFFF); return b; }
std::string h2(unsigned v) { char b[8]; snprintf(b, sizeof(b), "%02X", v & 0xFF); return b; }

// Numbers: JSON numbers are decimal, strings are hexadecimal ("F000", "0xF000", "F000h", "$F000").
long long arg_num(const json& args, const char* key, long long def, bool required = false)
{
    if (!args.contains(key) || args[key].is_null()) {
        if (required) throw ToolError{ std::string("missing argument '") + key + "'" };
        return def;
    }
    const json& v = args[key];
    if (v.is_number_integer()) return v.get<long long>();
    if (v.is_number()) return (long long)v.get<double>();
    if (v.is_string()) {
        std::string s = v.get<std::string>();
        long long out;
        bool has_prefix = s.find_first_of("xX$hH") != std::string::npos;
        if (parse_number(has_prefix ? s : s + "h", out)) return out;
    }
    throw ToolError{ std::string("invalid number for '") + key + "' (use a JSON number or a hex string)" };
}

std::string arg_str(const json& args, const char* key, const char* def, bool required = false)
{
    if (!args.contains(key) || args[key].is_null()) {
        if (required) throw ToolError{ std::string("missing argument '") + key + "'" };
        return def ? def : "";
    }
    if (!args[key].is_string()) throw ToolError{ std::string("argument '") + key + "' must be a string" };
    return args[key].get<std::string>();
}

bool arg_bool(const json& args, const char* key, bool def)
{
    if (!args.contains(key) || args[key].is_null()) return def;
    if (args[key].is_boolean()) return args[key].get<bool>();
    throw ToolError{ std::string("argument '") + key + "' must be a boolean" };
}

Machine& need_machine(EmuHost& h)
{
    if (!h.machine()) throw ToolError{ "the machine is switched off (tool power) or not loaded" };
    return *h.machine();
}

std::string flags_str(uint8_t f)
{
    const char* names = "SZYHXPNC";
    std::string s;
    for (int i = 7; i >= 0; i--) s += (f & (1 << i)) ? names[7 - i] : '-';
    return s;
}

json regs_json(Machine& m)
{
    CpuRegs r;
    m.cpu->get_regs(r);
    DisasmResult d = m.disassemble(r.pc);
    return json{
        { "pc", h4(r.pc) }, { "sp", h4(r.sp) }, { "af", h4(r.af) }, { "bc", h4(r.bc) },
        { "de", h4(r.de) }, { "hl", h4(r.hl) }, { "ix", h4(r.ix) }, { "iy", h4(r.iy) },
        { "af_", h4(r.af2) }, { "bc_", h4(r.bc2) }, { "de_", h4(r.de2) }, { "hl_", h4(r.hl2) },
        { "i", h2(r.i) }, { "r", h2(r.r) }, { "im", r.im }, { "iff1", r.iff1 }, { "iff2", r.iff2 },
        { "flags", flags_str((uint8_t)r.af) }, { "halted", r.halted },
        { "next_instruction", d.text },
    };
}

json state_json(EmuHost& h)
{
    Machine& m = need_machine(h);
    json j;
    j["state"] = h.paused ? "paused" : "running";
    if (!h.status.empty()) j["stop_reason"] = h.status;
    j["cycles"] = m.bus.now;
    j["emulated_seconds"] = m.bus.seconds();
    j["registers"] = regs_json(m);
    return j;
}

// Stops after synchronous runs are reported to the caller and leave the machine paused.
json run_result(EmuHost& h, const char* what)
{
    Machine& m = need_machine(h);
    json j;
    if (h.paused && !h.status.empty()) j["previous_stop"] = h.status;
    if (m.stopped_on_breakpoint()) {
        h.paused = true;
        h.status = m.stop_text();
        j["stopped"] = m.stop_text();
        m.clear_break();
    } else {
        j["stopped"] = what;
    }
    j["state"] = h.paused ? "paused" : "running (real time between tool calls)";
    j["cycles"] = m.bus.now;
    j["registers"] = regs_json(m);
    return j;
}

std::string screen_dump(Machine& m)
{
    if (!m.display) throw ToolError{ "machine has no display card" };
    return screen_text(m);
}

SerialChannel* serial_arg(Machine& m, const json& args)
{
    long long ch = arg_num(args, "channel", 0);
    if (ch < 0 || ch >= (long long)m.serials.size())
        throw ToolError{ "no serial channel " + std::to_string(ch) + " (machine has " +
                         std::to_string(m.serials.size()) + ")" };
    return m.serials[(size_t)ch];
}

// ---------------------------------------------------------------- tools

typedef json (*ToolFn)(EmuHost& h, const json& args);

struct Tool {
    const char* name;
    const char* description;
    const char* schema;   // JSON schema of the arguments
    ToolFn fn;
};

json t_machine_info(EmuHost& h, const json&)
{
    Machine& m = need_machine(h);
    json cards = json::array();
    for (auto& c : m.cards) {
        json d;
        c->describe(d);
        cards.push_back(json{ { "slot", c->slot }, { "type", c->type }, { "name", c->name }, { "details", d } });
    }
    json serials = json::array();
    for (size_t i = 0; i < m.serials.size(); i++)
    {
        json br = json::array();
        for (auto& b : m.bridges)
            if (b->channel == m.serials[i]) br.push_back(b->describe());
        serials.push_back(json{ { "channel", i }, { "name", m.serials[i]->name },
                                { "terminal_connected", (bool)m.serials[i]->connected }, { "cts", (bool)m.serials[i]->cts },
                                { "host_connections", br } });
    }
    json j = state_json(h);
    j["emulator"] = std::string("SAPIemu ") + sapiemu_version();
    j["machine"] = m.name;
    j["machine_file"] = h.machine_file();
    j["cpu_hz"] = m.bus.cpu_hz;
    j["cpu_card_hz"] = m.cpu ? m.cpu->cpu_display_hz() : m.bus.cpu_hz;   // clock of the CPU (JPR-1V TURBO)
    j["max_speed"] = h.max_speed;
    j["speed_percent"] = h.max_speed ? 0 : h.pace_percent;
    j["speed_hz"] = h.target_hz();
    j["cards"] = cards;
    j["serial_channels"] = serials;
    j["keyboard"] = m.keyboard() ? m.keyboard()->type_name() : "none";
    if (m.keyboard() && !m.keyboard()->status().empty()) j["keyboard_status"] = m.keyboard()->status();
    return json_result(j);
}

json t_list_machines(EmuHost&, const json&)
{
    json list = json::array();
    for (const std::string& f : list_machine_files()) list.push_back(f);
    return json_result(json{ { "machines", list } });
}

json param_json(const CardParam* p)
{
    static const char* const kinds[] = { "choice", "int", "hex", "bool", "file", "dir", "text", "section", "keyboard" };
    json j = { { "path", p->path }, { "kind", kinds[p->kind] }, { "label", p->en }, { "label_cs", p->cs },
               { "default", param_default(p) } };
    if (p->kind == PK_INT || p->kind == PK_HEX) {
        j["min"] = p->min;
        j["max"] = p->max;
    }
    if (p->options && p->options[0].value) {
        json o = json::array();
        for (int i = 0; p->options[i].value; i++)
            o.push_back({ { "value", json::parse(p->options[i].value) }, { "label", p->options[i].en } });
        j[p->kind == PK_CHOICE ? "options" : "suggestions"] = o;
    }
    if (p->kind == PK_KEYBOARD) {
        json o = json::array();
        for (const KeyboardPreset& k : keyboard_presets())
            o.push_back({ { "value", json::parse(k.config) }, { "label", k.name } });
        j["options"] = o;
    }
    return j;
}

json t_card_catalog(EmuHost&, const json& a)
{
    std::string only = arg_str(a, "type", "", false);
    json cards = json::array();
    for (const CardInfo* ci : card_catalog()) {
        if (!only.empty() && only != ci->type) continue;
        json c = { { "type", ci->type }, { "title", ci->en }, { "title_cs", ci->cs },
                   { "class", card_class_name(ci->cls, false) }, { "slots", ci->width }, { "emulated", ci->emulated } };
        json params = json::array();
        for (const CardParam* p = ci->params; p && p->path; p++) params.push_back(param_json(p));
        c["params"] = params;
        cards.push_back(c);
    }
    json buses = json::array();
    for (const BusInfo* b : bus_catalog())
        buses.push_back({ { "type", b->type }, { "title", b->en }, { "slots", b->slots }, { "jp3", b->has_jp3 } });
    return json_result(json{ { "cards", cards }, { "buses", buses },
                             { "notes", "Slot X1 is the CPU card (unless ARB-1C JP3 = 1-2). Parameter paths with "
                                        "dots are nested objects, a numeric part is an array index. A section "
                                        "parameter is an optional object." } });
}

json problems_json(const json& cfg, bool& runnable)
{
    json pr = json::array();
    runnable = true;
    for (const CfgProblem& p : machine_all_problems(cfg)) {
        pr.push_back({ { "slot", p.slot }, { "error", p.error }, { "text", p.en } });
        if (p.error) runnable = false;
    }
    return pr;
}

json t_machine_map(EmuHost& h, const json& a)
{
    std::string path = arg_str(a, "path", "", false);
    json cfg;
    if (a.contains("config") && a["config"].is_object()) {
        cfg = a["config"];
    } else if (!path.empty()) {
        std::string text;
        if (!file_read_text(data_path(path), text)) return error_result("cannot read " + path);
        cfg = json::parse(text, nullptr, false, true);
        if (cfg.is_discarded()) return error_result("syntax error in " + path);
    } else {
        cfg = need_machine(h).config;
    }
    MachineMap m;
    machine_map_build(cfg, m);
    auto ranges = [&](const std::vector<MapRange>& rs, int digits) {
        json out = json::array();
        for (const MapRange& r : rs) {
            char lo[8], hi[8];
            snprintf(lo, sizeof(lo), "%0*Xh", digits, r.lo);
            snprintf(hi, sizeof(hi), "%0*Xh", digits, r.hi);
            json users = json::array();
            for (const MapUser& u : r.users) {
                std::string acc = std::string((u.flags & DEC_READ) ? "R" : "") + ((u.flags & DEC_WRITE) ? "W" : "");
                json ju = { { "card", m.cards[(size_t)u.card].name }, { "slot", m.cards[(size_t)u.card].slot },
                            { "function", u.what }, { "access", acc } };
                if (u.flags & DEC_ONCARD) ju["on_cpu_card"] = true;
                users.push_back(ju);
            }
            json jr = { { "from", lo }, { "to", hi }, { "users", users } };
            if (r.conflict) jr["conflict"] = true;
            if (r.shared_write) jr["shared_write"] = true;
            out.push_back(jr);
        }
        return out;
    };
    json j;
    j["io"] = ranges(m.io, 2);
    if (m.map_matters) {
        json mm = json::object();
        for (int s = 0; s < 4; s++) {
            char key[24];
            snprintf(key, sizeof(key), "MAP1=%d MAP2=%d", s & 1, (s >> 1) & 1);
            mm[key] = ranges(m.mem[s], 4);
        }
        j["memory_by_map"] = mm;
    } else {
        j["memory"] = ranges(m.mem[0], 4);
    }
    json unknown = json::array();
    for (const MapCard& c : m.cards)
        if (!c.known && !c.declared) unknown.push_back(c.name);
    if (!unknown.empty()) j["decoding_unknown"] = unknown;
    json pr = json::array();
    for (const CfgProblem& p : machine_map_problems(m)) pr.push_back({ { "error", p.error }, { "text", p.en } });
    j["conflicts"] = pr;
    return json_result(j);
}

json t_get_machine_config(EmuHost& h, const json& a)
{
    std::string path = arg_str(a, "path", "", false);
    json cfg;
    if (path.empty()) {
        cfg = need_machine(h).config;
        path = h.machine_file();
    } else {
        std::string text;
        if (!file_read_text(data_path(path), text)) return error_result("cannot read " + path);
        cfg = json::parse(text, nullptr, false, true);
        if (cfg.is_discarded()) return error_result("syntax error in " + path);
    }
    bool runnable;
    json j = { { "file", path }, { "config", cfg }, { "problems", problems_json(cfg, runnable) },
               { "runnable", runnable } };
    if (arg_bool(a, "jsonc", false)) j["jsonc"] = machine_cfg_to_jsonc(cfg);
    return json_result(j);
}

json t_set_machine_config(EmuHost& h, const json& a)
{
    if (!a.contains("config") || !a["config"].is_object()) throw ToolError{ "'config' must be a machine description object" };
    const json& cfg = a["config"];
    bool runnable;
    json pr = problems_json(cfg, runnable);
    std::string save = arg_str(a, "save_as", "", false);
    bool start = arg_bool(a, "start", true);
    json j = { { "problems", pr }, { "runnable", runnable } };
    if (!save.empty()) {
        size_t dot = save.find_last_of('.');
        std::string e = dot == std::string::npos ? std::string() : save.substr(dot);
        if (e != ".sapi" && e != ".jsonc" && e != ".json") throw ToolError{ "save_as must end with .sapi" };
        std::string full = data_path(save);
        std::string text = machine_cfg_to_jsonc(cfg);
        make_dirs(path_dir(full));
        if (!file_write_text(full, text)) return error_result("cannot write " + full);
        j["saved"] = data_relative(full);
    }
    if (start) {
        if (!runnable) {
            j["started"] = false;
            j["error"] = "the machine cannot run (see problems)";
            return json_result(j);
        }
        std::string err;
        if (!h.load_config(cfg, save.empty() ? std::string() : data_relative(data_path(save)), err)) {
            j["started"] = false;
            j["error"] = err;
            return json_result(j);
        }
        j["started"] = true;
    }
    return json_result(j);
}

json t_load_machine(EmuHost& h, const json& a)
{
    std::string path = arg_str(a, "path", nullptr, true);
    std::string err;
    if (!h.load(path, err)) return error_result(err);
    return t_machine_info(h, a);
}

json t_power(EmuHost& h, const json& a)
{
    std::string state = arg_str(a, "state", nullptr, true);
    if (state == "off") {
        h.power_off();
        return text_result("switched off");
    }
    if (state != "on" && state != "cycle") throw ToolError{ "state must be on, off or cycle" };
    if (state == "cycle") h.power_off();
    std::string err;
    if (!h.machine() && !h.power_on(err)) return error_result(err);
    return t_machine_info(h, a);
}

// The machine is switched off first (tapes, cassettes and RAM disks saved), then the program ends; the
// answer goes out before the MCP server stops.
json t_quit(EmuHost& h, const json&)
{
    h.power_off();
    h.quit_requested = true;
    return text_result("switched off, the emulator ends");
}

json t_reset(EmuHost& h, const json&)
{
    need_machine(h).reset();
    return json_result(state_json(h));
}

json t_pause(EmuHost& h, const json&)
{
    need_machine(h);
    h.paused = true;
    return json_result(state_json(h));
}

json t_resume(EmuHost& h, const json& a)
{
    need_machine(h);
    if (a.contains("max_speed")) h.max_speed = arg_bool(a, "max_speed", false);
    if (a.contains("turbo")) h.max_speed = arg_bool(a, "turbo", false);   // older name of max_speed
    if (a.contains("speed_percent")) {
        int p = a["speed_percent"].is_number() ? a["speed_percent"].get<int>() : 0;
        if (p < 10 || p > 1000) throw ToolError{ "speed_percent must be 10 .. 1000 (100 = real time)" };
        h.pace_percent = p;
        h.max_speed = false;
    }
    h.paused = false;
    h.status.clear();
    return json_result(json{ { "state", "running" }, { "max_speed", h.max_speed },
                             { "speed_percent", h.max_speed ? 0 : h.pace_percent }, { "speed_hz", h.target_hz() } });
}

json t_step(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    long long count = arg_num(a, "count", 1);
    bool over = arg_bool(a, "over", false);
    if (count < 1 || count > 100000) throw ToolError{ "count must be 1..100000" };
    h.paused = true;
    json trace = json::array();
    for (long long i = 0; i < count; i++) {
        CpuRegs r;
        m.cpu->get_regs(r);
        if (count <= 50) trace.push_back(h4(r.pc) + "  " + m.disassemble(r.pc).text);
        if (over) m.step_over(); else m.step();
        if (m.stop_reason() == STOP_BREAKPOINT || m.stop_reason() == STOP_WATCHPOINT) break;
    }
    json j = run_result(h, "step");
    if (!trace.empty()) j["executed"] = trace;
    return json_result(j);
}

json t_run_for(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    long long ms = arg_num(a, "ms", 0, true);
    if (ms < 0 || ms > 120000) throw ToolError{ "ms must be 0..120000" };
    m.run_seconds((double)ms / 1000.0);
    return json_result(run_result(h, "time elapsed"));
}

json t_run_until(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    long long timeout = arg_num(a, "timeout_ms", 5000);
    if (timeout < 1 || timeout > 120000) throw ToolError{ "timeout_ms must be 1..120000" };
    bool has_addr = a.contains("address") && !a["address"].is_null();
    std::string text = arg_str(a, "screen_text", "");
    std::string serial = arg_str(a, "serial_text", "");
    if (!has_addr && text.empty() && serial.empty())
        throw ToolError{ "give at least one of address, screen_text, serial_text" };

    uint16_t addr = has_addr ? (uint16_t)arg_num(a, "address", 0) : 0;
    bool had_bp = has_addr && m.has_breakpoint(addr);
    if (has_addr) m.set_breakpoint(addr, true);
    SerialChannel* ch = serial.empty() ? nullptr : serial_arg(m, a);
    uint64_t serial_pos = ch ? ch->host_end() : 0;
    std::string serial_seen;

    const double slice = 0.005;
    double t = 0;
    const char* why = "timeout";
    bool ok = false;
    for (; t < timeout / 1000.0; t += slice) {
        m.run_seconds(slice);
        if (ch) ch->host_read(serial_pos, serial_seen, 1 << 20);
        if (has_addr && m.stopped_on_breakpoint() && m.cpu->instr_pc == addr) { why = "address reached"; ok = true; m.clear_break(); break; }
        if (m.stopped_on_breakpoint()) { why = "breakpoint"; break; }
        if (!text.empty() && screen_contains(m, text)) { why = "screen text found"; ok = true; break; }
        if (ch && serial_seen.find(serial) != std::string::npos) { why = "serial text found"; ok = true; break; }
    }
    if (has_addr && !had_bp) m.set_breakpoint(addr, false);
    if (has_addr && ok && strcmp(why, "address reached") == 0) h.paused = true;   // stay at the address
    json j = run_result(h, why);
    j["success"] = ok;
    j["emulated_ms"] = (int)(t * 1000);
    return json_result(j);
}

json t_get_registers(EmuHost& h, const json&)
{
    return json_result(regs_json(need_machine(h)));
}

json t_set_registers(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    CpuRegs r;
    m.cpu->get_regs(r);
    struct { const char* key; uint16_t* p; } w16[] = {
        { "af", &r.af }, { "bc", &r.bc }, { "de", &r.de }, { "hl", &r.hl }, { "ix", &r.ix }, { "iy", &r.iy },
        { "sp", &r.sp }, { "pc", &r.pc }, { "af_", &r.af2 }, { "bc_", &r.bc2 }, { "de_", &r.de2 }, { "hl_", &r.hl2 },
    };
    for (auto& w : w16)
        if (a.contains(w.key)) *w.p = (uint16_t)arg_num(a, w.key, 0);
    if (a.contains("i")) r.i = (uint8_t)arg_num(a, "i", 0);
    if (a.contains("r")) r.r = (uint8_t)arg_num(a, "r", 0);
    if (a.contains("im")) r.im = (uint8_t)arg_num(a, "im", 0);
    if (a.contains("iff1")) r.iff1 = arg_bool(a, "iff1", false);
    if (a.contains("iff2")) r.iff2 = arg_bool(a, "iff2", false);
    h.paused = true;
    m.cpu->set_regs(r);
    return json_result(regs_json(m));
}

json t_read_memory(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    long long addr = arg_num(a, "address", 0, true);
    long long len = arg_num(a, "length", 256);
    if (len < 1 || len > 4096) throw ToolError{ "length must be 1..4096" };
    std::string fmt = arg_str(a, "format", "dump");
    std::string out;
    if (fmt == "hex") {
        for (long long i = 0; i < len; i++) out += h2(m.peek((uint16_t)(addr + i)));
        return text_result(out);
    }
    for (long long row = 0; row < len; row += 16) {
        uint16_t base = (uint16_t)(addr + row);
        out += h4(base) + ":";
        std::string chars;
        for (int i = 0; i < 16; i++) {
            if (row + i < len) {
                uint8_t b = m.peek((uint16_t)(base + i));
                out += " " + h2(b);
                chars += (b >= 0x20 && b != 0x7F) ? sapi_code_to_utf8(b) : ".";
            } else {
                out += "   ";
            }
        }
        out += "  " + chars + "\n";
    }
    return text_result(out);
}

json t_write_memory(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    long long addr = arg_num(a, "address", 0, true);
    std::string data = arg_str(a, "data", nullptr, true);
    std::string hex;
    for (char c : data) if (isxdigit((unsigned char)c)) hex += c;
    if (hex.size() % 2) throw ToolError{ "data must contain an even number of hex digits" };
    for (size_t i = 0; i < hex.size(); i += 2) {
        long long v;
        parse_number(hex.substr(i, 2) + "h", v);
        m.poke((uint16_t)(addr + i / 2), (uint8_t)v);
    }
    return text_result("wrote " + std::to_string(hex.size() / 2) + " bytes at " + h4((unsigned)addr));
}

json t_io_read(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    uint8_t port = (uint8_t)arg_num(a, "port", 0, true);
    uint8_t v = m.bus.io_read(port);
    return json_result(json{ { "port", h2(port) }, { "value", h2(v) } });
}

json t_io_write(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    uint8_t port = (uint8_t)arg_num(a, "port", 0, true);
    uint8_t v = (uint8_t)arg_num(a, "value", 0, true);
    m.bus.io_write(port, v);
    m.bus.break_request = false;
    return text_result("OUT " + h2(port) + "h," + h2(v) + "h");
}

json t_disassemble(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    CpuRegs r;
    m.cpu->get_regs(r);
    uint16_t addr = (uint16_t)arg_num(a, "address", r.pc);
    long long count = arg_num(a, "count", 16);
    if (count < 1 || count > 500) throw ToolError{ "count must be 1..500" };
    std::string out;
    for (long long i = 0; i < count; i++) {
        DisasmResult d = m.disassemble(addr);
        std::string bytes;
        for (int k = 0; k < d.length; k++) bytes += h2(m.peek((uint16_t)(addr + k))) + " ";
        char line[96];
        snprintf(line, sizeof(line), "%s%c %-12s %s\n", h4(addr).c_str(),
                 addr == r.pc ? '>' : (m.has_breakpoint(addr) ? '*' : ' '), bytes.c_str(), d.text.c_str());
        out += line;
        addr = (uint16_t)(addr + d.length);
    }
    return text_result(out);
}

int kind_flags(const std::string& kind, bool& io)
{
    io = false;
    if (kind == "exec") return 0;
    if (kind == "mem_read") return WATCH_READ;
    if (kind == "mem_write") return WATCH_WRITE;
    if (kind == "mem_rw") return WATCH_READ | WATCH_WRITE;
    io = true;
    if (kind == "io_in") return WATCH_READ;
    if (kind == "io_out") return WATCH_WRITE;
    if (kind == "io_rw") return WATCH_READ | WATCH_WRITE;
    throw ToolError{ "kind must be exec, mem_read, mem_write, mem_rw, io_in, io_out or io_rw" };
}

json t_set_breakpoint(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    long long addr = arg_num(a, "address", 0, true);
    std::string kind = arg_str(a, "kind", "exec");
    bool io;
    int flags = kind_flags(kind, io);
    if (kind == "exec") m.set_breakpoint((uint16_t)addr, true);
    else if (io) m.set_io_watch((uint8_t)addr, m.bus.io_watch[(uint8_t)addr] | flags);
    else m.set_mem_watch((uint16_t)addr, m.bus.mem_watch[(uint16_t)addr] | flags);
    return text_result(kind + " breakpoint set at " + (io ? h2((unsigned)addr) : h4((unsigned)addr)));
}

json t_clear_breakpoint(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    if (arg_bool(a, "all", false)) {
        m.clear_breakpoints();
        for (int i = 0; i < 65536; i++) if (m.bus.mem_watch[i]) m.set_mem_watch((uint16_t)i, 0);
        for (int i = 0; i < 256; i++) m.set_io_watch((uint8_t)i, 0);
        return text_result("all breakpoints cleared");
    }
    long long addr = arg_num(a, "address", 0, true);
    std::string kind = arg_str(a, "kind", "exec");
    bool io;
    int flags = kind_flags(kind, io);
    if (kind == "exec") m.set_breakpoint((uint16_t)addr, false);
    else if (io) m.set_io_watch((uint8_t)addr, m.bus.io_watch[(uint8_t)addr] & ~flags);
    else m.set_mem_watch((uint16_t)addr, m.bus.mem_watch[(uint16_t)addr] & ~flags);
    return text_result("cleared");
}

json t_list_breakpoints(EmuHost& h, const json&)
{
    Machine& m = need_machine(h);
    json exec = json::array(), mem = json::array(), io = json::array();
    for (uint16_t a : m.breakpoints()) exec.push_back(h4(a));
    static const char* const k_rw[4] = { "", "read", "write", "rw" };
    for (int i = 0; i < 65536; i++)
        if (m.bus.mem_watch[i]) mem.push_back(json{ { "address", h4(i) }, { "on", k_rw[m.bus.mem_watch[i] & 3] } });
    for (int i = 0; i < 256; i++)
        if (m.bus.io_watch[i]) io.push_back(json{ { "port", h2(i) }, { "on", k_rw[m.bus.io_watch[i] & 3] } });
    return json_result(json{ { "exec", exec }, { "memory", mem }, { "io", io } });
}

json t_load_hex(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    std::string content = arg_str(a, "content", "");
    std::string path = arg_str(a, "path", "");
    if (content.empty() == path.empty()) throw ToolError{ "give exactly one of 'path' or 'content'" };
    if (!path.empty() && !file_read_text(data_path(path), content))
        return error_result("cannot read " + data_path(path));
    std::string err;
    int n = 0;
    if (!m.load_ihex_text(content, err, &n)) return error_result(err);
    return text_result("loaded " + std::to_string(n) + " bytes");
}

json t_load_binary(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    std::string path = arg_str(a, "path", nullptr, true);
    long long addr = arg_num(a, "address", 0, true);
    std::string err;
    int n = 0;
    if (!m.load_binary(path, (uint16_t)addr, err, &n)) return error_result(err);
    return text_result("loaded " + std::to_string(n) + " bytes at " + h4((unsigned)addr));
}

json keys_result(EmuHost& h, const json& a, const std::string& msg)
{
    Machine& m = need_machine(h);
    long long run_ms = arg_num(a, "run_ms", 0);
    if (run_ms > 0) {
        // let the program take the keys
        m.run_seconds((double)std::min<long long>(run_ms, 60000) / 1000.0);
        json j = run_result(h, "time elapsed");
        j["keyboard"] = msg;
        j["keys_pending"] = m.keyboard()->pending();
        return json_result(j);
    }
    return text_result(msg);
}

json t_type_text(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    std::string text = arg_str(a, "text", nullptr, true);
    if (!m.keyboard()) return error_result("machine has no keyboard");
    size_t before = m.keyboard()->pending();
    m.type_text(text);
    size_t n = m.keyboard()->pending() - std::min(before, m.keyboard()->pending());
    return keys_result(h, a, std::string(m.keyboard()->type_name()) + ": queued " + std::to_string(n) + " keys");
}

const struct { const char* name; int key; } k_key_names[] = {
    { "enter", HK_ENTER }, { "backspace", HK_BACKSPACE }, { "tab", HK_TAB }, { "esc", HK_ESCAPE },
    { "up", HK_UP }, { "down", HK_DOWN }, { "left", HK_LEFT }, { "right", HK_RIGHT },
    { "home", HK_HOME }, { "end", HK_END }, { "pgup", HK_PGUP }, { "pgdn", HK_PGDN },
    { "insert", HK_INSERT }, { "delete", HK_DELETE }, { "pause", HK_PAUSE },
    { "f1", HK_F1 }, { "f2", HK_F2 }, { "f3", HK_F3 }, { "f4", HK_F4 }, { "f6", HK_F6 }, { "f7", HK_F7 },
    { "kp0", HK_KP0 }, { "kp1", HK_KP1 }, { "kp2", HK_KP2 }, { "kp3", HK_KP3 }, { "kp4", HK_KP4 },
    { "kp5", HK_KP5 }, { "kp6", HK_KP6 }, { "kp7", HK_KP7 }, { "kp8", HK_KP8 }, { "kp9", HK_KP9 },
    { "kp.", HK_KP_DOT }, { "kp_enter", HK_KP_ENTER },
};

json t_press_key(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    std::string name = arg_str(a, "key", nullptr, true);
    if (!m.keyboard()) return error_result("machine has no keyboard");
    int key = HK_NONE;
    for (const auto& k : k_key_names)
        if (name == k.name) key = k.key;
    if (key == HK_NONE) throw ToolError{ "unknown key '" + name + "'" };
    int mods = (arg_bool(a, "shift", false) ? HM_SHIFT : 0) | (arg_bool(a, "ctrl", false) ? HM_CTRL : 0);
    if (!m.host_key(key, mods))
        return error_result(std::string(m.keyboard()->type_name()) + " has no key for '" + name + "'");
    return keys_result(h, a, std::string(m.keyboard()->type_name()) + ": key " + name);
}

json t_set_keyboard(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    std::string name = arg_str(a, "name", nullptr, true);
    std::string err;
    if (!m.set_keyboard(name, err)) return error_result(err);
    return text_result("keyboard: " + name);
}

json t_screen_text(EmuHost& h, const json&)
{
    Machine& m = need_machine(h);
    return text_result(screen_dump(m));
}

json t_cpu_turbo(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    if (!m.cpu || !m.cpu->has_cpu_turbo()) return error_result("the CPU card has no TURBO switch (JPR-1V has)");
    std::string err;
    if (a.contains("on") && !m.set_live_param(m.cpu, "turbo", a.value("on", false), err)) return error_result(err);
    return text_result(json{ { "turbo", m.cpu->cpu_turbo() }, { "cpu_hz", m.cpu->cpu_display_hz() } }.dump(2));
}

json t_joystick(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    long long port = arg_num(a, "port", 0);
    if (port < 0 || port > 1) throw ToolError{ "port must be 0 (first joystick) or 1 (second joystick)" };
    static const char* names[8] = { "down", "up", "right", "left", "fire1", "fire2", "button3", "button4" };
    if (a.contains("pressed")) {
        uint8_t s = 0;
        if (!a["pressed"].is_array()) throw ToolError{ "pressed must be an array of names" };
        for (const json& p : a["pressed"]) {
            std::string n = p.is_string() ? p.get<std::string>() : std::string();
            int bit = -1;
            for (int i = 0; i < 8; i++)
                if (n == names[i]) bit = i;
            if (bit < 0) throw ToolError{ "unknown joystick input '" + n + "'" };
            s |= (uint8_t)(1 << bit);
        }
        m.bus.joystick[port] = s;
    }
    json st = json::array();
    for (int i = 0; i < 8; i++)
        if (m.bus.joystick[port] & (1 << i)) st.push_back(names[i]);
    return text_result(json{ { "port", port }, { "pressed", st } }.dump());
}

// A seven segment pattern (a..g = bits 0..6) as the digit it shows, '?' otherwise.
static std::string seg_digit(uint8_t v)
{
    static const uint8_t font[16] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
                                      0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };
    uint8_t s = v & 0x7F;
    std::string r;
    if (!s) r = " ";
    else if (s == 0x40) r = "-";
    else {
        r = "?";
        for (int i = 0; i < 16; i++)
            if (font[i] == s || (i == 7 && s == 0x27) || (i == 9 && s == 0x67)) r = std::string(1, "0123456789AbCdEF"[i]);
    }
    if (v & 0x80) r += ".";
    return r;
}

json t_diag_board(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    if (!m.cpu || !m.cpu->k3_diag()) return error_result("no diagnostic board on K3 (JPR-1S, JPR-1X parameter k3 = diag)");
    if (a.contains("press")) {   // hold a button, let the program see it, release it
        std::string p = arg_str(a, "press", nullptr, true);
        int b = p == "+" ? 0 : p == "-" ? 1 : -1;
        if (b < 0) throw ToolError{ "press must be \"+\" or \"-\"" };
        long long hold = arg_num(a, "hold_ms", 150), after = arg_num(a, "after_ms", 200);
        m.cpu->k3_button(b, true);
        m.run_seconds((double)std::min<long long>(std::max<long long>(hold, 1), 10000) / 1000.0);
        m.cpu->k3_button(b, false);
        if (after > 0) m.run_seconds((double)std::min<long long>(after, 10000) / 1000.0);
    }
    uint8_t l = m.cpu->k3_display(0), r = m.cpu->k3_display(1);
    char hx[8];
    json j;
    snprintf(hx, sizeof(hx), "%02Xh", l);
    j["left_p2_out"] = hx;
    snprintf(hx, sizeof(hx), "%02Xh", r);
    j["right_p1_out"] = hx;
    j["shows"] = seg_digit(l) + seg_digit(r);
    return text_result(j.dump());
}

json t_plant(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    FurnacePlant* p = dynamic_cast<FurnacePlant*>(m.plant.get());
    Hrk1* panel = dynamic_cast<Hrk1*>(m.keyboard());
    if (!p && !panel) return error_result("this machine has no plant (description \"plant\") and no APS-5 panel (keyboard HRK-1)");
    if (a.contains("lock")) {
        long long l = arg_num(a, "lock", 2);
        if (!panel) throw ToolError{ "lock: no HRK-1 panel" };
        if (l != 1 && l != 2) throw ToolError{ "lock must be 1 or 2" };
        panel->lock = (int)l;
    }
    if (a.contains("outage_s")) {
        if (!panel) throw ToolError{ "outage_s: no HRK-1 panel" };
        if (!a["outage_s"].is_number() || a["outage_s"].get<double>() < 0) throw ToolError{ "outage_s must be a number >= 0" };
        panel->outage(a["outage_s"].get<double>());
    }
    if (p) {
        if (a.contains("ambient_c")) p->ambient = a["ambient_c"].is_number() ? a["ambient_c"].get<double>() : p->ambient;
        if (a.contains("block_c")) p->block_c = a["block_c"].is_number() ? a["block_c"].get<double>() : p->block_c;
        if (a.contains("battery_v")) p->battery = a["battery_v"].is_number() ? a["battery_v"].get<double>() : p->battery;
        if (a.value("reset", false)) p->reset_process();
        if (a.contains("channel")) {
            long long ch = arg_num(a, "channel", 1);
            if (ch < 1 || ch > (long long)p->furnaces.size()) throw ToolError{ "channel must be 1.." + std::to_string(p->furnaces.size()) };
            Furnace& f = p->furnaces[(size_t)ch - 1];
            if (a.contains("temperature_c")) {
                if (!a["temperature_c"].is_number()) throw ToolError{ "temperature_c must be a number" };
                f.th = f.tch = f.tm = a["temperature_c"].get<double>();
            }
            if (a.contains("tc_broken")) f.tc_broken = a.value("tc_broken", false);
            if (a.contains("heater_broken")) f.heater_broken = a.value("heater_broken", false);
            if (a.contains("li_a")) f.li[0] = a.value("li_a", true);
            if (a.contains("li_b")) f.li[1] = a.value("li_b", true);
        }
    }
    json j;
    if (p) {
        m.bus.advance_all();
        p->describe(j);
    }
    if (panel) {
        j["panel"] = { { "lock", panel->lock }, { "fault_led", panel->led_fault() }, { "powered", panel->powered() },
                       { "outage_left_s", round(panel->outage_left() * 10) / 10 },
                       { "relay_ka1", panel->relay() ? "energized (K0-K2)" : "released (K0-K1)" } };
    }
    return json_result(j);
}

json t_plotter(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    Minigraf* g = nullptr;
    std::string card;
    for (auto& c : m.cards)
        if (c->minigraf()) {
            g = c->minigraf();
            card = c->name;
        }
    if (!g) return error_result("no plotter in this machine (DPP-1 parameter minigraf)");
    std::string err;
    if (a.value("new_sheet", false)) g->new_sheet();
    json j;
    if (a.contains("png")) {
        std::string p = data_path(arg_str(a, "png", nullptr, true));
        make_dirs(path_dir(p));
        if (!g->save_png(p, (int)arg_num(a, "px_per_mm", 8), a.value("crop", false), err)) return error_result(err);
        j["png"] = p;
    }
    if (a.contains("svg")) {
        std::string p = data_path(arg_str(a, "svg", nullptr, true));
        make_dirs(path_dir(p));
        if (!g->save_svg(p, a.value("crop", false), err)) return error_result(err);
        j["svg"] = p;
    }
    size_t points = 0;
    int32_t x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
    for (const std::vector<int32_t>& st : g->strokes)
        for (size_t i = 0; i + 1 < st.size(); i += 2) {
            points++;
            x0 = std::min(x0, st[i]);
            x1 = std::max(x1, st[i]);
            y0 = std::min(y0, st[i + 1]);
            y1 = std::max(y1, st[i + 1]);
        }
    const double k = 1.0 / Minigraf::STEPS_PER_MM;
    j["card"] = card;
    j["plotter"] = "ARITMA Minigraf 0507";
    j["x_mm"] = g->x() * k;
    j["y_mm"] = g->y() * k;
    j["pen"] = g->pen_down() ? "down" : "up";
    char b[8];
    snprintf(b, sizeof(b), "%02Xh", g->lines());
    j["lines"] = b;
    j["strokes"] = g->strokes.size();
    j["points"] = points;
    if (points)
        j["drawing_mm"] = json{ { "x", x0 * k }, { "y", y0 * k }, { "w", (x1 - x0) * k }, { "h", (y1 - y0) * k } };
    j["steps"] = g->steps;
    j["lost_steps"] = g->lost;
    j["steps_at_stop"] = g->stopped;
    j["steps_over_400_per_s"] = g->fast;
    return json_result(j);
}

json t_kzd1(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    Card* card = nullptr;
    for (auto& c : m.cards)
        if (c->kzd1() && (!a.contains("card") || c->name == a.value("card", std::string()) || c->type == a.value("card", std::string()))) {
            card = c.get();
            break;
        }
    if (!card) return error_result("no KZD-1 in this machine (DPP-1 parameter kzd1)");
    Kzd1* k = card->kzd1();
    std::string err;
    if (a.value("eject", false)) k->eject();
    if (a.contains("new")) {
        std::string p = data_path(arg_str(a, "new", nullptr, true));
        KzdCassette empty;
        if (!empty.save(p, err) || !k->insert(p, err)) return error_result(err);
    }
    if (a.contains("insert") && !k->insert(data_path(arg_str(a, "insert", nullptr, true)), err)) return error_result(err);
    std::string made;
    if (a.contains("program")) {   // a program becomes a new cassette (K_I and K_S as MIKOS/K writes them)
        TapeProgram p;
        p.name = arg_str(a, "name", "");
        p.address = arg_num(a, "address", -1);
        p.entry = arg_num(a, "entry", -1);
        KzdCassette c;
        std::string src = data_path(arg_str(a, "program", nullptr, true));
        if (!kzd_from_program(src, p, arg_str(a, "volume", ""), c, made, err)) return error_result(err);
        std::string name = tape_name_clean(p.name);
        k->insert(c, kzd_program_path(name.empty() ? tape_default_name(src) : name));
        k->flush();
    }
    if (a.value("flip", false)) k->flip();
    if (a.contains("tab_a")) k->set_tab(0, a.value("tab_a", true));
    if (a.contains("tab_b")) k->set_tab(1, a.value("tab_b", true));
    if (a.value("flush", false)) k->flush();
    json j;
    j["card"] = card->name;
    j["cassette"] = k->loaded() ? k->path() : std::string();
    if (k->loaded()) {
        j["side"] = k->side ? "B" : "A";
        j["position_m"] = round(k->position_m() * 1000) / 1000;
        j["tab_a"] = k->tab(0);
        j["tab_b"] = k->tab(1);
        j["transitions"] = k->transitions();
    }
    if (!made.empty()) j["program"] = made;
    j["motion"] = k->motion();
    j["speed_m_s"] = round(k->speed() * 100) / 100;
    j["overload_motor1"] = k->overload(1);
    j["overload_motor2"] = k->overload(2);
    char b[8];
    snprintf(b, sizeof(b), "%02Xh", k->controls());
    j["controls_port_a"] = b;
    return json_result(j);
}

json t_mouse(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    if (!m.bus.mouse_connected) return error_result("no mouse is plugged in (MPH-1V parameters k4 / k2, ANK-1M2 keyboard.joyport)");
    m.bus.mouse_dx += (int)arg_num(a, "dx", 0);
    m.bus.mouse_dy += (int)arg_num(a, "dy", 0);
    if (a.contains("buttons")) {
        if (!a["buttons"].is_array()) throw ToolError{ "buttons must be an array of names" };
        uint8_t b = 0;
        for (const json& n : a["buttons"]) {
            std::string s = n.is_string() ? n.get<std::string>() : std::string();
            if (s == "left") b |= MOUSE_LEFT;
            else if (s == "right") b |= MOUSE_RIGHT;
            else throw ToolError{ "unknown mouse button '" + s + "'" };
        }
        m.bus.mouse_buttons = b;
    }
    json bt = json::array();
    if (m.bus.mouse_buttons & MOUSE_LEFT) bt.push_back("left");
    if (m.bus.mouse_buttons & MOUSE_RIGHT) bt.push_back("right");
    return text_result(json{ { "pending_dx", m.bus.mouse_dx }, { "pending_dy", m.bus.mouse_dy }, { "buttons", bt } }.dump());
}

// Save state: into a named slot in memory (quick checkpoints while debugging) or a file.
json t_save_state(EmuHost& h, const json& a)
{
    std::string err, file = a.value("file", std::string()), name = a.value("name", std::string("default"));
    std::vector<uint8_t> d;
    if (!h.save_state(d, err)) return error_result(err);
    json r{ { "bytes", d.size() }, { "cycles", h.machine()->bus.now } };
    if (!file.empty()) {
        std::string path = data_path(file);
        std::string dir = path_dir(path);
        if (!dir.empty()) make_dirs(dir);
        if (!file_write_all(path, d.data(), d.size())) return error_result("cannot write " + path);
        r["file"] = path;
    } else {
        h.snapshots[name] = d;
        r["name"] = name;
    }
    return text_result(r.dump(2));
}

json t_load_state(EmuHost& h, const json& a)
{
    std::string err, file = a.value("file", std::string()), name = a.value("name", std::string("default"));
    std::vector<uint8_t> d;
    if (!file.empty()) {
        if (!file_read_all(data_path(file), d)) return error_result("cannot read " + data_path(file));
    } else {
        auto it = h.snapshots.find(name);
        if (it == h.snapshots.end()) return error_result("no state named '" + name + "' (save_state first)");
        d = it->second;
    }
    if (!h.load_state(d, err)) return error_result(err);
    Machine& m = need_machine(h);
    CpuRegs r;
    m.cpu->get_regs(r);
    char pc[8];
    snprintf(pc, sizeof(pc), "%04X", r.pc);
    json names = json::array();
    for (auto& s : h.snapshots) names.push_back(s.first);
    return text_result(json{ { "machine", m.name }, { "cycles", m.bus.now }, { "pc", pc }, { "paused", h.paused },
                             { "memory_states", names } }.dump(2));
}

json t_audio_record(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    if (!m.bus.audio_rate) return error_result("the machine has no sound card (MPH-1V)");
    bool start = a.value("start", false);
    if (start) {
        m.bus.audio_record(true);
        return text_result("recording");
    }
    m.bus.audio_record(false);
    const std::vector<int16_t>& s = m.bus.audio_rec;
    double sum = 0;
    int peak = 0;
    for (int16_t v : s) {
        sum += (double)v * v;
        if (v > peak) peak = v;
        if (-v > peak) peak = -v;
    }
    json r{ { "seconds", (double)s.size() / m.bus.audio_rate }, { "rate", m.bus.audio_rate },
            { "rms", s.empty() ? 0.0 : sqrt(sum / (double)s.size()) }, { "peak", peak } };
    if (m.bus.audio_lost) r["lost_samples"] = m.bus.audio_lost;   // dropped by the mixer: the recording has gaps
    std::string file = a.value("file", std::string());
    if (!file.empty()) {
        std::string path = data_path(file);
        if (!wav_write_mono16(path, s.data(), s.size(), m.bus.audio_rate)) return error_result("cannot write " + path);
        r["file"] = path;
    }
    return text_result(r.dump(2));
}

json t_screenshot(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    std::string card = a.value("display", std::string());
    DisplayDevice* d = m.find_display(card);
    if (!d) return error_result(card.empty() ? "machine has no display card" : "no display card '" + card + "'");
    long long scale = arg_num(a, "scale", 2);
    if (scale < 1 || scale > 4) throw ToolError{ "scale must be 1..4" };
    int w = d->screen_width(), hh = d->screen_height();
    std::vector<uint32_t> px((size_t)w * hh);
    d->render(px.data(), 0xFF101010u);
    std::vector<uint8_t> png = png_encode_argb(px.data(), w, hh, (int)scale, (int)scale);
    return json{ { "content", json::array({ json{ { "type", "image" }, { "data", base64(png) }, { "mimeType", "image/png" } } }) },
                 { "isError", false } };
}

json t_serial_send(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    SerialChannel* ch = serial_arg(m, a);
    std::string text = arg_str(a, "text", nullptr, true);
    ch->host_send((const uint8_t*)text.data(), text.size());
    return text_result("queued " + std::to_string(text.size()) + " bytes to " + ch->name);
}

json t_serial_receive(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    SerialChannel* ch = serial_arg(m, a);
    long long max = arg_num(a, "max_bytes", 4096);
    std::string data;
    ch->host_receive(data, (size_t)std::max<long long>(1, std::min<long long>(max, 1 << 20)));
    if (arg_bool(a, "hex", false)) {
        std::string hx;
        for (char c : data) hx += h2((uint8_t)c) + " ";
        return text_result(hx);
    }
    std::string out;
    for (char c : data) {
        uint8_t b = (uint8_t)c;
        if (b == '\r' || b == '\n' || b == '\t' || (b >= 0x20 && b < 0x7F)) out += c;
        else if (b >= 0x80) out += sapi_code_to_utf8(b);
        else out += "<" + h2(b) + ">";
    }
    return text_result(out);
}

json t_serial_set_connected(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    SerialChannel* ch = serial_arg(m, a);
    if (!a.contains("connected") && !a.contains("cts")) throw ToolError{ "give 'connected' (DCD) and/or 'cts'" };
    if (a.contains("connected")) ch->connected = arg_bool(a, "connected", true);
    if (a.contains("cts")) ch->cts = arg_bool(a, "cts", true);
    return text_result(ch->name + std::string(ch->connected ? ": DCD active" : ": DCD inactive") +
                       std::string(ch->cts ? ", CTS/DSR active" : ", CTS/DSR inactive"));
}

// DSM-1 by its port ("10h") or card name; the first one when not given.
Dsm1* find_dsm1(Machine& m, const json& a)
{
    std::string want = arg_str(a, "card", "", false);
    long long port = -1;
    if (!want.empty()) parse_number(want, port);
    for (auto& c : m.cards) {
        Dsm1* d = dynamic_cast<Dsm1*>(c.get());
        if (!d) continue;
        if (want.empty() || c->name == want || (port >= 0 && d->base_port() == (uint8_t)port)) return d;
    }
    throw ToolError{ want.empty() ? "machine has no DSM-1 card" : "no DSM-1 '" + want + "'" };
}

json tape_json(Dsm1* d)
{
    json j;
    d->describe(j);
    j["card"] = d->name;
    return j;
}

json t_tape_status(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    if (a.contains("card")) return json_result(tape_json(find_dsm1(m, a)));
    json list = json::array();
    for (auto& c : m.cards)
        if (Dsm1* d = dynamic_cast<Dsm1*>(c.get())) list.push_back(tape_json(d));
    return json_result(json{ { "recorders", list } });
}

json t_tape_insert(EmuHost& h, const json& a)
{
    Dsm1* d = find_dsm1(need_machine(h), a);
    std::string path = arg_str(a, "path", nullptr, true);
    bool create = arg_bool(a, "new", false);
    TapeDeck* t = d->tape();
    t->eject();
    std::string err;
    if (create) {
        if (!t->create(path, err)) return error_result(err);
    } else if (tape_is_program(path)) {   // a program becomes a tape (read only)
        TapeProgram p;
        std::string f = arg_str(a, "format", "mikos");
        if (!tape_format_parse(f, p.format)) throw ToolError{ "format must be mikos or mikrobasic" };
        p.name = arg_str(a, "name", "");
        p.address = arg_num(a, "address", -1);
        p.entry = arg_num(a, "entry", -1);
        if (!t->load_program(path, p, err)) return error_result(err);
    } else {
        if (!t->load(path, err)) return error_result(err);
        if (a.contains("write_protect")) t->write_protect = arg_bool(a, "write_protect", true) || t->read_only();
    }
    return json_result(tape_json(d));
}

json t_tape_control(EmuHost& h, const json& a)
{
    Dsm1* d = find_dsm1(need_machine(h), a);
    TapeDeck* t = d->tape();
    std::string act = arg_str(a, "action", nullptr, true);
    std::string err;
    if (act == "eject") {
        t->eject();
    } else if (act == "save") {
        if (!t->save(err)) return error_result(err);
    } else if (act == "export") {
        if (!t->export_wav(arg_str(a, "path", nullptr, true), err)) return error_result(err);
    } else {
        if (!t->loaded()) return error_result("no cassette in the recorder");
        if (act == "play") t->mode = TapeDeck::PLAY;
        else if (act == "record") {
            if (t->write_protect) return error_result("the cassette is write protected");
            t->mode = TapeDeck::RECORD;
        } else if (act == "stop") t->mode = TapeDeck::STOP;
        else if (act == "rewind") { t->mode = TapeDeck::STOP; t->pos = 0; }
        else if (act == "unprotect") {
            if (t->read_only()) return error_result(t->is_mp3 ? "an MP3 cassette cannot be recorded on"
                                                              : "a cassette made from a program cannot be recorded on");
            t->write_protect = false;
        } else if (act == "protect") t->write_protect = true;
        else throw ToolError{ "action must be play, record, stop, rewind, eject, save, export, protect or unprotect" };
    }
    return json_result(tape_json(d));
}

Pmd32Sd* find_pmd32(Machine& m)
{
    for (auto& c : m.cards)
        if (Pmd32Sd* p = c->pmd32sd()) return p;
    throw ToolError{ "machine has no card with PMD32-SD (IDE-1, DPP-1)" };
}

json t_pmd32_images(EmuHost& h, const json&)
{
    Pmd32Sd* p = find_pmd32(need_machine(h));
    json drives = json::array();
    for (int d = 0; d < Pmd32Sd::DRIVES; d++)
        drives.push_back(json{ { "drive", std::string(1, (char)('A' + d)) },
                               { "image", p->drive_present(d) ? json(p->drive_path(d)) : json(nullptr) },
                               { "wp", p->drive_wp(d) } });
    return json_result(json{ { "sd_card", p->root() }, { "drives", drives }, { "images", p->list_images() },
                             { "note", "SAPI CP/M: drive A = D:, drive B = E:" } });
}

json t_pmd32_mount(EmuHost& h, const json& a)
{
    Pmd32Sd* p = find_pmd32(need_machine(h));
    std::string drv = arg_str(a, "drive", nullptr, true);
    if (drv.size() != 1 || toupper((unsigned char)drv[0]) < 'A' || toupper((unsigned char)drv[0]) > 'D')
        throw ToolError{ "drive must be A, B, C or D" };
    int d = toupper((unsigned char)drv[0]) - 'A';
    std::string image = arg_str(a, "image", "");
    if (image.empty()) {
        p->eject(d);
        return text_result("drive " + drv + " ejected");
    }
    int rv = p->mount(d, image, arg_bool(a, "wp", false));
    if (rv != 0) return error_result("cannot insert " + image + " (PMD32-SD error " + std::to_string(rv) + ")");
    return text_result("drive " + drv + ": " + image + (p->drive_wp(d) ? " (write protected)" : ""));
}

json t_printer_output(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    std::string want = arg_str(a, "card", "");
    for (auto& c : m.cards) {
        Printer* p = c->printer();
        if (!p || (!want.empty() && want != c->type && want != c->name)) continue;
        long long n = arg_num(a, "lines", 200);
        std::string text = p->text((size_t)std::max<long long>(1, std::min<long long>(n, 100000)));
        if (arg_bool(a, "clear", false)) p->new_sheet();
        return json_result(json{ { "card", c->type }, { "output", p->path() }, { "bytes", p->bytes },
                                 { "busy", p->busy(m.bus.now) }, { "text", text } });
    }
    throw ToolError{ want.empty() ? "machine has no printer (JPR X3 or DPP-1 printer section)" : "no printer on " + want };
}

json t_io_log(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    if (a.contains("enable")) {
        m.bus.io_log_enabled = arg_bool(a, "enable", true);
        if (arg_bool(a, "clear", false)) { m.bus.io_log.clear(); m.bus.io_log_next = 0; }
    }
    long long count = arg_num(a, "count", 50);
    std::vector<IoLogEntry> log;
    m.bus.io_log_copy(log, (size_t)std::max<long long>(1, std::min<long long>(count, 4096)));
    std::string out = std::string("io log ") + (m.bus.io_log_enabled ? "enabled" : "disabled") + "\n";
    for (const IoLogEntry& e : log) {
        char line[64];
        snprintf(line, sizeof(line), "%12llu PC=%04X %s %02X %s %02X\n", (unsigned long long)e.time, e.pc,
                 e.write ? "OUT" : "IN ", e.port, e.write ? "<-" : "->", e.value);
        out += line;
    }
    return text_result(out);
}

json t_get_log(EmuHost&, const json& a)
{
    long long count = arg_num(a, "count", 50);
    std::vector<std::string> lines;
    log_recent(lines, (int)std::max<long long>(1, std::min<long long>(count, 1000)));
    std::string out;
    for (auto& l : lines) out += l + "\n";
    return text_result(out);
}

const char* const S_NONE = R"({"type":"object","properties":{}})";

const Tool k_tools[] = {
    { "machine_info", "Emulator and machine overview: machine name, run state, CPU clock and cycles, cards in slots with their jumper settings, serial channels, registers.",
      S_NONE, t_machine_info },
    { "list_machines", "List machine description files (machines/*.sapi including subfolders such as machines/zps/) that can be loaded with load_machine.",
      S_NONE, t_list_machines },
    { "card_catalog", "Catalog of SAPI-1 cards and buses for machine descriptions: card types (emulated or placeholder), slots they take and every configuration parameter (jumpers, ROM files, options with meaning, defaults). Optionally only one card type.",
      R"({"type":"object","properties":{"type":{"type":"string"}}})", t_card_catalog },
    { "machine_map", "Port and memory map of a machine from the cards' own address decoders (PROM dumps, jumpers): which card answers which I/O port and memory range (per MAP1/MAP2 state when it matters), with conflicts (two cards answering a read). For the running machine, a file ('path') or a description ('config').",
      R"({"type":"object","properties":{"path":{"type":"string"},"config":{"type":"object"}}})", t_machine_map },
    { "get_machine_config", "Machine description (cards in slots X1.., jumpers) of the running machine, or of a file when 'path' is given, with layout problems. jsonc=true adds the commented .sapi file text as the configurator writes it.",
      R"({"type":"object","properties":{"path":{"type":"string"},"jsonc":{"type":"boolean","default":false}}})", t_get_machine_config },
    { "set_machine_config", "Use a machine description (as returned by get_machine_config, edited): optionally save it as a commented .sapi file (JSON with comments) (path relative to the data directory, e.g. machines/my.sapi) and power the machine on with it (start=true, default). Cards not emulated yet are passive; a machine whose CPU card is not emulated cannot start.",
      R"({"type":"object","properties":{"config":{"type":"object"},"save_as":{"type":"string"},"start":{"type":"boolean","default":true}},"required":["config"]})", t_set_machine_config },
    { "tape_status", "Cassette recorders of the DSM-1 cards: cassette file, position and length, PLAY/RECORD/STOP, motor (runs only when the program closes the relay), write protection, whether the DSM-1 is switched to the recorder.",
      R"({"type":"object","properties":{"card":{"type":"string","description":"DSM-1 port (10h) or card name; all when omitted"}}})", t_tape_status },
    { "tape_insert", "Put a cassette into a DSM-1 recorder: an existing WAV or MP3 file from anywhere (write protected unless write_protect=false; MP3 always), or new=true for a new empty WAV tape that can be recorded on, or a program (.hex/.ihx Intel HEX, .bin with address) that becomes a tape at once (read only): format mikos (MIKOS V4/V5: .K_L name, then .J; the program must start at 4400h or higher, .J starts it at its lowest address, entry adds a JMP before it) or mikrobasic (ZPS-1 LOAD: a BASIC program at 40EFh, or machine code at 4102h or higher with a line 1 CALL entry added, RUN starts it). name = name on the tape (default from the file name, 11 characters). Recorded tapes are saved on eject, power off and exit.",
      R"({"type":"object","properties":{"path":{"type":"string"},"new":{"type":"boolean","default":false},"write_protect":{"type":"boolean"},"format":{"type":"string","enum":["mikos","mikrobasic"],"default":"mikos"},"name":{"type":"string"},"address":{"type":"string","description":"hex, load address of a .bin"},"entry":{"type":"string","description":"hex, start address, default the lowest address"},"card":{"type":"string"}},"required":["path"]})", t_tape_insert },
    { "tape_control", "Buttons of a DSM-1 recorder: play, record, stop, rewind (to the start), eject, save, protect / unprotect, export (the tape into the WAV file path, e.g. to play it into a real SAPI). The tape moves only while PLAY or RECORD is pressed and the program switches the motor relay on (MIKROBASIC LOAD / SAVE do that).",
      R"({"type":"object","properties":{"action":{"type":"string","enum":["play","record","stop","rewind","eject","save","export","protect","unprotect"]},"path":{"type":"string","description":"WAV file for export"},"card":{"type":"string"}},"required":["action"]})", t_tape_control },
    { "power", "Power switch of the machine: off (RAM lost, battery backed SRAM and RAM disks kept, host connections closed), on (with the last machine description), cycle = off and on. Reset (tool reset) keeps the RAM.",
      R"({"type":"object","properties":{"state":{"type":"string","enum":["on","off","cycle"]}},"required":["state"]})", t_power },
    { "quit", "End the emulator process (sapiemu-cli --mcp or the GUI): the machine is switched off first, so recorded tapes, cassettes, battery backed memories and RAM disks are saved, then the program exits.",
      R"({"type":"object","properties":{}})", t_quit },
    { "load_machine", "Load a machine description (cards and jumpers) and power it on. Path is relative to the emulator data directory, e.g. machines/mikromon.sapi.",
      R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})", t_load_machine },
    { "reset", "Press the reset button (/RES): CPU card BOOT flag set, cards reset. RAM content is kept.",
      S_NONE, t_reset },
    { "pause", "Pause real-time emulation.", S_NONE, t_pause },
    { "resume", "Resume the emulation; optionally speed_percent = speed of the whole machine against real time (100 = real time, 200 = twice as fast, not the CPU clock of the card) or max_speed = as fast as possible.",
      R"({"type":"object","properties":{"speed_percent":{"type":"integer"},"max_speed":{"type":"boolean"}}})", t_resume },
    { "step", "Execute instructions one by one (pauses the emulation). over=true steps over CALL/RST/DJNZ/LDIR. Returns the executed instructions and registers.",
      R"({"type":"object","properties":{"count":{"type":"integer","default":1},"over":{"type":"boolean","default":false}}})", t_step },
    { "run_for", "Run the machine synchronously (as fast as possible) for the given emulated time in milliseconds or until a breakpoint.",
      R"({"type":"object","properties":{"ms":{"type":"integer"}},"required":["ms"]})", t_run_for },
    { "run_until", "Run synchronously until PC reaches an address, a text appears on the screen or on a serial channel, a breakpoint hits, or the emulated timeout expires. Addresses are hex strings.",
      R"({"type":"object","properties":{"address":{"type":"string","description":"hex, e.g. F000"},"screen_text":{"type":"string"},"serial_text":{"type":"string"},"channel":{"type":"integer","default":0},"timeout_ms":{"type":"integer","default":5000}}})", t_run_until },
    { "get_registers", "Read Z80 registers (hex), flags and the next instruction.", S_NONE, t_get_registers },
    { "set_registers", "Set Z80 registers. Values are hex strings (e.g. \"F000\") or decimal numbers. Setting pc restarts execution there. Pauses the emulation.",
      R"({"type":"object","properties":{"pc":{"type":"string"},"sp":{"type":"string"},"af":{"type":"string"},"bc":{"type":"string"},"de":{"type":"string"},"hl":{"type":"string"},"ix":{"type":"string"},"iy":{"type":"string"},"i":{"type":"string"},"r":{"type":"string"},"im":{"type":"integer"},"iff1":{"type":"boolean"},"iff2":{"type":"boolean"}}})", t_set_registers },
    { "read_memory", "Read memory as seen by the CPU (including boot ROM overlay), without side effects. format 'dump' = hex dump with characters (KOI-8 cs), 'hex' = plain hex string.",
      R"({"type":"object","properties":{"address":{"type":"string","description":"hex"},"length":{"type":"integer","default":256},"format":{"type":"string","enum":["dump","hex"]}},"required":["address"]})", t_read_memory },
    { "write_memory", "Write bytes to memory through the bus (RAM, video RAM). data is a hex string, e.g. \"3E 41 D3 12\".",
      R"({"type":"object","properties":{"address":{"type":"string"},"data":{"type":"string"}},"required":["address","data"]})", t_write_memory },
    { "io_read", "IN from an I/O port. Has the same side effects as a CPU read (e.g. reading UART data clears its flag).",
      R"({"type":"object","properties":{"port":{"type":"string","description":"hex"}},"required":["port"]})", t_io_read },
    { "io_write", "OUT to an I/O port.",
      R"({"type":"object","properties":{"port":{"type":"string"},"value":{"type":"string"}},"required":["port","value"]})", t_io_write },
    { "disassemble", "Disassemble Z80 code (default from PC). '>' marks PC, '*' a breakpoint.",
      R"({"type":"object","properties":{"address":{"type":"string"},"count":{"type":"integer","default":16}}})", t_disassemble },
    { "set_breakpoint", "Set a breakpoint: exec (PC), mem_read/mem_write/mem_rw (memory address) or io_in/io_out/io_rw (port). Real-time emulation pauses when it hits.",
      R"({"type":"object","properties":{"address":{"type":"string","description":"hex address or port"},"kind":{"type":"string","enum":["exec","mem_read","mem_write","mem_rw","io_in","io_out","io_rw"],"default":"exec"}},"required":["address"]})", t_set_breakpoint },
    { "clear_breakpoint", "Remove a breakpoint, or all of them with all=true.",
      R"({"type":"object","properties":{"address":{"type":"string"},"kind":{"type":"string"},"all":{"type":"boolean"}}})", t_clear_breakpoint },
    { "list_breakpoints", "List all breakpoints and watchpoints.", S_NONE, t_list_breakpoints },
    { "load_hex", "Load an Intel HEX program into memory, from a file path (absolute or relative to the data directory) or from inline content.",
      R"({"type":"object","properties":{"path":{"type":"string"},"content":{"type":"string"}}})", t_load_hex },
    { "load_binary", "Load a binary file into memory at the given hex address.",
      R"({"type":"object","properties":{"path":{"type":"string"},"address":{"type":"string"}},"required":["path","address"]})", t_load_binary },
    { "type_text", "Type text on the SAPI keyboard of the machine (see machine_info). Use \\r for Enter, \\u001b for Esc. EKL-1 and Consul 259.11 (KOI-8 cs) type Czech letters, Consul 262.3 and 259.11 (KOI-7) only ASCII, ANK-1 only upper case letters, digits and its symbols. Optionally run run_ms of emulated time so the program reads the keys.",
      R"({"type":"object","properties":{"text":{"type":"string"},"run_ms":{"type":"integer","default":0}},"required":["text"]})", t_type_text },
    { "press_key", "Press a key without text on the SAPI keyboard, mapped like the host key in the GUI (docs/desky/klavesnice.md): cursor keys, f1-f4/f6/f7 (Consul ROL/COPY/BREAK and keys D0-D2; ANK-1 f6 = INT, f7 = T), pause (Consul 259.11 CLEAR), numeric keypad (Consul 262.3 B0-B9/AE). Optionally run run_ms of emulated time.",
      R"({"type":"object","properties":{"key":{"type":"string","enum":["enter","backspace","tab","esc","up","down","left","right","home","end","pgup","pgdn","insert","delete","pause","f1","f2","f3","f4","f6","f7","kp0","kp1","kp2","kp3","kp4","kp5","kp6","kp7","kp8","kp9","kp.","kp_enter"]},"shift":{"type":"boolean","default":false},"ctrl":{"type":"boolean","default":false},"run_ms":{"type":"integer","default":0}},"required":["key"]})", t_press_key },
    { "set_keyboard", "Plug another keyboard into the CPU card (connector X2). The machine keeps running; software must support the keyboard's protocol.",
      R"J({"type":"object","properties":{"name":{"type":"string","enum":["EKL-1","Consul 262.3","Consul 262.3 (bez 7474)","Consul 259.11","Consul 259.11 (KOI-8 cs)","ANK-1","ANK-1 (MIKOS)","ANK-1M2"]}},"required":["name"]})J", t_set_keyboard },
    { "screen_text", "Visible text of the display card (AND-1Z: 64x20) as UTF-8.", S_NONE, t_screen_text },
    { "cpu_turbo", "TURBO switch of the CPU card (JPR-1V: CPU 2 or 4 MHz, bus 2 MHz) while running; without 'on' it only reports.",
      R"({"type":"object","properties":{"on":{"type":"boolean"}}})", t_cpu_turbo },
    { "diag_board", "Diagnostic board on K3 of JPR-1S / JPR-1X (parameter k3 = diag): two seven segment displays (left = P2-OUT, right = P1-OUT; segments a..g, dp = bits 0..7) and two buttons, \"+\" on P2-IN0 (JPR-1X: interrupt request R0) and \"-\" on P2-IN7 (R7). Without arguments it reports what the displays show; press = \"+\" or \"-\" holds the button for hold_ms (default 150) while the machine runs, then runs after_ms (default 200).",
      R"({"type":"object","properties":{"press":{"type":"string","enum":["+","-"]},"hold_ms":{"type":"integer","default":150},"after_ms":{"type":"integer","default":200}}})", t_diag_board },
    { "plotter", "Plotter ARITMA Minigraf 0507 on DPP-1 port B (driver V.1-MGF: .G=1800 init, .G=1809 test, .G=180F screen copy, .G=1815 system printer; BASIC EXP CALL(6171...)). Without arguments the pen position on the paper (mm: x from its left edge, y up the paper from where it was put in; the paper is an A4 wide strip), the pen, strokes, the extent of the drawing, steps, steps against the stop of the carriage (the origin is set so), lost steps (the code skipped a phase). new_sheet = a clean sheet under the pen, png / svg = save the paper (paths relative to the data root, px_per_mm for PNG, default 8; crop = only the drawing with 5 mm around).",
      R"({"type":"object","properties":{"new_sheet":{"type":"boolean"},"png":{"type":"string"},"svg":{"type":"string"},"px_per_mm":{"type":"integer","default":8},"crop":{"type":"boolean","default":false}}})", t_plotter },
    { "kzd1", "KZD-1 cassette data recorder (on DPP-1 X2, MIKOS/K): without arguments its state (cassette, side, tape position in metres, motion, motor overloads, the control lines). insert = a .kzd cassette (created empty when missing), new = an empty cassette into a new file (replaces it), eject, flip (the other side faces the head), tab_a / tab_b (break-out write tabs present = recording allowed), flush (write the cassette file now). program = an Intel HEX or binary file (address = its load address, hex) made into a new cassette work/kzd-1/NAME.kzd as MIKOS/K writes it (K_I with tape name volume, then K_S with name; entry = start address for .J): .K_Z, .K_L NAME, .J. Paths relative to the data root.",
      R"({"type":"object","properties":{"card":{"type":"string"},"insert":{"type":"string"},"new":{"type":"string"},"eject":{"type":"boolean"},"flip":{"type":"boolean"},"tab_a":{"type":"boolean"},"tab_b":{"type":"boolean"},"flush":{"type":"boolean"},"program":{"type":"string"},"name":{"type":"string"},"volume":{"type":"string"},"address":{"type":"string"},"entry":{"type":"string"}}})", t_kzd1 },
    { "plant", "APS-5: the plant (furnaces, thermocouples, terminal block) and the front panel HRK-1. Without arguments it reports the temperatures, heaters (DRV-1A contacts), logic inputs/outputs and the panel (key switch, fault LED, watchdog relay). Arguments change it: lock (1 operator, 2 programming), outage_s (a mains failure of that many seconds: the power fail warning, then no power, then power again; run the machine to see it), ambient_c, block_c (isothermal terminal block), battery_v, reset (all furnaces to the ambient temperature), and for channel (1-8): temperature_c (sets the furnace at once), tc_broken, heater_broken, li_a / li_b (logic inputs, true = H open). Keys of the panel: type_text with 0-9, M, R, backspace (<-), CR (->).",
      R"({"type":"object","properties":{"lock":{"type":"integer","enum":[1,2]},"outage_s":{"type":"number"},"ambient_c":{"type":"number"},"block_c":{"type":"number"},"battery_v":{"type":"number"},"reset":{"type":"boolean"},"channel":{"type":"integer"},"temperature_c":{"type":"number"},"tc_broken":{"type":"boolean"},"heater_broken":{"type":"boolean"},"li_a":{"type":"boolean"},"li_b":{"type":"boolean"}}})", t_plant },
    { "joystick", "Joysticks of the machine (MPH-1V connectors K4/K2 or the ANK-1M2 keyboard connector set to joystick; ANK-1M2 follows port 0): port 0 = the first joystick, 1 = the second. 'pressed' sets what is held (names down, up, right, left, fire1, fire2); without it only reports. The GUI changes it when the gamepad or the keyboard joystick moves.",
      R"({"type":"object","properties":{"port":{"type":"integer","default":0},"pressed":{"type":"array","items":{"type":"string","enum":["down","up","right","left","fire1","fire2"]}}}})", t_joystick },
    { "mouse", "Mouse of the machine (an MPH-1V connector or the ANK-1M2 keyboard connector set to mouse_atari / mouse_amiga): dx, dy = counts to move (+x right, +y down), sent as quadrature steps at 1000 per second and axis; 'buttons' sets what is held (left, right). Reports the counts still pending.",
      R"({"type":"object","properties":{"dx":{"type":"integer","default":0},"dy":{"type":"integer","default":0},"buttons":{"type":"array","items":{"type":"string","enum":["left","right"]}}}})", t_mouse },
    { "save_state", "Save the state of the whole machine (CPU, memories incl. RAM disks, cards, keyboard, serial channels; large media such as the IDE disk only by reference): into memory under 'name' (default \"default\") for quick checkpoints, or into 'file' (.sapistate, relative paths from the data root).",
      R"({"type":"object","properties":{"name":{"type":"string"},"file":{"type":"string"}}})", t_save_state },
    { "load_state", "Load a state saved by save_state (memory 'name' or 'file', also the GUI's .sapistate files): the machine of the state is built and continues from that moment; breakpoints, watchpoints and the pause are kept.",
      R"({"type":"object","properties":{"name":{"type":"string"},"file":{"type":"string"}}})", t_load_state },
    { "audio_record", "Record the sound card output (MPH-1V, mono, 49716 Hz): start=true starts, then run the emulation; a call without start stops and reports seconds, rms and peak (16 bit; lost_samples if the mixer dropped any), and with 'file' saves a WAV (relative paths from the data root, i.e. the folder with roms/ and machines/ of the emulator, not the caller's folder). The recording follows the emulated time of the machine: after resume it also takes what runs in real time between the calls.",
      R"({"type":"object","properties":{"start":{"type":"boolean","default":false},"file":{"type":"string"}}})", t_audio_record },
    { "screenshot", "PNG image of the display: the console screen, or display = card name of another display output (e.g. \"DGD-1\" graphics).",
      R"({"type":"object","properties":{"scale":{"type":"integer","default":2},"display":{"type":"string"}}})", t_screenshot },
    { "serial_send", "Send text to a serial channel (host -> SAPI), at the configured baud rate.",
      R"({"type":"object","properties":{"text":{"type":"string"},"channel":{"type":"integer","default":0}},"required":["text"]})", t_serial_send },
    { "serial_receive", "Read data the SAPI sent on a serial channel since the last serial_receive.",
      R"({"type":"object","properties":{"channel":{"type":"integer","default":0},"max_bytes":{"type":"integer","default":4096},"hex":{"type":"boolean","default":false}}})", t_serial_receive },
    { "serial_set_connected", "Modem input lines of a serial channel: connected = DCD (ECMON and MikroMon pick the serial console at boot when DCD is active), cts = CTS/DSR (terminal ready; programs sending with RTS/CTS handshake such as DOPC.COM wait for it).",
      R"({"type":"object","properties":{"channel":{"type":"integer","default":0},"connected":{"type":"boolean"},"cts":{"type":"boolean"}}})", t_serial_set_connected },
    { "pmd32_images", "PMD32-SD floppy emulator (on IDE-1): inserted images of drives A-D and image files available on the SD card directory.",
      S_NONE, t_pmd32_images },
    { "pmd32_mount", "Insert an image into a PMD32-SD drive (A-D; in SAPI CP/M A = D:, B = E:), or eject it with an empty image name. The image type must be listed in images.cfg.",
      R"({"type":"object","properties":{"drive":{"type":"string","enum":["A","B","C","D"]},"image":{"type":"string"},"wp":{"type":"boolean","default":false}},"required":["drive"]})", t_pmd32_mount },
    { "printer_output", "Text printed so far by the printer (JPR X3 or DPP-1): the last lines, form feeds as \\f; ESC/P sequences left out (the raw bytes are in the output file). clear=true starts a new sheet after reading.",
      R"({"type":"object","properties":{"card":{"type":"string","description":"card type or name; the first printer when omitted"},"lines":{"type":"integer","default":200},"clear":{"type":"boolean","default":false}}})", t_printer_output },
    { "io_log", "Show recent I/O port accesses; enable=true starts logging (clear=true empties the log).",
      R"({"type":"object","properties":{"enable":{"type":"boolean"},"clear":{"type":"boolean"},"count":{"type":"integer","default":50}}})", t_io_log },
    { "get_log", "Recent emulator log messages (warnings, bus conflicts...).",
      R"({"type":"object","properties":{"count":{"type":"integer","default":50}}})", t_get_log },
};

} // namespace

json mcp_tool_list(void)
{
    json tools = json::array();
    for (const Tool& t : k_tools)
        tools.push_back(json{ { "name", t.name }, { "description", t.description }, { "inputSchema", json::parse(t.schema) } });
    return tools;
}

json mcp_tool_call(EmuHost& host, const std::string& name, const json& args)
{
    for (const Tool& t : k_tools) {
        if (name != t.name) continue;
        try {
            return t.fn(host, args.is_object() ? args : json::object());
        } catch (const ToolError& e) {
            return error_result(e.msg);
        }
    }
    return error_result("unknown tool '" + name + "'");
}
