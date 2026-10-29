// Media window: cassette recorders of DSM-1 cards, images of the PMD32-SD drives,
// the IDE disk of IDE-1 cards, cartridge disks of RKD-1 and the magnetic tape drives of RMP-1.
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/theme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
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
#include "core/version.h"

// The name of a medium (ui.h): the folders of a working copy make the path long, the tooltip has it whole.
void file_name_text(const std::string& path, bool disabled)
{
    std::string name = path;
    while (name.size() > 1 && (name.back() == '/' || name.back() == '\\')) name.pop_back();   // a folder
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (disabled) ImGui::TextDisabled("%s", name.c_str());
    else ImGui::TextUnformatted(name.c_str());
    if (!ImGui::IsItemHovered()) return;
    std::string full = data_path(path);   // a relative one from the data root ("work/..." from the work root)
#ifdef _WIN32
    for (char& c : full) if (c == '/') c = '\\';
#endif
    tip(full.c_str());
}

// ---------------------------------------------------------------- cassette recorder

// The names are tr() keys: the dialog gets them in the UI language (dialog_filters).
static const SDL_DialogFileFilter k_tape_filter[] = { { "Cassette (WAV, MP3) or program (HEX, TXT, BIN)", "wav;mp3;hex;ihx;txt;bin" },
                                                      { "Cassette (WAV, MP3)", "wav;mp3" },
                                                      { "Program (Intel HEX, TXT, BIN)", "hex;ihx;txt;bin" } };
static const SDL_DialogFileFilter k_wav_filter[] = { { "WAV", "wav" } };
static const SDL_DialogFileFilter k_hex_filter[] = { { "Intel HEX (HEX, IHX, TXT)", "hex;ihx;txt" }, { "*", "*" } };
static const SDL_DialogFileFilter k_img_filter[] = { { "Floppy disk (IMG, IMD, 8SD, SP2, SP1)", "img;8sd;imd;sp2;sp1" },
                                                     { "*", "*" } };
static const SDL_DialogFileFilter k_state_filter[] = { { "SAPIemu state", "sapistate" } };
static const SDL_DialogFileFilter k_kzd_filter[] = { { "KZD-1 cassette (KZD)", "kzd" }, { "*", "*" } };
static const SDL_DialogFileFilter k_kzd_open_filter[] = { { "KZD-1 cassette (KZD)", "kzd" },
                                                          { "Program (HEX, BIN)", "hex;ihx;bin" }, { "*", "*" } };

static const SDL_DialogFileFilter k_rkd_filter[] = { { "RKD-1 cartridge (RKD)", "rkd" }, { "*", "*" } };
static const SDL_DialogFileFilter k_tap_filter[] = { { "Magnetic tape (SIMH TAP)", "tap" }, { "*", "*" } };

// The filters with their names in the UI language. SDL reads them until the dialog closes: the names are strings of
// the translation table (they stay), the array is static (one dialog of this window at a time, tape_dialog).
static const SDL_DialogFileFilter* dialog_filters(const SDL_DialogFileFilter* f, int n)
{
    static SDL_DialogFileFilter s_tr[4];
    for (int i = 0; i < n && i < 4; i++) s_tr[i] = { tr(f[i].name), f[i].pattern };
    return s_tr;
}
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
    // the first time: a file to open in media/ (the programs, tapes and disks that come with the emulator; a disk from
    // there is used through its working copy), a new file in work/
    bool opens = kind == 1 || kind == 3 || kind == 4 || kind == 6 || kind == 10 || kind == 12 || kind == 14;
    std::string where = !d.last_dir.empty() ? d.last_dir : data_path(opens && dir_exists(data_path("media")) ? "media" : "work");
    if (kind == 7 || kind == 8) {   // states: in work/state unless chosen elsewhere before
        where = data_path("work/state");
        make_dirs(where);
    }
    where = dialog_location(where);
    // the filters of each kind (the others, 2 = a new cassette: a WAV to save)
    static const struct { int kind; const SDL_DialogFileFilter* f; int n; bool open; } k_kinds[] = {
        { 1, k_tape_filter, 3, true },      { 3, k_hex_filter, 2, true },   { 6, nullptr, 0, true },
        { 7, k_state_filter, 1, false },    { 8, k_state_filter, 1, true }, { 4, k_img_filter, 2, true },
        { 5, k_img_filter, 1, false },      { 9, k_wav_filter, 1, false },  { 10, k_kzd_open_filter, 3, true },
        { 11, k_kzd_filter, 1, false },     { 12, k_rkd_filter, 2, true },  { 13, k_rkd_filter, 1, false },
        { 14, k_tap_filter, 2, true },      { 15, k_tap_filter, 1, false },
    };
    const SDL_DialogFileFilter* f = k_wav_filter;
    int n = 1;
    bool open = false;
    for (const auto& k : k_kinds)
        if (k.kind == kind) {
            f = k.f;
            n = k.n;
            open = k.open;
        }
    const SDL_DialogFileFilter* shown = f ? dialog_filters(f, n) : nullptr;
    if (open) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, shown, n, where.c_str(), false);
    else SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, shown, n, where.c_str());
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
    else if (!ihex_parse(text, hex_scan_byte, &s, err)) ui.hex_error = tr_core(err);
    else if (!s.count) ui.hex_error = tr("The file holds no data.");
    else if (s.hi > 0xFFFF) ui.hex_error = tr("The file does not fit into memory there.");
    ui.hex_bytes = s.count;
    ui.hex_lo = s.lo;
    ui.hex_hi = s.hi;
}

// Why a state did not load, for the user: a state of a version of SAPIemu that this one does not read is named as such;
// the core's reason (English, for MCP) then comes last as a detail. `back`: how the machine that ran is now
// (StateBack of EmuHost::load_state).
std::string state_load_error(UiState& ui, const std::string& path, const std::string& err, int back)
{
    std::string s = std::string(tr("Cannot load the state")) + ":\n";
    std::vector<uint8_t> d;
    StateOrigin o;
    char buf[512];
    bool known = file_read_all(path, d) && Machine::state_origin(d, o);
    bool detail = false;   // the core's reason after the machine line
    if (known && o.format > STATE_FORMAT) {
        snprintf(buf, sizeof(buf), tr("It was saved by a newer version of SAPIemu (%s). This version (%s) cannot read it."),
                 o.app.empty() ? "?" : o.app.c_str(), sapiemu_version());
        s += buf;
    } else if (known && o.format < STATE_FORMAT_MIN) {
        snprintf(buf, sizeof(buf), tr("It was saved by an old version of SAPIemu. This version (%s) no longer reads it."),
                 sapiemu_version());
        s += buf;
    } else if (known && o.app != sapiemu_version()) {   // read, but a card did not fit: another version saved it
        snprintf(buf, sizeof(buf), tr("The state was saved by another version of SAPIemu (%s), this is version %s."),
                 o.app.empty() ? tr("an older alpha") : o.app.c_str(), sapiemu_version());
        s += buf;
        detail = true;
    } else {
        s += tr_core(err);
    }
    s += "\n\n";
    if (back == STATE_BACK_RESTARTED)
        s += tr("The running machine could not be kept as it was: it was switched off and on, its memory is lost.");
    else if (back == STATE_BACK_RESET)
        s += tr("The running machine did not come back as it was (the Log says why): it was reset.");
    else
        s += tr(ui.host->machine() ? "The machine runs on as it was." : "Nothing was changed.");
    if (detail) s += std::string("\n\n") + tr("Details:") + " " + tr_core(err);
    return s;
}

static void state_file(UiState& ui, std::string path, bool save)
{
    std::string err;
    if (save) {
        size_t slash = path.find_last_of("/\\");
        if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".sapistate";
        if (ui.host->save_state_file(path, err)) ui_notice(ui, true, std::string(tr("State saved")) + ": " + path);
        else ui_notice(ui, false, std::string(tr("Cannot save the state")) + ":\n" + tr_core(err));
    } else {
        int back = STATE_BACK_SAME;
        if (ui.host->load_state_file(path, err, &back)) ui_notice(ui, true, std::string(tr("State loaded")) + ": " + path);
        else ui_notice(ui, false, state_load_error(ui, path, err, back));
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
    if (!m->set_live_param(c, p, nlohmann::json(image), err))   // the Error window and the Log (a bad image: the old
        ui_notice(ui, false, c->name + ": " +                    // disk stays in, Fdc8272Card::floppy_insert)
                                 tr(image.empty() ? "Cannot take the medium out" : "Cannot put the medium in") + ":\n" +
                                 tr_core(err));
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

// The medium of unit `u` is a file of media/ used through a working copy (the description only, no look at the disk:
// asked every frame by the "..." button).
static bool media_copied(Machine* m, Card* c, int u, std::string& img)
{
    img = media_image(m, c, u);
    for (char& ch : img) if (ch == '\\') ch = '/';
    nlohmann::json* cc = m ? m->card_config(c) : nullptr;
    bool copies = !cc || cc->value("working_copy", true);
    return copies && img.rfind("media/", 0) == 0;
}

static bool media_original(Machine* m, Card* c, int u, std::string& orig, std::string& work)
{
    std::string img;
    if (!media_copied(m, c, u, img)) return false;
    orig = data_path(img);
    work = data_path("work/" + img.substr(6));
    return file_exists(orig);
}

static std::string s_restore_card;   // Restore from the original: asked in a modal window
static int s_restore_unit = -1;
static bool s_restore_open = false;

// The "..." button after Eject: Restore from the original (the working copy in work/ gets the file of media/
// again). No "Eject without saving": the media are saved while running (a few seconds after the last write), it
// would throw away only the last seconds (decided 2026-10-29). Disabled when the medium is not from media/ (an empty
// drive, an own file): its only item could not be used.
static void media_more(UiState& ui, Card* c, int u)
{
    Machine* m = ui.host->machine();
    std::string img;
    bool copied = media_copied(m, c, u, img);
    ImGui::SameLine();
    ImGui::BeginDisabled(!copied);
    if (ImGui::Button("...##more")) ImGui::OpenPopup("more");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (copied) tip(tr("Restore from the original"));
        else tipf("%s\n%s", tr("Restore from the original"),
                  tr("Only for media of media/ used through a working copy."));
    }
    if (!ImGui::BeginPopup("more")) return;
    std::string orig, work;
    bool can = media_original(m, c, u, orig, work);
    if (ImGui::MenuItem(tr("Restore from the original..."), nullptr, false, can)) {
        s_restore_card = c->name;
        s_restore_unit = u;
        s_restore_open = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        tip(can ? tr("The working copy in work/ is replaced by the original in media/.")
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
        ui_notice(ui, ok, ok ? std::string(tr("Restored from the original")) + ": " + work
                             : std::string(tr("Cannot write")) + " " + work);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    // Esc = Cancel; Enter does nothing: Restore throws away the changes of the working copy. The keyboard navigation
    // (Tab) starts at Cancel too.
    bool cancel = ImGui::Button(tr("Cancel"));
    ImGui::SetItemDefaultFocus();
    if (cancel || popup_escape()) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// A file chosen for `kind` (TapeDialog::kind) of the unit `drive` of the card `card`: the answer of a file dialog of this
// window or of the File menu (tape_dialog_result), a file dropped on the main window (file_drop). Empty = cancelled.
static void file_chosen(UiState& ui, Machine* m, int kind, std::string path, const std::string& card, int drive)
{
    TapeDialog& d = ui.tape_dlg;
    if (kind == 7 || kind == 8) {   // save state (needs no machine for loading)
        if (path.empty()) return;
        state_file(ui, path, kind == 7);
        return;
    }
    if (path.empty() || !m) return;
    d.last_dir = path_dir(path);
    if (kind != 9) ui.focus_display = 2;   // a medium put in, a program chosen: the keyboard back to the display
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
                ui_notice(ui, false, std::string(tr("Cannot create the cassette")) + ":\n" + tr_core(err));
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
                ui_notice(ui, false, std::string(tr("Cannot create the disk")) + ":\n" + tr_core(err));
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
        if (t->export_wav(path, err)) ui_notice(ui, true, std::string(tr("Tape saved as WAV")) + ": " + path);
        else ui_notice(ui, false, tr_core(err));
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
        if (!t->create(path, err)) ui_notice(ui, false, std::string(tr("Cannot create the cassette")) + ":\n" + tr_core(err));
        else media_note(ui, dsm, 0, data_relative(path));
    }
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
    file_chosen(ui, m, kind, path, card, drive);
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
        file_name_text(t->path);
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
    media_more(ui, dsm, 0);
    ImGui::BeginDisabled(!t->loaded());
    ImGui::SameLine();
    if (ImGui::Button(tr("Save as WAV..."))) tape_dialog(ui, 9, dsm->name);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        tip(tr("A copy of the tape as a WAV file, e.g. to play it into a real SAPI."));
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!t->loaded());
    // the lit button also carries the symbol of its mode (play, record, stop): not the colour alone (WCAG 1.4.1)
    struct Btn { const char* label; int mode; const char* symbol; };
    const Btn btns[3] = { { "PLAY", TapeDeck::PLAY, "\xE2\x96\xB6 " }, { "REC", TapeDeck::RECORD, "\xE2\x97\x8F " },
                          { "STOP", TapeDeck::STOP, "\xE2\x96\xA0 " } };
    if (ImGui::Button(tr("<< Rewind"))) {
        t->mode = TapeDeck::STOP;
        t->pos = 0;
    }
    for (const Btn& b : btns) {
        ImGui::SameLine();
        bool on = t->mode == b.mode;
        bool dis = b.mode == TapeDeck::RECORD && t->write_protect;
        ImGui::BeginDisabled(dis);
        if (on) {   // a lit button of the deck, the same in every look
            ImGui::PushStyleColor(ImGuiCol_Button, b.mode == TapeDeck::RECORD ? IM_COL32(170, 40, 40, 255) : IM_COL32(30, 110, 50, 255));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        }
        char lbl[64];   // the id stays when the symbol comes and goes
        snprintf(lbl, sizeof(lbl), "%s%s###%s", on ? b.symbol : "", tr(b.label), b.label);
        if (ImGui::Button(lbl)) t->mode = (TapeDeck::Mode)b.mode;
        if (on) ImGui::PopStyleColor(2);
        ImGui::EndDisabled();
        if (dis && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            tip(tr("The cassette is write protected"));
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
    ImGui::ProgressBar(len > 0 ? (float)(p / len) : 0.0f, ImVec2(16.0f * ImGui::GetFontSize(), 0), over);
    ImGui::SameLine();
    if (dsm->motor_on()) ImGui::TextColored(theme().ok, "%s", tr("motor running"));
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
        panel_text(dl, ImVec2(o.x + 1.1f * u, o.y + 0.8f * u), IM_COL32(30, 30, 30, 255), name.c_str());
        char sd[4] = { (char)(k->side ? 'B' : 'A'), 0 };
        dl->AddText(panel_font(), 1.4f * u, ImVec2(o.x + w - 2.0f * u, o.y + 0.65f * u), IM_COL32(170, 30, 30, 255), sd);
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
        panel_text(dl, ImVec2(o.x + 1.0f * u, o.y + 4.0f * u), IM_COL32(150, 150, 150, 255), tr("(no cassette)"));
    }
    ImGui::Dummy(ImVec2(w, h));
    if (picture_only())   // the cassette with a margin (a preview of the machine)
        picture_mark(o.x - 0.3f * u, o.y - 0.3f * u, o.x + w + 0.3f * u, o.y + h + 0.3f * u);
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (has) {
        ImGui::Text("%s %c   %.2f / %.0f m", tr("Side"), k->side ? 'B' : 'A', k->position_m(), KzdCassette::LENGTH_UM * 1e-6);
        ImGui::ProgressBar((float)(k->position_m() / (KzdCassette::LENGTH_UM * 1e-6)), ImVec2(12.0f * u, 0), "");
    }
    double sp = k->speed();
    if (sp != 0 || k->target() != 0)
        ImGui::TextColored(k->recording() ? theme().error : theme().ok, "%s  %.2f m/s", tr(k->motion()), sp);
    else
        ImGui::TextDisabled("%s", tr("stop"));
    if (k->overload(1) || k->overload(2))
        ImGui::TextColored(theme().warn, "%s %d", tr("End of tape: overload of motor"), k->overload(1) ? 1 : 2);
    if (has) {
        bool ta = k->tab(0), tb = k->tab(1);
        const char* hint = tr("The break-out tab of the side: present = recording allowed (MIKOS/K checks it)");
        if (ImGui::Checkbox(tr("Tab A"), &ta)) k->set_tab(0, ta);
        if (ImGui::IsItemHovered()) tip(hint);
        ImGui::SameLine();
        if (ImGui::Checkbox(tr("Tab B"), &tb)) k->set_tab(1, tb);
        if (ImGui::IsItemHovered()) tip(hint);
    }
    ImGui::EndGroup();
    if (ImGui::Button(tr("Insert..."))) tape_dialog(ui, 10, card->name);
    ImGui::SameLine();
    if (ImGui::Button(tr("New cassette..."))) tape_dialog(ui, 11, card->name);
    ImGui::SameLine();
    ImGui::BeginDisabled(!has);
    if (ImGui::Button(tr("Eject"))) media_set(ui, card, 0, "");
    ImGui::EndDisabled();
    media_more(ui, card, 0);
    ImGui::BeginDisabled(!has);
    ImGui::SameLine();
    if (ImGui::Button(tr("Turn over"))) k->flip();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        tip(tr("The other side of the cassette faces the head (the position is mirrored)."));
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
        tip(tr("Unit of the controller. The CP/M letter is from the machine description, \"probably\" "
               "= seen while running (the drive of the last BIOS SELDSK at a sector read)."));
}

static void draw_pmd32(Pmd32Sd* p)
{
    std::vector<std::string> images = p->list_images();
    ImGui::TextDisabled("%s", tr("SD card:"));
    ImGui::SameLine();
    file_name_text(p->root(), true);
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
        ImGui::SetNextItemWidth(16.0f * ImGui::GetFontSize());
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
        ui_notice(ui, false, std::string(tr("The quick slot of this machine is empty")) + ":\n" + path);
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
    file_name_text(ui.hex_path);   // the whole path in the tooltip
    if (ok) {
        ImGui::Text("%s %d B", tr("Size:"), ui.hex_bytes);
        ImGui::TextDisabled("%s", tr("Address: from the records of the file"));
        ImGui::Text("%04X-%04Xh", (unsigned)ui.hex_lo, (unsigned)ui.hex_hi);
        if (ui.hex_lo == 0x100) cpm_save_hint(ui.hex_path, ui.hex_hi, "COM");
    } else {
        ImGui::TextColored(theme().error, "%s", ui.hex_error.c_str());
    }
    ImGui::BeginDisabled(!ok || !m);
    // Load is the default (Enter): the file was chosen for it; Esc = Cancel
    if (theme_accent_button(tr("Load")) || (ok && m && popup_enter())) {
        std::string text, err;
        int n = 0;
        char done[96];   // "Loaded 0100-1FFFh, 7936 B: file" in the status bar and the Log, a failure in the Error window
        snprintf(done, sizeof(done), "%s %04X-%04Xh, ", tr("Loaded"), (unsigned)ui.hex_lo, (unsigned)ui.hex_hi);
        if (!file_read_text(ui.hex_path, text))
            ui_notice(ui, false, std::string(tr("Cannot load the program")) + ":\n" + tr("Cannot read") + " " + ui.hex_path);
        else if (!m->load_ihex_text(text, err, &n))
            ui_notice(ui, false, std::string(tr("Cannot load the program")) + ":\n" + ui.hex_path + ": " + tr_core(err));
        else ui_notice(ui, true, done + std::to_string(n) + " B: " + ui.hex_path);
        ui.hex_path.clear();
        ui.focus_display = 2;   // the program is started on the keyboard of SAPI
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel")) || popup_escape()) {
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
    file_name_text(ui.bin_path);   // the whole path in the tooltip
    ImGui::Text("%s %lld B", tr("Size:"), size);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
    ImGui::InputText(tr("Address (hex)"), ui.bin_addr, sizeof(ui.bin_addr), ImGuiInputTextFlags_CharsHexadecimal);
    long long addr = strtol(ui.bin_addr, nullptr, 16);
    bool fits = size > 0 && addr >= 0 && addr + size <= 0x10000;
    if (fits) {
        ImGui::Text("%04llX-%04llXh", addr, addr + size - 1);
        if (addr == 0x100) cpm_save_hint(ui.bin_path, addr + size - 1, nullptr);
    } else {
        ImGui::TextColored(theme().error, "%s", size < 0 ? tr("Cannot read the file.")
                                                        : tr("The file does not fit into memory there."));
    }
    ImGui::BeginDisabled(!fits || !m);
    // Load is the default (Enter, also in the address field); Esc = Cancel
    if (theme_accent_button(tr("Load")) || (fits && m && popup_enter())) {
        std::string err;
        int n = 0;
        if (!m->load_binary(ui.bin_path, (uint16_t)addr, err, &n)) {
            ui_notice(ui, false, std::string(tr("Cannot load the program")) + ":\n" + tr_core(err));
        } else {
            char done[96];
            snprintf(done, sizeof(done), "%s %04llX-%04llXh, %d B: ", tr("Loaded"), addr, addr + (n > 0 ? n - 1 : 0), n);
            ui_notice(ui, true, done + ui.bin_path);
        }
        ui.bin_path.clear();
        ui.focus_display = 2;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel")) || popup_escape()) {
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
    file_name_text(ui.tprog_path);   // the whole path in the tooltip
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
    if (!ui.tprog_error.empty()) ImGui::TextColored(theme().error, "%s", ui.tprog_error.c_str());
    Card* card = m ? m->find_card(ui.tprog_card) : nullptr;
    Dsm1* dsm = ui.tprog_kzd ? nullptr : dynamic_cast<Dsm1*>(card);
    Kzd1* kzd = ui.tprog_kzd && card ? card->kzd1() : nullptr;
    ImGui::BeginDisabled(!dsm && !kzd);
    // Insert is the default (Enter); Esc = Cancel
    if (theme_accent_button(tr("Insert")) || ((dsm || kzd) && popup_enter())) {
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
                ui_notice(ui, true, info + " -> " + to);
            }
        } else {
            TapeDeck* t = dsm->tape();
            t->eject();
            ok = t->load_program(ui.tprog_path, p, err);
            if (ok) media_note(ui, dsm, 0, data_relative(ui.tprog_path));
        }
        if (ok) {
            ui.tprog_path.clear();
            ui.focus_display = 2;   // LOAD or .K_L is typed on the keyboard of SAPI
            ImGui::CloseCurrentPopup();
        } else {
            ui.tprog_error = tr_core(err);
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel")) || popup_escape()) {
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
            file_name_text(d.path);
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
        media_more(ui, r, i);
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
        ImGui::TextColored(theme().error, "%s", tr("The cartridge is not formatted: A: cannot be used without it."));
    ImGui::TextUnformatted(tr("Then Reset (F12): the CBIOS does not notice a changed cartridge. Copy SH.COM, SH.OVR, "
                              "STARTSYS.SUB and other files with PIP, e.g. through the fixed disk C:."));
    if (!s_rkd_sys_error.empty()) ImGui::TextColored(theme().error, "%s", s_rkd_sys_error.c_str());
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
            s_rkd_sys_error = tr_core(err);
            LOGE("RKD-1: %s", err.c_str());
        }
    }
    ImGui::SameLine();
    // Esc = Cancel; Enter does nothing: writing the system (and the format) overwrites the cartridge. The keyboard
    // navigation (Tab) starts at Cancel too.
    bool cancel = ImGui::Button(tr("Cancel"));
    ImGui::SetItemDefaultFocus();
    if (cancel || popup_escape()) {
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
            file_name_text(c.path);
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
        media_more(ui, r, i);
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
                tip(tr("Write the CP/M system on the cartridge (emulator help, no original SYSGEN)"));
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
    dl->AddText(panel_font(), 1.15f * s, ImVec2(o.x + 0.8f * s, o.y + 0.5f * s), IM_COL32(40, 40, 40, 255), "IZOT SM 5300.01");
    if (!picture_only()) {   // the number of the unit is the emulator's text, no lettering: not in a preview
        char unit_s[32];
        snprintf(unit_s, sizeof(unit_s), "%s %d", tr("unit"), i);
        panel_text(dl, ImVec2(o.x + w - 5.0f * s, o.y + 0.6f * s), IM_COL32(90, 90, 90, 255), unit_s);
    }
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
        panel_text(dl, ImVec2(o.x + w * 0.5f - 3.0f * s, o.y + 10.2f * s), IM_COL32(170, 170, 170, 255), tr("(no tape)"));
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
        ImVec2 ts = panel_text_size(lamps[k].name);
        panel_text(dl, ImVec2(c.x - ts.x * 0.5f, c.y + 0.8f * s), IM_COL32(50, 50, 50, 255), lamps[k].name);
        lamp_tooltip(c, 0.55f * s, lamps[k].name, lamps[k].on);
    }
    if (picture_only() && i == 0)   // the first drive with a margin (a preview of the machine)
        picture_mark(o.x - 0.3f * s, o.y - 0.3f * s, o.x + w + 0.3f * s, o.y + h + 0.3f * s);
    // the buttons of the drive, on its panel
    ImGui::SetCursorScreenPos(ImVec2(o.x + 0.8f * s, o.y + 14.4f * s));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(236, 234, 226, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(250, 248, 240, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(200, 198, 190, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(30, 30, 30, 255));
    // the widths from the labels (OFF-LINE, the longest, was cut off in equal widths), the rest of the panel shared
    // evenly; the padding of the frames no wider than that share
    static const char* const k_btn[4] = { "LOAD", "OFF-LINE", "REWIND", "UNLOAD" };
    const ImGuiStyle& st = ImGui::GetStyle();
    float bw[4], sum = 0;
    for (int k = 0; k < 4; k++) sum += bw[k] = ImGui::CalcTextSize(k_btn[k]).x;
    float extra = (w - 1.6f * s - 3 * st.ItemSpacing.x - sum) / 4;
    if (extra < 0) extra = 0;
    for (int k = 0; k < 4; k++) bw[k] += extra;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(extra * 0.5f < st.FramePadding.x ? extra * 0.5f : st.FramePadding.x, st.FramePadding.y));
    ImGui::BeginDisabled(!t.loaded || u.loaded);
    if (ImGui::Button("LOAD", ImVec2(bw[0], 1.8f * s))) r->press_load(i);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        tip(tr("Thread the tape and wind it to the BOT marker (RDY)."));
    ImGui::SameLine();
    ImGui::BeginDisabled(!u.loaded);
    if (ImGui::Button(u.online ? "OFF-LINE" : "ON-LINE", ImVec2(bw[1], 1.8f * s))) r->press_online(i);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        tip(tr("ON-LINE: the computer controls the drive (MTDAT: \"DISTANC\")."));
    ImGui::SameLine();
    ImGui::BeginDisabled(!u.loaded || u.online || u.pos <= 0);
    if (ImGui::Button("REWIND", ImVec2(bw[2], 1.8f * s))) r->press_rewind(i);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!u.loaded || u.online);
    if (ImGui::Button("UNLOAD", ImVec2(bw[3], 1.8f * s))) r->press_unload(i);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        tip(tr("Rewind and unthread (OFF-LINE only)."));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    ImGui::SetCursorScreenPos(o);
    ImGui::Dummy(ImVec2(w, h));

    // the right side: the reel, the position, the map of the tape and the block at the head
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (t.loaded) {
        ImGui::TextUnformatted(tr("Tape:"));
        ImGui::SameLine();
        file_name_text(t.path);
    } else ImGui::TextDisabled("%s", tr("(no tape)"));
    if (ImGui::Button(tr("Insert..."))) tape_dialog(ui, 14, r->name, i);
    ImGui::SameLine();
    if (ImGui::Button(tr("New tape..."))) tape_dialog(ui, 15, r->name, i);
    ImGui::SameLine();
    ImGui::BeginDisabled(!t.loaded || u.online);
    if (ImGui::Button(tr("Eject"))) media_set(ui, r, i, "");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && u.online)
        tip(tr("Switch the drive OFF-LINE first."));
    ImGui::BeginDisabled(u.online);
    media_more(ui, r, i);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(u.loaded);
    ImGui::Checkbox(tr("Write ring"), &u.ring);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        tip(tr("The ring in the reel allows writing, without it WPT. Changed only with the tape unthreaded."));
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
                tip(tr("The name the programs ask for (MTDAT: Jmeno pasky, MTCOPY: JP)."));
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
        md->AddRectFilled(ImVec2(x, y0 - 0.3f * s), ImVec2(x + 0.3f * s, y1 + 0.3f * s), ImGui::GetColorU32(ImGuiCol_TextDisabled));   // BOT foil
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
                if (it.kind == 2) tipf("%s, %.2f m", tr("tape mark"), at);
                else if (it.kind == 0) tipf("%s %s, %.2f m", tr("label"), it.label, at);
                else tipf("%d %s (%s %u B), %.2f m%s", it.count, tr("blocks"), tr("the first"),
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
                              ImGui::GetColorU32(theme().warn));   // the head
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

// ---------------------------------------------------------------- files dropped on the main window

// The suffix of a file in lower case, without the dot ("" = none).
static std::string file_suffix(const std::string& path)
{
    size_t slash = path.find_last_of("/\\"), dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return std::string();
    std::string s = path.substr(dot + 1);
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return s;
}

static bool suffix_in(const std::string& s, const char* const* list)
{
    for (int i = 0; list[i]; i++)
        if (s == list[i]) return true;
    return false;
}

// A text file that is Intel HEX: its first character other than a space is the colon of a record.
static bool text_is_hex(const std::string& path)
{
    FILE* f = file_open(path, "rb");
    if (!f) return false;
    int c;
    while ((c = fgetc(f)) == ' ' || c == '\t' || c == '\r' || c == '\n') {}
    fclose(f);
    return c == ':';
}

// The unit of the running machine a dropped medium goes into: the first empty one of the kind (TapeDialog::kind: 4
// floppy drives, 12 cartridge units of RKD-1, 14 tape units of RMP-1, 10 KZD-1, 1 cassette recorders of DSM-1), else
// the first one (empty = false). False: the machine has no unit of the kind.
static bool drop_unit(Machine* m, int kind, std::string& card, int& unit, bool& empty)
{
    bool found = false;
    empty = false;
    auto consider = [&](const std::string& c, int u, bool loaded) {
        if (found && (empty || loaded)) return;   // the first one, then the first empty one
        card = c;
        unit = u;
        empty = !loaded;
        found = true;
    };
    for (auto& c : m->cards) {
        Card* p = c.get();
        if (kind == 4)
            for (int i = 0; p->floppy_drive(i); i++) consider(p->name, i, p->floppy_drive(i)->present());
        if (Rkd1* r = kind == 12 ? dynamic_cast<Rkd1*>(p) : nullptr)
            for (int i = 0; i < Rkd1::UNITS; i++)
                if (r->units[i].fitted) consider(p->name, i, r->units[i].disk[Rkd1::CARTRIDGE].loaded);
        if (Rmp1* r = kind == 14 ? dynamic_cast<Rmp1*>(p) : nullptr)
            for (int i = 0; i < Rmp1::UNITS; i++)
                if (r->units[i].fitted) consider(p->name, i, r->units[i].tape.loaded);
        if (Kzd1* k = kind == 10 ? p->kzd1() : nullptr) consider(p->name, 0, k->loaded());
        if (Dsm1* d = kind == 1 ? dynamic_cast<Dsm1*>(p) : nullptr) consider(p->name, 0, d->tape()->loaded());
    }
    return found;
}

void file_drop(UiState& ui, const std::string& path)
{
    Machine* m = ui.host->machine();
    bool busy;
    {   // a native file dialog waits for its answer: it would take the place of this file
        std::lock_guard<std::mutex> lk(ui.tape_dlg.mtx);
        busy = ui.tape_dlg.kind != 0;
    }
    {
        std::lock_guard<std::mutex> lk(ui.rack.dlg_mtx);
        busy = busy || ui.rack.dlg_kind != RackEditor::DLG_NONE;
    }
    if (busy) {
        ui_notice(ui, true, std::string(tr("Close the open dialog first, then drop the file again")) + ": " + path);
        return;
    }
    std::string s = file_suffix(path);
    static const char* const k_machine[] = { "sapi", "jsonc", nullptr };
    static const char* const k_hex[] = { "hex", "ihx", nullptr };
    static const char* const k_floppy[] = { "img", "imd", "8sd", "sp2", "sp1", nullptr };   // k_img_filter
    static const char* const k_cassette[] = { "wav", "mp3", nullptr };
    if (suffix_in(s, k_machine)) {   // as File -> Start a machine from a file (the question about unsaved changes)
        rack_start_machine(ui, data_relative(path));
        return;
    }
    if (s == "sapistate") {   // as File -> Load state
        file_chosen(ui, m, 8, path, std::string(), 0);
        return;
    }
    // the rest goes into the running machine: what it is, as the dialogs of the File menu and of Media take it
    int kind = 0;
    const char* what = nullptr;   // the medium, for the messages: the name of its filter in the dialog (a tr() key)
    if (suffix_in(s, k_hex) || (s == "txt" && text_is_hex(path))) kind = 3;
    else if (s == "bin") kind = 6;
    else if (suffix_in(s, k_floppy)) { kind = 4; what = "Floppy disk (IMG, IMD, 8SD, SP2, SP1)"; }
    else if (s == "rkd") { kind = 12; what = "RKD-1 cartridge (RKD)"; }
    else if (s == "tap") { kind = 14; what = "Magnetic tape (SIMH TAP)"; }
    else if (s == "kzd") { kind = 10; what = "KZD-1 cassette (KZD)"; }
    else if (suffix_in(s, k_cassette)) { kind = 1; what = "Cassette (WAV, MP3)"; }
    if (!kind) {
        ui_notice(ui, true, std::string(tr("Not a file SAPIemu takes (a machine, a state, a program, a disk, a "
                                           "cassette)")) + ": " + path);
        return;
    }
    if (!m) {
        ui_notice(ui, true, std::string(tr("The machine is switched off: switch it on, then drop the file again")) + ": " +
                                path);
        return;
    }
    if (kind == 3 || kind == 6) {   // a program: the question with its addresses, as File -> Load program
        file_chosen(ui, m, kind, path, std::string(), 0);
        return;
    }
    std::string card;
    int unit = 0;
    bool empty = false;
    if (!drop_unit(m, kind, card, unit, empty)) {
        ui_notice(ui, false, std::string(tr("This machine has no drive for this medium")) + ": " + tr(what) + "\n" + path);
        return;
    }
    // a disk, a cartridge, a reel: only into an empty unit (one in use is not taken from the running program); a
    // cassette into a recorder as Insert... does (the cassette in it is saved while running)
    bool cassette = kind == 1 || kind == 10;
    if (!empty && !cassette) {
        ui_notice(ui, false, std::string(tr("Every drive for this medium is in use: eject a medium in the Media window, "
                                            "then drop the file again.")) + "\n" + path);
        return;
    }
    std::string where = card;
    if (kind == 4 || kind == 12 || kind == 14)
        where += std::string(", ") + tr(kind == 4 ? "Drive" : "Unit") + " " + std::to_string(unit);
    uint64_t said = ui.notice_ns;
    file_chosen(ui, m, kind, path, card, unit);
    if (ui.notice_ns == said)   // nothing said on the way (a bad image, a program made into a tape asks first)
        ui_notice(ui, true, std::string(tr("Inserted into")) + " " + where + ": " + path);
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
    layout_window(LW_MEDIA);   // 48 x 28 font heights, beside the guide
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
            theme_section(title);
            draw_tape(ui, dsm);
            continue;
        }
        if (c->floppy_drive(0)) {
            any = true;
            char title[96];
            snprintf(title, sizeof(title), "%s %s", c->name.c_str(), tr("floppy drives"));
            theme_section(title);
            draw_floppies(ui, c.get());
            continue;
        }
        if (Rkd1* rk = dynamic_cast<Rkd1*>(c.get())) {
            any = true;
            char title[96];
            snprintf(title, sizeof(title), "%s: %s", c->name.c_str(), tr("cartridge disk"));
            theme_section(title);
            draw_rkd(ui, rk);
            continue;
        }
        if (Rmp1* mt = dynamic_cast<Rmp1*>(c.get())) {
            any = true;
            char title[128];
            snprintf(title, sizeof(title), "%s: %s", c->name.c_str(), tr("magnetic tape IZOT SM 5300 (9 tracks, 800 bpi NRZI, 12.5 ips)"));
            theme_section(title);
            draw_rmp(ui, mt);
            continue;
        }
        if (Kzd1* k = c->kzd1()) {
            any = true;
            char title[96];
            snprintf(title, sizeof(title), "%s: %s", c->name.c_str(), tr("KZD-1 cassette data recorder"));
            theme_section(title);
            draw_kzd(ui, c.get(), k);
            if (!c->pmd32sd()) continue;
        }
        Ide1* ide = dynamic_cast<Ide1*>(c.get());
        if (!ide && !c->pmd32sd()) continue;
        any = true;
        theme_section(c->name.c_str());
        if (IdeDisk* d = ide ? ide->disk() : nullptr) {
            if (d->present()) {
                ImGui::TextUnformatted(tr("IDE disk:"));
                ImGui::SameLine();
                file_name_text(d->path());
                ImGui::SameLine();
                ImGui::Text("(%u MB%s)", d->sector_count() / 2048, d->is_readonly() ? tr(", read only") : "");
            } else {
                ImGui::TextUnformatted(tr("IDE disk: none"));
            }
        }
        if (Pmd32Sd* p = c->pmd32sd()) {
            ImGui::TextUnformatted("PMD32-SD");
            draw_pmd32(p);
        }
    }
    if (!any) ImGui::TextUnformatted(tr("This machine has no removable media."));
    ImGui::End();
}
