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
#include "cards/rpd1.h"
#include "core/host.h"
#include "core/log.h"
#include "core/util.h"

// ---------------------------------------------------------------- cassette recorder

static const SDL_DialogFileFilter k_tape_filter[] = { { "Kazeta (WAV, MP3)", "wav;mp3" } };
static const SDL_DialogFileFilter k_wav_filter[] = { { "WAV", "wav" } };
static const SDL_DialogFileFilter k_hex_filter[] = { { "Intel HEX", "hex;ihx" } };
static const SDL_DialogFileFilter k_img_filter[] = { { "Disketa 8\" (IMG)", "img" } };

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
#ifdef _WIN32
    for (char& c : where) if (c == '/') c = '\\';
#endif
    if (kind == 1) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_tape_filter, 1, where.c_str(), false);
    else if (kind == 3) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_hex_filter, 1, where.c_str(), false);
    else if (kind == 6) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, nullptr, 0, where.c_str(), false);
    else if (kind == 4) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_img_filter, 1, where.c_str(), false);
    else if (kind == 5) SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_img_filter, 1, where.c_str());
    else SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_wav_filter, 1, where.c_str());
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
    if (path.empty() || !m) return;
    d.last_dir = path_dir(path);
    if (kind == 3) {   // a program into memory, as if loaded from the tape
        std::string text, err;
        int n = 0;
        uint16_t lo = 0, hi = 0;
        if (!file_read_text(path, text)) LOGE("cannot read %s", path.c_str());
        else if (!m->load_ihex_text(text, err, &n, &lo, &hi)) LOGE("%s: %s", path.c_str(), err.c_str());
        else {
            LOGI("%s: %d bytes loaded into memory", path.c_str(), n);
            ui.hex_done = path;   // the range and the CP/M SAVE command in a message
            ui.hex_bytes = n;
            ui.hex_lo = lo;
            ui.hex_hi = hi;
        }
        return;
    }
    if (kind == 6) {   // a binary file: ask for the address first
        ui.bin_path = path;
        ui.bin_ask = true;
        return;
    }
    if (kind == 4 || kind == 5) {   // a floppy for an RPD-1 drive
        Rpd1* r = dynamic_cast<Rpd1*>(m->find_card(card));
        if (!r) return;
        std::string err;
        if (kind == 5) {   // a new disk, formatted: every byte E5h (empty CP/M directory)
            size_t slash = path.find_last_of("/\\");
            if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) path += ".img";
            std::vector<uint8_t> blank(Rpd1::IMAGE_SIZE, 0xE5);
            if (!file_write_all(path, blank.data(), blank.size())) {
                LOGE("cannot write %s", path.c_str());
                return;
            }
        }
        // a disk from media/ is used through its working copy
        if (!r->insert(drive, data_relative(path), false, err)) LOGE("%s", err.c_str());
        return;
    }
    Dsm1* dsm = dynamic_cast<Dsm1*>(m->find_card(card));
    if (!dsm) return;
    std::string err;
    TapeDeck* t = dsm->tape();
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
    } else {
        ImGui::TextDisabled("%s", tr("(no cassette)"));
    }
    if (ImGui::Button(tr("Insert..."))) tape_dialog(ui, 1, dsm->name);
    ImGui::SameLine();
    if (ImGui::Button(tr("New cassette..."))) tape_dialog(ui, 2, dsm->name);
    ImGui::SameLine();
    ImGui::BeginDisabled(!t->loaded());
    if (ImGui::Button(tr("Eject"))) t->eject();
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
    ImGui::BeginDisabled(t->is_mp3);
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

static void draw_pmd32(Pmd32Sd* p)
{
    std::vector<std::string> images = p->list_images();
    ImGui::TextDisabled("%s %s", tr("SD card:"), p->root().c_str());
    if (!ImGui::BeginTable("pmd", 3, ImGuiTableFlags_SizingFixedFit)) return;
    for (int d = 0; d < Pmd32Sd::DRIVES; d++) {
        ImGui::PushID(d);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%c:", 'A' + d);
        if (d < 2) {
            ImGui::SameLine();
            ImGui::TextDisabled("(CP/M %c:)", 'D' + d);
        }
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

// After File -> Load program (Intel HEX): where it went and how to save it in CP/M.
static void hex_load_info(UiState& ui)
{
    const char* title = tr("Program loaded");
    if (!ui.hex_done.empty() && !ImGui::IsPopupOpen(title)) ImGui::OpenPopup(title);
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::TextUnformatted(ui.hex_done.c_str());
    if (ui.hex_bytes > 0) {
        ImGui::Text("%04X-%04Xh, %d B", ui.hex_lo, ui.hex_hi, ui.hex_bytes);
        if (ui.hex_lo == 0x100) cpm_save_hint(ui.hex_done, ui.hex_hi, "COM");
    } else {
        ImGui::TextUnformatted(tr("The file holds no data."));
    }
    if (ImGui::Button(tr("OK")) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ui.hex_done.clear();
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
    const char* title = tr("Load binary file into memory");
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
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", size < 0 ? tr("Cannot read the file.")
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

// ---------------------------------------------------------------- floppy drives of RPD-1

static void draw_rpd1(UiState& ui, Rpd1* r)
{
    ImGui::PushID(r);
    for (int i = 0; i < Rpd1::DRIVES; i++) {
        Rpd1::Drive& d = r->drives[i];
        ImGui::PushID(i);
        ImGui::Text("%s %c:", tr("Drive"), 'A' + i);
        ImGui::SameLine();
        if (d.data.empty()) ImGui::TextDisabled("%s", tr("(no disk)"));
        else {
            ImGui::TextUnformatted(d.path.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", d.path.c_str());
        }
        if (ImGui::Button(tr("Insert..."))) tape_dialog(ui, 4, r->name, i);
        ImGui::SameLine();
        if (ImGui::Button(tr("New disk..."))) tape_dialog(ui, 5, r->name, i);
        ImGui::SameLine();
        ImGui::BeginDisabled(d.data.empty());
        if (ImGui::Button(tr("Eject"))) r->eject(i);
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
    hex_load_info(ui);
    if (!ui.show_media) return;
    Machine* m = ui.host->machine();
    ImGui::SetNextWindowSize(ImVec2(560, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(tr("Media"), &ui.show_media) || !m) {
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
        if (Rpd1* rpd = dynamic_cast<Rpd1*>(c.get())) {
            any = true;
            char title[96];
            snprintf(title, sizeof(title), "%s %s", c->name.c_str(), tr("floppy drives 8\""));
            ImGui::SeparatorText(title);
            draw_rpd1(ui, rpd);
            continue;
        }
        Ide1* ide = dynamic_cast<Ide1*>(c.get());
        if (!ide) continue;
        any = true;
        ImGui::SeparatorText(c->name.c_str());
        if (IdeDisk* d = ide->disk()) {
            if (d->present())
                ImGui::Text("%s %s (%u MB%s)", tr("IDE disk:"), d->path().c_str(), d->sector_count() / 2048,
                            d->is_readonly() ? tr(", read only") : "");
            else
                ImGui::TextUnformatted(tr("IDE disk: none"));
        }
        if (Pmd32Sd* p = ide->pmd32()) {
            ImGui::TextUnformatted("PMD32-SD");
            draw_pmd32(p);
        }
    }
    if (!any) ImGui::TextUnformatted(tr("This machine has no removable media."));
    ImGui::End();
}
