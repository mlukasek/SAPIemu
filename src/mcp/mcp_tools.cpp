#include "mcp/mcp_tools.h"
#include "cards/catalog.h"
#include "cards/ide1.h"
#include "core/charset.h"
#include "core/headless.h"
#include "core/host.h"
#include "core/log.h"
#include "core/machine_cfg.h"
#include "core/png.h"
#include "core/util.h"
#include "core/version.h"
#include "input/keyboards.h"

#include <ctype.h>
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
    j["turbo"] = h.turbo;
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
    for (const CfgProblem& p : machine_cfg_problems(cfg)) {
        pr.push_back({ { "slot", p.slot }, { "error", p.error }, { "text", p.en } });
        if (p.error) runnable = false;
    }
    return pr;
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
        if (!file_write_all(full, text.data(), text.size())) return error_result("cannot write " + full);
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
    if (a.contains("turbo")) h.turbo = arg_bool(a, "turbo", false);
    if (a.contains("speed_mhz")) {
        double mhz = a["speed_mhz"].is_number() ? a["speed_mhz"].get<double>() : 0.0;
        if (mhz < 0 || mhz > 100) throw ToolError{ "speed_mhz must be 0 (CPU clock) .. 100" };
        h.speed_hz = (uint32_t)(mhz * 1e6);
    }
    h.paused = false;
    h.status.clear();
    return json_result(json{ { "state", "running" }, { "turbo", h.turbo }, { "speed_hz", h.target_hz() } });
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

json t_screenshot(EmuHost& h, const json& a)
{
    Machine& m = need_machine(h);
    if (!m.display) return error_result("machine has no display card");
    long long scale = arg_num(a, "scale", 2);
    if (scale < 1 || scale > 4) throw ToolError{ "scale must be 1..4" };
    int w = m.display->screen_width(), hh = m.display->screen_height();
    std::vector<uint32_t> px((size_t)w * hh);
    m.display->render(px.data(), 0xFFE8E8E8u, 0xFF101010u);
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

Ide1* find_ide1(Machine& m)
{
    for (auto& c : m.cards) {
        Ide1* i = dynamic_cast<Ide1*>(c.get());
        if (i && i->pmd32()) return i;
    }
    throw ToolError{ "machine has no IDE-1 card with PMD32-SD" };
}

json t_pmd32_images(EmuHost& h, const json&)
{
    Pmd32Sd* p = find_ide1(need_machine(h))->pmd32();
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
    Pmd32Sd* p = find_ide1(need_machine(h))->pmd32();
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
    { "get_machine_config", "Machine description (cards in slots X1.., jumpers) of the running machine, or of a file when 'path' is given, with layout problems. jsonc=true adds the commented .sapi file text as the configurator writes it.",
      R"({"type":"object","properties":{"path":{"type":"string"},"jsonc":{"type":"boolean","default":false}}})", t_get_machine_config },
    { "set_machine_config", "Use a machine description (as returned by get_machine_config, edited): optionally save it as a commented .sapi file (JSON with comments) (path relative to the data directory, e.g. machines/my.sapi) and power the machine on with it (start=true, default). Cards not emulated yet are passive; a machine whose CPU card is not emulated cannot start.",
      R"({"type":"object","properties":{"config":{"type":"object"},"save_as":{"type":"string"},"start":{"type":"boolean","default":true}},"required":["config"]})", t_set_machine_config },
    { "power", "Power switch of the machine: off (RAM lost, battery backed SRAM and RAM disks kept, host connections closed), on (with the last machine description), cycle = off and on. Reset (tool reset) keeps the RAM.",
      R"({"type":"object","properties":{"state":{"type":"string","enum":["on","off","cycle"]}},"required":["state"]})", t_power },
    { "load_machine", "Load a machine description (cards and jumpers) and power it on. Path is relative to the emulator data directory, e.g. machines/mikromon.sapi.",
      R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})", t_load_machine },
    { "reset", "Press the reset button (/RES): CPU card BOOT flag set, cards reset. RAM content is kept.",
      S_NONE, t_reset },
    { "pause", "Pause real-time emulation.", S_NONE, t_pause },
    { "resume", "Resume real-time emulation; optionally turbo = as fast as possible, or speed_mhz = emulated cycles per real second in MHz (0 = the CPU clock of the machine; e.g. 4 on a 2 MHz machine runs everything twice as fast).",
      R"({"type":"object","properties":{"turbo":{"type":"boolean"},"speed_mhz":{"type":"number"}}})", t_resume },
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
      R"J({"type":"object","properties":{"name":{"type":"string","enum":["EKL-1","Consul 262.3","Consul 262.3 (bez 7474)","Consul 259.11","Consul 259.11 (KOI-8 cs)","ANK-1"]}},"required":["name"]})J", t_set_keyboard },
    { "screen_text", "Visible text of the display card (AND-1Z: 64x20) as UTF-8.", S_NONE, t_screen_text },
    { "screenshot", "PNG image of the display.",
      R"({"type":"object","properties":{"scale":{"type":"integer","default":2}}})", t_screenshot },
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
