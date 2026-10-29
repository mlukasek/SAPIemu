// Window "Printer": the paper of the printers of the machine (JPR X3, DPP-1).
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/theme.h"

#include <stdio.h>
#include <map>
#include "imgui.h"
#include "chips/printer.h"
#include "core/host.h"
#include "core/machine.h"

void draw_printer(UiState& ui)
{
    if (!ui.show_printer) return;
    Machine* m = ui.host->machine();
    layout_window(LW_PRINTER);
    if (!ImGui::Begin(tr_id("Printer"), &ui.show_printer) || !m) {
        ImGui::End();
        return;
    }
    int count = 0;
    for (auto& c : m->cards)
        if (c->printer()) count++;
    if (!count) {
        ImGui::TextDisabled("%s", tr("No printer. Configurator: the printer on X3 of the CPU card or on DPP-1."));
        ImGui::End();
        return;
    }
    static std::map<const Printer*, uint64_t> seen;   // generation shown last: follow the new lines
    int left = count;
    for (auto& c : m->cards) {
        Printer* p = c->printer();
        if (!p) continue;
        ImGui::PushID(p);
        std::string head = c->type + " " + tr("printer");
        theme_section(head.c_str());
        if (ImGui::Button(tr("New sheet"))) p->new_sheet();
        ImGui::SameLine();
        if (ImGui::Button(tr("Copy text"))) ImGui::SetClipboardText(p->text(1000000).c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s, %llu B%s", p->path().c_str(), (unsigned long long)p->bytes,
                            p->busy(m->bus.now) ? ", BUSY" : "");
        float h = ImGui::GetContentRegionAvail().y / (float)left - ImGui::GetFrameHeightWithSpacing() * (left > 1 ? 2.0f : 0.0f);
        if (h < 80) h = 80;
        left--;
        ImGui::BeginChild("paper", ImVec2(0, h), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        if (ui.mono) ImGui::PushFont(ui.mono, 0.0f);
        bool at_end = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2;
        int n = (int)p->lines.size() + 1;
        std::string cur = p->current_line();
        ImGuiListClipper clip;
        clip.Begin(n);
        while (clip.Step())
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; i++) {
                const std::string& line = i + 1 < n ? p->lines[(size_t)i] : cur;
                if (line == "\f") ImGui::TextDisabled("%s", tr("- - - - - new page - - - - -"));
                else ImGui::TextUnformatted(line.c_str());
            }
        if (seen[p] != p->generation && at_end) ImGui::SetScrollHereY(1.0f);
        seen[p] = p->generation;
        if (ui.mono) ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopID();
    }
    ImGui::End();
}
