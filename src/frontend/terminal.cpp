// Serial terminal window: a character cell terminal showing what SAPI sends on a
// serial channel; typed keys are sent to SAPI. Files go to SAPI as they are, as text
// (UTF-8 -> SAPI codes, line ends -> CR) or with XMODEM; the clipboard copies the
// mouse selection (or the whole terminal text) and pastes as typed text.
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "core/charset.h"
#include "core/host.h"
#include "core/util.h"

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

// Text from the PC: line ends (CR LF, LF, CR) as CR = Enter, Czech letters as SAPI codes.
static std::string text_codes(const std::string& utf8)
{
    std::string s;
    s.reserve(utf8.size());
    for (size_t i = 0; i < utf8.size(); i++) {
        if (utf8[i] == '\r') {
            s += '\r';
            if (i + 1 < utf8.size() && utf8[i + 1] == '\n') i++;
        } else if (utf8[i] == '\n') {
            s += '\r';
        } else {
            s += utf8[i];
        }
    }
    return sapi_codes_from_utf8(s);
}

void terminal_copy(UiState& ui)
{
    if (ui.terminal_channel >= (int)ui.terms.size()) return;
    const TerminalState& t = ui.terms[ui.terminal_channel];
    ImGui::SetClipboardText(terminal_copy_text(t).c_str());
    ui.term_status = terminal_has_selection(t) ?tr("Selection copied to the clipboard.") : tr("Copied to the clipboard.");
}

void terminal_paste(UiState& ui)
{
    const char* c = ImGui::GetClipboardText();
    if (c && *c) send(ui, text_codes(c));
}

static void SDLCALL term_dialog_done(void* userdata, const char* const* files, int)
{
    TapeDialog* d = (TapeDialog*)userdata;
    std::lock_guard<std::mutex> lk(d->mtx);
    d->result = (files && files[0]) ? files[0] : "";
    d->done = true;
}

static void term_dialog(UiState& ui, int kind)
{
    TapeDialog& d = ui.term_dlg;
    {
        std::lock_guard<std::mutex> lk(d.mtx);
        if (d.kind) return;
        d.kind = kind;
        d.done = false;
    }
    std::string where = d.last_dir.empty() ? data_path(kind == 4 ? "work" : "media") : d.last_dir;
#ifdef _WIN32
    for (char& c : where) if (c == '/') c = '\\';
#endif
    if (kind == 4) SDL_ShowSaveFileDialog(term_dialog_done, &d, ui.window, nullptr, 0, where.c_str());
    else SDL_ShowOpenFileDialog(term_dialog_done, &d, ui.window, nullptr, 0, where.c_str(), false);
}

static bool transfer_active(const UiState& ui) { return ui.xmodem.active() || ui.xmodem_rx.active(); }

// The chosen file: queued for SAPI as it is or as text, or the XMODEM sender started.
static void term_dialog_poll(UiState& ui, Machine* m)
{
    TapeDialog& d = ui.term_dlg;
    int kind;
    std::string path;
    {
        std::lock_guard<std::mutex> lk(d.mtx);
        if (!d.kind || !d.done) return;
        kind = d.kind;
        path = d.result;
        d.kind = 0;
        d.done = false;
    }
    if (path.empty() || ui.terminal_channel >= (int)m->serials.size()) return;
    d.last_dir = path_dir(path);
    SerialChannel* rch = m->serials[ui.terminal_channel];
    if (kind == 4) {   // receive: the file is written when the transfer ends
        ui.xmodem_rx_path = path;
        ui.xmodem_rx_saved = false;
        ui.xmodem_channel = ui.terminal_channel;
        ui.xmodem_pos = rch->host_end();
        ui.xmodem_last = ImGui::GetTime();
        std::string nak = ui.xmodem_rx.start();
        rch->host_send((const uint8_t*)nak.data(), nak.size());
        return;
    }
    std::vector<uint8_t> data;
    if (!file_read_all(path, data)) {
        ui.term_status = tr("Cannot read the file.");
        return;
    }
    SerialChannel* ch = m->serials[ui.terminal_channel];
    char msg[512];
    std::string name = path.substr(path.find_last_of("/\\") + 1);
    if (kind == 3) {
        ui.xmodem.start(data);
        ui.xmodem_channel = ui.terminal_channel;
        ui.xmodem_pos = ch->host_end();
        ui.xmodem_last = ImGui::GetTime();
        ui.term_status = name;
        return;
    }
    std::string codes = kind == 2 ? text_codes(std::string(data.begin(), data.end()))
                                  : std::string(data.begin(), data.end());
    ch->host_send((const uint8_t*)codes.data(), codes.size());
    snprintf(msg, sizeof(msg), tr("%s: %zu B to send"), name.c_str(), codes.size());
    ui.term_status = msg;
}

// The XMODEM receiver: packets since the last frame; NAK every 3 s until the sender starts, then
// after 10 s without data; the file written at the end.
static void xmodem_rx_pump(UiState& ui, Machine* m)
{
    XmodemReceiver& r = ui.xmodem_rx;
    if (r.state == XmodemReceiver::DONE && !ui.xmodem_rx_saved) {
        ui.xmodem_rx_saved = true;
        std::vector<uint8_t> f = r.file(ui.term_strip_sub);
        char msg[600];
        if (file_write_all(ui.xmodem_rx_path, f.data(), f.size()))
            snprintf(msg, sizeof(msg), tr("XMODEM: received %zu B into %s"), f.size(), ui.xmodem_rx_path.c_str());
        else snprintf(msg, sizeof(msg), "%s", tr("Cannot write the file."));
        ui.term_status = msg;
    }
    if (!r.active()) return;
    if (ui.xmodem_channel < 0 || ui.xmodem_channel >= (int)m->serials.size()) {
        r.cancel();
        return;
    }
    SerialChannel* ch = m->serials[ui.xmodem_channel];
    std::string in, out;
    ch->host_read(ui.xmodem_pos, in, 1 << 16);
    double now = ImGui::GetTime();
    if (!in.empty()) {
        out = r.feed(in);
        ui.xmodem_last = now;
    } else if (now - ui.xmodem_last > (r.state == XmodemReceiver::WAIT_FIRST ? 3.0 : 10.0)) {
        out = r.timeout();
        ui.xmodem_last = now;
    }
    if (!out.empty()) ch->host_send((const uint8_t*)out.data(), out.size());
}

// The XMODEM sender: what the receiver sent since the last frame, a timeout of 10 s.
static void xmodem_pump(UiState& ui, Machine* m)
{
    xmodem_rx_pump(ui, m);
    if (!ui.xmodem.active()) return;
    if (ui.xmodem_channel < 0 || ui.xmodem_channel >= (int)m->serials.size()) {
        ui.xmodem.cancel();
        return;
    }
    SerialChannel* ch = m->serials[ui.xmodem_channel];
    std::string in, out;
    ch->host_read(ui.xmodem_pos, in, 1 << 16);
    double now = ImGui::GetTime();
    if (!in.empty()) {
        out = ui.xmodem.feed(in);
        ui.xmodem_last = now;
    } else if (ui.xmodem.state != XmodemSender::WAIT_START && now - ui.xmodem_last > 10.0) {
        out = ui.xmodem.resend();
        ui.xmodem_last = now;
    }
    if (!out.empty()) ch->host_send((const uint8_t*)out.data(), out.size());
}

// The next button on this line when it fits, else on the next one (narrow windows).
static void same_line_if_fits(const char* next_label)
{
    const ImGuiStyle& st = ImGui::GetStyle();
    float w = ImGui::CalcTextSize(next_label, nullptr, true).x + st.FramePadding.x * 2.0f;
    float right = ImGui::GetItemRectMax().x + st.ItemSpacing.x + w;
    if (right <= ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x) ImGui::SameLine();
}

// Buttons for files and the clipboard, the progress of a send.
static void draw_send(UiState& ui, SerialChannel* ch)
{
    if (ImGui::Button(tr("Send file..."))) term_dialog(ui, 1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The bytes of the file as they are (e.g. Intel HEX for BOOT of MIKROBIOS)."));
    same_line_if_fits(tr("Send text file..."));
    if (ImGui::Button(tr("Send text file..."))) term_dialog(ui, 2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("UTF-8 text: Czech letters as SAPI codes, line ends as CR (Enter)."));
    same_line_if_fits(tr("Send file XMODEM..."));
    if (ImGui::Button(tr("Send file XMODEM...")) && !transfer_active(ui)) term_dialog(ui, 3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("XMODEM (checksum or CRC) to a receiving program in SAPI, e.g. DOCPM10.COM in CP/M."));
    same_line_if_fits(tr("Receive file XMODEM..."));
    if (ImGui::Button(tr("Receive file XMODEM...")) && !transfer_active(ui)) term_dialog(ui, 4);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("XMODEM from a sending program in SAPI, e.g. DOPC10.COM in CP/M. XMODEM carries no name: choose the file first."));
    same_line_if_fits(tr("Cut 1Ah off the end"));
    ImGui::Checkbox(tr("Cut 1Ah off the end"), &ui.term_strip_sub);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("XMODEM pads the last packet with 1Ah (CP/M text ends with it). Off for binary files that end with 1Ah."));
    same_line_if_fits(tr("Copy"));
    if (ImGui::Button(tr("Copy"))) terminal_copy(ui);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The selection (drag with the mouse, double click = word) or all text to the clipboard (Alt+C, Ctrl+Insert)."));
    same_line_if_fits(tr("Paste"));
    if (ImGui::Button(tr("Paste"))) terminal_paste(ui);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("The clipboard as typed text (Alt+V, Shift+Insert)."));

    char msg[256];
    const XmodemSender& x = ui.xmodem;
    const XmodemReceiver& r = ui.xmodem_rx;
    size_t left = ch->host_pending_rx();
    if (r.active()) {
        if (r.state == XmodemReceiver::WAIT_FIRST) snprintf(msg, sizeof(msg), "%s", tr("XMODEM receive: waiting for the sender (NAK)..."));
        else snprintf(msg, sizeof(msg), tr("XMODEM receive: packet %zu"), r.packets);
        ImGui::TextUnformatted(msg);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Cancel"))) {
            std::string out = ui.xmodem_rx.cancel();
            ch->host_send((const uint8_t*)out.data(), out.size());
        }
    } else if (r.state == XmodemReceiver::FAILED && !x.active() && x.state != XmodemSender::FAILED &&
               x.state != XmodemSender::DONE) {
        snprintf(msg, sizeof(msg), tr("XMODEM failed: %s"), r.error.c_str());
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", msg);
    } else if (x.active()) {
        if (x.state == XmodemSender::WAIT_START) snprintf(msg, sizeof(msg), "%s", tr("XMODEM: waiting for the receiver (NAK or C)..."));
        else snprintf(msg, sizeof(msg), tr("XMODEM: packet %zu of %zu (%s)"), x.packet + 1 > x.packets ? x.packets : x.packet + 1,
                      x.packets, x.crc ? "CRC" : "checksum");
        ImGui::TextUnformatted(msg);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Cancel")) && ui.xmodem_channel >= 0) {
            std::string out = ui.xmodem.cancel();
            ch->host_send((const uint8_t*)out.data(), out.size());
        }
    } else if (left > 0 && x.state != XmodemSender::WAIT_EOT_ACK) {
        snprintf(msg, sizeof(msg), tr("%zu B left to send"), left);
        ImGui::TextUnformatted(msg);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Stop"))) ch->host_drop_pending();
    } else if (x.state == XmodemSender::DONE) {
        snprintf(msg, sizeof(msg), tr("XMODEM: done, %zu packets"), x.packets);
        ImGui::TextDisabled("%s", msg);
    } else if (x.state == XmodemSender::FAILED) {
        snprintf(msg, sizeof(msg), tr("XMODEM failed: %s"), x.error.c_str());
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", msg);
    } else if (!ui.term_status.empty()) {
        ImGui::TextDisabled("%s", ui.term_status.c_str());
    }
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

// Mouse selection: dragging selects between cell boundaries, a double click a word, a triple click
// a line. org = top left of line 0, cw x lh = one cell.
static void term_mouse(UiState& ui, TerminalState& t, ImVec2 org, float cw, float lh)
{
    ImVec2 mp = ImGui::GetMousePos();
    float fx = (mp.x - org.x) / cw;
    int i = (int)floorf((mp.y - org.y) / lh);
    int col = (int)floorf(fx + 0.5f);   // the nearest cell boundary
    if (col < 0) col = 0;
    if (col > t.cols) col = t.cols;
    int n = (int)t.lines.size();
    // a click into the text (a click on the scrollbar makes it active first)
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) {
        int clicks = ImGui::GetMouseClickedCount(ImGuiMouseButton_Left);
        ui.term_selecting = false;
        if (clicks == 2) {
            terminal_select_word(t, i, (int)floorf(fx));
            return;
        }
        if (clicks >= 3) {
            terminal_select_line(t, i);
            return;
        }
        if (n == 0) return;
        if (i < 0) { i = 0; col = 0; }
        if (i >= n) { i = n - 1; col = t.cols; }
        t.sel = true;
        t.sel_line0 = t.sel_line1 = t.dropped + (uint64_t)i;
        t.sel_col0 = t.sel_col1 = col;
        ui.term_selecting = true;
        return;
    }
    if (!ui.term_selecting) return;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || n == 0) {
        ui.term_selecting = false;
        return;
    }
    // above or below the window: scroll a line per frame
    float top = ImGui::GetWindowPos().y, bottom = top + ImGui::GetWindowHeight();
    if (mp.y < top) ImGui::SetScrollY(ImGui::GetScrollY() - lh);
    else if (mp.y > bottom) ImGui::SetScrollY(ImGui::GetScrollY() + lh);
    if (i < 0) { i = 0; col = 0; }
    if (i >= n) { i = n - 1; col = t.cols; }
    t.sel_line1 = t.dropped + (uint64_t)i;
    t.sel_col1 = col;
}

void draw_terminal(UiState& ui)
{
    ui.terminal_focused = false;
    if (!ui.show_terminal) return;
    Machine* m = ui.host->machine();
    ImGui::SetNextWindowSize(ImVec2(700, 460), ImGuiCond_FirstUseEver);
    if (m) {
        if (ui.machine_gen != ui.host->machine_generation()) {
            if (ui.xmodem.active()) ui.xmodem.cancel();
            if (ui.xmodem_rx.active()) ui.xmodem_rx.cancel();
        }
        xmodem_pump(ui, m);
        term_dialog_poll(ui, m);
    }
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
        if (ui.xmodem_rx.active() && (int)i == ui.xmodem_channel) {   // packets are not text
            ui.terms[i].pos = m->serials[i]->host_end();
            continue;
        }
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
    same_line_if_fits(tr("Clear"));
    if (ImGui::Button(tr("Clear"))) terminal_clear(t);
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
    draw_send(ui, ch);

    // NoMove: dragging in the text selects instead of moving the window
    ImGui::BeginChild("termtext", ImVec2(0, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove);
    if (ImGui::BeginPopupContextWindow("termmenu")) {
        if (ImGui::MenuItem(tr("Copy"), "Alt+C")) terminal_copy(ui);
        if (ImGui::MenuItem(tr("Paste"), "Alt+V")) terminal_paste(ui);
        ImGui::EndPopup();
    }
    if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
    float cw = ImGui::CalcTextSize("M").x;
    float lh = ImGui::GetTextLineHeightWithSpacing();
    term_mouse(ui, t, ImGui::GetCursorScreenPos(), cw, lh);
    ImU32 sel_col = ImGui::GetColorU32(ImGuiCol_TextSelectedBg);
    ImGuiListClipper clipper;
    clipper.Begin((int)t.lines.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            int c0, c1;
            if (terminal_sel_span(t, i, &c0, &c1))
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x + cw * c0, p.y), ImVec2(p.x + cw * c1, p.y + lh), sel_col);
            ImGui::TextUnformatted(t.lines[i].empty() ? " " : terminal_line_utf8(t.lines[i]).c_str());
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
