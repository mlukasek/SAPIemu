// Serial terminal window: a character cell terminal showing what SAPI sends on a
// serial channel; typed keys are sent to SAPI.
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imgui.h"
#include "core/charset.h"
#include "core/host.h"

// ---------------------------------------------------------------- window

static void sync_terms(UiState& ui, Machine* m)
{
    if (ui.machine_gen != ui.host->machine_generation() || ui.terms.size() != m->serials.size()) {
        TerminalState t0;
        t0.emulation = ui.term_emulation;
        t0.cr_newline = ui.term_cr_newline;
        t0.local_echo = ui.term_local_echo;
        ui.terms.assign(m->serials.size(), t0);
        ui.machine_gen = ui.host->machine_generation();
        if (ui.terminal_channel >= (int)ui.terms.size()) ui.terminal_channel = 0;
    }
}

static void send(UiState& ui, const std::string& codes)
{
    Machine* m = ui.host->machine();
    if (!m || ui.terminal_channel >= (int)m->serials.size()) return;
    m->serials[ui.terminal_channel]->host_send((const uint8_t*)codes.data(), codes.size());
    TerminalState& t = ui.terms[ui.terminal_channel];
    if (t.local_echo) terminal_feed(t, codes);
}

bool terminal_key(UiState& ui, int code)
{
    if (!ui.terminal_focused) return false;
    send(ui, std::string(1, (char)code));
    return true;
}

bool terminal_text(UiState& ui, const char* utf8)
{
    if (!ui.terminal_focused) return false;
    send(ui, sapi_codes_from_utf8(utf8));
    return true;
}

// Host connections of the channel: TCP server (TeraTerm, telnet) and host COM port.
static void draw_bridges(UiState& ui, Machine* m, SerialChannel* ch)
{
    SerialBridge* tcp = nullptr;
    SerialBridge* com = nullptr;
    for (auto& b : m->bridges) {
        if (b->channel != ch) continue;
        if (strcmp(b->kind(), "tcp") == 0) tcp = b.get();
        else com = b.get();
    }
    ImGui::PushID("bridges");
    // TCP
    if (tcp) {
        if (ImGui::Button(tr("Stop TCP"))) { m->remove_bridges(ch, "tcp"); tcp = nullptr; }
        if (tcp) {
            ImGui::SameLine();
            ImGui::TextColored(tcp->client_connected() ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
                               "%s", tcp->describe().c_str());
        }
    } else {
        ImGui::SetNextItemWidth(70);
        ImGui::InputInt("##tcpport", &ui.tcp_port, 0);
        ImGui::SameLine();
        if (ImGui::Button(tr("Start TCP server"))) {
            std::string err;
            SerialBridge* b = tcp_bridge_start(ch, "127.0.0.1", ui.tcp_port, ui.tcp_dcd_follows, ui.tcp_protocol, err);
            if (b) m->add_bridge(b);
            else ui.bridge_error = err;
        }
        ImGui::SameLine();
        const char* protos[] = { "telnet", "raw", "auto" };
        ImGui::SetNextItemWidth(80);
        ImGui::Combo("##tcpproto", &ui.tcp_protocol, protos, 3);
        ImGui::SameLine();
        ImGui::Checkbox(tr("DCD = client connected"), &ui.tcp_dcd_follows);
    }
    // COM
    if (com) {
        if (ImGui::Button(tr("Close COM"))) { m->remove_bridges(ch, "com"); com = nullptr; }
        if (com) {
            ImGui::SameLine();
            ImGui::TextUnformatted(com->describe().c_str());
        }
    } else {
        ImGui::SetNextItemWidth(70);
        ImGui::InputText("##comport", ui.com_port, sizeof(ui.com_port));
        ImGui::SameLine();
        if (ImGui::Button(tr("Open COM port"))) {
            std::string err;
            SerialBridge* b = com_bridge_start(ch, ui.com_port, ch->baud, err);
            if (b) m->add_bridge(b);
            else ui.bridge_error = err;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%d Bd", ch->baud);
    }
    if (!ui.bridge_error.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", ui.bridge_error.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) ui.bridge_error.clear();
    }
    ImGui::PopID();
}

static std::string line_utf8(const std::string& l)
{
    std::string s;
    for (char c : l) {
        uint8_t b = (uint8_t)c;
        s += (b >= 0x20) ? sapi_code_to_utf8(b) : " ";
    }
    return s;
}

void draw_terminal(UiState& ui)
{
    ui.terminal_focused = false;
    if (!ui.show_terminal) return;
    Machine* m = ui.host->machine();
    ImGui::SetNextWindowSize(ImVec2(700, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(tr_id("Serial terminal"), &ui.show_terminal) || !m) {
        ImGui::End();
        return;
    }
    sync_terms(ui, m);
    if (m->serials.empty()) {
        ImGui::TextUnformatted(tr("This machine has no serial port."));
        ImGui::End();
        return;
    }
    for (size_t i = 0; i < m->serials.size(); i++) {
        std::string data;
        m->serials[i]->host_read(ui.terms[i].pos, data, 1 << 16);
        if (!data.empty()) terminal_feed(ui.terms[i], data);
    }

    if (m->serials.size() > 1) {
        for (size_t i = 0; i < m->serials.size(); i++) {
            if (i) ImGui::SameLine();
            ImGui::RadioButton(m->serials[i]->name.c_str(), &ui.terminal_channel, (int)i);
        }
    } else {
        ImGui::TextUnformatted(m->serials[0]->name.c_str());
    }
    TerminalState& t = ui.terms[ui.terminal_channel];
    SerialChannel* ch = m->serials[ui.terminal_channel];
    ImGui::SameLine();
    bool conn = ch->connected;
    if (ImGui::Checkbox(tr("Terminal connected (DCD)"), &conn)) ch->connected = conn;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("ECMON and MikroMon use the serial console at boot when DCD is active."));
    ImGui::SameLine();
    bool cts = ch->cts;
    if (ImGui::Checkbox(tr("Ready (CTS)"), &cts)) ch->cts = cts;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("CTS/DSR: the terminal is ready to receive. Programs sending with RTS/CTS handshake (DOPC.COM, MikroMon USYNC) wait for it."));
    ImGui::SameLine();
    if (ImGui::Button(tr("Clear"))) { t.lines.clear(); t.row = t.col = 0; }
    const char* emus[] = { tr("SAPI (ECMON/AND-1Z)"), "ANSI/VT100", tr("No emulation (^X)") };
    ImGui::SetNextItemWidth(180);
    ImGui::Combo("##emu", &t.emulation, emus, 3);
    ImGui::SameLine();
    ImGui::Checkbox(tr("CR = new line"), &t.cr_newline);
    ImGui::SameLine();
    ImGui::Checkbox(tr("Local echo"), &t.local_echo);
    ImGui::SameLine();
    bool pacing = ch->pacing;
    if (ImGui::Checkbox(tr("No overrun"), &pacing)) ch->pacing = pacing;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Send the next character only after SAPI has read the previous one."));
    draw_bridges(ui, m, ch);

    ImGui::BeginChild("termtext", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
    float cw = ImGui::CalcTextSize("M").x;
    ImGuiListClipper clipper;
    clipper.Begin((int)t.lines.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::TextUnformatted(t.lines[i].empty() ? " " : line_utf8(t.lines[i]).c_str());
            if (i == t.row) {
                // cursor: underline block at the cursor column
                float h = ImGui::GetTextLineHeight();
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x + cw * t.col, p.y + h - 3),
                                                          ImVec2(p.x + cw * (t.col + 1), p.y + h),
                                                          ImGui::GetColorU32(ImGuiCol_Text));
            }
        }
    }
    if (ui.mono) ImGui::PopFont();
    if (t.scroll_to_bottom) { ImGui::SetScrollHereY(1.0f); t.scroll_to_bottom = false; }
    ImGui::EndChild();
    ui.terminal_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::End();
}
