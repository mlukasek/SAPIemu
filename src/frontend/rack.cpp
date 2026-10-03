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

const ImVec4 TXT_ERROR(1.0f, 0.42f, 0.38f, 1.0f);
const ImVec4 TXT_WARN(1.0f, 0.82f, 0.35f, 1.0f);
const ImVec4 TXT_OK(0.55f, 0.9f, 0.55f, 1.0f);

// ---------------------------------------------------------------- file dialogs

const SDL_DialogFileFilter k_machine_filter[] = { { "SAPI-1 (*.jsonc, *.json)", "jsonc;json" } };

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
    if (!file_write_all(full, text.data(), text.size())) {
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
    if (path.empty()) return;   // cancelled
    if (kind == RackEditor::DLG_OPEN) {
        editor_open(ui, path, false);
    } else if (kind == RackEditor::DLG_RUN) {
        std::string rel = data_relative(path), err;
        if (!ui.host->load(rel, err)) {
            // show it in the configurator anyway (e.g. a CPU card not emulated yet)
            editor_open(ui, rel, false);
            set_message(r, std::string(tr("The machine cannot be started")) + ": " + err, true);
            r.machine_gen = ui.host->machine_generation();
            ui.show_rack = true;
        }
    } else if (kind == RackEditor::DLG_SAVE) {
        size_t slash = path.find_last_of("/\\");
        if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".jsonc";
        editor_save(ui, path);
    } else if (kind == RackEditor::DLG_PARAM) {
        json& cards = r.cfg["cards"];
        if (r.dlg_card < 0 || r.dlg_card >= (int)cards.size()) return;
        json& card = cards[(size_t)r.dlg_card];
        const CardInfo* ci = card_info(card.value("type", std::string()));
        for (const CardParam* p = ci ? ci->params : nullptr; p && p->path; p++)
            if (r.dlg_param == p->path) card_set_param(card, ci, p, data_relative(path));
    }
}

// Follow the running machine while the editor holds no unsaved changes.
void follow_machine(UiState& ui)
{
    RackEditor& r = ui.rack;
    Machine* m = ui.host->machine();
    if (!m || r.machine_gen == ui.host->machine_generation()) return;
    if (r.cfg.is_null() || r.cfg == r.saved) {
        r.cfg = m->config;
        r.saved = r.cfg;
        r.path = ui.host->machine_file();
        r.sel = 0;
    }
    r.machine_gen = ui.host->machine_generation();
}

// ---------------------------------------------------------------- frame drawing

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

void text_centered(ImDrawList* dl, ImVec2 c, float size, ImU32 col, const char* text)
{
    ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    dl->AddText(ImGui::GetFont(), size, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), col, text);
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

void draw_panel(ImDrawList* dl, float u, ImVec2 a, ImVec2 b, const CardInfo* ci, const std::string& type, bool error)
{
    bool dim = !ci || !ci->emulated;
    dl->AddRectFilled(a, b, dim ? COL_PANEL_DIM : COL_PANEL, 2.0f);
    dl->AddRect(a, b, COL_PANEL_EDGE, 2.0f);
    float cx = (a.x + b.x) * 0.5f;
    dl->AddCircleFilled(ImVec2(cx, a.y + 0.55f * u), 0.22f * u, COL_PANEL_EDGE);   // screws
    dl->AddCircleFilled(ImVec2(cx, b.y - 0.55f * u), 0.22f * u, COL_PANEL_EDGE);
    ImU32 tc = dim ? COL_TEXT_DIM : COL_TEXT;
    const char* label = ci ? ci->panel : "?";
    text_centered(dl, ImVec2(cx, a.y + 1.45f * u), 0.82f * u, tc, label);
    text_centered(dl, ImVec2(cx, b.y - 1.45f * u), 0.82f * u, tc, label);
    text_vertical(dl, ImVec2(cx, (a.y + b.y) * 0.5f), 0.9f * u, tc, type.c_str());
    if (error) dl->AddCircleFilled(ImVec2(b.x - 0.45f * u, a.y + 0.45f * u), 0.28f * u, COL_ERROR);
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

    float u = ImGui::GetFontSize();
    float sw = 2.3f * u, ph = 15.0f * u, pad = 0.8f * u;
    float spn_w = 3.0f * u, zdr_w = 10.0f * u;
    float w = pad + n * sw + 0.3f * u + spn_w + zdr_w + pad;
    float h = pad + ph + 1.6f * u + pad;

    ImGui::BeginChild("frame", ImVec2(0, h + ImGui::GetStyle().ScrollbarSize + 4), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), COL_CHASSIS, 0.6f * u);
    dl->AddRect(o, ImVec2(o.x + w, o.y + h), COL_CHASSIS_EDGE, 0.6f * u, 0, 2.0f);
    float y0 = o.y + pad, y1 = y0 + ph;
    float sx0 = o.x + pad;
    dl->AddRectFilled(ImVec2(sx0, y0), ImVec2(sx0 + n * sw, y1), COL_INSIDE);

    auto slot_x = [&](int s) { return sx0 + (float)(n - s) * sw; };   // left edge of slot s

    // empty slots: card guides at the top and the bottom
    for (int s = 1; s <= n; s++) {
        float x = slot_x(s);
        float cx = x + sw * 0.5f;
        dl->AddRectFilled(ImVec2(cx - 0.18f * u, y0), ImVec2(cx + 0.18f * u, y0 + 1.2f * u), COL_GUIDE);
        dl->AddRectFilled(ImVec2(cx - 0.18f * u, y1 - 1.2f * u), ImVec2(cx + 0.18f * u, y1), COL_GUIDE);
        char lab[8];
        snprintf(lab, sizeof(lab), "X%d", s);
        text_centered(dl, ImVec2(cx, y1 + 0.8f * u), 0.8f * u, COL_TEXT, lab);
    }

    // cards
    int clicked = -1, ctx = -1, hovered = -1;
    if (cfg.contains("cards") && cfg["cards"].is_array()) {
        for (size_t i = 0; i < cfg["cards"].size(); i++) {
            const json& c = cfg["cards"][i];
            int s = card_first_slot(c);
            int wd = card_cfg_width(c);
            if (s < 1 || s + wd - 1 > n) continue;
            std::string type = c.value("type", std::string("?"));
            const CardInfo* ci = card_info(type);
            ImVec2 a(slot_x(s + wd - 1) + 1.0f, y0), b(slot_x(s) + sw - 1.0f, y1);
            bool err = false;
            for (int k = 0; k < wd; k++) err = err || slot_has_error(problems, s + k);
            draw_panel(dl, u, a, b, ci, type, err);
        }
    }
    // hit areas per slot, highlight
    for (int s = 1; s <= n; s++) {
        int idx = machine_cfg_card_at(cfg, s);
        int first = idx >= 0 ? card_first_slot(cfg["cards"][(size_t)idx]) : s;
        char id[16];
        snprintf(id, sizeof(id), "slot%d", s);
        ImVec2 a(slot_x(s), y0), b(slot_x(s) + sw, y1);
        if (panel_button(id, a, b)) clicked = first;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ctx = first;
        if (ImGui::IsItemHovered()) {
            hovered = first;
            if (idx >= 0) {
                const json& c = cfg["cards"][(size_t)idx];
                const CardInfo* ci = card_info(c.value("type", std::string()));
                ImGui::SetTooltip("X%d: %s\n%s%s", first, c.value("type", std::string("?")).c_str(),
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
        dl->AddRect(ImVec2(slot_x(s + wd - 1), y0 - 2), ImVec2(slot_x(s) + sw, y1 + 2), col, 2.0f, 0, th);
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

    // SPN-1: power indicators
    float px = sx0 + n * sw + 0.3f * u;
    ImVec2 sa(px, y0), sb(px + spn_w, y1);
    dl->AddRectFilled(sa, sb, COL_PANEL, 2.0f);
    dl->AddRect(sa, sb, COL_PANEL_EDGE, 2.0f);
    float scx = (sa.x + sb.x) * 0.5f;
    text_centered(dl, ImVec2(scx, y0 + 0.9f * u), 0.82f * u, COL_TEXT, "SPN");
    text_centered(dl, ImVec2(scx, y0 + 2.6f * u), 0.62f * u, COL_TEXT, loc("POWER", "ZDROJ"));
    const char* volts[4] = { "+12V", "+5V", "-5V", "-12V" };
    for (int i = 0; i < 4; i++) {
        float y = y0 + (3.8f + 1.2f * i) * u;
        dl->AddText(ImGui::GetFont(), 0.6f * u, ImVec2(sa.x + 0.25f * u, y - 0.32f * u), COL_TEXT, volts[i]);
        dl->AddCircleFilled(ImVec2(sb.x - 0.6f * u, y), 0.3f * u, m ? COL_LED_ON : COL_LED_OFF);
    }
    text_centered(dl, ImVec2(scx, y1 - 0.9f * u), 0.82f * u, COL_TEXT, "SPN");

    // ZDR-1A: CPU frequency, power, reset, turbo
    float zx = sb.x;
    ImVec2 za(zx, y0), zb(zx + zdr_w, y1);
    dl->AddRectFilled(za, zb, COL_PANEL, 2.0f);
    dl->AddRect(za, zb, COL_PANEL_EDGE, 2.0f);
    float zcx = (za.x + zb.x) * 0.5f;
    dl->AddText(ImGui::GetFont(), 0.82f * u, ImVec2(za.x + 0.4f * u, y0 + 0.5f * u), COL_TEXT, "ZDR");
    text_centered(dl, ImVec2(zcx, y0 + 2.2f * u), 0.7f * u, COL_TEXT, loc("CPU FREQUENCY", "FREKVENCE CPU"));
    ImVec2 da(zcx - 2.2f * u, y0 + 2.9f * u), db(zcx + 2.2f * u, y0 + 5.3f * u);
    dl->AddRectFilled(da, db, IM_COL32(10, 10, 10, 255), 3.0f);
    char freq[16];
    if (m && !host.paused && host.speed_percent > 0)
        snprintf(freq, sizeof(freq), "%.1f", m->bus.cpu_hz / 1e6 * host.speed_percent / 100.0);
    else
        snprintf(freq, sizeof(freq), "--");
    text_centered(dl, ImVec2(zcx, (da.y + db.y) * 0.5f), 1.9f * u, IM_COL32(60, 255, 120, 255), freq);
    // "SAPI 1"
    text_centered(dl, ImVec2(zcx, y0 + 7.4f * u), 2.2f * u, COL_TEXT, "SAPI 1");
    text_centered(dl, ImVec2(zcx + 1.0f, y0 + 7.4f * u), 2.2f * u, COL_TEXT, "SAPI 1");
    // buttons
    struct Btn { const char* en; const char* cs; ImU32 col; };
    const Btn btns[3] = {
        { "POWER", "NAPÁJENÍ", IM_COL32(60, 200, 120, 255) },
        { "RESET", "RESET", IM_COL32(220, 50, 50, 255) },
        { "TURBO", "TURBO", host.turbo ? IM_COL32(255, 230, 120, 255) : IM_COL32(240, 240, 240, 255) },
    };
    float bw = 1.7f * u, gap = (zdr_w - 3 * bw) / 4.0f;
    for (int i = 0; i < 3; i++) {
        float bx = za.x + gap + i * (bw + gap);
        float by = y0 + 10.6f * u;
        text_centered(dl, ImVec2(bx + bw * 0.5f, by - 0.7f * u), 0.6f * u, COL_TEXT, loc(btns[i].en, btns[i].cs));
        ImVec2 ba(bx, by), bb(bx + bw, by + bw);
        dl->AddRectFilled(ImVec2(ba.x - 2, ba.y - 2), ImVec2(bb.x + 2, bb.y + 2), IM_COL32(60, 60, 60, 255), 3.0f);
        dl->AddRectFilled(ba, bb, btns[i].col, 3.0f);
        char id[16];
        snprintf(id, sizeof(id), "btn%d", i);
        if (panel_button(id, ba, bb)) {
            if (i == 0) editor_apply(ui);
            else if (i == 1 && m) m->reset();
            else if (i == 2) host.turbo = !host.turbo;
        }
        if (ImGui::IsItemHovered()) {
            const char* tip = i == 0 ? tr("Switch off and on with the edited configuration")
                            : i == 1 ? tr("Reset (F12)") : tr("Turbo (F8)");
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
    if (ImGui::Selectable(tr("(empty)"), cur.empty())) put_card(r, slot, nullptr);
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
    const char* label = loc(p->en, p->cs);
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
        int cur = -2;
        if (v.is_null()) cur = -1;
        for (size_t i = 0; i < kp.size(); i++)
            if (json::parse(kp[i].config) == v) cur = (int)i;
        std::string preview = cur == -1 ? tr("(none)") : cur >= 0 ? kp[(size_t)cur].name
                                                                   : std::string(tr("custom")) + ": " + scalar(v);
        if (ImGui::BeginCombo(label, preview.c_str())) {
            if (ImGui::Selectable(tr("(none)"), cur == -1)) card_set_param(card, ci, p, json());
            for (size_t i = 0; i < kp.size(); i++)
                if (ImGui::Selectable(kp[i].name, cur == (int)i)) card_set_param(card, ci, p, json::parse(kp[i].config));
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
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", r.cfg.value("name", std::string()).c_str());
    ImGui::SetNextItemWidth(30.0f * u);
    if (ImGui::InputText(tr("Name"), buf, sizeof(buf))) r.cfg["name"] = std::string(buf);
    char desc[1024];
    snprintf(desc, sizeof(desc), "%s", r.cfg.value("description", std::string()).c_str());
    if (ImGui::InputTextMultiline(tr("Description"), desc, sizeof(desc), ImVec2(30.0f * u, 3.6f * u),
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
    if (card) snprintf(preview, sizeof(preview), "%s  %s", type.c_str(), ci ? loc(ci->en, ci->cs) : "");
    else snprintf(preview, sizeof(preview), "%s", tr("(empty)"));
    char lab[32];
    snprintf(lab, sizeof(lab), "X%d", slot);
    if (ImGui::BeginCombo(lab, preview, ImGuiComboFlags_HeightLarge)) {
        card_menu_items(ui, slot, true);
        ImGui::EndCombo();
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
        bool nested = param_section(ci, p) != nullptr;
        if (nested) ImGui::Indent();
        draw_param(ui, *card, idx, ci, p);
        if (nested) ImGui::Unindent();
    }
}

void new_menu(UiState& ui)
{
    RackEditor& r = ui.rack;
    if (ImGui::Selectable(tr("Empty machine"))) {
        json cfg = { { "name", tr("New machine") }, { "bus", { { "type", "ARB-1C-8" }, { "jp3", "2-3" } } },
                     { "cards", json::array() } };
        cfg["cards"].push_back(card_new_config(card_info("JPR-1ZI"), 1));
        r.cfg = cfg;
        r.saved = json();
        r.path.clear();
        r.sel = 1;
        set_message(r, tr("New machine: insert cards into the slots."), false);
    }
    ImGui::SeparatorText(tr("From a template or another machine"));
    for (const std::string& f : list_machine_files()) {
        if (ImGui::Selectable(f.c_str())) editor_open(ui, f, true);
    }
}

} // namespace

void rack_open_in_editor(UiState& ui, const std::string& path)
{
    editor_open(ui, path, false);
    ui.show_rack = true;
}

void rack_open_and_run(UiState& ui)
{
    start_dialog(ui, RackEditor::DLG_RUN, std::string());
}

void draw_rack(UiState& ui)
{
    handle_dialog(ui);   // also for File -> Open while the window is hidden
    follow_machine(ui);
    if (!ui.show_rack) return;
    RackEditor& r = ui.rack;
    float u = ImGui::GetFontSize();
    ImGui::SetNextWindowSize(ImVec2(62.0f * u, 46.0f * u), ImGuiCond_FirstUseEver);
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
    std::vector<CfgProblem> problems = machine_cfg_problems(r.cfg);
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
    if (r.sel == 0) draw_bus_props(r);
    else draw_slot_props(ui, r.sel);
    ImGui::EndChild();
    ImGui::End();
}
