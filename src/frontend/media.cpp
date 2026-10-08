// Media window: cassette recorders of DSM-1 cards, images of the PMD32-SD drives
// and the IDE disk of IDE-1 cards.
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "cards/dsm1.h"
#include "cards/ide1.h"
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

static int s_new_disk_fmt = 0;   // format of a new disk (index into floppy_geoms())

static void SDLCALL tape_dialog_done(void* userdata, const char* const* files, int)
{
    TapeDialog* d = (TapeDialog*)userdata;
    std::lock_guard<std::mutex> lk(d->mtx);
    d->result = (files && files[0]) ? files[0] : "";
    d->done = true;
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
#ifdef _WIN32
    for (char& c : where) if (c == '/') c = '\\';
#endif
    if (kind == 1) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_tape_filter, 3, where.c_str(), false);
    else if (kind == 3) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_hex_filter, 2, where.c_str(), false);
    else if (kind == 6) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, nullptr, 0, where.c_str(), false);
    else if (kind == 7) SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_state_filter, 1, where.c_str());
    else if (kind == 8) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_state_filter, 1, where.c_str(), false);
    else if (kind == 4) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_img_filter, 2, where.c_str(), false);
    else if (kind == 5) SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_img_filter, 1, where.c_str());
    else if (kind == 9) SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_wav_filter, 1, where.c_str());
    else if (kind == 10) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_kzd_filter, 2, where.c_str(), false);
    else if (kind == 11) SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_kzd_filter, 1, where.c_str());
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
        if (!k->insert(path, err)) LOGE("%s", err.c_str());
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
        if (!r->floppy_insert(drive, data_relative(path), false, err)) LOGE("%s", err.c_str());
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
        ui.tprog_ask = true;
        ui.tprog_error.clear();
        snprintf(ui.tprog_name, sizeof(ui.tprog_name), "%s", tape_default_name(path).c_str());
        return;
    }
    t->eject();
    if (kind == 1) {
        if (!t->load(path, err)) LOGE("%s", err.c_str());
    } else {
        size_t slash = path.find_last_of("/\\");
        if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".wav";
        if (!t->create(path, err)) LOGE("%s", err.c_str());
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
    if (ImGui::Button(tr("Eject"))) t->eject();
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
    if (ImGui::Button(tr("Eject"))) k->eject();
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
    if (!ImGui::BeginTable("pmd", 3, ImGuiTableFlags_SizingFixedFit)) return;
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
    ImGui::RadioButton(tr("MIKOS (ZPS-2): .K_L name, then .J"), &ui.tprog_format, TAPE_MIKOS);
    ImGui::RadioButton(tr("MIKROBASIC (ZPS-1): LOAD"), &ui.tprog_format, TAPE_MIKROBASIC);
    ImGui::TextDisabled("%s", ui.tprog_format == TAPE_MIKOS
                                  ? tr("From 4400h up; .J starts the program at its lowest address.")
                                  : tr("A BASIC program at 40EFh, or machine code from 4102h up (RUN starts it)."));
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
    Dsm1* dsm = m ? dynamic_cast<Dsm1*>(m->find_card(ui.tprog_card)) : nullptr;
    ImGui::BeginDisabled(!dsm);
    if (ImGui::Button(tr("Insert"))) {
        TapeProgram p;
        p.format = ui.tprog_format;
        p.name = ui.tprog_name;
        p.address = bin ? strtol(ui.tprog_addr, nullptr, 16) : -1;
        p.entry = ui.tprog_entry[0] ? strtol(ui.tprog_entry, nullptr, 16) : -1;
        std::string err;
        TapeDeck* t = dsm->tape();
        t->eject();
        if (t->load_program(ui.tprog_path, p, err)) {
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
        if (ImGui::Button(tr("Eject"))) r->floppy_eject(i);
        ImGui::SameLine();
        ImGui::Checkbox(tr("Write protect"), &d.write_protect);
        ImGui::SameLine();
        ImGui::Text("%s %d%s", tr("track"), d.track, d.dirty ? tr(", modified") : "");
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::PopID();
}

void draw_media(UiState& ui)
{
    tape_dialog_result(ui, ui.host->machine());   // also the HEX and binary dialogs of the File menu
    bin_load_question(ui, ui.host->machine());
    hex_load_question(ui, ui.host->machine());
    tape_program_question(ui, ui.host->machine());
    if (!ui.show_media) return;
    Machine* m = ui.host->machine();
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
