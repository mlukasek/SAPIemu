// Serial terminal window: shows what SAPI sends on a serial channel and sends typed keys.
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <stdio.h>
#include "imgui.h"
#include "core/charset.h"
#include "core/host.h"

static const size_t MAX_LINES = 3000;

static void sync_terms(UiState& ui, Machine* m)
{
    if (ui.machine_gen != ui.host->machine_generation() || ui.terms.size() != m->serials.size()) {
        ui.terms.assign(m->serials.size(), TerminalState());
        ui.machine_gen = ui.host->machine_generation();
        if (ui.terminal_channel >= (int)ui.terms.size()) ui.terminal_channel = 0;
    }
}

static void feed(TerminalState& t, const std::string& data)
{
    for (char c : data) {
        uint8_t b = (uint8_t)c;
        bool was_cr = t.last_cr;
        t.last_cr = (b == '\r');
        if (b == '\r' || b == '\n') {
            // CR, LF and CR LF all end one line (SAPI software uses CR or CR LF)
            if (b == '\n' && was_cr) continue;
            t.lines.push_back(t.cur);
            t.cur.clear();
        } else if (b == 0x08) {
            if (!t.cur.empty()) {
                // remove the last UTF-8 character
                size_t i = t.cur.size() - 1;
                while (i > 0 && ((uint8_t)t.cur[i] & 0xC0) == 0x80) i--;
                t.cur.erase(i);
            }
        } else if (b == 0x0C) {
            t.lines.clear();
            t.cur.clear();
        } else if (b == 0x07) {
            // bell: ignored
        } else if (b >= 0x20 && b != 0x7F) {
            t.cur += sapi_code_to_utf8(b);
        } else if (b < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "^%c", '@' + b);
            t.cur += buf;
        }
        t.scroll_to_bottom = true;
    }
    if (t.lines.size() > MAX_LINES) t.lines.erase(t.lines.begin(), t.lines.begin() + (t.lines.size() - MAX_LINES));
}

static void send(UiState& ui, const std::string& codes)
{
    Machine* m = ui.host->machine();
    if (!m || ui.terminal_channel >= (int)m->serials.size()) return;
    m->serials[ui.terminal_channel]->host_send((const uint8_t*)codes.data(), codes.size());
    TerminalState& t = ui.terms[ui.terminal_channel];
    if (t.local_echo) feed(t, codes);
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

void draw_terminal(UiState& ui)
{
    ui.terminal_focused = false;
    if (!ui.show_terminal) return;
    Machine* m = ui.host->machine();
    ImGui::SetNextWindowSize(ImVec2(660, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(tr("Serial terminal"), &ui.show_terminal) || !m) {
        ImGui::End();
        return;
    }
    sync_terms(ui, m);
    if (m->serials.empty()) {
        ImGui::TextUnformatted(tr("This machine has no serial port."));
        ImGui::End();
        return;
    }
    // receive data for all channels
    for (size_t i = 0; i < m->serials.size(); i++) {
        std::string data;
        m->serials[i]->host_read(ui.terms[i].pos, data, 1 << 16);
        if (!data.empty()) feed(ui.terms[i], data);
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
    ImGui::SameLine();
    ImGui::Checkbox(tr("Local echo"), &t.local_echo);
    ImGui::SameLine();
    if (ImGui::Button(tr("Clear"))) { t.lines.clear(); t.cur.clear(); }

    ImGui::BeginChild("termtext", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
    ImGuiListClipper clipper;
    clipper.Begin((int)t.lines.size() + 1);
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            if (i < (int)t.lines.size()) ImGui::TextUnformatted(t.lines[i].c_str());
            else ImGui::Text("%s_", t.cur.c_str());
        }
    }
    if (ui.mono) ImGui::PopFont();
    if (t.scroll_to_bottom) { ImGui::SetScrollHereY(1.0f); t.scroll_to_bottom = false; }
    ImGui::EndChild();
    ui.terminal_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::End();
}
