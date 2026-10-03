// Media window: cassette recorders of DSM-1 cards, images of the PMD32-SD drives
// and the IDE disk of IDE-1 cards.
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <stdio.h>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "cards/dsm1.h"
#include "cards/ide1.h"
#include "core/host.h"
#include "core/log.h"
#include "core/util.h"

// ---------------------------------------------------------------- cassette recorder

static const SDL_DialogFileFilter k_tape_filter[] = { { "Kazeta (WAV, MP3)", "wav;mp3" } };
static const SDL_DialogFileFilter k_wav_filter[] = { { "WAV", "wav" } };
static const SDL_DialogFileFilter k_hex_filter[] = { { "Intel HEX", "hex;ihx" } };

static void SDLCALL tape_dialog_done(void* userdata, const char* const* files, int)
{
    TapeDialog* d = (TapeDialog*)userdata;
    std::lock_guard<std::mutex> lk(d->mtx);
    d->result = (files && files[0]) ? files[0] : "";
    d->done = true;
}

static void tape_dialog(UiState& ui, int kind, const std::string& card)
{
    TapeDialog& d = ui.tape_dlg;
    {
        std::lock_guard<std::mutex> lk(d.mtx);
        if (d.kind) return;
        d.kind = kind;
        d.done = false;
        d.card = card;
    }
    std::string where = d.last_dir.empty() ? data_path("work") : d.last_dir;
#ifdef _WIN32
    for (char& c : where) if (c == '/') c = '\\';
#endif
    if (kind == 1) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_tape_filter, 1, where.c_str(), false);
    else if (kind == 3) SDL_ShowOpenFileDialog(tape_dialog_done, &d, ui.window, k_hex_filter, 1, where.c_str(), false);
    else SDL_ShowSaveFileDialog(tape_dialog_done, &d, ui.window, k_wav_filter, 1, where.c_str());
}

static void tape_dialog_result(UiState& ui, Machine* m)
{
    TapeDialog& d = ui.tape_dlg;
    int kind;
    std::string path, card;
    {
        std::lock_guard<std::mutex> lk(d.mtx);
        if (!d.kind || !d.done) return;
        kind = d.kind;
        path = d.result;
        card = d.card;
        d.kind = 0;
    }
    if (path.empty() || !m) return;
    d.last_dir = path_dir(path);
    if (kind == 3) {   // a program into memory, as if loaded from the tape
        std::string text, err;
        int n = 0;
        if (!file_read_text(path, text)) LOGE("cannot read %s", path.c_str());
        else if (!m->load_ihex_text(text, err, &n)) LOGE("%s: %s", path.c_str(), err.c_str());
        else LOGI("%s: %d bytes loaded into memory", path.c_str(), n);
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

void draw_media(UiState& ui)
{
    tape_dialog_result(ui, ui.host->machine());   // also the HEX dialog of the File menu
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
