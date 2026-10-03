// Media window: images of the PMD32-SD drives and the IDE disk of IDE-1 cards.
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <stdio.h>
#include "imgui.h"
#include "cards/ide1.h"
#include "core/host.h"

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

void draw_media(UiState& ui)
{
    if (!ui.show_media) return;
    Machine* m = ui.host->machine();
    ImGui::SetNextWindowSize(ImVec2(560, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(tr("Media"), &ui.show_media) || !m) {
        ImGui::End();
        return;
    }
    bool any = false;
    for (auto& c : m->cards) {
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
