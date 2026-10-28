// Media window: cassette recorders of DSM-1 cards, images of the PMD32-SD drives,
// the IDE disk of IDE-1 cards, cartridge disks of RKD-1 and the magnetic tape drives of RMP-1.
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "cards/dsm1.h"
#include "cards/ide1.h"
#include "cards/rkd1.h"
#include "cards/rmp1.h"
#include "chips/floppy.h"
#include "chips/kzd1.h"
#include "core/host.h"
#include "core/ihex.h"
#include "core/log.h"
#include "core/util.h"

// ---------------------------------------------------------------- cassette recorder

static const SDL_DialogFileFilter k_tape_filter[] = { { "Kazeta (WAV, MP3) nebo program (HEX, TXT, BIN)", "wav;mp3;hex;ihx;txt;bin" },
                                                      { "Kazeta (WAV, MP3)", "wav;mp3" },
                                                      { "Program (Intel HEX, TXT, BIN)", "hex;ihx;txt;bin" } };
static const SDL_DialogFileFilter k_wav_filter[] = { { "WAV", "wav" } };
static const SDL_DialogFileFilter k_hex_filter[] = { { "Intel HEX (HEX, IHX, TXT)", "hex;ihx;txt" }, { "*", "*" } };
static const SDL_DialogFileFilter k_img_filter[] = { { "Disketa (IMG, IMD, 8SD, SP2, SP1)", "img;8sd;imd;sp2;sp1" },
                                                     { "*", "*" } };
static const SDL_DialogFileFilter k_state_filter[] = { { "Stav SAPIemu", "sapistate" } };
static const SDL_DialogFileFilter k_kzd_filter[] = { { "Kazeta KZD-1 (KZD)", "kzd" }, { "*", "*" } };
static const SDL_DialogFileFilter k_kzd_open_filter[] = { { "Kazeta KZD-1 (KZD)", "kzd" },
                                                          { "Program (HEX, BIN)", "hex;ihx;bin" }, { "*", "*" } };

static const SDL_DialogFileFilter k_rkd_filter[] = { { "Kazeta RKD-1 (RKD)", "rkd" }, { "*", "*" } };
static const SDL_DialogFileFilter k_tap_filter[] = { { "Magnetická páska (SIMH TAP)", "tap" }, { "*", "*" } };
static int s_new_disk_fmt = 0;   // format of a new disk (index into floppy_geoms())

static void SDLCALL tape_dialog_done(void* userdata, const char* const* files, int)
{
    TapeDialog* d = (TapeDialog*)userdata;
    std::lock_guard<std::mutex> lk(d->mtx);
    d->result = (files && files[0]) ? files[0] : "";
    d->done = true;
}

std::string dialog_location(const std::string& path)
{
    std::string s = path;
    if (!s.empty() && s.back() != '/' && s.back() != '\\' && dir_exists(s)) s += '/';
#ifdef _WIN32
    for (char& c : s) if (c == '/') c = '\\';
#endif
    return s;
}

static void tape_dialog(UiState& ui, int kind, const std::string& card, int drive = 0)
{
    TapeDialog& d = ui.tape_dlg;
    {
        std::lock_guard<std::mutex> lk(d.mtx);
        if (d.kind) return;
        d.kind = kind;
        d.done = false;
        d.card = card;
        d.drive = drive;
    }
    std::string where = d.last_dir.empty() ? data_path("work") : d.last_dir;
    if (kind == 7 || kind == 8) {   // states: in work/state unless chosen elsewhere before
        where = data_path("work/state");
        make_dirs(where);
    }
    where = dialog_location(where);
    if (kind == 1) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_tape_filter, 3, where.c_str(), false);
    else if (kind == 3) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_hex_filter, 2, where.c_str(), false);
    else if (kind == 6) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, nullptr, 0, where.c_str(), false);
    else if (kind == 7) SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_state_filter, 1, where.c_str());
    else if (kind == 8) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_state_filter, 1, where.c_str(), false);
    else if (kind == 4) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_img_filter, 2, where.c_str(), false);
    else if (kind == 5) SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_img_filter, 1, where.c_str());
    else if (kind == 9) SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_wav_filter, 1, where.c_str());
    else if (kind == 10) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_kzd_open_filter, 3, where.c_str(), false);
    else if (kind == 11) SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_kzd_filter, 1, where.c_str());
    else if (kind == 12) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_rkd_filter, 2, where.c_str(), false);
    else if (kind == 13) SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_rkd_filter, 1, where.c_str());
    else if (kind == 14) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_tap_filter, 2, where.c_str(), false);
    else if (kind == 15) SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_tap_filter, 1, where.c_str());
    else SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_wav_filter, 1, where.c_str());
}

struct HexScan {
    int count;
    uint32_t lo, hi;
};

static void hex_scan_byte(void* user, uint32_t addr, uint8_t)
{
    HexScan* s = (HexScan*)user;
    if (!s->count || addr < s->lo) s->lo = addr;
    if (!s->count || addr > s->hi) s->hi = addr;
    s->count++;
}

// The addresses of an Intel HEX file, without loading it (the question before File -> Load program).
static void hex_scan(UiState& ui, const std::string& path)
{
    ui.hex_path = path;
    ui.hex_ask = true;
    ui.hex_error.clear();
    std::string text, err;
    HexScan s = { 0, 0, 0 };
    if (!file_read_text(path, text)) ui.hex_error = tr("Cannot read the file.");
    else if (!ihex_parse(text, hex_scan_byte, &s, err)) ui.hex_error = err;
    else if (!s.count) ui.hex_error = tr("The file holds no data.");
    else if (s.hi > 0xFFFF) ui.hex_error = tr("The file does not fit into memory there.");
    ui.hex_bytes = s.count;
    ui.hex_lo = s.lo;
    ui.hex_hi = s.hi;
}

static void state_notice(UiState& ui, bool ok, const std::string& text)
{
    ui.notice = text;
    ui.notice_error = !ok;
    ui.notice_ns = SDL_GetTicksNS();
}

static void state_file(UiState& ui, std::string path, bool save)
{
    std::string err;
    if (save) {
        size_t slash = path.find_last_of("/\\");
        if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".sapistate";
        if (ui.host->save_state_file(path, err)) state_notice(ui, true, std::string(tr("State saved")) + ": " + path);
        else state_notice(ui, false, std::string(tr("Cannot save the state")) + ":\n" + err);
    } else {
        if (ui.host->load_state_file(path, err)) state_notice(ui, true, std::string(tr("State loaded")) + ": " + path);
        else state_notice(ui, false, std::string(tr("Cannot load the state")) + ":\n" + err);
    }
}

// ---------------------------------------------------------------- media and the machine description

// The medium of unit `u` of the card changes ("" = taken out): through the machine, so the description
// (the configurator) keeps up with the Media window.
static void media_set(UiState& ui, Card* c, int u, const std::string& image)
{
    Machine* m = ui.host->machine();
    const char* p = c ? c->media_param(u) : nullptr;
    if (!m || !p) return;
    std::string err;
    if (!m->set_live_param(c, p, nlohmann::json(image), err)) LOGE("%s: %s", c->name.c_str(), err.c_str());
}

// The card has the medium already (a program made into a tape here): the description only.
static void media_note(UiState& ui, Card* c, int u, const std::string& image)
{
    Machine* m = ui.host->machine();
    const char* p = c ? c->media_param(u) : nullptr;
    if (m && p) m->note_param(c, p, nlohmann::json(image));
}

// The image the description names for unit `u`, and when it is a file of media/ used through its working
// copy, both paths (Restore from the original).
static std::string media_image(Machine* m, Card* c, int u)
{
    const char* p = c ? c->media_param(u) : nullptr;
    nlohmann::json* cc = m && p ? m->card_config(c) : nullptr;
    const CardInfo* ci = cc ? card_info(c->type) : nullptr;
    for (const CardParam* q = ci ? ci->params : nullptr; q && q->path; q++)
        if (!strcmp(q->path, p)) {
            nlohmann::json v = card_param_value(*cc, q);
            return v.is_string() ? v.get<std::string>() : std::string();
        }
    return std::string();
}

static bool media_original(Machine* m, Card* c, int u, std::string& orig, std::string& work)
{
    std::string img = media_image(m, c, u);
    for (char& ch : img) if (ch == '\\') ch = '/';
    nlohmann::json* cc = m ? m->card_config(c) : nullptr;
    bool copies = !cc || cc->value("working_copy", true);
    if (!copies || img.rfind("media/", 0) != 0) return false;
    orig = data_path(img);
    work = data_path("work/" + img.substr(6));
    return file_exists(orig);
}

static std::string s_restore_card;   // Restore from the original: asked in a modal window
static int s_restore_unit = -1;
static bool s_restore_open = false;

// The "..." button after Eject: Eject without saving (the changes still in memory are thrown away),
// Restore from the original (the working copy in work/ gets the file of media/ again).
static void media_more(UiState& ui, Card* c, int u, bool has)
{
    Machine* m = ui.host->machine();
    ImGui::SameLine();
    if (ImGui::Button("...##more")) ImGui::OpenPopup("more");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Eject without saving, restore from the original"));
    if (!ImGui::BeginPopup("more")) return;
    if (ImGui::MenuItem(tr("Eject without saving"), nullptr, false, has)) {
        c->media_discard(u);
        media_set(ui, c, u, "");
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tr("What was written on it since it was put in is thrown away (only what is still in memory)."));
    std::string orig, work;
    bool can = media_original(m, c, u, orig, work);
    if (ImGui::MenuItem(tr("Restore from the original..."), nullptr, false, can)) {
        s_restore_card = c->name;
        s_restore_unit = u;
        s_restore_open = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", can ? tr("The working copy in work/ is replaced by the original in media/.")
                                    : tr("Only for media of media/ used through a working copy."));
    ImGui::EndPopup();
}

static void media_restore_dialog(UiState& ui)
{
    Machine* m = ui.host->machine();
    if (s_restore_open) {
        ImGui::OpenPopup(tr("Restore from the original"));
        s_restore_open = false;
    }
    if (!ImGui::BeginPopupModal(tr("Restore from the original"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    Card* c = m ? m->find_card(s_restore_card) : nullptr;
    std::string orig, work;
    bool can = c && media_original(m, c, s_restore_unit, orig, work);
    if (can) {
        ImGui::Text("%s", tr("The working copy"));
        ImGui::TextDisabled("%s", work.c_str());
        ImGui::Text("%s", tr("is replaced by the original"));
        ImGui::TextDisabled("%s", orig.c_str());
        ImGui::TextUnformatted(tr("All changes made in the working copy are lost."));
    } else ImGui::TextUnformatted(tr("The medium has no original in media/."));
    ImGui::BeginDisabled(!can);
    if (ImGui::Button(tr("Restore"))) {
        std::string img = media_image(m, c, s_restore_unit);
        c->media_discard(s_restore_unit);
        media_set(ui, c, s_restore_unit, "");
        std::vector<uint8_t> d;
        bool ok = file_read_all(orig, d) && file_write_all(work, d.data(), d.size());
        media_set(ui, c, s_restore_unit, img);
        state_notice(ui, ok, ok ? std::string(tr("Restored from the original")) + ": " + work
                                : std::string(tr("Cannot write")) + " " + work);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel"))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

static void tape_dialog_result(UiState& ui, Machine* m)
{
    TapeDialog& d = ui.tape_dlg;
    int kind;
    std::string path, card;
    int drive;
    {
        std::lock_guard<std::mutex> lk(d.mtx);
        if (!d.kind || !d.done) return;
        kind = d.kind;
        path = d.result;
        card = d.card;
        drive = d.drive;
        d.kind = 0;
    }
    if (kind == 7 || kind == 8) {   // save state (needs no machine for loading)
        if (path.empty()) return;
        state_file(ui, path, kind == 7);
        return;
    }
    if (path.empty() || !m) return;
    d.last_dir = path_dir(path);
    if (kind == 3) {   // a program: show where it goes first
        hex_scan(ui, path);
        return;
    }
    if (kind == 6) {   // a binary file: ask for the address first
        ui.bin_path = path;
        ui.bin_ask = true;
        return;
    }
    if (kind == 10 && tape_is_program(path)) {   // a program: made into a cassette, the name first
        ui.tprog_path = path;
        ui.tprog_card = card;
        ui.tprog_kzd = true;
        ui.tprog_ask = true;
        ui.tprog_error.clear();
        snprintf(ui.tprog_name, sizeof(ui.tprog_name), "%s", tape_default_name(path).c_str());
        return;
    }
    if (kind == 10 || kind == 11) {   // a cassette for the KZD-1 data recorder
        Card* r = m->find_card(card);
        Kzd1* k = r ? r->kzd1() : nullptr;
        if (!k) return;
        std::string err;
        if (kind == 11) {   // a new one: empty (the save dialog asked before replacing a file)
            size_t slash = path.find_last_of("/\\");
            if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".kzd";
            KzdCassette empty;
            if (!empty.save(path, err)) {
                LOGE("%s", err.c_str());
                return;
            }
        }
        media_set(ui, r, 0, data_relative(path));
        return;
    }
    if (kind == 12 || kind == 13) {   // a cartridge for a unit of the RKD-1 (a new one: unformatted)
        Rkd1* r = dynamic_cast<Rkd1*>(m->find_card(card));
        if (!r) return;
        size_t slash = path.find_last_of("/\\");
        if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".rkd";
        if (kind == 13) file_remove(path);   // the save dialog asked before replacing it
        media_set(ui, r, drive, data_relative(path));
        return;
    }
    if (kind == 14 || kind == 15) {   // a reel for a unit of the RMP-1 (a new one: blank tape)
        Rmp1* r = dynamic_cast<Rmp1*>(m->find_card(card));
        if (!r) return;
        size_t slash = path.find_last_of("/\\");
        if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".tap";
        if (kind == 15) file_remove(path);   // the save dialog asked before replacing it
        media_set(ui, r, drive, data_relative(path));
        return;
    }
    if (kind == 4 || kind == 5) {   // a floppy for a drive of a disk controller (RPD-1, FDC-1)
        Card* r = m->find_card(card);
        if (!r || !r->floppy_drive(drive)) return;
        std::string err;
        if (kind == 5) {   // a new disk, formatted: every byte E5h (empty CP/M directory)
            size_t slash = path.find_last_of("/\\");
            if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) {
                int size = floppy_geoms()[s_new_disk_fmt]->size;   // SAPI-1.5 formats keep their own suffix
                path += size == 327680 ? ".sp2" : size == 163840 ? ".sp1" : ".img";
            }
            if (!FloppyDrive::create(path, floppy_geoms()[s_new_disk_fmt], err)) {
                LOGE("%s", err.c_str());
                return;
            }
        }
        // a disk from media/ is used through its working copy
        media_set(ui, r, drive, data_relative(path));
        return;
    }
    Dsm1* dsm = dynamic_cast<Dsm1*>(m->find_card(card));
    if (!dsm) return;
    std::string err;
    TapeDeck* t = dsm->tape();
    if (kind == 9) {   // export: the tape stays in the recorder
        size_t slash = path.find_last_of("/\\");
        if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".wav";
        if (t->export_wav(path, err)) state_notice(ui, true, std::string(tr("Tape saved as WAV")) + ": " + path);
        else state_notice(ui, false, err);
        return;
    }
    if (kind == 1 && tape_is_program(path)) {   // a program: the format of the tape first
        ui.tprog_path = path;
        ui.tprog_card = card;
        ui.tprog_kzd = false;
        ui.tprog_ask = true;
        ui.tprog_error.clear();
        snprintf(ui.tprog_name, sizeof(ui.tprog_name), "%s", tape_default_name(path).c_str());
        return;
    }
    if (kind == 1) {
        media_set(ui, dsm, 0, data_relative(path));
    } else {
        size_t slash = path.find_last_of("/\\");
        if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".wav";
        t->eject();
        if (!t->create(path, err)) LOGE("%s", err.c_str());
        else media_note(ui, dsm, 0, data_relative(path));
    }
}

static std::string mmss(double s)
{
    char b[16];
    int t = (int)s;
    snprintf(b, sizeof(b), "%d:%02d", t / 60, t % 60);
    return b;
}

static void draw_tape(UiState& ui, Dsm1* dsm)
{
    TapeDeck* t = dsm->tape();
    ImGui::PushID(dsm);
    if (t->loaded()) {
        ImGui::TextUnformatted(t->path.c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t->path.c_str());
        if (t->generated) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", t->program_info.c_str());
            ImGui::PopStyleColor();
        }
    } else {
        ImGui::TextDisabled("%s", tr("(no cassette)"));
    }
    if (ImGui::Button(tr("Insert..."))) tape_dialog(ui, 1, dsm->name);
    ImGui::SameLine();
    if (ImGui::Button(tr("New cassette..."))) tape_dialog(ui, 2, dsm->name);
    ImGui::SameLine();
    ImGui::BeginDisabled(!t->loaded());
    if (ImGui::Button(tr("Eject"))) media_set(ui, dsm, 0, "");
    ImGui::EndDisabled();
    media_more(ui, dsm, 0, t->loaded());
    ImGui::BeginDisabled(!t->loaded());
    ImGui::SameLine();
    if (ImGui::Button(tr("Save as WAV..."))) tape_dialog(ui, 9, dsm->name);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tr("A copy of the tape as a WAV file, e.g. to play it into a real SAPI."));
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!t->loaded());
    struct Btn { const char* label; int mode; };
    const Btn btns[3] = { { "PLAY", TapeDeck::PLAY }, { "REC", TapeDeck::RECORD }, { "STOP", TapeDeck::STOP } };
    if (ImGui::Button(tr("<< Rewind"))) {
        t->mode = TapeDeck::STOP;
        t->pos = 0;
    }
    for (const Btn& b : btns) {
        ImGui::SameLine();
        bool on = t->mode == b.mode;
        bool dis = b.mode == TapeDeck::RECORD && t->write_protect;
        ImGui::BeginDisabled(dis);
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, b.mode == TapeDeck::RECORD ? IM_COL32(170, 40, 40, 255) : IM_COL32(40, 120, 60, 255));
        if (ImGui::Button(tr(b.label))) t->mode = (TapeDeck::Mode)b.mode;
        if (on) ImGui::PopStyleColor();
        ImGui::EndDisabled();
        if (dis && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", tr("The cassette is write protected"));
    }
    ImGui::SameLine();
    bool wp = t->write_protect;
    ImGui::BeginDisabled(t->read_only());
    if (ImGui::Checkbox(tr("Write protect"), &wp)) {
        t->write_protect = wp;
        if (wp && t->mode == TapeDeck::RECORD) t->mode = TapeDeck::STOP;
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    double len = t->length_seconds(), p = t->position_seconds();
    char over[64];
    snprintf(over, sizeof(over), "%s / %s", mmss(p).c_str(), mmss(len).c_str());
    ImGui::ProgressBar(len > 0 ? (float)(p / len) : 0.0f, ImVec2(260, 0), over);
    ImGui::SameLine();
    if (dsm->motor_on()) ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", tr("motor running"));
    else ImGui::TextDisabled("%s", tr("motor stopped"));
    ImGui::PopID();
}

// KZD-1: the cassette drawn with its reels (the tape wound from one to the other), its state and controls.
static void draw_kzd(UiState& ui, Card* card, Kzd1* k)
{
    ImGui::PushID(k);
    float u = ImGui::GetFontSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    float w = 15.0f * u, h = 9.4f * u;
    bool has = k->loaded();
    ImU32 shell = has ? IM_COL32(52, 54, 60, 255) : IM_COL32(36, 36, 38, 255);
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), shell, 0.5f * u);
    dl->AddRect(o, ImVec2(o.x + w, o.y + h), IM_COL32(120, 122, 128, 255), 0.5f * u, 0, 1.5f);
    if (has) {
        // label strip and the side letter
        dl->AddRectFilled(ImVec2(o.x + 0.8f * u, o.y + 0.6f * u), ImVec2(o.x + w - 0.8f * u, o.y + 2.0f * u),
                          IM_COL32(225, 215, 180, 255), 0.2f * u);
        std::string name = k->path();
        size_t sl = name.find_last_of("/\\");
        if (sl != std::string::npos) name = name.substr(sl + 1);
        dl->AddText(ImVec2(o.x + 1.1f * u, o.y + 0.8f * u), IM_COL32(30, 30, 30, 255), name.c_str());
        char sd[4] = { (char)(k->side ? 'B' : 'A'), 0 };
        dl->AddText(ImGui::GetFont(), 1.4f * u, ImVec2(o.x + w - 2.0f * u, o.y + 0.65f * u), IM_COL32(170, 30, 30, 255), sd);
        // reels: hub 11 mm, full 24 mm; the tape of this side from the left reel to the right one
        const double hub = 0.011, full = 0.024, len = KzdCassette::LENGTH_UM * 1e-6;
        double p = k->position_m();
        double left = len - p;
        double rl = sqrt(hub * hub + (full * full - hub * hub) * left / len);
        double rr = sqrt(hub * hub + (full * full - hub * hub) * p / len);
        float sc = 2.6f * u / (float)full;   // pixels a metre
        ImVec2 cl(o.x + 4.2f * u, o.y + 5.3f * u), cr(o.x + w - 4.2f * u, o.y + 5.3f * u);
        ImU32 tape = IM_COL32(92, 58, 36, 255);
        dl->AddCircleFilled(cl, (float)rl * sc, tape, 40);
        dl->AddCircleFilled(cr, (float)rr * sc, tape, 40);
        // the tape between the reels past the head (bottom)
        float hy = o.y + h - 0.9f * u;
        dl->AddLine(ImVec2(cl.x, cl.y + (float)rl * sc), ImVec2(o.x + 3.0f * u, hy), tape, 2.0f);
        dl->AddLine(ImVec2(o.x + 3.0f * u, hy), ImVec2(o.x + w - 3.0f * u, hy), tape, 2.0f);
        dl->AddLine(ImVec2(o.x + w - 3.0f * u, hy), ImVec2(cr.x, cr.y + (float)rr * sc), tape, 2.0f);
        bool rec = k->recording() && k->speed() != 0;
        dl->AddRectFilled(ImVec2(o.x + w * 0.5f - 0.8f * u, hy - 0.1f * u), ImVec2(o.x + w * 0.5f + 0.8f * u, hy + 0.6f * u),
                          rec ? IM_COL32(220, 50, 40, 255) : IM_COL32(170, 172, 178, 255), 0.15f * u);
        // hubs turning: the angle follows the tape wound on the reel
        for (int side = 0; side < 2; side++) {
            ImVec2 c = side ? cr : cl;
            double r = side ? rr : rl;
            float a = (float)((side ? p : -left) / (r > 0 ? r : hub));
            float hr = (float)hub * sc;
            dl->AddCircleFilled(c, hr, IM_COL32(215, 215, 220, 255), 24);
            for (int s = 0; s < 6; s++) {
                float t = a + s * 3.14159265f / 3;
                dl->AddLine(c, ImVec2(c.x + cosf(t) * hr * 0.85f, c.y + sinf(t) * hr * 0.85f), IM_COL32(60, 60, 64, 255), 2.0f);
            }
        }
    } else {
        dl->AddText(ImVec2(o.x + 1.0f * u, o.y + 4.0f * u), IM_COL32(150, 150, 150, 255), tr("(no cassette)"));
    }
    ImGui::Dummy(ImVec2(w, h));
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (has) {
        ImGui::Text("%s %c   %.2f / %.0f m", tr("Side"), k->side ? 'B' : 'A', k->position_m(), KzdCassette::LENGTH_UM * 1e-6);
        ImGui::ProgressBar((float)(k->position_m() / (KzdCassette::LENGTH_UM * 1e-6)), ImVec2(12.0f * u, 0), "");
    }
    double sp = k->speed();
    if (sp != 0 || k->target() != 0)
        ImGui::TextColored(k->recording() ? ImVec4(1, 0.45f, 0.4f, 1) : ImVec4(0.5f, 1, 0.5f, 1), "%s  %.2f m/s", tr(k->motion()), sp);
    else
        ImGui::TextDisabled("%s", tr("stop"));
    if (k->overload(1) || k->overload(2))
        ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "%s %d", tr("End of tape: overload of motor"), k->overload(1) ? 1 : 2);
    if (has) {
        bool ta = k->tab(0), tb = k->tab(1);
        const char* tip = tr("The break-out tab of the side: present = recording allowed (MIKOS/K checks it)");
        if (ImGui::Checkbox(tr("Tab A"), &ta)) k->set_tab(0, ta);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        ImGui::SameLine();
        if (ImGui::Checkbox(tr("Tab B"), &tb)) k->set_tab(1, tb);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    }
    ImGui::EndGroup();
    if (ImGui::Button(tr("Insert..."))) tape_dialog(ui, 10, card->name);
    ImGui::SameLine();
    if (ImGui::Button(tr("New cassette..."))) tape_dialog(ui, 11, card->name);
    ImGui::SameLine();
    ImGui::BeginDisabled(!has);
    if (ImGui::Button(tr("Eject"))) media_set(ui, card, 0, "");
    ImGui::EndDisabled();
    media_more(ui, card, 0, has);
    ImGui::BeginDisabled(!has);
    ImGui::SameLine();
    if (ImGui::Button(tr("Turn over"))) k->flip();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tr("The other side of the cassette faces the head (the position is mirrored)."));
    ImGui::EndDisabled();
    ImGui::PopID();
}

// " (C:)" from the machine description, " (probably C:)" seen while running, else nothing
static std::string cpm_suffix(char cfg, char seen)
{
    if (cfg) return std::string(" (") + cfg + ":)";
    if (seen) return std::string(" (") + tr("probably") + " " + seen + ":)";
    return std::string();
}

static void cpm_tooltip(void)
{
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Unit of the controller. The CP/M letter is from the machine description, \"probably\" "
                                   "= seen while running (the drive of the last BIOS SELDSK at a sector read)."));
}

static void draw_pmd32(Pmd32Sd* p)
{
    std::vector<std::string> images = p->list_images();
    ImGui::TextDisabled("%s %s", tr("SD card:"), p->root().c_str());
    ImGui::PushID(p);   // two cards may have a PMD32-SD (IDE-1, DPP-1)
    if (!ImGui::BeginTable("pmd", 3, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::PopID();
        return;
    }
    for (int d = 0; d < Pmd32Sd::DRIVES; d++) {
        ImGui::PushID(d);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%c%s", 'A' + d, cpm_suffix(p->cpm_cfg[d], p->cpm_seen[d]).c_str());
        cpm_tooltip();
        ImGui::TableNextColumn();
        std::string cur = p->drive_present(d) ? p->drive_path(d) : std::string(tr("(empty)"));
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("##img", cur.c_str())) {
            if (ImGui::Selectable(tr("(empty)"), !p->drive_present(d))) p->eject(d);
            for (const std::string& im : images)
                if (ImGui::Selectable(im.c_str(), im == p->drive_path(d))) p->mount(d, im, p->drive_wp(d));
            ImGui::EndCombo();
        }
        ImGui::TableNextColumn();
        bool wp = p->drive_wp(d);
        if (ImGui::Checkbox(tr("Write protect"), &wp)) p->set_wp(d, wp);
        ImGui::PopID();
    }
    ImGui::EndTable();
    ImGui::Checkbox(tr("SD card locked"), &p->card_locked);
    ImGui::PopID();
}

void hex_load_dialog(UiState& ui)
{
    tape_dialog(ui, 3, std::string());
}

void bin_load_dialog(UiState& ui)
{
    tape_dialog(ui, 6, std::string());
}

void state_save_dialog(UiState& ui)
{
    tape_dialog(ui, 7, std::string());
}

void state_load_dialog(UiState& ui)
{
    tape_dialog(ui, 8, std::string());
}

void state_quick(UiState& ui, bool save)
{
    if (save && !ui.host->machine()) return;
    std::string path = ui.host->quick_state_path();
    if (!save && !file_exists(path)) {
        state_notice(ui, false, std::string(tr("The quick slot of this machine is empty")) + ":\n" + path);
        return;
    }
    state_file(ui, path, save);
}

// CP/M file name (8.3, capitals) for a host file: letters and digits of its name, the given
// extension or its own. "Void - Terrania.rad" -> "VOIDTERR.RAD".
static std::string cpm_name(const std::string& path, const char* ext)
{
    size_t slash = path.find_last_of("/\\");
    std::string base = path.substr(slash == std::string::npos ? 0 : slash + 1);
    size_t dot = base.find_last_of('.');
    std::string own = dot == std::string::npos ? std::string() : base.substr(dot + 1);
    if (dot != std::string::npos) base.resize(dot);
    std::string out;
    for (char c : base) {
        if (out.size() == 8) break;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            out += (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
    }
    if (out.empty()) out = "PROGRAM";
    std::string e = ext ? std::string(ext) : own;
    std::string ue;
    for (char c : e) {
        if (ue.size() == 3) break;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            ue += (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
    }
    return ue.empty() ? out : out + "." + ue;
}

// The CP/M command that saves what was loaded from 0100h up to `last`.
static void cpm_save_hint(const std::string& path, long long last, const char* ext)
{
    long long pages = (last - 0x100) / 256 + 1;
    ImGui::Text("CP/M: SAVE %lld %s", pages, cpm_name(path, ext).c_str());
}

const ImVec4 k_load_error(1.0f, 0.4f, 0.4f, 1.0f);

// File -> Load program (Intel HEX): the addresses come from the file; shown with the size and
// the pages for CP/M SAVE before anything is written (the same layout as for a binary file).
static void hex_load_question(UiState& ui, Machine* m)
{
    const char* title = tr_id("Load program into memory (Intel HEX)");
    if (ui.hex_ask) {
        ImGui::OpenPopup(title);
        ui.hex_ask = false;
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    bool ok = ui.hex_error.empty();
    ImGui::TextUnformatted(ui.hex_path.c_str());
    if (ok) {
        ImGui::Text("%s %d B", tr("Size:"), ui.hex_bytes);
        ImGui::TextDisabled("%s", tr("Address: from the records of the file"));
        ImGui::Text("%04X-%04Xh", (unsigned)ui.hex_lo, (unsigned)ui.hex_hi);
        if (ui.hex_lo == 0x100) cpm_save_hint(ui.hex_path, ui.hex_hi, "COM");
    } else {
        ImGui::TextColored(k_load_error, "%s", ui.hex_error.c_str());
    }
    ImGui::BeginDisabled(!ok || !m);
    if (ImGui::Button(tr("Load"))) {
        std::string text, err;
        int n = 0;
        if (!file_read_text(ui.hex_path, text)) LOGE("cannot read %s", ui.hex_path.c_str());
        else if (!m->load_ihex_text(text, err, &n)) LOGE("%s: %s", ui.hex_path.c_str(), err.c_str());
        else LOGI("%s: %d bytes loaded at %04X-%04Xh", ui.hex_path.c_str(), n, (unsigned)ui.hex_lo, (unsigned)ui.hex_hi);
        ui.hex_path.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel"))) {
        ui.hex_path.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

static long long file_size_of(const std::string& path)
{
    FILE* f = file_open(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long long n = ftell(f);
    fclose(f);
    return n;
}

// The address for File -> Load binary file, with the size and the pages for CP/M SAVE.
static void bin_load_question(UiState& ui, Machine* m)
{
    const char* title = tr_id("Load binary file into memory");
    if (ui.bin_ask) {
        ImGui::OpenPopup(title);
        ui.bin_ask = false;
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    long long size = file_size_of(ui.bin_path);
    ImGui::TextUnformatted(ui.bin_path.c_str());
    ImGui::Text("%s %lld B", tr("Size:"), size);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
    ImGui::InputText(tr("Address (hex)"), ui.bin_addr, sizeof(ui.bin_addr), ImGuiInputTextFlags_CharsHexadecimal);
    long long addr = strtol(ui.bin_addr, nullptr, 16);
    bool fits = size > 0 && addr >= 0 && addr + size <= 0x10000;
    if (fits) {
        ImGui::Text("%04llX-%04llXh", addr, addr + size - 1);
        if (addr == 0x100) cpm_save_hint(ui.bin_path, addr + size - 1, nullptr);
    } else {
        ImGui::TextColored(k_load_error, "%s", size < 0 ? tr("Cannot read the file.")
                                                        : tr("The file does not fit into memory there."));
    }
    ImGui::BeginDisabled(!fits || !m);
    if (ImGui::Button(tr("Load"))) {
        std::string err;
        int n = 0;
        if (!m->load_binary(ui.bin_path, (uint16_t)addr, err, &n)) LOGE("%s", err.c_str());
        else LOGI("%s: %d bytes loaded at %04llXh", ui.bin_path.c_str(), n, addr);
        ui.bin_path.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel"))) {
        ui.bin_path.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// Media -> Insert a program into a recorder: the format, the name and the addresses of the tape.
static void tape_program_question(UiState& ui, Machine* m)
{
    const char* title = tr_id("Program on a cassette");
    if (ui.tprog_ask) {
        ImGui::OpenPopup(title);
        ui.tprog_ask = false;
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::TextUnformatted(ui.tprog_path.c_str());
    if (ui.tprog_kzd) {   // KZD-1: a new cassette as MIKOS/K writes it
        ImGui::TextUnformatted(tr("A new KZD-1 cassette (work/kzd-1) with the program as MIKOS/K saves it."));
        ImGui::TextDisabled("%s", tr("From 4400h up; .K_Z, .K_L name, then .J starts the program at its lowest address."));
    } else {
        ImGui::RadioButton(tr("MIKOS (ZPS-2): .K_L name, then .J"), &ui.tprog_format, TAPE_MIKOS);
        ImGui::RadioButton(tr("MIKROBASIC (ZPS-1): LOAD"), &ui.tprog_format, TAPE_MIKROBASIC);
        ImGui::TextDisabled("%s", ui.tprog_format == TAPE_MIKOS
                                      ? tr("From 4400h up; .J starts the program at its lowest address.")
                                      : tr("A BASIC program at 40EFh, or machine code from 4102h up (RUN starts it)."));
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::InputText(tr("Name on the tape"), ui.tprog_name, sizeof(ui.tprog_name), ImGuiInputTextFlags_CharsUppercase);
    bool bin = !tape_is_hex(ui.tprog_path);
    if (bin) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
        ImGui::InputText(tr("Address (hex)"), ui.tprog_addr, sizeof(ui.tprog_addr), ImGuiInputTextFlags_CharsHexadecimal);
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
    ImGui::InputText(tr("Start (hex, empty = lowest address)"), ui.tprog_entry, sizeof(ui.tprog_entry),
                     ImGuiInputTextFlags_CharsHexadecimal);
    if (!ui.tprog_error.empty()) ImGui::TextColored(k_load_error, "%s", ui.tprog_error.c_str());
    Card* card = m ? m->find_card(ui.tprog_card) : nullptr;
    Dsm1* dsm = ui.tprog_kzd ? nullptr : dynamic_cast<Dsm1*>(card);
    Kzd1* kzd = ui.tprog_kzd && card ? card->kzd1() : nullptr;
    ImGui::BeginDisabled(!dsm && !kzd);
    if (ImGui::Button(tr("Insert"))) {
        TapeProgram p;
        p.format = ui.tprog_kzd ? TAPE_MIKOS : ui.tprog_format;
        p.name = ui.tprog_name;
        p.address = bin ? strtol(ui.tprog_addr, nullptr, 16) : -1;
        p.entry = ui.tprog_entry[0] ? strtol(ui.tprog_entry, nullptr, 16) : -1;
        std::string err, info;
        bool ok;
        if (kzd) {
            KzdCassette c;
            ok = kzd_from_program(ui.tprog_path, p, "", c, info, err);
            if (ok) {
                std::string name = tape_name_clean(p.name);
                std::string to = kzd_program_path(name.empty() ? tape_default_name(ui.tprog_path) : name);
                kzd->insert(c, to);
                media_note(ui, card, 0, data_relative(to));
                kzd->flush();
                state_notice(ui, true, info + " -> " + to);
            }
        } else {
            TapeDeck* t = dsm->tape();
            t->eject();
            ok = t->load_program(ui.tprog_path, p, err);
            if (ok) media_note(ui, dsm, 0, data_relative(ui.tprog_path));
        }
        if (ok) {
            ui.tprog_path.clear();
            ImGui::CloseCurrentPopup();
        } else {
            ui.tprog_error = err;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel"))) {
        ui.tprog_path.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// ---------------------------------------------------------------- floppy drives (RPD-1, FDC-1)

static void draw_floppies(UiState& ui, Card* r)
{
    ImGui::PushID(r);
    const FloppyGeom* const* geoms = floppy_geoms();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr("Format of a new disk"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 11);
    if (ImGui::BeginCombo("##fmt", geoms[s_new_disk_fmt]->name)) {
        for (int g = 0; geoms[g]; g++)
            if (ImGui::Selectable(geoms[g]->name, g == s_new_disk_fmt)) s_new_disk_fmt = g;
        ImGui::EndCombo();
    }
    for (int i = 0; r->floppy_drive(i); i++) {
        FloppyDrive& d = *r->floppy_drive(i);
        ImGui::PushID(i);
        ImGui::Text("%s %d%s:", tr("Drive"), i, cpm_suffix(d.cpm_cfg, d.cpm_seen).c_str());
        cpm_tooltip();
        ImGui::SameLine();
        if (!d.present()) ImGui::TextDisabled("%s", tr("(no disk)"));
        else {
            ImGui::TextUnformatted(d.path.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", d.path.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", d.format.c_str());
        }
        if (ImGui::Button(tr("Insert..."))) tape_dialog(ui, 4, r->name, i);
        ImGui::SameLine();
        if (ImGui::Button(tr("New disk..."))) tape_dialog(ui, 5, r->name, i);
        ImGui::SameLine();
        ImGui::BeginDisabled(!d.present());
        if (ImGui::Button(tr("Eject"))) media_set(ui, r, i, "");
        ImGui::EndDisabled();
        media_more(ui, r, i, d.present());
        ImGui::BeginDisabled(!d.present());
        ImGui::SameLine();
        ImGui::Checkbox(tr("Write protect"), &d.write_protect);
        ImGui::SameLine();
        ImGui::Text("%s %d%s", tr("track"), d.track, d.dirty ? tr(", modified") : "");
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::PopID();
}

// Emulator help "System..." (the SYSGEN of Tesla for RKD-1 was not found): the system tracks of A: from the
// system cartridge, optionally a format first (as FORMTRKD). Marked as help in the dialog.
static int s_rkd_sys_unit = -1;
static bool s_rkd_sys_format = false;
static std::string s_rkd_sys_error;

static void rkd_system_dialog(Rkd1* r, bool open)
{
    const char* title = tr_id("System on the cartridge (SYSGEN)");
    if (open) ImGui::OpenPopup(title);
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    int u = s_rkd_sys_unit;
    float wrap = ImGui::GetFontSize() * 34;
    ImGui::PushTextWrapPos(wrap);
    ImGui::TextUnformatted(tr("Emulator help: the original SYSGEN for RKD-1 was not found. This writes CP/M 2.2 of "
                              "Tesla Elstroj (CCP, BDOS, CBIOS 87-02-13) on tracks 0 and 1 of A:, byte for byte as on "
                              "the system cartridge. The files on A: and B: stay."));
    ImGui::TextDisabled("%s %s", tr("Source:"), Rkd1::SYSTEM_SOURCE);
    ImGui::Checkbox(tr("Format the whole cartridge first (A: and B: as FORMTRKD does; erases all files)"),
                    &s_rkd_sys_format);
    if (u >= 0 && !s_rkd_sys_format && !r->platter_formatted(u, Rkd1::CARTRIDGE))
        ImGui::TextColored(k_load_error, "%s", tr("The cartridge is not formatted: A: cannot be used without it."));
    ImGui::TextUnformatted(tr("Then Reset (F12): the CBIOS does not notice a changed cartridge. Copy SH.COM, SH.OVR, "
                              "STARTSYS.SUB and other files with PIP, e.g. through the fixed disk C:."));
    if (!s_rkd_sys_error.empty()) ImGui::TextColored(k_load_error, "%s", s_rkd_sys_error.c_str());
    ImGui::PopTextWrapPos();
    if (ImGui::Button(tr("Write the system"))) {
        std::string err;
        bool ok = u >= 0;
        if (ok && s_rkd_sys_format) ok = r->format_platter(u, Rkd1::CARTRIDGE, err);
        if (ok) ok = r->write_system(u, Rkd1::SYSTEM_SOURCE, err);
        if (ok) {
            s_rkd_sys_error.clear();
            s_rkd_sys_unit = -1;
            ImGui::CloseCurrentPopup();
        } else {
            s_rkd_sys_error = err;
            LOGE("RKD-1: %s", err.c_str());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel"))) {
        s_rkd_sys_error.clear();
        s_rkd_sys_unit = -1;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

static void draw_rkd(UiState& ui, Rkd1* r)
{
    bool open_sys = false;
    ImGui::PushID(r);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr("New cartridge"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 13);
    const char* fmts[2] = { "24 x 256 B (Tesla CP/M)", "12 x 512 B (SMEP)" };
    int f = r->new_sectors == 12 ? 1 : 0;
    if (ImGui::Combo("##rkdfmt", &f, fmts, 2)) r->new_sectors = f ? 12 : 24;
    for (int i = 0; i < Rkd1::UNITS; i++) {
        Rkd1::Unit& u = r->units[i];
        if (!u.fitted) continue;
        ImGui::PushID(i);
        RkdPlatter& c = u.disk[Rkd1::CARTRIDGE];
        ImGui::Text("%s %d (%s), %s:", tr("Unit"), i, u.drive == Rkd1::CM5400 ? "CM 5400" : "CM 5403", tr("cartridge"));
        ImGui::SameLine();
        if (!c.loaded) ImGui::TextDisabled("%s", tr("(no cartridge)"));
        else {
            ImGui::TextUnformatted(c.path.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", c.path.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%d x %d B)", c.sectors, c.sectors == 12 ? 512 : 256);
        }
        if (ImGui::Button(tr("Insert..."))) tape_dialog(ui, 12, r->name, i);
        ImGui::SameLine();
        if (ImGui::Button(tr("New cartridge..."))) tape_dialog(ui, 13, r->name, i);
        ImGui::SameLine();
        ImGui::BeginDisabled(!c.loaded);
        if (ImGui::Button(tr("Eject"))) media_set(ui, r, i, "");
        ImGui::EndDisabled();
        media_more(ui, r, i, c.loaded);
        ImGui::BeginDisabled(!c.loaded);
        ImGui::SameLine();
        ImGui::Checkbox(tr("Write protect"), &c.write_protect);
        ImGui::SameLine();
        ImGui::Text("%s %d%s", tr("track"), u.track, c.dirty ? tr(", modified") : "");
        if (u.drive == Rkd1::CM5403 && c.sectors == 24) {   // emulator help, the dialog says so
            ImGui::SameLine();
            if (ImGui::Button(tr("System..."))) {
                s_rkd_sys_unit = i;
                s_rkd_sys_format = !r->platter_formatted(i, Rkd1::CARTRIDGE);
                s_rkd_sys_error.clear();
                open_sys = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tr("Write the CP/M system on the cartridge (emulator help, no original SYSGEN)"));
        }
        ImGui::EndDisabled();
        RkdPlatter& x = u.disk[Rkd1::FIXED];
        ImGui::Text("   %s:", tr("fixed disk"));
        ImGui::SameLine();
        if (x.loaded) {
            ImGui::TextUnformatted(x.path.c_str());
            ImGui::SameLine();
            ImGui::Checkbox(tr("Write protect##fixed"), &x.write_protect);
        } else ImGui::TextDisabled("-");
        ImGui::PopID();
    }
    rkd_system_dialog(r, open_sys);
    ImGui::PopID();
}

// ---------------------------------------------------------------- RMP-1 magnetic tape

// EBCDIC (code page 037) to ASCII for the labels and the dump, '.' for the rest
static char ebcdic(uint8_t c)
{
    static const char* const tab =
        "................................................................"
        " ...........<(+|&.........!$*);.-/.........,%_>?.........`:#@'=\""
        ".abcdefghi.......jklmnopqr.......~stuvwxyz......^.........[]...."
        "{ABCDEFGHI......}JKLMNOPQR......\\.STUVWXYZ......0123456789......";
    return tab[c];
}

// What the tape holds, grouped for the map: the records change only while the drive writes, the
// grouping is made again when the rows change (at most four times a second).
struct RmpMapItem {
    int kind;          // 0 label (VOL1, HDR1...), 1 data blocks, 2 tape mark
    int count;         // data blocks in the group
    int bad;           // of them with a parity, CRC or LRC error
    int64_t pos;       // the first row
    int64_t end;       // the last row
    char label[5];
    size_t bytes;      // the size of the first block
};
struct RmpMapCache {
    const MagTape* tape = nullptr;
    size_t rows = 0;
    int64_t last = -1;
    uint64_t time = 0;
    std::vector<RmpMapItem> items;
    std::vector<MagTape::Record> recs;
    // the labels (JSEP): the name of the tape from VOL1 and its code, the files from HDR1 and HDR2, as
    // MTDAT, DIRMT and MTCOPY want them
    std::string volume;
    char code = 0;               // 'D' EBCDIC, 'A' ASCII, 0 = no VOL1
    std::vector<std::string> files;
};

// a field of a label (columns from..to as in the standard, 1-based), in the code of the tape, spaces trimmed
static std::string label_field(const std::vector<uint8_t>& d, int from, int to, char code)
{
    std::string s;
    for (int i = from - 1; i < to && i < (int)d.size(); i++) s += code == 'A' ? (char)d[(size_t)i] : ebcdic(d[(size_t)i]);
    size_t e = s.find_last_not_of(' ');
    return e == std::string::npos ? std::string() : s.substr(0, e + 1);
}

static void rmp_labels(RmpMapCache& c)
{
    c.volume.clear();
    c.code = 0;
    c.files.clear();
    for (size_t k = 0; k < c.recs.size(); k++) {
        const std::vector<uint8_t>& d = c.recs[k].data;
        if (c.recs[k].mark || d.size() != 80) continue;
        char code = 0;
        if (!memcmp(d.data(), "\xC8\xC4\xD9", 3) || !memcmp(d.data(), "\xE5\xD6\xD3", 3)) code = 'D';   // HDR, VOL
        else if (!memcmp(d.data(), "HDR", 3) || !memcmp(d.data(), "VOL", 3)) code = 'A';
        if (!code) continue;
        std::string id = label_field(d, 1, 4, code);
        if (id == "VOL1" && c.code == 0) {
            c.code = code;
            c.volume = label_field(d, 5, 10, code);
        } else if (id == "HDR1") {
            std::string name = label_field(d, 5, 21, code);
            int seq = atoi(label_field(d, 32, 35, code).c_str());
            std::string sizes;
            if (k + 1 < c.recs.size() && c.recs[k + 1].data.size() == 80 &&
                label_field(c.recs[k + 1].data, 1, 4, code) == "HDR2") {
                const std::vector<uint8_t>& h2 = c.recs[k + 1].data;
                char b[64];
                snprintf(b, sizeof(b), ", %s %d, %s %d", tr("block"), atoi(label_field(h2, 6, 10, code).c_str()),
                         tr("record"), atoi(label_field(h2, 11, 15, code).c_str()));
                sizes = b;
            }
            char line[128];
            snprintf(line, sizeof(line), "%d  %s%s", seq, name.c_str(), sizes.c_str());
            c.files.push_back(line);
        }
    }
}
static RmpMapCache s_rmp_map[Rmp1::UNITS];

static void rmp_map_update(RmpMapCache& c, const MagTape& t)
{
    int64_t last = t.rows.empty() ? -1 : t.rows.back().pos;
    uint64_t now = SDL_GetTicks();
    if (c.tape == &t && c.rows == t.rows.size() && c.last == last) return;
    if (c.tape == &t && now - c.time < 250) return;
    c.tape = &t;
    c.rows = t.rows.size();
    c.last = last;
    c.time = now;
    c.recs = t.records();
    rmp_labels(c);
    c.items.clear();
    for (const MagTape::Record& r : c.recs) {
        int64_t end = r.pos;
        RmpMapItem it = { 1, 1, r.bad ? 1 : 0, r.pos, end, "", r.data.size() };
        if (r.mark) it.kind = 2;
        else if (r.data.size() == 80) {   // a JSEP label: VOL1, HDR1, HDR2, EOF1, EOF2, EOV1...
            char l[5];
            for (int i = 0; i < 4; i++) l[i] = ebcdic(r.data[i]);
            l[4] = 0;
            if ((!strncmp(l, "VOL", 3) || !strncmp(l, "HDR", 3) || !strncmp(l, "EOF", 3) || !strncmp(l, "EOV", 3)) &&
                l[3] >= '1' && l[3] <= '9') {
                it.kind = 0;
                memcpy(it.label, l, 5);
            }
        }
        if (it.kind == 1 && !c.items.empty() && c.items.back().kind == 1) {
            c.items.back().count++;
            c.items.back().bad += it.bad;
            c.items.back().end = r.pos;
            continue;
        }
        c.items.push_back(it);
    }
}

// One unit: the drive drawn with its reels, lamps and buttons, the tape and what is on it.
static void draw_rmp_unit(UiState& ui, Rmp1* r, int i)
{
    Rmp1::Unit& u = r->units[i];
    MagTape& t = u.tape;
    ImGui::PushID(i);
    float s = ImGui::GetFontSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    float w = 20.0f * s, h = 16.9f * s;
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), IM_COL32(222, 220, 208, 255), 0.4f * s);
    dl->AddRect(o, ImVec2(o.x + w, o.y + h), IM_COL32(120, 118, 110, 255), 0.4f * s, 0, 1.5f);
    dl->AddText(ImGui::GetFont(), 1.15f * s, ImVec2(o.x + 0.8f * s, o.y + 0.5f * s), IM_COL32(40, 40, 40, 255), "IZOT SM 5300.01");
    char unit_s[32];
    snprintf(unit_s, sizeof(unit_s), "%s %d", tr("unit"), i);
    dl->AddText(ImVec2(o.x + w - 5.0f * s, o.y + 0.6f * s), IM_COL32(90, 90, 90, 255), unit_s);
    // the tape deck: reels of 216 mm with a hub of 100 mm, 350 m of tape from the left reel to the right one
    ImVec2 p0(o.x + 0.8f * s, o.y + 2.0f * s), p1(o.x + w - 0.8f * s, o.y + 11.4f * s);
    dl->AddRectFilled(p0, p1, IM_COL32(44, 46, 50, 255), 0.3f * s);
    const double hub = 0.050, full = 0.105, len = (double)MagTape::END_POS / MagTape::UNIT * 0.0254;
    double pm = t.loaded ? (double)(u.pos > 0 ? u.pos : 0) / MagTape::UNIT * 0.0254 : 0;
    float sc = 3.4f * s / (float)full;
    ImVec2 cl(o.x + 5.0f * s, o.y + 5.8f * s), cr(o.x + w - 5.0f * s, o.y + 5.8f * s);
    for (int side = 0; side < 2; side++) {
        ImVec2 c = side ? cr : cl;
        dl->AddCircleFilled(c, (float)full * sc, IM_COL32(150, 152, 158, 255), 48);   // the flange
        dl->AddCircle(c, (float)full * sc, IM_COL32(190, 192, 198, 255), 48, 1.5f);
    }
    if (t.loaded) {
        double on_left = len - pm;   // the supply reel (left) holds what is still before the head
        double rl = sqrt(hub * hub + (full * full - hub * hub) * on_left / len);
        double rr = u.loaded ? sqrt(hub * hub + (full * full - hub * hub) * pm / len) : hub;
        ImU32 tape = IM_COL32(86, 56, 38, 255);
        dl->AddCircleFilled(cl, (float)rl * sc, tape, 48);
        if (u.loaded) dl->AddCircleFilled(cr, (float)rr * sc, tape, 48);
        // the path past the tension arms and the heads at the bottom
        float hy = o.y + 10.6f * s;
        ImVec2 al(o.x + 3.0f * s, o.y + 9.9f * s), ar(o.x + w - 3.0f * s, o.y + 9.9f * s);
        if (u.loaded) {
            dl->AddLine(ImVec2(cl.x - (float)rl * sc * 0.3f, cl.y + (float)rl * sc * 0.95f), al, tape, 2.5f);
            dl->AddLine(al, ImVec2(o.x + w * 0.5f - 1.4f * s, hy), tape, 2.5f);
            dl->AddLine(ImVec2(o.x + w * 0.5f + 1.4f * s, hy), ar, tape, 2.5f);
            dl->AddLine(ar, ImVec2(cr.x + (float)rr * sc * 0.3f, cr.y + (float)rr * sc * 0.95f), tape, 2.5f);
            dl->AddLine(ImVec2(o.x + w * 0.5f - 1.4f * s, hy), ImVec2(o.x + w * 0.5f + 1.4f * s, hy), tape, 2.5f);
        }
        dl->AddCircleFilled(al, 0.35f * s, IM_COL32(170, 172, 178, 255), 16);
        dl->AddCircleFilled(ar, 0.35f * s, IM_COL32(170, 172, 178, 255), 16);
        ImU32 head = u.writing ? IM_COL32(220, 50, 40, 255) : IM_COL32(150, 152, 160, 255);
        dl->AddRectFilled(ImVec2(o.x + w * 0.5f - 0.9f * s, hy - 0.2f * s), ImVec2(o.x + w * 0.5f + 0.9f * s, hy + 0.7f * s),
                          head, 0.15f * s);
        // hubs turning: the angle follows the tape wound on (the right reel turns only when threaded)
        for (int side = 0; side < 2; side++) {
            ImVec2 c = side ? cr : cl;
            double rad = side ? rr : rl;
            float a = (float)((side ? pm : -on_left) / rad);
            if (side && !u.loaded) a = 0;
            float hr = (float)hub * sc * 0.75f;
            dl->AddCircleFilled(c, hr, IM_COL32(200, 202, 208, 255), 32);
            for (int k = 0; k < 3; k++) {
                float an = a + k * 2.0943951f;
                dl->AddLine(c, ImVec2(c.x + cosf(an) * hr * 0.9f, c.y + sinf(an) * hr * 0.9f), IM_COL32(70, 70, 76, 255), 3.0f);
            }
            dl->AddCircleFilled(c, hr * 0.25f, IM_COL32(70, 70, 76, 255), 16);
        }
    } else {
        for (int side = 0; side < 2; side++) dl->AddCircleFilled(side ? cr : cl, (float)hub * sc * 0.75f, IM_COL32(200, 202, 208, 255), 32);
        dl->AddText(ImVec2(o.x + w * 0.5f - 3.0f * s, o.y + 10.2f * s), IM_COL32(170, 170, 170, 255), tr("(no tape)"));
    }
    // the lamps: what the controller reads in port B of D2B for this unit
    struct Lamp { const char* name; bool on; ImU32 col; };
    bool sel = r->selected() == i;
    const Lamp lamps[7] = {
        { "PON", r->power_on_reset() && sel, IM_COL32(240, 180, 40, 255) },
        { "ONL", u.online, IM_COL32(70, 220, 90, 255) },
        { "RDY", t.loaded && u.loaded && !u.rewinding, IM_COL32(70, 220, 90, 255) },
        { "BOT", r->bot(u), IM_COL32(240, 180, 40, 255) },
        { "EOT", r->eot(u), IM_COL32(240, 80, 60, 255) },
        { "RWS", u.rewinding, IM_COL32(240, 180, 40, 255) },
        { "WPT", t.loaded && !u.ring, IM_COL32(240, 80, 60, 255) },
    };
    float lx = o.x + 1.6f * s, ly = o.y + 12.3f * s, step = (w - 3.2f * s) / 6;
    for (int k = 0; k < 7; k++) {
        ImVec2 c(lx + k * step, ly);
        dl->AddCircleFilled(c, 0.55f * s, lamps[k].on ? lamps[k].col : IM_COL32(88, 88, 86, 255), 20);
        dl->AddCircle(c, 0.55f * s, IM_COL32(60, 60, 58, 255), 20, 1.0f);
        ImVec2 ts = ImGui::CalcTextSize(lamps[k].name);
        dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y + 0.8f * s), IM_COL32(50, 50, 50, 255), lamps[k].name);
    }
    // the buttons of the drive, on its panel
    ImGui::SetCursorScreenPos(ImVec2(o.x + 0.8f * s, o.y + 14.4f * s));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(236, 234, 226, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(250, 248, 240, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(200, 198, 190, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(30, 30, 30, 255));
    float bw = (w - 1.6f * s - 3 * ImGui::GetStyle().ItemSpacing.x) / 4;
    ImGui::BeginDisabled(!t.loaded || u.loaded);
    if (ImGui::Button("LOAD", ImVec2(bw, 1.8f * s))) r->press_load(i);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tr("Thread the tape and wind it to the BOT marker (RDY)."));
    ImGui::SameLine();
    ImGui::BeginDisabled(!u.loaded);
    if (ImGui::Button(u.online ? "OFF-LINE" : "ON-LINE", ImVec2(bw, 1.8f * s))) r->press_online(i);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tr("ON-LINE: the computer controls the drive (MTDAT: \"DISTANC\")."));
    ImGui::SameLine();
    ImGui::BeginDisabled(!u.loaded || u.online || u.pos <= 0);
    if (ImGui::Button("REWIND", ImVec2(bw, 1.8f * s))) r->press_rewind(i);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!u.loaded || u.online);
    if (ImGui::Button("UNLOAD", ImVec2(bw, 1.8f * s))) r->press_unload(i);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tr("Rewind and unthread (OFF-LINE only)."));
    ImGui::PopStyleColor(4);
    ImGui::SetCursorScreenPos(o);
    ImGui::Dummy(ImVec2(w, h));

    // the right side: the reel, the position, the map of the tape and the block at the head
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (t.loaded) {
        ImGui::Text("%s %s", tr("Tape:"), t.path.c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t.path.c_str());
    } else ImGui::TextDisabled("%s", tr("(no tape)"));
    if (ImGui::Button(tr("Insert..."))) tape_dialog(ui, 14, r->name, i);
    ImGui::SameLine();
    if (ImGui::Button(tr("New tape..."))) tape_dialog(ui, 15, r->name, i);
    ImGui::SameLine();
    ImGui::BeginDisabled(!t.loaded || u.online);
    if (ImGui::Button(tr("Eject"))) media_set(ui, r, i, "");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && u.online)
        ImGui::SetTooltip("%s", tr("Switch the drive OFF-LINE first."));
    ImGui::BeginDisabled(u.online);
    media_more(ui, r, i, t.loaded);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(u.loaded);
    ImGui::Checkbox(tr("Write ring"), &u.ring);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tr("The ring in the reel allows writing, without it WPT. Changed only with the tape unthreaded."));
    // below the buttons a fixed height, loaded or not, the block at the head shown or not: the unit below
    // stays where it is while the tape moves
    float L = ImGui::GetTextLineHeightWithSpacing(), sp = ImGui::GetStyle().ItemSpacing.y;
    float files_h = 3 * L, blk_h = 5 * L, map_h = 2.6f * s + ImGui::GetStyle().ScrollbarSize + 0.4f * s;
    float body_h = 3 * L + 0.5f * s + sp + files_h + sp + map_h + sp + blk_h + 2 * sp;
    ImGui::BeginChild("body", ImVec2(0, body_h), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    if (t.loaded) {
        const char* mot = u.rewinding ? tr("rewinding") : u.dir > 0 ? (u.writing ? tr("forward, writing") : tr("forward"))
                        : u.dir < 0 ? tr("reverse") : tr("stop");
        double pos_m = (double)u.pos / MagTape::UNIT * 0.0254;
        ImGui::Text("%s, %s %.1f m %s %.0f m%s", mot, tr("position"), pos_m, tr("of"), len, t.dirty ? tr(", modified") : "");
        ImGui::ProgressBar((float)(pos_m / len), ImVec2(-1, 0.5f * s), "");

        RmpMapCache& c = s_rmp_map[i];
        rmp_map_update(c, t);
        int data = 0, marks = 0;
        for (const MagTape::Record& rec : c.recs) (rec.mark ? marks : data)++;
        ImGui::TextDisabled("%s: %d %s, %d %s", tr("On the tape"), data, tr("blocks"), marks, tr("tape marks"));
        // what the commands of MTDAT, DIRMT and MTCOPY need: the name of the tape, the files with their sizes
        ImGui::BeginChild("files", ImVec2(-1, files_h), ImGuiChildFlags_None);   // more files: it scrolls
        if (c.code) {
            ImGui::Text("%s %s   %s %c", tr("Tape name (VOL1):"), c.volume.c_str(), tr("code"), c.code);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tr("The name the programs ask for (MTDAT: Jmeno pasky, MTCOPY: JP)."));
            for (const std::string& f : c.files) ImGui::TextDisabled("  %s", f.c_str());
        } else if (!t.rows.empty()) {
            ImGui::TextDisabled("%s", tr("No VOL1 label (not initialized by MTDAT or MTCOPY)."));
        }
        ImGui::EndChild();
        // the map: the groups in order, not to scale (the gaps would hide everything)
        float mh = 2.6f * s;
        ImGui::BeginChild("map", ImVec2(-1, map_h), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar);
        ImDrawList* md = ImGui::GetWindowDrawList();
        ImVec2 m0 = ImGui::GetCursorScreenPos();
        float x = m0.x + 0.2f * s, y0 = m0.y + 0.5f * s, y1 = m0.y + mh - 0.3f * s;
        md->AddRectFilled(ImVec2(x, y0 - 0.3f * s), ImVec2(x + 0.3f * s, y1 + 0.3f * s), IM_COL32(230, 230, 230, 255));   // BOT foil
        x += 0.8f * s;
        float head_x = -1;
        for (size_t k = 0; k < c.items.size(); k++) {
            const RmpMapItem& it = c.items[k];
            char txt[32];
            float iw;
            ImU32 col;
            if (it.kind == 2) {
                txt[0] = 0;
                iw = 0.45f * s;
                col = IM_COL32(220, 70, 60, 255);
            } else if (it.kind == 0) {
                snprintf(txt, sizeof(txt), "%s", it.label);
                iw = 3.0f * s;
                col = IM_COL32(120, 80, 170, 255);
            } else {
                if (it.count == 1) snprintf(txt, sizeof(txt), "%u B", (unsigned)it.bytes);
                else snprintf(txt, sizeof(txt), "%d x", it.count);
                iw = ImGui::CalcTextSize(txt).x + 1.0f * s;
                col = it.bad ? IM_COL32(220, 140, 40, 255) : IM_COL32(50, 110, 200, 255);
            }
            if (head_x < 0 && u.pos < it.pos) head_x = x - 0.2f * s;
            else if (head_x < 0 && u.pos <= it.end + MagTape::ROW * 12) head_x = x + iw * 0.5f;
            md->AddRectFilled(ImVec2(x, y0), ImVec2(x + iw, y1), col, 0.15f * s);
            if (txt[0]) md->AddText(ImVec2(x + 0.5f * s, (y0 + y1) * 0.5f - 0.5f * s), IM_COL32(255, 255, 255, 255), txt);
            if (ImGui::IsMouseHoveringRect(ImVec2(x, y0), ImVec2(x + iw, y1))) {
                double at = (double)it.pos / MagTape::UNIT * 0.0254;
                if (it.kind == 2) ImGui::SetTooltip("%s, %.2f m", tr("tape mark"), at);
                else if (it.kind == 0) ImGui::SetTooltip("%s %s, %.2f m", tr("label"), it.label, at);
                else ImGui::SetTooltip("%d %s (%s %u B), %.2f m%s", it.count, tr("blocks"), tr("the first"),
                                       (unsigned)it.bytes, at, it.bad ? tr(", with errors") : "");
            }
            x += iw + 0.25f * s;
        }
        if (head_x < 0) head_x = x;
        if (u.dir != 0 || u.rewinding) {   // moving: the head stays in view
            float vw = ImGui::GetWindowWidth(), at = head_x - m0.x;   // m0 is the content origin (scrolled)
            if (at < ImGui::GetScrollX() + 1.0f * s || at > ImGui::GetScrollX() + vw - 1.0f * s)
                ImGui::SetScrollX(at - vw * 0.5f);
        }
        md->AddTriangleFilled(ImVec2(head_x - 0.4f * s, m0.y), ImVec2(head_x + 0.4f * s, m0.y), ImVec2(head_x, m0.y + 0.45f * s),
                              IM_COL32(250, 200, 40, 255));
        ImGui::Dummy(ImVec2(x - m0.x + 0.5f * s, mh));
        ImGui::EndChild();

        // the block last passed under the read head (as the controller reads it), in its own fixed lines
        ImGui::BeginChild("blk", ImVec2(-1, blk_h), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        const MagTape::Record* last = nullptr;
        int idx = 0;
        for (size_t k = 0; k < c.recs.size(); k++)
            if (c.recs[k].pos <= u.pos) {
                last = &c.recs[k];
                idx = (int)k + 1;
            }
        if (last && !last->mark) {
            ImGui::TextDisabled("%s %d, %u B%s:", tr("Block at the head"), idx, (unsigned)last->data.size(),
                                last->bad ? tr(", with errors") : tr(", parity, CRC and LRC good"));
            // the first 48 bytes in hex, then the first 160 as EBCDIC text
            for (size_t off = 0; off < last->data.size() && off < 48; off += 16) {
                char line[80];
                int n = snprintf(line, sizeof(line), "%04X ", (unsigned)off);
                for (size_t k = 0; k < 16 && off + k < last->data.size(); k++)
                    n += snprintf(line + n, sizeof(line) - n, " %02X", last->data[off + k]);
                ImGui::TextUnformatted(line);
            }
            char txt[81];   // one line of text, what does not fit is cut off
            size_t k;
            for (k = 0; k < 80 && k < last->data.size(); k++) txt[k] = ebcdic(last->data[k]);
            txt[k] = 0;
            ImGui::TextUnformatted(txt);
        } else if (last) ImGui::TextDisabled("%s %d: %s", tr("Block at the head"), idx, tr("tape mark"));
        ImGui::EndChild();
    }
    ImGui::EndChild();
    ImGui::EndGroup();
    ImGui::PopID();
}

static void draw_rmp(UiState& ui, Rmp1* r)
{
    ImGui::PushID(r);
    for (int i = 0; i < Rmp1::UNITS; i++)
        if (r->units[i].fitted) draw_rmp_unit(ui, r, i);
    ImGui::PopID();
}

void draw_media(UiState& ui)
{
    tape_dialog_result(ui, ui.host->machine());   // also the HEX and binary dialogs of the File menu
    bin_load_question(ui, ui.host->machine());
    hex_load_question(ui, ui.host->machine());
    tape_program_question(ui, ui.host->machine());
    media_restore_dialog(ui);
    if (!ui.show_media) return;
    Machine* m = ui.host->machine();
    first_use_right(760, 440, 1);
    ImGui::SetNextWindowSize(ImVec2(760, 440), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(tr_id("Media"), &ui.show_media) || !m) {
        ImGui::End();
        return;
    }
    bool any = false;
    for (auto& c : m->cards) {
        if (Dsm1* dsm = dynamic_cast<Dsm1*>(c.get())) {
            any = true;
            char title[96];
            snprintf(title, sizeof(title), "%s #%02X %s", c->type.c_str(), dsm->base_port(), tr("cassette recorder"));
            ImGui::SeparatorText(title);
            draw_tape(ui, dsm);
            continue;
        }
        if (c->floppy_drive(0)) {
            any = true;
            char title[96];
            snprintf(title, sizeof(title), "%s %s", c->name.c_str(), tr("floppy drives"));
            ImGui::SeparatorText(title);
            draw_floppies(ui, c.get());
            continue;
        }
        if (Rkd1* rk = dynamic_cast<Rkd1*>(c.get())) {
            any = true;
            char title[96];
            snprintf(title, sizeof(title), "%s: %s", c->name.c_str(), tr("cartridge disk"));
            ImGui::SeparatorText(title);
            draw_rkd(ui, rk);
            continue;
        }
        if (Rmp1* mt = dynamic_cast<Rmp1*>(c.get())) {
            any = true;
            char title[128];
            snprintf(title, sizeof(title), "%s: %s", c->name.c_str(), tr("magnetic tape IZOT SM 5300 (9 tracks, 800 bpi NRZI, 12.5 ips)"));
            ImGui::SeparatorText(title);
            draw_rmp(ui, mt);
            continue;
        }
        if (Kzd1* k = c->kzd1()) {
            any = true;
            char title[96];
            snprintf(title, sizeof(title), "%s: %s", c->name.c_str(), tr("KZD-1 cassette data recorder"));
            ImGui::SeparatorText(title);
            draw_kzd(ui, c.get(), k);
            if (!c->pmd32sd()) continue;
        }
        Ide1* ide = dynamic_cast<Ide1*>(c.get());
        if (!ide && !c->pmd32sd()) continue;
        any = true;
        ImGui::SeparatorText(c->name.c_str());
        if (IdeDisk* d = ide ? ide->disk() : nullptr) {
            if (d->present())
                ImGui::Text("%s %s (%u MB%s)", tr("IDE disk:"), d->path().c_str(), d->sector_count() / 2048,
                            d->is_readonly() ? tr(", read only") : "");
            else
                ImGui::TextUnformatted(tr("IDE disk: none"));
        }
        if (Pmd32Sd* p = c->pmd32sd()) {
            ImGui::TextUnformatted("PMD32-SD");
            draw_pmd32(p);
        }
    }
    if (!any) ImGui::TextUnformatted(tr("This machine has no removable media."));
    ImGui::End();
}
