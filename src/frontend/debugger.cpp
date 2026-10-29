// Debugger windows: CPU registers, disassembly with breakpoints, stack, memory, I/O log.
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/theme.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "imgui.h"
#include "core/charset.h"
#include "core/host.h"
#include "core/util.h"

static bool parse_hex16(const char* s, uint16_t& out)
{
    long long v;
    std::string t = s;
    if (t.empty()) return false;
    if (t.find_first_of("xX$hH") == std::string::npos) t += "h";
    if (!parse_number(t, v) || v < 0 || v > 0xFFFF) return false;
    out = (uint16_t)v;
    return true;
}

// A field for a 16-bit address in hex (FFFF and room for the cursor), in units of the font: it grows with the text.
static float hex_field_width(void)
{
    return 4.4f * ImGui::GetFontSize();
}

static void reg_edit(Machine* m, const char* name, uint16_t& value, bool editable, bool& changed)
{
    ImGui::TextUnformatted(name);
    ImGui::SameLine(ImGui::CalcTextSize("AF' ").x + ImGui::GetStyle().ItemSpacing.x);   // past the longest name
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("FFFFFF").x);
    ImGuiInputTextFlags fl = ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue;
    if (!editable) fl |= ImGuiInputTextFlags_ReadOnly;
    char id[16];
    snprintf(id, sizeof(id), "##%s", name);
    if (ImGui::InputScalar(id, ImGuiDataType_U16, &value, nullptr, nullptr, "%04X", fl)) changed = true;
}

static void draw_controls(UiState& ui, Machine* m)
{
    EmuHost& h = *ui.host;
    if (ImGui::Button(h.paused ? tr("Run (F5)") : tr("Pause (F5)"))) h.paused = !h.paused;
    ImGui::SameLine();
    ImGui::BeginDisabled(!h.paused);
    if (ImGui::Button(tr("Step (F11)"))) { m->step(); h.status = m->stop_text(); }
    ImGui::SameLine();
    if (ImGui::Button(tr("Step over (F10)"))) { m->step_over(); h.status = m->stop_text(); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("Reset (F12)"))) request_reset(ui);   // the APS-5 unit asks first
    ImGui::SameLine();
    if (ImGui::TextLink(tr("Keyboard shortcuts"))) {   // F9 breakpoint: the keys as in Visual Studio
        ui.show_shortcuts = true;
        ImGui::SetWindowFocus(tr_id("Keyboard shortcuts"));   // in front when it is open already
    }
    if (!h.status.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(theme().warn, "%s", h.status.c_str());
    }
}

static void draw_registers(UiState& ui, Machine* m)
{
    CpuRegs r;
    m->cpu->get_regs(r);
    bool editable = ui.host->paused;
    bool changed = false;
    if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
    if (ImGui::BeginTable("regs", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableNextColumn(); reg_edit(m, "AF", r.af, editable, changed);
        ImGui::TableNextColumn(); reg_edit(m, "AF'", r.af2, editable, changed);
        ImGui::TableNextColumn(); reg_edit(m, "BC", r.bc, editable, changed);
        ImGui::TableNextColumn(); reg_edit(m, "BC'", r.bc2, editable, changed);
        ImGui::TableNextColumn(); reg_edit(m, "DE", r.de, editable, changed);
        ImGui::TableNextColumn(); reg_edit(m, "DE'", r.de2, editable, changed);
        ImGui::TableNextColumn(); reg_edit(m, "HL", r.hl, editable, changed);
        ImGui::TableNextColumn(); reg_edit(m, "HL'", r.hl2, editable, changed);
        ImGui::TableNextColumn(); reg_edit(m, "IX", r.ix, editable, changed);
        ImGui::TableNextColumn(); reg_edit(m, "IY", r.iy, editable, changed);
        ImGui::TableNextColumn(); reg_edit(m, "SP", r.sp, editable, changed);
        ImGui::TableNextColumn(); reg_edit(m, "PC", r.pc, editable, changed);
        ImGui::EndTable();
    }
    const char* names = "SZYHXPNC";
    char flags[9];
    for (int i = 0; i < 8; i++) flags[i] = (r.af & (0x80 >> i)) ? names[i] : '-';
    flags[8] = 0;
    ImGui::Text("F: %s  I=%02X R=%02X IM%d IFF1=%d IFF2=%d%s", flags, r.i, r.r, r.im, r.iff1, r.iff2,
                r.halted ? "  HALT" : "");
    ImGui::Text("%s %llu  (%.3f s)", tr("Cycles"), (unsigned long long)m->bus.now, m->bus.seconds());
    if (ui.mono) ImGui::PopFont();
    if (changed && editable) m->cpu->set_regs(r);
}

static void draw_disasm(UiState& ui, Machine* m)
{
    CpuRegs r;
    m->cpu->get_regs(r);
    ImGui::Checkbox(tr("Follow PC"), &ui.disasm_follow);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(hex_field_width());
    if (ImGui::InputText("##dgoto", ui.disasm_goto, sizeof(ui.disasm_goto),
                         ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (parse_hex16(ui.disasm_goto, ui.disasm_addr)) ui.disasm_follow = false;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", tr("address; a click on a line or F9 = breakpoint"));

    if (ui.disasm_follow) {
        // keep PC visible: restart the listing at PC when it leaves the window
        uint16_t a = ui.disasm_addr;
        bool visible = false;
        for (int i = 0; i < 20; i++) {
            if (a == r.pc) { visible = true; break; }
            a = (uint16_t)(a + m->disassemble(a).length);
        }
        if (!visible) ui.disasm_addr = r.pc;
    }

    ImGui::BeginChild("disasm", ImVec2(0, 21.0f * ImGui::GetFontSize()), ImGuiChildFlags_Borders);
    if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
    uint16_t a = ui.disasm_addr;
    for (int i = 0; i < 24; i++) {
        DisasmResult d = m->disassemble(a);
        char bytes[16] = "";
        for (int k = 0; k < d.length && k < 4; k++) snprintf(bytes + k * 3, 4, "%02X ", m->peek((uint16_t)(a + k)));
        bool bp = m->has_breakpoint(a);
        bool cur = (a == r.pc);
        char line[96];
        snprintf(line, sizeof(line), "%c%c %04X  %-12s %s", bp ? '*' : ' ', cur ? '>' : ' ', a, bytes, d.text.c_str());
        if (cur || bp) {   // the row in the colour of the PC or of a breakpoint ('>' and '*' tell it too)
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                p, ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + ImGui::GetTextLineHeightWithSpacing()),
                ImGui::GetColorU32(cur ? theme().pc_bg : theme().bp_bg));
        }
        char id[16];
        snprintf(id, sizeof(id), "##dl%d", i);
        ImGui::PushID(i);
        if (ImGui::Selectable(line, false)) m->set_breakpoint(a, !bp);
        if (ImGui::IsItemHovered()) ui.disasm_hover = a;   // F9 (handle_key, next frame) takes this row
        ImGui::PopID();
        a = (uint16_t)(a + d.length);
    }
    if (ui.mono) ImGui::PopFont();
    ImGui::EndChild();
}

static void draw_breakpoints(UiState& ui, Machine* m)
{
    static const char* const kinds[] = { "exec", "mem read", "mem write", "IN", "OUT" };
    ImGui::SetNextItemWidth(hex_field_width());
    ImGui::InputText("##bpaddr", ui.bp_input, sizeof(ui.bp_input), ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(combo_width(kinds, 5));
    ImGui::Combo("##bpkind", &ui.bp_kind, kinds, 5);
    ImGui::SameLine();
    uint16_t addr;
    if (ImGui::Button(tr("Add")) && parse_hex16(ui.bp_input, addr)) {
        switch (ui.bp_kind) {
        case 0: m->set_breakpoint(addr, true); break;
        case 1: m->set_mem_watch(addr, m->bus.mem_watch[addr] | WATCH_READ); break;
        case 2: m->set_mem_watch(addr, m->bus.mem_watch[addr] | WATCH_WRITE); break;
        case 3: m->set_io_watch((uint8_t)addr, m->bus.io_watch[(uint8_t)addr] | WATCH_READ); break;
        default: m->set_io_watch((uint8_t)addr, m->bus.io_watch[(uint8_t)addr] | WATCH_WRITE); break;
        }
    }
    if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
    int id = 0;
    for (uint16_t b : m->breakpoints()) {
        ImGui::PushID(id++);
        if (ImGui::SmallButton("x")) m->set_breakpoint(b, false);
        ImGui::SameLine();
        ImGui::Text("exec  %04X", b);
        ImGui::PopID();
    }
    if (m->bus.any_mem_watch) {
        for (int i = 0; i < 65536; i++) {
            uint8_t w = m->bus.mem_watch[i];
            if (!w) continue;
            ImGui::PushID(id++);
            if (ImGui::SmallButton("x")) m->set_mem_watch((uint16_t)i, 0);
            ImGui::SameLine();
            ImGui::Text("mem   %04X %s%s", i, (w & WATCH_READ) ? "R" : "", (w & WATCH_WRITE) ? "W" : "");
            ImGui::PopID();
        }
    }
    for (int i = 0; i < 256; i++) {
        uint8_t w = m->bus.io_watch[i];
        if (!w) continue;
        ImGui::PushID(id++);
        if (ImGui::SmallButton("x")) m->set_io_watch((uint8_t)i, 0);
        ImGui::SameLine();
        ImGui::Text("port  %02X %s%s", i, (w & WATCH_READ) ? "IN " : "", (w & WATCH_WRITE) ? "OUT" : "");
        ImGui::PopID();
    }
    if (ui.mono) ImGui::PopFont();
}

static void draw_stack(UiState& ui, Machine* m)
{
    CpuRegs r;
    m->cpu->get_regs(r);
    if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
    for (int i = 0; i < 8; i++) {
        uint16_t a = (uint16_t)(r.sp + i * 2);
        uint16_t v = (uint16_t)(m->peek(a) | (m->peek((uint16_t)(a + 1)) << 8));
        ImGui::Text("%04X: %04X", a, v);
    }
    if (ui.mono) ImGui::PopFont();
}

void draw_debugger(UiState& ui)
{
    ui.disasm_hover = -1;   // no row under the mouse unless draw_disasm finds one
    if (!ui.show_debugger) return;
    Machine* m = ui.host->machine();
    layout_window(LW_DEBUGGER);
    if (!ImGui::Begin(tr_id("Debugger"), &ui.show_debugger) || !m || !m->cpu) {
        ImGui::End();
        return;
    }
    draw_controls(ui, m);
    ImGui::Separator();
    draw_registers(ui, m);
    ImGui::Separator();
    draw_disasm(ui, m);
    if (ImGui::CollapsingHeader(tr("Breakpoints"), ImGuiTreeNodeFlags_DefaultOpen)) draw_breakpoints(ui, m);
    if (ImGui::CollapsingHeader(tr("Stack"))) draw_stack(ui, m);
    ImGui::End();
}

void draw_memory(UiState& ui)
{
    if (!ui.show_memory) return;
    Machine* m = ui.host->machine();
    layout_window(LW_MEMORY);
    if (!ImGui::Begin(tr_id("Memory"), &ui.show_memory) || !m) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(tr("Go to"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(hex_field_width());
    bool jump = ImGui::InputText("##mgoto", ui.mem_goto, sizeof(ui.mem_goto),
                                 ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);
    if (jump) parse_hex16(ui.mem_goto, ui.mem_addr);
    ImGui::SameLine();
    ImGui::TextUnformatted(tr("Write"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(hex_field_width());
    ImGui::InputText("##meaddr", ui.mem_edit_addr, sizeof(ui.mem_edit_addr), ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(12.5f * ImGui::GetFontSize());
    ImGui::InputTextWithHint("##medata", "3E 41 ...", ui.mem_edit_data, sizeof(ui.mem_edit_data));
    ImGui::SameLine();
    uint16_t wa;
    if (ImGui::Button("OK") && parse_hex16(ui.mem_edit_addr, wa)) {
        std::string hex;
        for (const char* p = ui.mem_edit_data; *p; p++) if (isxdigit((unsigned char)*p)) hex += *p;
        for (size_t i = 0; i + 1 < hex.size(); i += 2) {
            long long v;
            parse_number(hex.substr(i, 2) + "h", v);
            m->poke((uint16_t)(wa + i / 2), (uint8_t)v);
        }
    }

    ImGui::BeginChild("memview", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
    float line_h = ImGui::GetTextLineHeightWithSpacing();
    if (jump) ImGui::SetScrollY((ui.mem_addr / 16) * line_h);
    ImGuiListClipper clipper;
    clipper.Begin(4096, line_h);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
            uint16_t base = (uint16_t)(row * 16);
            char hex[64];
            std::string chars;
            int n = 0;
            for (int i = 0; i < 16; i++) {
                uint8_t b = m->peek((uint16_t)(base + i));
                n += snprintf(hex + n, sizeof(hex) - n, "%02X%s", b, i == 7 ? "  " : " ");
                chars += (b >= 0x20 && b != 0x7F) ? sapi_code_to_utf8(b) : ".";
            }
            ImGui::Text("%04X: %s %s", base, hex, chars.c_str());
        }
    }
    if (ui.mono) ImGui::PopFont();
    ImGui::EndChild();
    ImGui::End();
}

void draw_iolog(UiState& ui)
{
    if (!ui.show_iolog) return;
    Machine* m = ui.host->machine();
    layout_window(LW_IOLOG);
    if (!ImGui::Begin(tr_id("I/O log"), &ui.show_iolog) || !m) {
        ImGui::End();
        return;
    }
    ImGui::Checkbox(tr("Record"), &m->bus.io_log_enabled);
    ImGui::SameLine();
    if (ImGui::Button(tr("Clear"))) { m->bus.io_log.clear(); m->bus.io_log_next = 0; }
    std::vector<IoLogEntry> log;
    m->bus.io_log_copy(log, 1000);
    ImGui::BeginChild("iolog", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
    ImGuiListClipper clipper;
    clipper.Begin((int)log.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            const IoLogEntry& e = log[i];
            ImGui::Text("%10.6f  PC=%04X  %s %02X %s %02X", (double)e.time / m->bus.cpu_hz, e.pc,
                        e.write ? "OUT" : "IN ", e.port, e.write ? "<-" : "->", e.value);
        }
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    if (ui.mono) ImGui::PopFont();
    ImGui::EndChild();
    ImGui::End();
}
