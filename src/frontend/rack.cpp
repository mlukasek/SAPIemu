// Machine configurator ("Sestava"): front view of the SAPI-1 frame with the cards
// in their slots (X1 = CPU card on the right, next to the SPN and ZDR panels),
// card and jumper settings from the card catalog, opening and saving machine
// descriptions, power cycle with the edited description.
#include "frontend/i18n.h"
#include "frontend/ui.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>

#include <SDL3/SDL.h>
#include "imgui.h"

#include "cards/catalog.h"
#include "core/host.h"
#include "core/log.h"
#include "core/machine_cfg.h"
#include "core/util.h"
#include "input/keyboards.h"

using nlohmann::json;

namespace {

const char* loc(const char* en, const char* cs)
{
    return i18n_language() == LANG_CS ? cs : en;
}

std::string scalar(const json& v)
{
    if (v.is_string()) return v.get<std::string>();
    return v.dump(-1, ' ', false, json::error_handler_t::replace);
}

// colours of the frame (JZS-1A) and the aluminium front panels
const ImU32 COL_CHASSIS = IM_COL32(176, 196, 214, 255);
const ImU32 COL_CHASSIS_EDGE = IM_COL32(112, 132, 152, 255);
const ImU32 COL_INSIDE = IM_COL32(38, 42, 46, 255);
const ImU32 COL_GUIDE = IM_COL32(150, 156, 162, 255);
const ImU32 COL_PANEL = IM_COL32(214, 218, 222, 255);
const ImU32 COL_PANEL_DIM = IM_COL32(146, 149, 154, 255);
const ImU32 COL_PANEL_EDGE = IM_COL32(104, 108, 114, 255);
const ImU32 COL_TEXT = IM_COL32(18, 18, 18, 255);
const ImU32 COL_TEXT_DIM = IM_COL32(64, 64, 64, 255);
const ImU32 COL_SEL = IM_COL32(255, 196, 0, 255);
const ImU32 COL_HOVER = IM_COL32(255, 255, 255, 170);
const ImU32 COL_ERROR = IM_COL32(230, 50, 40, 255);
const ImU32 COL_LED_ON = IM_COL32(90, 240, 90, 255);
const ImU32 COL_LED_OFF = IM_COL32(40, 70, 40, 255);
const ImU32 COL_LED_RED_ON = IM_COL32(255, 52, 36, 255);
const ImU32 COL_LED_RED_OFF = IM_COL32(78, 30, 26, 255);
const ImU32 COL_LED_YEL_ON = IM_COL32(255, 214, 40, 255);
const ImU32 COL_LED_YEL_OFF = IM_COL32(80, 70, 24, 255);

const ImVec4 TXT_ERROR(1.0f, 0.42f, 0.38f, 1.0f);
const ImVec4 TXT_WARN(1.0f, 0.82f, 0.35f, 1.0f);
const ImVec4 TXT_OK(0.55f, 0.9f, 0.55f, 1.0f);

// ---------------------------------------------------------------- file dialogs

const SDL_DialogFileFilter k_machine_filter[] = { { "SAPI-1 (*.sapi)", "sapi;jsonc;json" } };

void SDLCALL dialog_done(void* userdata, const char* const* files, int)
{
    RackEditor* r = (RackEditor*)userdata;
    std::lock_guard<std::mutex> lk(r->dlg_mtx);
    r->dlg_result = (files && files[0]) ? files[0] : "";
    r->dlg_done = true;
}

bool dialog_busy(RackEditor& r)
{
    std::lock_guard<std::mutex> lk(r.dlg_mtx);
    return r.dlg_kind != RackEditor::DLG_NONE;
}

void start_dialog(UiState& ui, int kind, const std::string& location, bool folder = false)
{
    RackEditor& r = ui.rack;
    {
        std::lock_guard<std::mutex> lk(r.dlg_mtx);
        if (r.dlg_kind != RackEditor::DLG_NONE) return;
        r.dlg_kind = kind;
        r.dlg_done = false;
        r.dlg_result.clear();
    }
    std::string loc_full = location.empty() ? data_path("machines") : data_path(location);
#ifdef _WIN32
    for (char& c : loc_full) if (c == '/') c = '\\';
#endif
    const char* where = loc_full.c_str();
    if (kind == RackEditor::DLG_SAVE)
        SDL_ShowSaveFileDialog(dialog_done, &r, ui.window, k_machine_filter, 1, where);
    else if (kind == RackEditor::DLG_PARAM && folder)
        SDL_ShowOpenFolderDialog(dialog_done, &r, ui.window, where, false);
    else if (kind == RackEditor::DLG_PARAM)
        SDL_ShowOpenFileDialog(dialog_done, &r, ui.window, nullptr, 0, where, false);
    else
        SDL_ShowOpenFileDialog(dialog_done, &r, ui.window, k_machine_filter, 1, where, false);
}

void set_message(RackEditor& r, const std::string& text, bool error)
{
    r.message = text;
    r.message_error = error;
}

bool read_machine(const std::string& path, json& cfg, std::string& err)
{
    std::string text;
    if (!file_read_text(data_path(path), text)) {
        err = std::string(tr("Cannot read")) + " " + path;
        return false;
    }
    cfg = json::parse(text, nullptr, false, true);
    if (cfg.is_discarded() || !cfg.is_object()) {
        err = std::string(tr("Syntax error in")) + " " + path;
        return false;
    }
    return true;
}

// Open a description in the editor; as_template = a copy that is not saved yet.
void editor_open(UiState& ui, const std::string& path, bool as_template)
{
    RackEditor& r = ui.rack;
    json cfg;
    std::string err;
    if (!read_machine(path, cfg, err)) {
        set_message(r, err, true);
        return;
    }
    r.cfg = cfg;
    r.saved = as_template ? json() : cfg;
    r.path = as_template ? std::string() : data_relative(data_path(path));
    r.sel = 0;
    set_message(r, std::string(as_template ? tr("New machine from") : tr("Opened")) + " " + path, false);
}

bool editor_save(UiState& ui, const std::string& path)
{
    RackEditor& r = ui.rack;
    std::string full = data_path(path);
    std::string text = machine_cfg_to_jsonc(r.cfg);
    make_dirs(path_dir(full));
    if (!file_write_text(full, text)) {
        set_message(r, std::string(tr("Cannot write")) + " " + full, true);
        return false;
    }
    r.path = data_relative(full);
    r.saved = r.cfg;
    set_message(r, std::string(tr("Saved")) + " " + r.path, false);
    return true;
}

void editor_apply(UiState& ui)
{
    RackEditor& r = ui.rack;
    std::string err;
    if (!ui.host->load_config(r.cfg, r.path, err)) {
        set_message(r, std::string(tr("The machine cannot be started")) + ": " + err, true);
        return;
    }
    r.machine_gen = ui.host->machine_generation();
    set_message(r, tr("Machine switched off and on with this configuration."), false);
}

// Start a machine file (File -> Open machine, File -> Open file...).
void start_machine(UiState& ui, const std::string& rel)
{
    RackEditor& r = ui.rack;
    std::string err;
    if (!ui.host->load(rel, err)) {
        // show it in the configurator anyway (e.g. a CPU card not emulated yet)
        editor_open(ui, rel, false);
        set_message(r, std::string(tr("The machine cannot be started")) + ": " + err, true);
        r.machine_gen = ui.host->machine_generation();
        ui.show_rack = true;
        LOGE("%s", err.c_str());
    }
}

void follow_now(UiState& ui);
void new_empty(RackEditor& r);

// Carry out the action that waited for the "unsaved changes" question.
void do_pending(UiState& ui)
{
    RackEditor& r = ui.rack;
    int kind = r.pending;
    std::string path = r.pending_path;
    r.pending = RackEditor::PEND_NONE;
    if (kind != RackEditor::PEND_EXIT) r.message.clear();   // "Saved ..." was about the replaced description
    switch (kind) {
    case RackEditor::PEND_START: start_machine(ui, path); break;
    case RackEditor::PEND_OPEN: editor_open(ui, path, false); break;
    case RackEditor::PEND_TEMPLATE: editor_open(ui, path, true); break;
    case RackEditor::PEND_EMPTY: new_empty(r); break;
    case RackEditor::PEND_FOLLOW: follow_now(ui); break;
    case RackEditor::PEND_EXIT: ui.quit = true; break;
    default: break;
    }
}

bool editor_dirty(const RackEditor& r)
{
    return !r.cfg.is_null() && r.cfg != r.saved;
}

// An action that replaces the description in the editor: at once, or after the
// question when there are unsaved changes.
void request(UiState& ui, int kind, const std::string& path)
{
    RackEditor& r = ui.rack;
    r.pending = kind;
    r.pending_path = path;
    if (editor_dirty(r)) r.ask_unsaved = true;
    else do_pending(ui);
}

// The question: Save, Save as, Discard, Cancel. A modal popup of the main window, also
// while the configurator is hidden.
void draw_unsaved_question(UiState& ui)
{
    RackEditor& r = ui.rack;
    const char* id = "###unsaved";
    char title[160];
    snprintf(title, sizeof(title), "%s%s", tr("Unsaved changes"), id);
    if (r.ask_unsaved) {
        r.ask_unsaved = false;
        ImGui::OpenPopup(title);
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    std::string name = r.cfg.value("name", std::string("?"));
    ImGui::Text("%s", tr("The machine in the configurator has unsaved changes:"));
    ImGui::Text("  %s", name.c_str());
    ImGui::TextDisabled("  %s", r.path.empty() ? tr("(not saved yet)") : r.path.c_str());
    if (r.pending == RackEditor::PEND_FOLLOW)
        ImGui::TextWrapped("%s", tr("Another machine was started (MCP). Cancel keeps the changes in the configurator."));
    ImGui::Spacing();
    if (ImGui::Button(tr("Save"))) {
        ImGui::CloseCurrentPopup();
        if (r.path.empty()) {
            r.save_then_continue = true;
            start_dialog(ui, RackEditor::DLG_SAVE, std::string());
        } else if (editor_save(ui, r.path)) {
            do_pending(ui);
        } else {
            r.ask_unsaved = true;   // not saved (message in the configurator): ask again
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Save as..."))) {
        ImGui::CloseCurrentPopup();
        r.save_then_continue = true;
        start_dialog(ui, RackEditor::DLG_SAVE, r.path);
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Discard"))) {
        ImGui::CloseCurrentPopup();
        r.saved = r.cfg;   // nothing unsaved any more
        do_pending(ui);
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel"))) {
        ImGui::CloseCurrentPopup();
        r.pending = RackEditor::PEND_NONE;
    }
    ImGui::EndPopup();
}

void handle_dialog(UiState& ui)
{
    RackEditor& r = ui.rack;
    int kind;
    std::string path;
    {
        std::lock_guard<std::mutex> lk(r.dlg_mtx);
        if (r.dlg_kind == RackEditor::DLG_NONE || !r.dlg_done) return;
        kind = r.dlg_kind;
        path = r.dlg_result;
        r.dlg_kind = RackEditor::DLG_NONE;
        r.dlg_done = false;
    }
    if (path.empty()) {   // cancelled
        if (kind == RackEditor::DLG_SAVE && r.save_then_continue) {   // not saved: ask again
            r.save_then_continue = false;
            r.ask_unsaved = true;
        }
        return;
    }
    if (kind == RackEditor::DLG_OPEN) {
        request(ui, RackEditor::PEND_OPEN, path);
    } else if (kind == RackEditor::DLG_RUN) {
        request(ui, RackEditor::PEND_START, data_relative(path));
    } else if (kind == RackEditor::DLG_SAVE) {
        size_t slash = path.find_last_of("/\\");
        if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".sapi";
        bool saved = editor_save(ui, path);
        if (r.save_then_continue) {
            r.save_then_continue = false;
            if (saved) do_pending(ui);
            else r.ask_unsaved = true;
        }
    } else if (kind == RackEditor::DLG_PARAM) {
        json& cards = r.cfg["cards"];
        if (r.dlg_card < 0 || r.dlg_card >= (int)cards.size()) return;
        json& card = cards[(size_t)r.dlg_card];
        const CardInfo* ci = card_info(card.value("type", std::string()));
        for (const CardParam* p = ci ? ci->params : nullptr; p && p->path; p++)
            if (r.dlg_param == p->path) card_set_param(card, ci, p, data_relative(path));
    }
}

int card_first_slot(const json& card);

// The editor shows the description of the running machine (perhaps with unsaved changes).
bool editor_on_running(UiState& ui)
{
    return ui.host->machine() && ui.rack.path == ui.host->machine_file() && !ui.rack.cfg.is_null();
}

// The running card in `slot` of the type the editor has there, nullptr if none.
Card* running_card(UiState& ui, const json& card, int slot)
{
    Machine* m = ui.host->machine();
    if (!m) return nullptr;
    std::string type = card.value("type", std::string());
    for (auto& c : m->cards)
        if (c->slot == slot && c->type == type) return c.get();
    return nullptr;
}

DisplayDevice* running_display(UiState& ui, const json& card, int slot)
{
    return dynamic_cast<DisplayDevice*>(running_card(ui, card, slot));
}

// Parameters that take effect while running (Machine::live_param: the ink colour, the
// write mode of a RAM disk...): a change in the editor goes to the running machine at once.
void sync_live_params(UiState& ui)
{
    if (!editor_on_running(ui) || !ui.rack.cfg.contains("cards")) return;
    Machine* m = ui.host->machine();
    for (const json& card : ui.rack.cfg["cards"]) {
        Card* c = running_card(ui, card, card_first_slot(card));
        const CardInfo* ci = card_info(card.value("type", std::string()));
        json* rc = c ? m->card_config(c) : nullptr;
        if (!rc || !ci) continue;
        for (const CardParam* p = ci->params; p && p->path; p++) {
            if (p->kind == PK_SECTION || !m->live_param(c, p->path)) continue;
            json v = card_param_value(card, p);
            if (cfg_values_equal(v, card_param_value(*rc, p))) continue;
            std::string err;
            if (!m->set_live_param(c, p->path, v, err)) {
                LOGW("%s: %s", c->name.c_str(), err.c_str());
                card_set_param(*rc, ci, p, v);   // do not try again in every frame
            }
        }
    }
}

// Show the description of the running machine in the editor.
void follow_now(UiState& ui)
{
    RackEditor& r = ui.rack;
    Machine* m = ui.host->machine();
    if (!m) return;
    r.cfg = m->config;
    r.saved = r.cfg;
    r.path = ui.host->machine_file();
    r.sel = 0;
    r.message.clear();   // it was about the previous description ("Saved ...")
}

// Follow the running machine. Machines opened from the GUI ask before they start; a
// machine started by MCP asks afterwards when the editor holds unsaved changes of another
// description. The same machine started again (power cycle) keeps the changes.
void follow_machine(UiState& ui)
{
    RackEditor& r = ui.rack;
    Machine* m = ui.host->machine();
    if (!m || r.machine_gen == ui.host->machine_generation()) return;
    r.machine_gen = ui.host->machine_generation();
    if (!editor_dirty(r)) follow_now(ui);
    else if (r.path != ui.host->machine_file() && r.pending == RackEditor::PEND_NONE) {
        r.pending = RackEditor::PEND_FOLLOW;
        r.ask_unsaved = true;
    }
}

// ---------------------------------------------------------------- frame drawing
//
// Front panels after Libor Lasota's print data for the aluminium panels of his
// SAPI-1 frame (H:\SAPI-1\LiborLasota\Chassis\Potisk_ARTAX). Dimensions are in
// points (1/72 inch) as in the drawings: narrow panel 57 x 491.8, SPN two
// positions (114.2), ZDR seven positions (402.5); slot pitch 20.32 mm.

// Text rotated by 90 degrees (read from the bottom up), centred at c.
void text_vertical(ImDrawList* dl, ImVec2 c, float size, ImU32 col, const char* text)
{
    ImFont* f = ImGui::GetFont();
    ImVec2 ts = f->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    int v0 = dl->VtxBuffer.Size;
    dl->AddText(f, size, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), col, text);
    for (int i = v0; i < dl->VtxBuffer.Size; i++) {
        ImVec2 p = dl->VtxBuffer[i].pos;
        float dx = p.x - c.x, dy = p.y - c.y;
        dl->VtxBuffer[i].pos = ImVec2(c.x + dy, c.y - dx);
    }
}

const float PANEL_H = 491.8f;
const float PANEL_W = 57.0f;
const float PITCH = 57.6f;
const float SPN_W = 114.2f;
const float ZDR_W = 402.5f;

enum { PI_END = 0, PI_TEXT, PI_DSUB, PI_LED, PI_JACK, PI_FRB, PI_PORT, PI_NOTE, PI_IDC, PI_DSUB15 };

struct PanelItem {
    int kind;
    float x, y;          // centre
    const char* text;    // PI_TEXT (14 pt), PI_NOTE (12 pt, not on the print);
                         // PI_PORT: index of the connector in card_connector_ports() (13 pt);
                         // PI_LED: "r" red, "y" yellow, else green; lit by Card::panel_leds()
};

// Cards of the original design have FRB connectors in rectangular cut-outs (first set of
// Libor's prints: DSM, FDC, JPR): K3 top, K2 bottom, labels on the left; JPR has a round
// hole for the BOOT LED between them. The V boards have D-sub connectors.
const PanelItem k_pl_jpr_frb[] = {
    { PI_TEXT, 17.2f, 56.4f, "K3" }, { PI_FRB, 23.85f, 146.7f, nullptr }, { PI_LED, 19.9f, 245.9f, "r" },
    { PI_FRB, 23.85f, 345.1f, nullptr }, { PI_TEXT, 17.2f, 437.8f, "K2" },
    { PI_END, 0, 0, nullptr },
};
const PanelItem k_pl_fdc[] = {
    { PI_TEXT, 17.2f, 56.4f, "K3" }, { PI_FRB, 23.85f, 146.7f, nullptr },
    { PI_FRB, 23.85f, 345.1f, nullptr }, { PI_TEXT, 17.2f, 437.8f, "K2" },
    { PI_END, 0, 0, nullptr },
};
// DSM-1 (Tesla): K3 = X2 terminal with its port, K2 = X3 cassette
const PanelItem k_pl_dsm1[] = {
    { PI_TEXT, 17.2f, 56.4f, "K3" }, { PI_PORT, 41.0f, 56.4f, "0" }, { PI_FRB, 23.85f, 146.7f, nullptr },
    { PI_FRB, 23.85f, 345.1f, nullptr }, { PI_TEXT, 17.2f, 437.8f, "K2" }, { PI_NOTE, 41.0f, 437.8f, "MGF" },
    { PI_END, 0, 0, nullptr },
};
// DMR-1: the terminal connector where DSM-1 has it, no cassette
const PanelItem k_pl_dmr1[] = {
    { PI_TEXT, 17.2f, 56.4f, "K3" }, { PI_PORT, 41.0f, 56.4f, "0" }, { PI_FRB, 23.85f, 146.7f, nullptr },
    { PI_END, 0, 0, nullptr },
};
// AND-1, AND-1A, AND-1Z, DGD-1: one FRB connector, at the bottom position (as on the boards)
const PanelItem k_pl_frb_k2[] = {
    { PI_FRB, 23.85f, 345.1f, nullptr }, { PI_TEXT, 17.2f, 437.8f, "K2" },
    { PI_END, 0, 0, nullptr },
};
// DSM-1V: channel A on K3 (top), B on K2; the port of each next to its connector
const PanelItem k_pl_dsm[] = {
    { PI_PORT, 28.5f, 53.5f, "0" },
    { PI_TEXT, 28.5f, 71.3f, "K3" }, { PI_TEXT, 28.5f, 88.3f, "\xE2\x80\x9C" "A" "\xE2\x80\x9D" },
    { PI_DSUB, 29.0f, 146.8f, nullptr }, { PI_DSUB, 29.0f, 345.2f, nullptr },
    { PI_TEXT, 28.5f, 405.8f, "K2" }, { PI_TEXT, 28.5f, 422.8f, "\xE2\x80\x9C" "B" "\xE2\x80\x9D" },
    { PI_PORT, 28.5f, 441.5f, "1" },
    { PI_END, 0, 0, nullptr },
};

// IDE-1: IDC 40 for the disk (SV1, top), activity LED, D-sub 15 two-row for PMD32-SD (X2, bottom)
const PanelItem k_pl_ide1[] = {
    { PI_NOTE, 23.85f, 56.4f, "HDD" }, { PI_IDC, 23.85f, 146.7f, nullptr }, { PI_LED, 23.85f, 245.9f, "r" },
    { PI_DSUB15, 23.85f, 345.1f, nullptr }, { PI_NOTE, 23.85f, 437.8f, "PMD32" },
    { PI_END, 0, 0, nullptr },
};
// RPD-1: the top FRB connectors of both boards are linked by the cable KB-17 (drawn over the
// panels), the bottom one of RPD-1B goes to the floppy drives
const PanelItem k_pl_rpd1a[] = {
    { PI_TEXT, 17.2f, 56.4f, "K3" }, { PI_FRB, 23.85f, 146.7f, nullptr },
    { PI_END, 0, 0, nullptr },
};
const PanelItem k_pl_rpd1b[] = {
    { PI_TEXT, 17.2f, 56.4f, "K3" }, { PI_FRB, 23.85f, 146.7f, nullptr },
    { PI_FRB, 23.85f, 345.1f, nullptr }, { PI_TEXT, 17.2f, 437.8f, "K2" }, { PI_NOTE, 41.0f, 437.8f, "FDD" },
    { PI_END, 0, 0, nullptr },
};
// FDC-1: like RPD-1 the top connectors (X2) of both boards are linked by the same cable; the
// bottom one of FDC-1A (X3) goes to the floppy drives, that of FDC-1B (X3) has its own SEL, HL
// and MOT signals
const PanelItem k_pl_fdc1a[] = {
    { PI_TEXT, 17.2f, 56.4f, "K3" }, { PI_FRB, 23.85f, 146.7f, nullptr },
    { PI_FRB, 23.85f, 345.1f, nullptr }, { PI_TEXT, 17.2f, 437.8f, "K2" }, { PI_NOTE, 41.0f, 437.8f, "FDD" },
    { PI_END, 0, 0, nullptr },
};
const PanelItem k_pl_fdc1b[] = {
    { PI_TEXT, 17.2f, 56.4f, "K3" }, { PI_FRB, 23.85f, 146.7f, nullptr },
    { PI_FRB, 23.85f, 345.1f, nullptr }, { PI_TEXT, 17.2f, 437.8f, "K2" },
    { PI_END, 0, 0, nullptr },
};
// MFC-1A: one connector to the floppy drives (XP2)
const PanelItem k_pl_mfc1a[] = {
    { PI_TEXT, 17.2f, 56.4f, "K3" }, { PI_FRB, 23.85f, 146.7f, nullptr }, { PI_NOTE, 41.0f, 56.4f, "FDD" },
    { PI_END, 0, 0, nullptr },
};
// VGA display cards: K2 = VGA in the middle (AND-1V, DGD-1V, BGP, CGA)
const PanelItem k_pl_vga[] = {
    { PI_TEXT, 28.5f, 187.7f, "K2" }, { PI_DSUB, 29.0f, 246.0f, nullptr }, { PI_TEXT, 28.5f, 306.4f, "VGA" },
    { PI_END, 0, 0, nullptr },
};
const PanelItem k_pl_mph[] = {
    { PI_TEXT, 28.5f, 71.3f, "K4" }, { PI_TEXT, 28.5f, 88.3f, "OVL" }, { PI_DSUB, 29.0f, 146.8f, nullptr },
    { PI_TEXT, 28.5f, 222.9f, "K3" }, { PI_JACK, 29.6f, 246.0f, nullptr }, { PI_TEXT, 28.5f, 271.1f, "ZVUK" },
    { PI_DSUB, 29.0f, 345.2f, nullptr }, { PI_TEXT, 28.5f, 407.0f, "K2" },
    { PI_TEXT, 28.5f, 424.0f, "MY\xC5\xA0" },
    { PI_END, 0, 0, nullptr },
};
const PanelItem k_pl_zrd[] = {
    { PI_TEXT, 29.0f, 312.3f, "ZAM" }, { PI_LED, 19.8f, 332.3f, "g" },
    { PI_TEXT, 29.0f, 369.9f, "DSK" }, { PI_LED, 19.8f, 389.9f, "y" },
    { PI_END, 0, 0, nullptr },
};

// Layout of a card's panel by its front panel label; plain panel when unknown (cards
// without a front connector: RAM, REM, ZRMD...). `board` = board of a card of two (RPD-1).
const PanelItem* panel_layout(const CardInfo* ci, int board = 0)
{
    if (!ci) return nullptr;
    const char* p = ci->panel;
    if (!strcmp(ci->type, "RPD-1")) return board ? k_pl_rpd1b : k_pl_rpd1a;
    if (!strcmp(ci->type, "FDC-1")) return board ? k_pl_fdc1b : k_pl_fdc1a;
    if (!strcmp(ci->type, "MFC-1A")) return k_pl_mfc1a;
    if (!strcmp(ci->type, "IDE-1")) return k_pl_ide1;
    if (!strcmp(p, "JPR")) return k_pl_jpr_frb;   // all of them, JPR-1V too
    if (!strcmp(p, "FDC")) return k_pl_fdc;
    if (!strcmp(ci->type, "DSM-1")) return k_pl_dsm1;
    if (!strcmp(ci->type, "DMR-1")) return k_pl_dmr1;
    if (!strcmp(p, "DSM")) return k_pl_dsm;
    if (!strcmp(ci->type, "AND-1V") || !strcmp(ci->type, "DGD-1V") || !strcmp(p, "BGP") || !strcmp(p, "CGA"))
        return k_pl_vga;
    if (!strcmp(p, "AND") || !strcmp(p, "DGD")) return k_pl_frb_k2;   // the original display cards
    if (!strcmp(p, "MPH")) return k_pl_mph;
    if (!strcmp(ci->type, "ZRD-1V")) return k_pl_zrd;
    return nullptr;
}

const ImU32 COL_HOLE = IM_COL32(28, 30, 33, 255);
const ImU32 COL_SCREW = IM_COL32(186, 188, 192, 255);
const ImU32 COL_SCREW_SLOT = IM_COL32(90, 92, 96, 255);
const ImU32 COL_SHELL = IM_COL32(160, 162, 166, 255);
const ImU32 COL_LINE = IM_COL32(34, 31, 32, 255);

struct Panel {   // mapping of panel points to the screen
    ImDrawList* dl;
    ImVec2 o;    // top left corner of the panel
    float k;     // pixels per point
    ImVec2 p(float x, float y) const { return ImVec2(o.x + x * k, o.y + y * k); }
};

void pt_text(const Panel& P, float x, float y, float size, ImU32 col, const char* t, int align = 0, bool bold = false,
             float max_w = 0.0f)
{
    ImFont* f = ImGui::GetFont();
    float px = size * P.k;
    ImVec2 ts = f->CalcTextSizeA(px, FLT_MAX, 0.0f, t);
    if (max_w > 0 && ts.x > max_w * P.k) {   // long labels (ZRMD) shrink to the panel width
        px *= max_w * P.k / ts.x;
        ts = f->CalcTextSizeA(px, FLT_MAX, 0.0f, t);
    }
    ImVec2 c = P.p(x, y);
    float left = align == 0 ? c.x - ts.x * 0.5f : align < 0 ? c.x : c.x - ts.x;
    P.dl->AddText(f, px, ImVec2(left, c.y - ts.y * 0.5f), col, t);
    if (bold) P.dl->AddText(f, px, ImVec2(left + (px > 24 ? 1.2f : 0.7f), c.y - ts.y * 0.5f), col, t);
}

// oblong mounting hole 16.8 x 9.4 with a screw
void pt_hole(const Panel& P, float x, float y)
{
    P.dl->AddRectFilled(P.p(x, y), P.p(x + 16.8f, y + 9.4f), COL_HOLE, 4.7f * P.k);
    ImVec2 c = P.p(x + 8.4f, y + 4.7f);
    float r = 3.9f * P.k;
    P.dl->AddCircleFilled(c, r, COL_SCREW);
    P.dl->AddLine(ImVec2(c.x - r * 0.7f, c.y), ImVec2(c.x + r * 0.7f, c.y), COL_SCREW_SLOT, P.k > 0.8f ? 1.5f : 1.0f);
}

// vertical D-sub cut-out (body 28.3 x 52.1, screw tabs 21 wide, 91.9 high) with the connector in it
// `body` = half height of the shell cut-out, `tabs` = half height with the screw tabs:
// DE-9 26.05 / 45.95 points, DA-15 (two rows, 15 pins) 47 / 75
void pt_dsub(const Panel& P, float x, float y, float body = 26.05f, float tabs = 45.95f)
{
    ImDrawList* dl = P.dl;
    float k = P.k;
    dl->AddRectFilled(P.p(x - 14.15f, y - body), P.p(x + 14.15f, y + body), COL_HOLE, 2.8f * k);
    dl->AddRectFilled(P.p(x - 10.5f, y - tabs), P.p(x + 10.5f, y + tabs), COL_HOLE, 10.5f * k);
    // metal shell with the D-shaped insulator, standoffs in the tabs
    dl->AddRectFilled(P.p(x - 9.5f, y - body + 3.05f), P.p(x + 9.5f, y + body - 3.05f), COL_SHELL, 2.0f * k);
    // the connector stands upright: the long sides of the "D" are vertical, the left one is
    // longer and the short ends are bevelled towards the right
    float d_h = body - 6.05f;
    ImVec2 d[4] = { P.p(x - 6.0f, y - d_h), P.p(x + 6.0f, y - d_h + 3.0f), P.p(x + 6.0f, y + d_h - 3.0f),
                    P.p(x - 6.0f, y + d_h) };
    dl->AddConvexPolyFilled(d, 4, IM_COL32(52, 54, 58, 255));
    for (int s = -1; s <= 1; s += 2) {
        ImVec2 c = P.p(x, y + s * (tabs - 10.45f));
        dl->AddNgonFilled(c, 5.2f * k, COL_SHELL, 6);
        dl->AddCircleFilled(c, 2.2f * k, COL_HOLE);
    }
}

// IDC 40 header (2 x 20, 2.54 mm) with its black shroud and the key notch
void pt_idc(const Panel& P, float x, float y)
{
    ImDrawList* dl = P.dl;
    float k = P.k;
    dl->AddRectFilled(P.p(x - 13.0f, y - 82.0f), P.p(x + 13.0f, y + 82.0f), COL_HOLE, 1.5f * k);
    dl->AddRectFilled(P.p(x - 11.0f, y - 79.0f), P.p(x + 11.0f, y + 79.0f), IM_COL32(30, 30, 32, 255), 1.0f * k);
    dl->AddRectFilled(P.p(x - 7.0f, y - 74.0f), P.p(x + 7.0f, y + 74.0f), IM_COL32(12, 12, 14, 255));
    dl->AddRectFilled(P.p(x - 11.0f, y - 9.0f), P.p(x - 7.0f, y + 9.0f), IM_COL32(12, 12, 14, 255));   // key
    for (int i = 0; i < 20; i++) {
        float cy = y - 68.4f + i * 7.2f;
        for (int c = -1; c <= 1; c += 2)
            dl->AddRectFilled(P.p(x + c * 3.6f - 1.1f, cy - 1.1f), P.p(x + c * 3.6f + 1.1f, cy + 1.1f),
                              IM_COL32(214, 172, 74, 255));
    }
}

// FRB connector TY 513 in the rectangular cut-out of the print (22.7 x 150.2 points):
// the green face of the connector, two rows of gold contacts, a hole at each end
const float FRB_HALF = 75.1f;   // half height of the cut-out

void pt_frb(const Panel& P, float x, float y)
{
    ImDrawList* dl = P.dl;
    float k = P.k;
    dl->AddRectFilled(P.p(x - 11.35f, y - FRB_HALF), P.p(x + 11.35f, y + FRB_HALF), COL_HOLE, 3.0f * k);
    dl->AddRectFilled(P.p(x - 9.6f, y - 73.4f), P.p(x + 9.6f, y + 73.4f), IM_COL32(32, 112, 72, 255), 1.5f * k);
    for (int s = -1; s <= 1; s += 2)   // fixing holes at the ends
        dl->AddNgonFilled(P.p(x, y + s * 66.5f), 3.6f * k, COL_HOLE, 6);
    const ImU32 gold = IM_COL32(214, 172, 74, 255), pin = IM_COL32(120, 84, 30, 255);
    for (int i = 0; i < 15; i++) {   // contacts, two rows
        float cy = y - 56.0f + i * 8.0f;
        for (int c = -1; c <= 1; c += 2) {
            ImVec2 pc = P.p(x + c * 3.6f, cy);
            dl->AddCircleFilled(pc, 2.5f * k, gold);
            dl->AddCircleFilled(pc, 1.1f * k, pin);
        }
    }
}

void pt_led(const Panel& P, float x, float y, ImU32 on)
{
    P.dl->AddCircleFilled(P.p(x, y), 8.5f * P.k, COL_HOLE);
    P.dl->AddCircleFilled(P.p(x, y), 5.6f * P.k, on);
    if (on != COL_LED_OFF) P.dl->AddCircleFilled(P.p(x - 1.6f, y - 1.6f), 1.8f * P.k, IM_COL32(220, 255, 220, 200));
}

void pt_jack(const Panel& P, float x, float y)
{
    P.dl->AddCircleFilled(P.p(x, y), 11.35f * P.k, COL_HOLE);
    P.dl->AddCircle(P.p(x, y), 8.0f * P.k, COL_SHELL, 0, 2.5f * P.k);
    P.dl->AddCircleFilled(P.p(x, y), 3.2f * P.k, IM_COL32(10, 10, 10, 255));
}

// horizontal D-sub 9 (keyboard connectors on SPN)
void pt_de9(const Panel& P, float x, float y)
{
    P.dl->AddRectFilled(P.p(x - 20.3f, y - 11.8f), P.p(x + 20.3f, y + 11.8f), COL_HOLE, 3.0f * P.k);
    ImVec2 d[4] = { P.p(x - 12.5f, y - 6.5f), P.p(x + 12.5f, y - 6.5f), P.p(x + 10.0f, y + 6.5f),
                    P.p(x - 10.0f, y + 6.5f) };
    P.dl->AddConvexPolyFilled(d, 4, COL_SHELL);
    P.dl->AddCircleFilled(P.p(x - 16.6f, y), 2.4f * P.k, COL_SHELL);
    P.dl->AddCircleFilled(P.p(x + 16.6f, y), 2.4f * P.k, COL_SHELL);
}

void panel_plate(const Panel& P, float w, bool dim)
{
    P.dl->AddRectFilled(P.p(0, 0), P.p(w, PANEL_H), dim ? COL_PANEL_DIM : COL_PANEL, 1.5f * P.k);
    P.dl->AddRect(P.p(0, 0), P.p(w, PANEL_H), COL_PANEL_EDGE, 1.5f * P.k);
}

// A narrow card panel: holes, the label at the top and the bottom, connectors.
// Flat cable KB-17 between the top FRB connectors of RPD-1A (panel A) and RPD-1B (B, on its left);
// FDC-1A and FDC-1B are linked by the same cable (manual of FDC-1).
void draw_rpd_cable(const Panel& A, const Panel& B)
{
    ImDrawList* dl = A.dl;
    float k = A.k;
    const float x = 23.85f, y = 146.7f;
    const ImU32 hood = IM_COL32(70, 72, 76, 255), ribbon = IM_COL32(178, 180, 184, 235), wire = IM_COL32(150, 152, 156, 255);
    // the ribbon from the plug on B to the plug on A, over the panels
    ImVec2 b0 = B.p(x + 8.0f, y - 52.0f), a1 = A.p(x - 8.0f, y + 52.0f);
    dl->AddRectFilled(b0, a1, ribbon, 1.0f * k);
    for (float t = -48.0f; t <= 48.0f; t += 6.0f)
        dl->AddLine(B.p(x + 8.0f, y + t), A.p(x - 8.0f, y + t), wire, 1.0f);
    dl->AddRectFilled(B.p(x - 2.0f, y - 6.0f), B.p(x + 8.0f, y + 6.0f), ribbon);   // strain relief
    for (const Panel* P : { &A, &B }) {   // the plugs on the connectors
        dl->AddRectFilled(P->p(x - 10.5f, y - 70.0f), P->p(x + 10.5f, y + 70.0f), hood, 2.0f * k);
        dl->AddRect(P->p(x - 10.5f, y - 70.0f), P->p(x + 10.5f, y + 70.0f), IM_COL32(40, 42, 46, 255), 2.0f * k);
    }
    float fs = 9.0f * k;
    ImVec2 c = ImVec2((b0.x + a1.x) * 0.5f, (b0.y + a1.y) * 0.5f);
    ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0.0f, "KB-17");
    dl->AddText(ImGui::GetFont(), fs, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), IM_COL32(40, 40, 44, 255), "KB-17");
}

void draw_card_panel(const Panel& P, const CardInfo* ci, const std::string& type,
                     const std::vector<std::string>& ports, bool error, bool show_type, int board = 0,
                     unsigned leds = 0, Card* live = nullptr)
{
    int led = 0;   // index of the next LED (Card::panel_leds)
    char led_col[4] = { 0, 0, 0, 0 };   // colours set by the card's state (Card::panel_led_color)
    if (live)
        for (int i = 0; i < 4; i++) led_col[i] = live->panel_led_color(i);
    bool dim = !ci || !ci->emulated;
    panel_plate(P, PANEL_W, dim);
    pt_hole(P, 12.8f, 4.4f);
    pt_hole(P, 12.8f, 478.1f);
    ImU32 tc = dim ? COL_TEXT_DIM : COL_TEXT;
    const char* label = ci ? ci->panel : "?";
    pt_text(P, 28.5f, 33.6f, 20.0f, tc, label, 0, false, 50.0f);
    pt_text(P, 28.5f, 461.6f, 20.0f, tc, label, 0, false, 50.0f);
    // serial cards are often there more than once: the port of each connector next to it
    for (const PanelItem* it = panel_layout(ci, board); it && it->kind != PI_END; it++) {
        switch (it->kind) {
        case PI_TEXT: pt_text(P, it->x, it->y, 14.0f, tc, it->text); break;
        case PI_NOTE: pt_text(P, it->x, it->y, 12.0f, tc, it->text); break;
        case PI_PORT: {
            size_t i = (size_t)(it->text[0] - '0');
            if (i < ports.size() && !ports[i].empty()) pt_text(P, it->x, it->y, 13.0f, tc, ports[i].c_str(), 0, false, 50.0f);
            break;
        }
        case PI_DSUB: pt_dsub(P, it->x, it->y); break;
        case PI_FRB: pt_frb(P, it->x, it->y); break;
        case PI_IDC: pt_idc(P, it->x, it->y); break;
        case PI_DSUB15: pt_dsub(P, it->x, it->y, 47.0f, 75.0f); break;
        case PI_LED: {
            char c = led < 4 && led_col[led] ? led_col[led] : it->text ? it->text[0] : 'g';
            bool on = (leds >> led++) & 1u;
            ImU32 col = c == 'r' ? (on ? COL_LED_RED_ON : COL_LED_RED_OFF)
                      : c == 'y' ? (on ? COL_LED_YEL_ON : COL_LED_YEL_OFF) : (on ? COL_LED_ON : COL_LED_OFF);
            pt_led(P, it->x, it->y, col);
            break;
        }
        case PI_JACK: pt_jack(P, it->x, it->y); break;
        default: break;
        }
    }
    if (show_type) {
        // the exact card type across the panel, in the largest free gap between the panel items
        float lo = 50.0f, hi = 445.0f;   // between the labels
        float best_a = lo, best_b = hi;
        float occ[32][2];
        int no = 0;
        for (const PanelItem* it = panel_layout(ci, board); it && it->kind != PI_END && no < 32; it++) {
            float h = it->kind == PI_DSUB ? 46.0f : it->kind == PI_FRB ? FRB_HALF : it->kind == PI_IDC ? 82.0f
                    : it->kind == PI_DSUB15 ? 75.0f : it->kind == PI_JACK ? 12.0f : 9.0f;
            occ[no][0] = it->y - h - 4.0f;
            occ[no][1] = it->y + h + 4.0f;
            no++;
        }
        if (no > 0) {
            // walk the occupied intervals in order of their start, merging overlaps
            for (int i = 1; i < no; i++)
                for (int j = i; j > 0 && occ[j][0] < occ[j - 1][0]; j--) {
                    float t0 = occ[j][0], t1 = occ[j][1];
                    occ[j][0] = occ[j - 1][0];
                    occ[j][1] = occ[j - 1][1];
                    occ[j - 1][0] = t0;
                    occ[j - 1][1] = t1;
                }
            best_b = best_a;
            float y = lo;   // start of the current free run
            for (int i = 0; i <= no; i++) {
                float start = i < no ? occ[i][0] : hi;
                if (start > hi) start = hi;
                if (start - y > best_b - best_a) { best_a = y; best_b = start; }
                if (i < no && occ[i][1] > y) y = occ[i][1];
            }
        }
        // FRB cut-outs (and the connectors of IDE-1) sit left of the centre: the free strip
        // right of them takes the type
        bool left = false;
        for (const PanelItem* it = panel_layout(ci, board); it && it->kind != PI_END; it++)
            left |= it->kind == PI_FRB || it->kind == PI_IDC || it->kind == PI_DSUB15;
        float cx = PANEL_W * 0.5f, cy = (best_a + best_b) * 0.5f;
        if (left) {
            best_a = 70.0f;
            best_b = 422.0f;
            cx = 46.6f;   // between the cut-outs (FRB x 12.5-35.2, D-sub 15 to 38.0) and the edge
            cy = 245.9f;
        }
        ImFont* f = ImGui::GetFont();
        float px = ImGui::GetFontSize() * 0.85f;
        ImVec2 ts = f->CalcTextSizeA(px, FLT_MAX, 0.0f, type.c_str());
        float room = (best_b - best_a) * P.k - 8.0f;
        if (ts.x > room && room > 0) {
            px *= room / ts.x;
            ts = f->CalcTextSizeA(px, FLT_MAX, 0.0f, type.c_str());
        }
        ImVec2 c = P.p(cx, cy);
        float hw = ts.y * 0.5f + 2.0f, hh = ts.x * 0.5f + 4.0f;
        P.dl->AddRectFilled(ImVec2(c.x - hw, c.y - hh), ImVec2(c.x + hw, c.y + hh), IM_COL32(20, 24, 30, 205), 3.0f);
        text_vertical(P.dl, c, px, IM_COL32(240, 240, 240, 255), type.c_str());
    }
    if (error) P.dl->AddCircleFilled(P.p(PANEL_W - 9.0f, 24.0f), 6.0f * P.k + 1.5f, COL_ERROR);
}

void draw_spn(const Panel& P, bool power)
{
    panel_plate(P, SPN_W, false);
    pt_hole(P, 12.7f, 4.4f);
    pt_hole(P, 84.7f, 4.4f);
    pt_hole(P, 12.7f, 478.1f);
    pt_hole(P, 84.7f, 478.1f);
    pt_text(P, 8.4f, 33.6f, 20.0f, COL_TEXT, "SPN", -1);
    pt_text(P, 8.4f, 461.6f, 20.0f, COL_TEXT, "SPN", -1);
    pt_text(P, 16.0f, 72.0f, 14.0f, COL_TEXT, "ZDROJ", -1);
    float lw = 1.4f * P.k;
    P.dl->AddRect(P.p(17.0f, 85.4f), P.p(93.5f, 205.0f), COL_LINE, 3.8f * P.k, 0, lw);
    P.dl->AddRect(P.p(17.0f, 229.5f), P.p(93.5f, 406.9f), COL_LINE, 3.8f * P.k, 0, lw);
    const char* v1[4] = { "+12V", "+5V", "-5V", "-12V" };
    for (int i = 0; i < 4; i++) {
        float y = 101.9f + 28.8f * i;
        pt_text(P, 60.0f, y, 14.0f, COL_TEXT, v1[i], 1);
        pt_led(P, 83.5f, y, power ? COL_LED_ON : COL_LED_OFF);
    }
    for (int i = 0; i < 2; i++) {
        float y = 245.9f + 28.8f * i;
        pt_text(P, 60.0f, y, 14.0f, COL_TEXT, v1[i], 1);
        pt_led(P, 83.5f, y, power ? COL_LED_ON : COL_LED_OFF);
    }
    pt_de9(P, 57.0f, 316.8f);
    pt_de9(P, 57.0f, 373.5f);
    pt_text(P, 15.3f, 422.2f, 14.0f, COL_TEXT, "K2/K3", -1);
}

int card_first_slot(const json& card)
{
    long long v = 0;
    if (card.contains("slot")) {
        if (card["slot"].is_number_integer()) v = card["slot"].get<long long>();
        else if (card["slot"].is_string()) parse_number(card["slot"].get<std::string>(), v);
    }
    return (int)v;
}

bool slot_has_error(const std::vector<CfgProblem>& pr, int slot)
{
    for (const CfgProblem& p : pr)
        if (p.slot == slot && p.error) return true;
    return false;
}

bool panel_button(const char* id, ImVec2 a, ImVec2 b)
{
    ImGui::SetCursorScreenPos(a);
    return ImGui::InvisibleButton(id, ImVec2(b.x - a.x, b.y - a.y));
}

void card_menu_items(UiState& ui, int slot, bool combo);

void draw_frame(UiState& ui, const std::vector<CfgProblem>& problems)
{
    RackEditor& r = ui.rack;
    EmuHost& host = *ui.host;
    Machine* m = host.machine();
    const json& cfg = r.cfg;
    int n = machine_cfg_slots(cfg);

    // scale: fit the window width, within readable limits
    float u = ImGui::GetFontSize();
    const float margin = 16.0f, gap = 4.0f, label_band = 26.0f;
    float total_w = margin + n * PITCH + gap + SPN_W + 1.0f + ZDR_W + margin;
    float total_h = margin + PANEL_H + label_band + margin * 0.5f;
    float avail = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize;
    float k = avail / total_w;
    float kmin = 0.034f * u, kmax = 0.075f * u;
    if (k < kmin) k = kmin;
    if (k > kmax) k = kmax;
    float w = total_w * k, h = total_h * k;

    ImGui::Checkbox(tr("Card types on the panels"), &r.show_types);
    ImGui::BeginChild("frame", ImVec2(0, h + ImGui::GetStyle().ScrollbarSize + 4), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), COL_CHASSIS, 10.0f * k);
    dl->AddRect(o, ImVec2(o.x + w, o.y + h), COL_CHASSIS_EDGE, 10.0f * k, 0, 2.0f);
    float y0 = o.y + margin * k, y1 = y0 + PANEL_H * k;
    float sx0 = o.x + margin * k;
    dl->AddRectFilled(ImVec2(sx0, y0), ImVec2(sx0 + n * PITCH * k, y1), COL_INSIDE);

    auto slot_x = [&](int s) { return sx0 + (float)(n - s) * PITCH * k; };   // left edge of slot s

    // empty slots: card guides at the top and the bottom; slot names below
    for (int s = 1; s <= n; s++) {
        float cx = slot_x(s) + PITCH * k * 0.5f;
        dl->AddRectFilled(ImVec2(cx - 3.5f * k, y0), ImVec2(cx + 3.5f * k, y0 + 30.0f * k), COL_GUIDE);
        dl->AddRectFilled(ImVec2(cx - 3.5f * k, y1 - 30.0f * k), ImVec2(cx + 3.5f * k, y1), COL_GUIDE);
        char lab[8];
        snprintf(lab, sizeof(lab), "X%d", s);
        float fs = 13.0f * k < 0.75f * u ? 0.75f * u : 13.0f * k;
        ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0.0f, lab);
        dl->AddText(ImGui::GetFont(), fs, ImVec2(cx - ts.x * 0.5f, y1 + (label_band * k - ts.y) * 0.5f), COL_TEXT, lab);
    }

    // cards (a card of two boards gets two panels)
    if (cfg.contains("cards") && cfg["cards"].is_array()) {
        for (size_t i = 0; i < cfg["cards"].size(); i++) {
            const json& c = cfg["cards"][i];
            int s = card_first_slot(c);
            int wd = card_cfg_width(c);
            if (s < 1 || s + wd - 1 > n) continue;
            std::string type = c.value("type", std::string("?"));
            const CardInfo* ci = card_info(type);
            std::vector<std::string> ports = card_connector_ports(c);   // shown at the connectors, not in the name
            Card* live = editor_on_running(ui) ? running_card(ui, c, s) : nullptr;   // its LEDs follow the emulation
            unsigned leds = live ? live->panel_leds() : 0;
            for (int b = 0; b < wd; b++) {
                Panel P = { dl, ImVec2(slot_x(s + b) + (PITCH - PANEL_W) * 0.5f * k, y0), k };
                // a card of two boards (RPD-1): RPD-1A in its slot, RPD-1B left of it, both named
                std::string shown = wd > 1 ? type + (char)('A' + b) : type;
                draw_card_panel(P, ci, shown, ports, slot_has_error(problems, s + b), r.show_types, b, b == 0 ? leds : 0,
                                b == 0 ? live : nullptr);
            }
            if (wd == 2 && (type == "RPD-1" || type == "FDC-1")) {
                Panel A = { dl, ImVec2(slot_x(s) + (PITCH - PANEL_W) * 0.5f * k, y0), k };
                Panel B = { dl, ImVec2(slot_x(s + 1) + (PITCH - PANEL_W) * 0.5f * k, y0), k };
                draw_rpd_cable(A, B);
            }
        }
    }
    // hit areas per slot, highlight
    int clicked = -1, ctx = -1, hovered = -1;
    for (int s = 1; s <= n; s++) {
        int idx = machine_cfg_card_at(cfg, s);
        int first = idx >= 0 ? card_first_slot(cfg["cards"][(size_t)idx]) : s;
        char id[16];
        snprintf(id, sizeof(id), "slot%d", s);
        if (panel_button(id, ImVec2(slot_x(s), y0), ImVec2(slot_x(s) + PITCH * k, y1))) clicked = first;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ctx = first;
        if (ImGui::IsItemHovered()) {
            hovered = first;
            if (idx >= 0) {
                const json& c = cfg["cards"][(size_t)idx];
                const CardInfo* ci = card_info(c.value("type", std::string()));
                ImGui::SetTooltip("X%d: %s\n%s%s", first, card_display_name(c).c_str(),
                                  ci ? loc(ci->en, ci->cs) : tr("unknown card"),
                                  ci && !ci->emulated ? loc("\n(not emulated yet)", "\n(zatím neemulováno)") : "");
            } else {
                ImGui::SetTooltip("X%d: %s", s, tr("empty slot"));
            }
        }
    }
    auto outline = [&](int s, ImU32 col, float th) {
        int idx = machine_cfg_card_at(cfg, s);
        int wd = idx >= 0 ? card_cfg_width(cfg["cards"][(size_t)idx]) : 1;
        if (s < 1 || s + wd - 1 > n) return;
        dl->AddRect(ImVec2(slot_x(s + wd - 1) - 1, y0 - 3), ImVec2(slot_x(s) + PITCH * k + 1, y1 + 3), col, 3.0f, 0, th);
    };
    if (hovered > 0 && hovered != r.sel) outline(hovered, COL_HOVER, 1.5f);
    if (r.sel > 0) outline(r.sel, COL_SEL, 2.5f);
    if (clicked > 0) r.sel = clicked;
    if (ctx > 0) {
        r.sel = ctx;
        ImGui::OpenPopup("slotmenu");
    }
    if (ImGui::BeginPopup("slotmenu")) {
        ImGui::TextDisabled("X%d", r.sel);
        card_menu_items(ui, r.sel, false);
        ImGui::EndPopup();
    }

    // SPN-1: power indicators, keyboard connectors
    float px = sx0 + n * PITCH * k + gap * k;
    Panel S = { dl, ImVec2(px, y0), k };
    draw_spn(S, m != nullptr);

    // ZDR: CPU frequency, power, reset, turbo, BIOS
    Panel Z = { dl, ImVec2(px + (SPN_W + 1.0f) * k, y0), k };
    panel_plate(Z, ZDR_W, false);
    pt_hole(Z, 12.8f, 4.4f);
    pt_hole(Z, 372.8f, 4.4f);
    pt_hole(Z, 12.8f, 478.1f);
    pt_hole(Z, 372.8f, 478.1f);
    pt_text(Z, 8.5f, 33.6f, 20.0f, COL_TEXT, "ZDR", -1);
    pt_text(Z, 8.5f, 461.6f, 20.0f, COL_TEXT, "ZDR", -1);
    pt_text(Z, 48.5f, 89.0f, 14.0f, COL_TEXT, "FREKVENCE CPU", -1);
    dl->AddRect(Z.p(49.6f, 101.4f), Z.p(182.8f, 192.1f), COL_PANEL_EDGE, 0, 0, 1.0f);
    dl->AddRectFilled(Z.p(55.3f, 106.8f), Z.p(177.2f, 186.7f), IM_COL32(12, 14, 12, 255));
    char freq[16];
    if (m && !host.paused && host.speed_percent > 0) {
        // the effective CPU clock: the clock of the CPU card (JPR-1V TURBO included) times the
        // measured pace of the emulation; whole MHz from 100 up (the display has 3 digits)
        double mhz = (m->cpu ? m->cpu->cpu_display_hz() : m->bus.cpu_hz) / 1e6 * host.speed_percent / 100.0;
        snprintf(freq, sizeof(freq), mhz >= 99.95 ? "%.0f" : "%.1f", mhz);
    } else
        snprintf(freq, sizeof(freq), m ? "--" : "");
    pt_text(Z, 116.2f, 147.0f, 60.0f, IM_COL32(60, 255, 120, 255), freq);
    pt_text(Z, 219.9f, 178.5f, 48.0f, COL_TEXT, "SAPI 1", -1, true);
    // TURBO and BIOS go to the CPU card, as on the real frame (JPR-1V: the TURBO switch and the
    // BIOS jumper wired to the buttons); lit = TURBO on / EEPROM bank 1; grey = the card has none
    Card* cpu = m ? m->cpu : nullptr;
    bool has_turbo = cpu && m->live_param(cpu, "turbo"), has_bios = cpu && m->live_param(cpu, "bios");
    bool turbo_on = has_turbo && m->cpu->cpu_turbo();
    json* cpu_cfg = has_bios ? m->card_config(cpu) : nullptr;
    bool bank1 = cpu_cfg && cpu_cfg->value("bios", std::string("fitted")) == "open";
    const ImU32 btn_off = IM_COL32(236, 236, 236, 255), btn_none = IM_COL32(168, 170, 172, 255);
    struct Btn { const char* label; ImU32 col; };
    const Btn btns[4] = {
        { "NAPÁJENÍ", m ? IM_COL32(80, 230, 130, 255) : IM_COL32(34, 92, 58, 255) },
        { "RESET", IM_COL32(214, 52, 48, 255) },
        { "TURBO", !has_turbo ? btn_none : turbo_on ? IM_COL32(255, 160, 60, 255) : btn_off },
        { "BIOS", !has_bios ? btn_none : bank1 ? IM_COL32(255, 228, 110, 255) : btn_off },
    };
    for (int i = 0; i < 4; i++) {
        float bx = 48.2f + 85.05f * i;
        pt_text(Z, bx + 25.5f, 349.8f, 14.0f, COL_TEXT, btns[i].label);
        ImVec2 ba = Z.p(bx, 362.2f), bb = Z.p(bx + 51.0f, 413.2f);
        dl->AddRectFilled(ba, bb, IM_COL32(44, 46, 50, 255), 3.0f * k);
        // the latching POWER button sits deeper while switched on
        float in = (i == 0 && m) ? 2.2f : 0.0f;
        dl->AddRectFilled(Z.p(bx + 2.8f + in, 365.0f + in), Z.p(bx + 48.2f - in, 410.4f - in), btns[i].col, 4.0f * k);
        dl->AddRect(Z.p(bx + 2.8f, 365.0f), Z.p(bx + 48.2f, 410.4f), IM_COL32(0, 0, 0, 90), 4.0f * k);
        char id[16];
        snprintf(id, sizeof(id), "btn%d", i);
        if (panel_button(id, ba, bb)) {
            if (i == 0) {
                rack_power_switch(ui);
                m = host.machine();   // the old machine is gone
            }
            else if (i == 1 && m) m->reset();
            else if (i == 2 && has_turbo) rack_set_live_param(ui, cpu, "turbo", !turbo_on);
            else if (i == 3 && has_bios) rack_set_live_param(ui, cpu, "bios", bank1 ? "fitted" : "open");
        }
        if (ImGui::IsItemHovered()) {
            const char* tip = i == 0 ? (m ? tr("Switch off") : tr("Switch on with this configuration"))
                            : i == 1 ? tr("Reset (F12)")
                            : i == 2 ? (has_turbo ? tr("TURBO of the CPU card: CPU 2 / 4 MHz")
                                                  : tr("The CPU card has no TURBO: its clock is set by jumpers. "
                                                       "Emulation speed: F8 or Machine -> Emulation speed"))
                                     : (has_bios ? tr("BIOS jumper of JPR-1V: EEPROM bank 0 (MikroBIOS) / bank 1 (ECMON), "
                                                      "read after RESET")
                                                 : tr("BIOS: the EEPROM bank of JPR-1V (this CPU card has none)"));
            ImGui::SetTooltip("%s", tip);
        }
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && clicked < 0 &&
        !ImGui::IsAnyItemHovered())
        r.sel = 0;   // click on the frame itself: bus settings

    ImGui::SetCursorScreenPos(o);
    ImGui::Dummy(ImVec2(w, h));
    ImGui::EndChild();
}

// ---------------------------------------------------------------- properties

bool card_allowed(const json& cfg, const CardInfo* ci, int slot, int replacing, std::string& why)
{
    int n = machine_cfg_slots(cfg);
    bool x1 = machine_cfg_cpu_in_x1(cfg);
    if (x1 && slot == 1 && ci->cls != CC_CPU) { why = tr("X1 is reserved for the CPU card"); return false; }
    if (x1 && slot != 1 && ci->cls == CC_CPU) { why = tr("The CPU card belongs to X1"); return false; }
    if (slot + ci->width - 1 > n) { why = tr("Does not fit into the frame"); return false; }
    for (int k = 0; k < ci->width; k++) {
        int idx = machine_cfg_card_at(cfg, slot + k);
        if (idx >= 0 && idx != replacing) { why = tr("The next slot is occupied"); return false; }
    }
    return true;
}

void sort_cards(json& cfg)
{
    if (!cfg.contains("cards") || !cfg["cards"].is_array()) return;
    std::vector<json> v(cfg["cards"].begin(), cfg["cards"].end());
    std::stable_sort(v.begin(), v.end(), [](const json& a, const json& b) {
        return card_first_slot(a) < card_first_slot(b);
    });
    cfg["cards"] = v;
}

void put_card(RackEditor& r, int slot, const CardInfo* ci)
{
    if (!r.cfg.contains("cards") || !r.cfg["cards"].is_array()) r.cfg["cards"] = json::array();
    int idx = machine_cfg_card_at(r.cfg, slot);
    if (idx >= 0) r.cfg["cards"].erase((size_t)idx);
    if (ci) r.cfg["cards"].push_back(card_new_config(ci, slot));
    sort_cards(r.cfg);
}

// Card list for a slot: emulated cards first.
void card_menu_items(UiState& ui, int slot, bool combo)
{
    RackEditor& r = ui.rack;
    int idx = machine_cfg_card_at(r.cfg, slot);
    std::string cur = idx >= 0 ? r.cfg["cards"][(size_t)idx].value("type", std::string()) : std::string();
    if (ImGui::Selectable(loc("(empty)", "(prázdný)"), cur.empty())) put_card(r, slot, nullptr);   // a slot
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) ImGui::SeparatorText(tr("not emulated yet"));
        for (const CardInfo* ci : card_catalog()) {
            if (ci->emulated != (pass == 0)) continue;
            std::string why;
            bool ok = card_allowed(r.cfg, ci, slot, idx, why);
            char label[200];
            snprintf(label, sizeof(label), "%-10s %s", ci->type, loc(ci->en, ci->cs));
            ImGui::BeginDisabled(!ok);
            if (ImGui::Selectable(label, cur == ci->type) && cur != ci->type) put_card(r, slot, ci);
            ImGui::EndDisabled();
            if (!ok && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", why.c_str());
        }
    }
    (void)combo;
}

void draw_param(UiState& ui, json& card, int card_idx, const CardInfo* ci, const CardParam* p)
{
    RackEditor& r = ui.rack;
    float u = ImGui::GetFontSize();
    // a parameter the running card takes at once is marked
    Card* live = editor_on_running(ui) ? running_card(ui, card, card_first_slot(card)) : nullptr;
    std::string label_s = loc(p->en, p->cs);
    if (live && ui.host->machine()->live_param(live, p->path)) label_s += tr(" (applies at once)");
    const char* label = label_s.c_str();
    json v = card_param_value(card, p);
    ImGui::PushID(p->path);
    ImGui::SetNextItemWidth(16.0f * u);
    switch (p->kind) {
    case PK_CHOICE: {
        int oi = param_option_index(p, v);
        std::string preview = oi >= 0 ? loc(p->options[oi].en, p->options[oi].cs)
                                      : std::string(tr("custom")) + ": " + scalar(v);
        if (ImGui::BeginCombo(label, preview.c_str())) {
            for (int i = 0; p->options[i].value; i++)
                if (ImGui::Selectable(loc(p->options[i].en, p->options[i].cs), i == oi))
                    card_set_param(card, ci, p, json::parse(p->options[i].value));
            ImGui::EndCombo();
        }
        break;
    }
    case PK_INT: {
        long long x = 0;
        if (v.is_number_integer()) x = v.get<long long>();
        else if (v.is_string()) parse_number(v.get<std::string>(), x);
        if (ImGui::InputScalar(label, ImGuiDataType_S64, &x)) {
            if (x < p->min) x = p->min;
            if (x > p->max) x = p->max;
            card_set_param(card, ci, p, x);
        }
        break;
    }
    case PK_HEX: {
        long long x = 0;
        if (v.is_number_integer()) x = v.get<long long>();
        else if (v.is_string()) parse_number(v.get<std::string>(), x);
        char buf[24];
        snprintf(buf, sizeof(buf), "%02llXh", x);
        ImGui::InputText(label, buf, sizeof(buf));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            long long y;
            std::string t = buf;
            if (!t.empty() && t.back() != 'h' && t.back() != 'H') t += "h";
            if (parse_number(t, y) && y >= p->min && y <= p->max) {
                snprintf(buf, sizeof(buf), "%02llXh", y);
                card_set_param(card, ci, p, std::string(buf));
            }
        }
        break;
    }
    case PK_BOOL: {
        bool b = v.is_boolean() ? v.get<bool>() : false;
        if (ImGui::Checkbox(label, &b)) card_set_param(card, ci, p, b);
        break;
    }
    case PK_SECTION: {
        bool b = v.is_boolean() && v.get<bool>();
        if (ImGui::Checkbox(label, &b)) card_set_param(card, ci, p, b);
        break;
    }
    case PK_TEXT:
    case PK_FILE:
    case PK_DIR: {
        std::string s;
        if (v.is_string()) s = v.get<std::string>();
        else if (v.is_object()) s = v.value("image", std::string());
        char buf[512];
        snprintf(buf, sizeof(buf), "%s", s.c_str());
        bool file = p->kind != PK_TEXT;
        if (file) ImGui::SetNextItemWidth(16.0f * u - 2.0f * ImGui::GetFrameHeight() - 2 * ImGui::GetStyle().ItemInnerSpacing.x);
        if (ImGui::InputText(file ? "##v" : label, buf, sizeof(buf))) {
            if (buf[0]) card_set_param(card, ci, p, std::string(buf));
            else card_set_param(card, ci, p, p->kind == PK_TEXT ? json() : json(""));
        }
        if (file) {
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            bool has_opts = p->options && p->options[0].value;
            ImGui::BeginDisabled(!has_opts);
            if (ImGui::BeginCombo("##s", "", ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft)) {
                for (int i = 0; has_opts && p->options[i].value; i++) {
                    json o = json::parse(p->options[i].value);
                    char item[300];
                    snprintf(item, sizeof(item), "%s  (%s)", loc(p->options[i].en, p->options[i].cs),
                             scalar(o).c_str());
                    if (ImGui::Selectable(item, cfg_values_equal(o, v))) card_set_param(card, ci, p, o);
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::Button("...", ImVec2(ImGui::GetFrameHeight(), 0)) && !dialog_busy(r)) {
                r.dlg_card = card_idx;
                r.dlg_param = p->path;
                start_dialog(ui, RackEditor::DLG_PARAM, s.empty() ? std::string() : s, p->kind == PK_DIR);
            }
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::TextUnformatted(label);
        }
        break;
    }
    case PK_KEYBOARD: {
        const std::vector<KeyboardPreset>& kp = keyboard_presets();
        // the faithful mode ("strict", its own checkbox) is not a part of the preset
        json plain = v;
        bool strict = false;
        if (plain.is_object() && plain.contains("strict")) {
            strict = plain["strict"].is_boolean() && plain["strict"].get<bool>();
            plain.erase("strict");
        }
        int cur = -2;
        if (v.is_null()) cur = -1;
        for (size_t i = 0; i < kp.size(); i++)
            if (json::parse(kp[i].config) == plain) cur = (int)i;
        std::string preview = cur == -1 ? tr("(none)") : cur >= 0 ? kp[(size_t)cur].name
                                                                   : std::string(tr("custom")) + ": " + scalar(v);
        if (ImGui::BeginCombo(label, preview.c_str())) {
            if (ImGui::Selectable(tr("(none)"), cur == -1)) card_set_param(card, ci, p, json());
            for (size_t i = 0; i < kp.size(); i++)
                if (ImGui::Selectable(kp[i].name, cur == (int)i)) {
                    json k = json::parse(kp[i].config);
                    if (strict) k["strict"] = true;   // keep the faithful mode with another keyboard
                    card_set_param(card, ci, p, k);
                }
            ImGui::EndCombo();
        }
        break;
    }
    default:
        break;
    }
    ImGui::PopID();
}

void draw_bus_props(RackEditor& r)
{
    float u = ImGui::GetFontSize();
    // the name and the description are long (cards, jumpers): the width of the window
    float label_w = ImGui::CalcTextSize(tr("Description")).x + ImGui::GetStyle().ItemInnerSpacing.x + u;
    float desc_w = ImGui::GetContentRegionAvail().x - label_w;
    if (desc_w < 30.0f * u) desc_w = 30.0f * u;
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", r.cfg.value("name", std::string()).c_str());
    ImGui::SetNextItemWidth(desc_w);
    if (ImGui::InputText(tr("Name"), buf, sizeof(buf))) r.cfg["name"] = std::string(buf);
    static char desc[8192];   // about 8 lines
    snprintf(desc, sizeof(desc), "%s", r.cfg.value("description", std::string()).c_str());
    float desc_h = ImGui::GetTextLineHeight() * 8.0f + ImGui::GetStyle().FramePadding.y * 2.0f;
    if (ImGui::InputTextMultiline(tr("Description"), desc, sizeof(desc), ImVec2(desc_w, desc_h),
                                  ImGuiInputTextFlags_WordWrap)) {
        if (desc[0]) r.cfg["description"] = std::string(desc);
        else r.cfg.erase("description");
    }
    if (!r.cfg.contains("bus") || !r.cfg["bus"].is_object()) r.cfg["bus"] = { { "type", "ARB-1C-8" } };
    json& bus = r.cfg["bus"];
    std::string bt = bus.value("type", std::string("ARB-1C-8"));
    const BusInfo* bi = bus_info(bt);
    ImGui::SetNextItemWidth(30.0f * u);
    if (ImGui::BeginCombo(tr("Bus"), bi ? loc(bi->en, bi->cs) : bt.c_str())) {
        for (const BusInfo* b : bus_catalog()) {
            if (ImGui::Selectable(loc(b->en, b->cs), b == bi)) {
                bus["type"] = b->type;
                bus.erase("slots");   // positions follow the bus type
                if (!b->has_jp3) bus.erase("jp3");
                else if (!bus.contains("jp3")) bus["jp3"] = "2-3";
            }
        }
        ImGui::EndCombo();
    }
    if (bi && bi->has_jp3) {
        std::string jp3 = bus.value("jp3", std::string("2-3"));
        ImGui::SetNextItemWidth(30.0f * u);
        const char* l23 = loc("2-3: CPU card in X1 (normal)", "2-3: procesorová karta v X1 (běžné)");
        const char* l12 = loc("1-2: address buffers off, CPU card anywhere (PKT-1)",
                              "1-2: budiče adres vypnuté, procesor kdekoli (PKT-1)");
        if (ImGui::BeginCombo("JP3", jp3 == "1-2" ? l12 : l23)) {
            if (ImGui::Selectable(l23, jp3 != "1-2")) bus["jp3"] = "2-3";
            if (ImGui::Selectable(l12, jp3 == "1-2")) bus["jp3"] = "1-2";
            ImGui::EndCombo();
        }
    }
    ImGui::TextDisabled("%s", tr("Click a slot to insert, change or set up a card (right click = card list)."));
}

void draw_slot_props(UiState& ui, int slot)
{
    RackEditor& r = ui.rack;
    float u = ImGui::GetFontSize();
    int idx = machine_cfg_card_at(r.cfg, slot);
    json* card = idx >= 0 ? &r.cfg["cards"][(size_t)idx] : nullptr;
    std::string type = card ? card->value("type", std::string("?")) : std::string();
    const CardInfo* ci = card ? card_info(type) : nullptr;

    ImGui::SetNextItemWidth(30.0f * u);
    char preview[200];
    if (card) snprintf(preview, sizeof(preview), "%s  %s", card_display_name(*card).c_str(), ci ? loc(ci->en, ci->cs) : "");
    else snprintf(preview, sizeof(preview), "%s", loc("(empty)", "(prázdný)"));
    char lab[32];
    snprintf(lab, sizeof(lab), "X%d", slot);
    if (ImGui::BeginCombo(lab, preview, ImGuiComboFlags_HeightLarge)) {
        size_t n = r.cfg["cards"].size();
        card_menu_items(ui, slot, true);
        ImGui::EndCombo();
        // another card chosen: the card list changed under `card` (erased, pushed, sorted),
        // so the pointer is stale; the new card is drawn in the next frame
        int now = machine_cfg_card_at(r.cfg, slot);
        if (r.cfg["cards"].size() != n || now != idx ||
            (now >= 0 && r.cfg["cards"][(size_t)now].value("type", std::string()) != type))
            return;
    }
    if (card) {
        ImGui::SameLine();
        if (ImGui::Button(tr("Remove"))) {
            put_card(r, slot, nullptr);
            return;
        }
    }
    if (!card) return;
    if (ci && !ci->emulated)
        ImGui::TextColored(TXT_WARN, "%s", loc("Not emulated yet: the card occupies the slot but does not respond.",
                                              "Zatím neemulovaná: karta zabírá slot, ale neodpovídá."));
    if (!ci) {
        ImGui::TextColored(TXT_ERROR, "%s", tr("unknown card"));
        return;
    }
    if (!ci->params || !ci->params[0].path) {
        ImGui::TextDisabled("%s", tr("Nothing to set up on this card yet."));
        return;
    }
    for (const CardParam* p = ci->params; p->path; p++) {
        if (!card_param_active(*card, ci, p)) continue;
        if (!strcmp(p->path, "keyboard.strict")) {   // parallel keyboards only (not none, not ANK-1)
            const json* kb = cfg_find(*card, "keyboard");
            std::string kt = kb && kb->is_object() ? kb->value("type", std::string()) : std::string();
            if (kt.empty() || kt == "ANK-1") continue;
        }
        bool nested = param_section(ci, p) != nullptr;
        if (nested) ImGui::Indent();
        draw_param(ui, *card, idx, ci, p);
        if (nested) ImGui::Unindent();
    }
    // state of the running card that is not a parameter (the write lock of ZRD-1V...)
    Card* live = editor_on_running(ui) ? running_card(ui, *card, card_first_slot(*card)) : nullptr;
    std::string st = live ? live->live_status() : std::string();
    if (!st.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(st.find("locked by") != std::string::npos ? TXT_WARN : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                           "%s", tr(st.c_str()));
    }
}

// ---------------------------------------------------------------- port and memory map

// card colours of the map; red is kept for conflicts
ImU32 card_color(int i)
{
    static const ImU32 pal[10] = {
        IM_COL32(86, 156, 214, 255), IM_COL32(106, 190, 106, 255), IM_COL32(220, 190, 80, 255),
        IM_COL32(170, 120, 210, 255), IM_COL32(80, 196, 196, 255), IM_COL32(200, 140, 90, 255),
        IM_COL32(150, 170, 80, 255), IM_COL32(120, 140, 230, 255), IM_COL32(190, 190, 190, 255),
        IM_COL32(110, 200, 160, 255),
    };
    return pal[(i < 0 ? 0 : i) % 10];
}

std::string range_text(const MapRange& rg, int digits)
{
    char buf[32];
    if (rg.lo == rg.hi) snprintf(buf, sizeof(buf), "%0*Xh", digits, rg.lo);
    else snprintf(buf, sizeof(buf), "%0*X-%0*Xh", digits, rg.lo, digits, rg.hi);
    return buf;
}

std::string flags_text(int f)
{
    std::string s;
    if (f & DEC_READ) s += "R";
    if (f & DEC_WRITE) s += "W";
    if (f & DEC_ONCARD) s += loc(" (on card)", " (na kartě)");
    return s;
}

void map_table(const char* id, const MachineMap& m, const std::vector<MapRange>& ranges, int digits)
{
    ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable(id, 4, tf)) return;
    ImGui::TableSetupColumn(tr("Addresses"));
    ImGui::TableSetupColumn(tr("Card"));
    ImGui::TableSetupColumn(tr("Function"));
    ImGui::TableSetupColumn(tr("Access"));
    ImGui::TableHeadersRow();
    for (const MapRange& rg : ranges) {
        for (size_t k = 0; k < rg.users.size(); k++) {
            const MapUser& us = rg.users[k];
            ImGui::TableNextRow();
            ImVec4 col = rg.conflict ? TXT_ERROR : rg.shared_write ? TXT_WARN : ImGui::GetStyleColorVec4(ImGuiCol_Text);
            ImGui::TableNextColumn();
            if (k == 0) ImGui::TextColored(col, "%s", range_text(rg, digits).c_str());
            ImGui::TableNextColumn();
            {   // colour of the card as in the bar: drawn, not a widget (no ID clashes between rows)
                float u = ImGui::GetFontSize(), s = u * 0.7f;
                ImVec2 p = ImGui::GetCursorScreenPos();
                p.y += (ImGui::GetTextLineHeight() - s) * 0.5f;
                ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + s, p.y + s), card_color(us.card));
                ImGui::Dummy(ImVec2(s, ImGui::GetTextLineHeight()));
            }
            ImGui::SameLine();
            ImGui::TextColored(col, "%s (X%d)", m.cards[(size_t)us.card].name.c_str(), m.cards[(size_t)us.card].slot);
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%s", tr(us.what.c_str()));
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%s%s", flags_text(us.flags).c_str(),
                               k == 0 && rg.conflict ? loc("  CONFLICT", "  KOLIZE") : "");
        }
    }
    ImGui::EndTable();
}

// The address axis under a memory bar starting at x0, w wide (ticks every 4 KB, labels per 16 KB).
void memory_axis(ImVec2 o, float w)
{
    float u = ImGui::GetFontSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto x_of = [&](int a) { return o.x + w * (float)a / 65536.0f; };
    for (int a = 0; a <= 0x10000; a += 0x1000) {
        float x = x_of(a);
        dl->AddLine(ImVec2(x, o.y), ImVec2(x, o.y + ((a & 0x3FFF) ? 0.2f : 0.4f) * u), IM_COL32(150, 150, 150, 255));
    }
    const int marks[5] = { 0x0000, 0x4000, 0x8000, 0xC000, 0xFFFF };
    for (int i = 0; i < 5; i++) {
        char t[8];
        snprintf(t, sizeof(t), "%04X", marks[i]);
        float tw = ImGui::CalcTextSize(t).x;
        float x = x_of(marks[i] == 0xFFFF ? 0x10000 : marks[i]) - (i == 0 ? 0 : i == 4 ? tw : tw * 0.5f);
        dl->AddText(ImVec2(x, o.y + 0.45f * u), IM_COL32(200, 200, 200, 255), t);
    }
}

// 64 KB as a bar: one colour per card, conflicts red, the CPU card's EPROM as a strip on top.
void memory_bar(const MachineMap& m, const std::vector<MapRange>& ranges, bool axis = true)
{
    float u = ImGui::GetFontSize();
    float w = ImGui::GetContentRegionAvail().x - 4, h = 1.8f * u;
    ImVec2 o = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), IM_COL32(40, 40, 44, 255));
    auto x_of = [&](int a) { return o.x + w * (float)a / 65536.0f; };
    for (const MapRange& rg : ranges) {
        float x0 = x_of(rg.lo), x1 = x_of(rg.hi + 1);
        if (x1 - x0 < 1.0f) x1 = x0 + 1.0f;
        int bus_users = 0, oncard = -1, first = -1;
        for (const MapUser& us : rg.users) {
            if (us.flags & DEC_ONCARD) oncard = us.card;
            else { bus_users++; if (first < 0) first = us.card; }
        }
        if (first >= 0)
            dl->AddRectFilled(ImVec2(x0, o.y + 0.35f * h), ImVec2(x1, o.y + h), rg.conflict ? COL_ERROR : card_color(first));
        if (oncard >= 0) dl->AddRectFilled(ImVec2(x0, o.y), ImVec2(x1, o.y + 0.3f * h), card_color(oncard));
    }
    if (axis) memory_axis(ImVec2(o.x, o.y + h), w);
    ImGui::InvisibleButton("membar", ImVec2(w, h));
    if (ImGui::IsItemHovered()) {
        int a = (int)((ImGui::GetIO().MousePos.x - o.x) / w * 65536.0f);
        if (a < 0) a = 0;
        if (a > 0xFFFF) a = 0xFFFF;
        std::string who = tr("nothing (open bus, FFh)");
        for (const MapRange& rg : ranges) {
            if (a < rg.lo || a > rg.hi) continue;
            who.clear();
            for (const MapUser& us : rg.users)
                who += (who.empty() ? "" : ", ") + m.cards[(size_t)us.card].name + " " + tr(us.what.c_str());
        }
        ImGui::SetTooltip("%04Xh: %s", a, who.c_str());
    }
    if (axis) ImGui::Dummy(ImVec2(w, 1.2f * u));
}

// The range of a map that holds address a, nullptr if nobody answers there.
const MapRange* range_at(const std::vector<MapRange>& rs, int a)
{
    for (const MapRange& rg : rs)
        if (a >= rg.lo && a <= rg.hi) return &rg;
    return nullptr;
}

bool same_users(const MapRange* a, const MapRange* b)
{
    if (!a || !b) return a == b;
    return a->users == b->users;
}

// Addresses where the users may change in either map, sorted, from 0 to 10000h.
std::vector<int> map_bounds(const std::vector<MapRange>& a, const std::vector<MapRange>& b)
{
    std::vector<int> v = { 0, 0x10000 };
    for (const std::vector<MapRange>* rs : { &a, &b })
        for (const MapRange& rg : *rs) {
            v.push_back(rg.lo);
            v.push_back(rg.hi + 1);
        }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

const char* const k_map_states[4] = { "MAP1=0 MAP2=0", "MAP1=1 MAP2=0", "MAP1=0 MAP2=1", "MAP1=1 MAP2=1" };

// The memory in all four states of MAP1/MAP2, always in the same order: the chosen state as
// the full bar, the others thin. A thin bar shows its whole map, dimmed where it is the same
// as in the chosen state, so the places that differ (e.g. CGA-1V at C000h with MAP1 = MAP2 = 1)
// stand out. A click on a bar or its label chooses that state.
void map_state_bars(RackEditor& r)
{
    const MachineMap& m = r.map;
    float u = ImGui::GetFontSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float label_w = ImGui::CalcTextSize(k_map_states[3]).x + u;
    int sel = r.map_state & 3;
    const std::vector<MapRange>& cur = m.mem[sel];
    ImVec2 bar_o(0, 0);
    float bar_w = 0;
    for (int s = 0; s < 4; s++) {
        ImGui::PushID(s);
        ImVec2 p = ImGui::GetCursorScreenPos();
        bool chosen = s == sel;
        float row_h = chosen ? 1.8f * u : ImGui::GetFrameHeight();
        if (ImGui::InvisibleButton("state", ImVec2(label_w, row_h))) r.map_state = s;
        bool hl = ImGui::IsItemHovered();
        if (hl && !chosen) ImGui::SetTooltip("%s", tr("Click: show this map"));
        ImU32 tc = chosen ? IM_COL32(255, 220, 120, 255) : hl ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 160, 160, 255);
        dl->AddText(ImVec2(p.x, p.y + (row_h - u) * 0.5f), tc, k_map_states[s]);
        ImGui::SameLine(0, 0);
        ImVec2 o = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x - 4;
        bar_o = o;
        bar_w = w;
        if (chosen) {
            memory_bar(m, cur, false);
        } else {
            const std::vector<MapRange>& other = m.mem[s];
            std::vector<int> b = map_bounds(cur, other);
            float y0 = o.y + row_h * 0.3f, y1 = o.y + row_h * 0.7f;
            auto x_of = [&](int a) { return o.x + w * (float)a / 65536.0f; };
            dl->AddRectFilled(ImVec2(o.x, y0), ImVec2(o.x + w, y1), IM_COL32(40, 40, 44, 255));
            for (size_t i = 0; i + 1 < b.size(); i++) {
                const MapRange* ro = range_at(other, b[i]);
                bool same = same_users(range_at(cur, b[i]), ro);
                int first = -1;
                for (const MapUser& us : ro ? ro->users : std::vector<MapUser>())
                    if (!(us.flags & DEC_ONCARD)) { first = us.card; break; }
                if (same && first < 0) continue;   // nobody in both: the background
                ImU32 col = ro && ro->conflict ? COL_ERROR : first >= 0 ? card_color(first) : IM_COL32(110, 110, 116, 255);
                if (same) col = (col & 0x00FFFFFFu) | 0x50000000u;   // the same as the chosen state: dimmed
                float x0 = x_of(b[i]), x1 = x_of(b[i + 1]);
                if (x1 - x0 < 1.0f) x1 = x0 + 1.0f;
                dl->AddRectFilled(ImVec2(x0, same ? y0 + 0.1f * (y1 - y0) : y0), ImVec2(x1, same ? y1 - 0.1f * (y1 - y0) : y1), col);
            }
            if (ImGui::InvisibleButton("bar", ImVec2(w, row_h))) r.map_state = s;
            if (ImGui::IsItemHovered()) {
                int a = (int)((ImGui::GetIO().MousePos.x - o.x) / w * 65536.0f);
                if (a < 0) a = 0;
                if (a > 0xFFFF) a = 0xFFFF;
                const MapRange* ro = range_at(other, a);
                std::string who = ro ? std::string() : std::string(tr("nothing (open bus, FFh)"));
                for (const MapUser& us : ro ? ro->users : std::vector<MapUser>())
                    who += (who.empty() ? "" : ", ") + m.cards[(size_t)us.card].name + " " + tr(us.what.c_str());
                ImGui::SetTooltip("%s, %04Xh: %s\n%s", k_map_states[s], a, who.c_str(), tr("Click: show this map"));
            }
        }
        ImGui::PopID();
    }
    ImVec2 ax = ImGui::GetCursorScreenPos();
    memory_axis(ImVec2(bar_o.x, ax.y), bar_w);
    ImGui::Dummy(ImVec2(bar_w, 1.2f * u));
}

void draw_map(RackEditor& r)
{
    const MachineMap& m = r.map;
    if (!m.map_matters) r.map_state = 0;
    std::string unknown;
    for (const MapCard& c : m.cards)
        if (!c.known && !c.declared) unknown += (unknown.empty() ? "" : ", ") + c.name + " (X" + std::to_string(c.slot) + ")";
    if (!unknown.empty())
        ImGui::TextColored(TXT_WARN, "%s %s", tr("Decoding unknown (not emulated):"), unknown.c_str());

    ImGui::SeparatorText(tr("Memory"));
    const std::vector<MapRange>& mem = r.map.mem[r.map_state & 3];
    if (m.map_matters) {
        map_state_bars(r);
        ImGui::TextDisabled("%s", tr("Thin bars: the other states of MAP1/MAP2, bright where they differ (click to show)."));
    } else {
        memory_bar(m, mem);
    }
    if (mem.empty()) ImGui::TextDisabled("%s", tr("No card answers in the memory."));
    else map_table("memmap", m, mem, 4);
    ImGui::SeparatorText(tr("I/O ports"));
    if (m.io.empty()) ImGui::TextDisabled("%s", tr("No card answers on the ports."));
    else map_table("iomap", m, m.io, 2);
    ImGui::TextDisabled("%s", tr("Addresses nobody answers read FFh (open bus). The CPU card's EPROM is read on the card, writes go to the bus."));
}

void new_empty(RackEditor& r)
{
    json cfg = { { "name", tr("New machine") }, { "bus", { { "type", "ARB-1C-8" }, { "jp3", "2-3" } } },
                 { "cards", json::array() } };
    cfg["cards"].push_back(card_new_config(card_info("JPR-1ZI"), 1));
    r.cfg = cfg;
    r.saved = json();
    r.path.clear();
    r.sel = 1;
    set_message(r, tr("New machine: insert cards into the slots."), false);
}

void new_menu(UiState& ui)
{
    if (ImGui::Selectable(tr("Empty machine"))) request(ui, RackEditor::PEND_EMPTY, std::string());
    ImGui::SeparatorText(tr("From a template or another machine"));
    for (const std::string& f : list_machine_files()) {
        if (ImGui::Selectable(f.c_str())) request(ui, RackEditor::PEND_TEMPLATE, f);
    }
}

} // namespace

void rack_power_switch(UiState& ui)
{
    if (ui.host->powered()) {
        ui.host->power_off();
        set_message(ui.rack, tr("Switched off."), false);
    } else {
        editor_apply(ui);
    }
}

void rack_open_in_editor(UiState& ui, const std::string& path)
{
    editor_open(ui, path, false);
    ui.show_rack = true;
}

void rack_set_live_param(UiState& ui, Card* c, const char* path, const json& v)
{
    Machine* m = ui.host->machine();
    std::string err;
    if (!m || !c || !m->set_live_param(c, path, v, err)) {
        if (!err.empty()) LOGW("%s", err.c_str());
        return;
    }
    RackEditor& r = ui.rack;
    if (!editor_on_running(ui)) return;
    int idx = machine_cfg_card_at(r.cfg, c->slot);
    if (idx < 0) return;
    json& card = r.cfg["cards"][(size_t)idx];
    const CardInfo* ci = card_info(c->type);
    if (card.value("type", std::string()) != c->type || !ci) return;
    for (const CardParam* p = ci->params; p && p->path; p++)
        if (!strcmp(p->path, path)) card_set_param(card, ci, p, v);
}

void rack_set_display_ink(UiState& ui, DisplayDevice* d, const std::string& value)
{
    Card* c = dynamic_cast<Card*>(d);
    if (c && d->ink_param()) rack_set_live_param(ui, c, d->ink_param(), value);
}

void rack_start_machine(UiState& ui, const std::string& file)
{
    request(ui, RackEditor::PEND_START, file);
}

void rack_request_exit(UiState& ui)
{
    request(ui, RackEditor::PEND_EXIT, std::string());
}

void rack_open_and_run(UiState& ui)
{
    start_dialog(ui, RackEditor::DLG_RUN, std::string());
}

void draw_rack(UiState& ui)
{
    handle_dialog(ui);   // also for File -> Open while the window is hidden
    follow_machine(ui);
    sync_live_params(ui);
    draw_unsaved_question(ui);
    if (!ui.show_rack) return;
    RackEditor& r = ui.rack;
    float u = ImGui::GetFontSize();
    ImVec2 vp = ImGui::GetMainViewport()->WorkSize;   // first use: most of the main window
    ImGui::SetNextWindowSize(ImVec2(vp.x * 0.92f < 62.0f * u ? vp.x * 0.92f : 62.0f * u, vp.y * 0.9f), ImGuiCond_FirstUseEver);
    bool dirty = r.cfg != r.saved;
    char title[300];
    snprintf(title, sizeof(title), "%s - %s%s###rack", tr("Machine configuration"),
             r.cfg.value("name", std::string("?")).c_str(), dirty ? " *" : "");
    if (!ImGui::Begin(title, &ui.show_rack)) {
        ImGui::End();
        return;
    }
    if (r.cfg.is_null()) {
        ImGui::TextUnformatted(tr("No machine."));
        ImGui::End();
        return;
    }

    // toolbar
    if (ImGui::Button(tr("New"))) ImGui::OpenPopup("newmenu");
    if (ImGui::BeginPopup("newmenu")) {
        new_menu(ui);
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Open..."))) start_dialog(ui, RackEditor::DLG_OPEN, r.path.empty() ? std::string() : path_dir(data_path(r.path)));
    ImGui::SameLine();
    if (ImGui::Button(tr("Save"))) {
        if (r.path.empty()) start_dialog(ui, RackEditor::DLG_SAVE, std::string());
        else editor_save(ui, r.path);
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Save as..."))) start_dialog(ui, RackEditor::DLG_SAVE, r.path);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (r.map_cfg != r.cfg) {   // the map needs the cards built: only when something changed
        r.map_cfg = r.cfg;
        machine_map_build(r.cfg, r.map);
        r.problems = machine_cfg_problems(r.cfg);
        std::vector<CfgProblem> mp = machine_map_problems(r.map);
        r.problems.insert(r.problems.end(), mp.begin(), mp.end());
    }
    const std::vector<CfgProblem>& problems = r.problems;
    bool runnable = true;
    for (const CfgProblem& p : problems) if (p.error) runnable = false;
    ImGui::BeginDisabled(!runnable);
    if (ImGui::Button(tr("Switch on with this configuration"))) editor_apply(ui);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!dirty || r.saved.is_null());
    if (ImGui::Button(tr("Revert"))) {
        r.cfg = r.saved;
        set_message(r, tr("Changes reverted."), false);
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("%s%s", r.path.empty() ? tr("(not saved yet)") : r.path.c_str(),
                        dirty ? tr("  - unsaved changes") : "");
    if (!r.message.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(r.message_error ? TXT_ERROR : TXT_OK, "%s", r.message.c_str());
    }

    draw_frame(ui, problems);

    ImGui::BeginChild("props", ImVec2(0, 0), ImGuiChildFlags_None);
    for (const CfgProblem& p : problems)
        ImGui::TextColored(p.error ? TXT_ERROR : TXT_WARN, "%s", loc(p.en.c_str(), p.cs.c_str()));
    if (!problems.empty()) ImGui::Separator();
    if (r.sel > machine_cfg_slots(r.cfg)) r.sel = 0;
    if (ImGui::BeginTabBar("props_tabs")) {
        if (ImGui::BeginTabItem(r.sel == 0 ? tr("Machine and bus") : tr("Card settings"))) {
            if (r.sel == 0) draw_bus_props(r);
            else draw_slot_props(ui, r.sel);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("Port and memory map"), nullptr, r.open_map_tab ? ImGuiTabItemFlags_SetSelected : 0)) {
            r.open_map_tab = false;
            draw_map(r);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
    ImGui::End();
}
