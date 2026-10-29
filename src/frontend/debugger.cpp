// Debugger windows: the registers of the CPU the machine has (Z80, 8080, 8085), disassembly with breakpoints, stack,
// memory, I/O log. The disassembly is in Zilog mnemonics for all of them (core/disasm.h).
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/theme.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "imgui.h"
#include "core/charset.h"
#include "core/devices.h"
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

// A register in the table of the registers: its name in one column, on the line of the text in the field
// (AlignTextToFramePadding), the value in the next one; fmt "%04X" a word, "%02X" a byte, "%d" a small number (IM).
// Editable while paused, Enter writes it.
static void reg_cell(const char* name, void* value, const char* fmt, bool editable, bool& changed)
{
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(name);
    ImGui::TableNextColumn();
    bool word = fmt[2] == '4';
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(word ? "FFFFFF" : "FFFF").x);
    ImGuiInputTextFlags fl = ImGuiInputTextFlags_EnterReturnsTrue;
    fl |= fmt[1] == 'd' ? ImGuiInputTextFlags_CharsDecimal : ImGuiInputTextFlags_CharsHexadecimal;
    if (!editable) fl |= ImGuiInputTextFlags_ReadOnly;
    char id[16];
    snprintf(id, sizeof(id), "##%s", name);
    if (ImGui::InputScalar(id, word ? ImGuiDataType_U16 : ImGuiDataType_U8, value, nullptr, nullptr, fmt, fl))
        changed = true;
}

static void reg_none(void)   // a row with fewer registers
{
    ImGui::TableNextColumn();
    ImGui::TableNextColumn();
}

// The flags as check boxes after a label: the state in the tick, not in a colour; a click changes the flag while
// paused. `tip`: a tooltip key (the undocumented ones), nullptr = none.
struct FlagBit {
    const char* name;
    uint8_t mask;
    const char* tip;
};
static const FlagBit k_flags_z80[] = {
    { "S", 0x80, nullptr }, { "Z", 0x40, nullptr }, { "Y", 0x20, "Undocumented: bit 5 of the result." },
    { "H", 0x10, nullptr }, { "X", 0x08, "Undocumented: bit 3 of the result." }, { "P/V", 0x04, nullptr },
    { "N", 0x02, nullptr }, { "C", 0x01, nullptr }, { nullptr, 0, nullptr },
};
static const FlagBit k_flags_8080[] = {
    { "S", 0x80, nullptr }, { "Z", 0x40, nullptr }, { "AC", 0x10, nullptr }, { "P", 0x04, nullptr },
    { "CY", 0x01, nullptr }, { nullptr, 0, nullptr },
};
static const FlagBit k_flags_8085[] = {
    { "S", 0x80, nullptr },
    { "Z", 0x40, nullptr },
    { "K", 0x20, "Undocumented 8085 flag K (X5, UI): INX went over FFFFh or DCX under 0, after arithmetic V xor S; "
                 "JK and JNK test it." },
    { "AC", 0x10, nullptr },
    { "P", 0x04, nullptr },
    { "V", 0x02, "Undocumented 8085 flag V: overflow of a signed result (also DSUB); RSTV calls 40h when it is "
                 "set." },
    { "CY", 0x01, nullptr },
    { nullptr, 0, nullptr },
};

static void draw_flags(const FlagBit* fb, uint16_t& af, bool editable, bool& changed)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr("Flags"));
    ImGui::PushID("flags");
    ImGui::BeginDisabled(!editable);
    for (; fb->name; fb++) {
        ImGui::SameLine();
        bool on = (af & fb->mask) != 0;
        if (ImGui::Checkbox(fb->name, &on)) {
            af = (uint16_t)(on ? af | fb->mask : af & ~fb->mask);
            changed = true;
        }
        if (fb->tip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) tip(tr(fb->tip));
    }
    ImGui::EndDisabled();
    ImGui::PopID();
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

// The Z80: the main and the alternate registers, IX IY SP PC, I R IM, IFF1 IFF2 and the flags of F.
static void draw_regs_z80(CpuRegs& r, bool editable, bool& changed)
{
    ImGuiTableFlags tf = ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("regs", 6, tf)) {
        uint16_t* left[4] = { &r.af, &r.bc, &r.de, &r.hl };
        uint16_t* alt[4] = { &r.af2, &r.bc2, &r.de2, &r.hl2 };
        uint16_t* right[4] = { &r.ix, &r.iy, &r.sp, &r.pc };
        static const char* const ln[4] = { "AF", "BC", "DE", "HL" };
        static const char* const an[4] = { "AF'", "BC'", "DE'", "HL'" };
        static const char* const rn[4] = { "IX", "IY", "SP", "PC" };
        for (int i = 0; i < 4; i++) {
            reg_cell(ln[i], left[i], "%04X", editable, changed);
            reg_cell(an[i], alt[i], "%04X", editable, changed);
            reg_cell(rn[i], right[i], "%04X", editable, changed);
        }
        uint8_t im = r.im;
        reg_cell("I", &r.i, "%02X", editable, changed);
        reg_cell("R", &r.r, "%02X", editable, changed);
        reg_cell("IM", &im, "%d", editable, changed);
        r.im = im > 2 ? 2 : im;
        ImGui::EndTable();
    }
    draw_flags(k_flags_z80, r.af, editable, changed);
    const char* iff = tr("IFF1: maskable interrupts enabled (EI, DI); IFF2 keeps it during an NMI (LD A,I shows it in "
                         "P/V).");
    ImGui::BeginDisabled(!editable);
    if (ImGui::Checkbox("IFF1", &r.iff1)) changed = true;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) tip(iff);
    ImGui::SameLine();
    if (ImGui::Checkbox("IFF2", &r.iff2)) changed = true;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) tip(iff);
    ImGui::EndDisabled();
}

// The 8080 and the 8085 with the names of Intel: A and the flags (PSW), B C, D E, H L as bytes, SP PC; the flags
// S Z AC P CY (the 8085 also the undocumented K and V), the interrupt enable; the 8085 what RIM reads.
static void draw_regs_8080(CpuRegs& r, bool i8085, bool editable, bool& changed)
{
    uint8_t v[8] = { (uint8_t)(r.af >> 8), (uint8_t)r.af, (uint8_t)(r.bc >> 8), (uint8_t)r.bc,
                     (uint8_t)(r.de >> 8), (uint8_t)r.de, (uint8_t)(r.hl >> 8), (uint8_t)r.hl };
    bool bytes = false;
    if (ImGui::BeginTable("regs", 6, ImGuiTableFlags_SizingFixedFit)) {
        static const char* const names[8] = { "A", "F", "B", "C", "D", "E", "H", "L" };
        for (int i = 0; i < 4; i++) {
            reg_cell(names[2 * i], &v[2 * i], "%02X", editable, bytes);
            reg_cell(names[2 * i + 1], &v[2 * i + 1], "%02X", editable, bytes);
            if (i == 0) reg_cell("SP", &r.sp, "%04X", editable, changed);
            else if (i == 1) reg_cell("PC", &r.pc, "%04X", editable, changed);
            else reg_none();
        }
        ImGui::EndTable();
    }
    if (bytes) {
        r.af = (uint16_t)(v[0] << 8 | v[1]);
        r.bc = (uint16_t)(v[2] << 8 | v[3]);
        r.de = (uint16_t)(v[4] << 8 | v[5]);
        r.hl = (uint16_t)(v[6] << 8 | v[7]);
        changed = true;
    }
    draw_flags(i8085 ? k_flags_8085 : k_flags_8080, r.af, editable, changed);
    ImGui::BeginDisabled(!editable);
    if (ImGui::Checkbox(i8085 ? "IE" : "INTE", &r.iff1)) changed = true;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) tip(tr("Interrupts enabled (EI, DI)."));
    ImGui::EndDisabled();
    if (!i8085) return;
    ImGui::SameLine(0, 2.0f * ImGui::GetFontSize());   // apart from the label of the check box
    ImGui::Text("SID=%d SOD=%d", (r.rim >> 7) & 1, r.sod ? 1 : 0);
    ImGui::Text("RIM %02X  M7.5=%d M6.5=%d M5.5=%d  I7.5=%d I6.5=%d I5.5=%d", r.rim, (r.rim >> 2) & 1, (r.rim >> 1) & 1,
                r.rim & 1, (r.rim >> 6) & 1, (r.rim >> 5) & 1, (r.rim >> 4) & 1);
    if (ImGui::IsItemHovered())
        tip(tr("What RIM reads now: the masks of SIM (M, 1 = masked) and the interrupts waiting (I) of RST 7.5, 6.5 "
               "and 5.5."));
}

// The registers of the CPU the machine has: those of the Z80, or of the 8080 / 8085 (cpu_isa).
static void draw_registers(UiState& ui, Machine* m)
{
    CpuRegs r;
    memset(&r, 0, sizeof(r));   // a CPU fills what it has
    m->cpu->get_regs(r);
    bool editable = ui.host->paused;
    bool changed = false;
    int isa = m->cpu->cpu_isa();
    if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
    if (isa == CpuCard::ISA_Z80) draw_regs_z80(r, editable, changed);
    else draw_regs_8080(r, isa == CpuCard::ISA_8085, editable, changed);
    if (r.halted) {
        ImGui::SameLine();
        ImGui::TextUnformatted("  HALT");
    }
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
        snprintf(line, sizeof(line), "   %04X  %-12s %s", a, bytes, d.text.c_str());   // 3 columns: the marks
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();   // the top left corner of the text of the row
        if (cur || bp)   // the row in the colour of the PC or of a breakpoint (the marks tell it too)
            dl->AddRectFilled(p, ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + ImGui::GetTextLineHeightWithSpacing()),
                              ImGui::GetColorU32(cur ? theme().pc_bg : theme().bp_bg));
        ImGui::PushID(i);
        if (ImGui::Selectable(line, false)) m->set_breakpoint(a, !bp);
        if (ImGui::IsItemHovered()) ui.disasm_hover = a;   // F9 (handle_key, next frame) takes this row
        ImGui::PopID();
        // the column of the marks as in Visual Studio: a dot = a breakpoint (F9), an arrow = the PC over the dot;
        // shapes, not colours alone
        float lh = ImGui::GetTextLineHeight();
        ImVec2 c(p.x + 1.3f * ImGui::CalcTextSize(" ").x, p.y + 0.5f * lh);
        if (bp) dl->AddCircleFilled(c, 0.4f * lh, ImGui::GetColorU32(theme().error));
        if (cur) {
            float s = 0.26f * lh;
            ImVec2 t0(c.x - s, c.y - s), t1(c.x + s, c.y), t2(c.x - s, c.y + s);
            dl->AddTriangleFilled(t0, t1, t2, ImGui::GetColorU32(theme().warn));
            dl->AddTriangle(t0, t1, t2, ImGui::GetColorU32(theme().text), 1.0f);
        }
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

// The text column of the memory view: the SAPI character of a byte (core/charset.cpp) when the font in use has its
// glyph, else "." as for the control codes. The 2x3 semigraphics 80h-BFh are Unicode sextants beyond the 16-bit code
// points of the font atlas: they would show as the replacement character.
static const char* mem_char(uint8_t b)
{
    static ImFont* s_font = nullptr;
    static bool s_has[256];
    ImFont* f = ImGui::GetFont();
    if (f != s_font) {   // asked once for each font (the font of the look may change)
        s_font = f;
        for (int i = 0; i < 256; i++) {
            std::string s = sapi_code_to_utf8((uint8_t)i);
            size_t pos = 0;
            int32_t cp = utf8_next(s, pos);
            s_has[i] = i >= 0x20 && i != 0x7F && cp > 0 && cp <= IM_UNICODE_CODEPOINT_MAX && f->IsGlyphInFont((ImWchar)cp);
        }
    }
    return s_has[b] ? sapi_code_to_utf8(b) : ".";
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

    if (ui.mem_jump) {   // --ui-memory: as typed into Go to with Enter
        ui.mem_jump = false;
        jump = parse_hex16(ui.mem_goto, ui.mem_addr);
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
                chars += mem_char(b);
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
