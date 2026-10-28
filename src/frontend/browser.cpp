// The window "Choose a machine": the machines in categories (core/machine_index.h) with a preview, a short text
// for newcomers and the technical description, Start; and the window "Guide": what to try in a machine, a small
// Markdown file (machines/guides/<id>.md) shown with headings, lists and code blocks that can be typed into SAPI.
// The guide opens itself when a machine of a category with guides starts, unless the user said no (settings).
#include "frontend/ui.h"
#include "frontend/i18n.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "core/host.h"
#include "core/log.h"
#include "core/machine.h"
#include "core/machine_cfg.h"
#include "core/machine_index.h"
#include "core/util.h"

static bool english(void)
{
    return i18n_language() == LANG_EN;
}

static const std::string& pick(const std::string& cs, const std::string& en)
{
    return english() && !en.empty() ? en : cs;
}

static std::string entry_name(const MachineEntry& e)
{
    if (english() && !e.title_en.empty()) return e.title_en;
    return e.title.empty() ? e.name : e.title;
}

// ---------------------------------------------------------------- previews

struct Preview {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;
};
static std::map<std::string, Preview> s_previews;

static const Preview* preview(UiState& ui, const std::string& rel)
{
    auto it = s_previews.find(rel);
    if (it != s_previews.end()) return it->second.tex ? &it->second : nullptr;
    Preview p;
    SDL_Renderer* r = ui.window ? SDL_GetRenderer(ui.window) : nullptr;
    SDL_Surface* s = r ? SDL_LoadPNG(data_path(rel).c_str()) : nullptr;
    if (s) {
        p.tex = SDL_CreateTextureFromSurface(r, s);
        p.w = s->w;
        p.h = s->h;
        SDL_DestroySurface(s);
    }
    s_previews[rel] = p;
    return p.tex ? &s_previews[rel] : nullptr;
}

// ---------------------------------------------------------------- the window "Choose a machine"

void browser_open(UiState& ui)
{
    ui.show_browser = true;
    ui.browser_loaded = false;   // read the index again, the window comes to the front
}

static void remember_recent(UiState& ui, const std::string& file)
{
    ui.recent.erase(std::remove(ui.recent.begin(), ui.recent.end(), file), ui.recent.end());
    ui.recent.insert(ui.recent.begin(), file);
    if (ui.recent.size() > 8) ui.recent.resize(8);
}

static void start(UiState& ui, const std::string& file)
{
    remember_recent(ui, file);
    rack_start_machine(ui, file);
    ui.show_browser = false;
    ui.guide_gen = 0;   // the guide of the new machine may open itself
}

static bool matches(const MachineEntry& e, const char* filter)
{
    if (!filter[0]) return true;
    std::string hay = e.file + " " + e.name + " " + e.title + " " + e.title_en + " " + e.summary + " " + e.summary_en;
    std::string f = filter;
    std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c) { return (char)tolower(c); });
    std::transform(f.begin(), f.end(), f.begin(), [](unsigned char c) { return (char)tolower(c); });
    return hay.find(f) != std::string::npos;
}

static void machine_item(UiState& ui, const MachineEntry& e, const char* id_suffix)
{
    bool sel = ui.browser_sel == e.file;
    std::string label = entry_name(e) + "##" + e.file + id_suffix;
    // the running one in green (not a mark: arrows belong to the headers that fold)
    bool running = e.file == ui.host->machine_file();
    if (running) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.90f, 0.55f, 1.0f));
    bool clicked = ImGui::Selectable(label.c_str(), sel, ImGuiSelectableFlags_AllowDoubleClick);
    if (running) {
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Running now"));
    }
    if (clicked) {
        ui.browser_sel = e.file;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) start(ui, e.file);
    }
}

// the cards of the machine by slots with the devices plugged into them (read when the selection changes)
static void parts(const MachineEntry& e)
{
    static std::string s_file;
    static bool s_en = false;
    static std::vector<MachinePart> s_parts;
    if (e.file != s_file || english() != s_en) {
        s_file = e.file;
        s_en = english();
        s_parts.clear();
        std::string text;
        if (file_read_text(data_path(e.file), text)) {
            nlohmann::json j = nlohmann::json::parse(text, nullptr, false, true /* comments */);
            if (!j.is_discarded()) s_parts = machine_cfg_parts(j, !s_en);
        }
    }
    if (s_parts.empty() || !ImGui::CollapsingHeader(tr("Cards and devices"), ImGuiTreeNodeFlags_DefaultOpen)) return;
    if (!ImGui::BeginTable("parts", 3, ImGuiTableFlags_SizingFixedFit)) return;
    ImGui::TableSetupColumn("slot");
    ImGui::TableSetupColumn("type");
    ImGui::TableSetupColumn("title", ImGuiTableColumnFlags_WidthStretch);
    for (const MachinePart& p : s_parts) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (p.slot > 0) ImGui::TextDisabled("X%d", p.slot);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(p.type.c_str());
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", p.title.c_str());
        for (const std::string& d : p.devices) {
            ImGui::Bullet();
            ImGui::TextWrapped("%s", d.c_str());
        }
    }
    ImGui::EndTable();
    ImGui::Spacing();
}

static void details(UiState& ui, const MachineEntry& e)
{
    float avail = ImGui::GetContentRegionAvail().x;
    if (!e.preview.empty()) {
        if (const Preview* p = preview(ui, e.preview)) {
            float u = ImGui::GetFontSize();
            float w = std::min(avail, 35.0f * u), h = w * p->h / (float)p->w;
            if (h > 22.0f * u) {   // a tall picture (the sheet of a plotter) keeps the text in sight
                w *= 22.0f * u / h;
                h = 22.0f * u;
            }
            ImGui::Image(ImTextureRef((ImTextureID)(intptr_t)p->tex), ImVec2(w, h));
        }
    } else {
        ImGui::TextDisabled("%s", tr("No preview"));
    }
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.45f, 1.0f));
    ImGui::TextWrapped("%s", entry_name(e).c_str());
    ImGui::PopStyleColor();
    if (entry_name(e) != e.name) ImGui::TextDisabled("%s", e.name.c_str());
    const std::string& sum = pick(e.summary, e.summary_en);
    if (!sum.empty()) ImGui::TextWrapped("%s", sum.c_str());
    ImGui::Spacing();
    if (ImGui::Button(tr("Start"))) start(ui, e.file);
    ImGui::SameLine();
    if (!e.guide.empty()) {
        if (ImGui::Button(tr("Guide"))) {
            ui.show_guide = true;
            ui.guide_machine = e.file;
        }
        ImGui::SameLine();
    }
    if (ImGui::Button(tr("Open in the configurator"))) {
        rack_open_in_editor(ui, e.file);
        ui.show_rack = true;
    }
    ImGui::Spacing();
    parts(e);
    if (!e.description.empty() && ImGui::CollapsingHeader(tr("Technical description"), sum.empty() ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        ImGui::TextWrapped("%s", e.description.c_str());
    ImGui::TextDisabled("%s", e.file.c_str());
}

void draw_browser(UiState& ui)
{
    if (!ui.show_browser) return;
    if (!ui.browser_loaded) {
        ImGui::SetNextWindowFocus();
        std::string err;
        machine_index_load(ui.machine_index, err);
        // a machine the release leaves out (sapi1v_PoP, package.py REPO_ONLY) is skipped; test_machine_index
        // checks the index in the repository
        if (!err.empty()) LOGD("%s", err.c_str());
        ui.browser_loaded = true;
        if (ui.browser_sel.empty()) ui.browser_sel = ui.host->machine_file();
        if (!machine_index_find(ui.machine_index, ui.browser_sel) && !ui.machine_index.machines.empty() &&
            !ui.machine_index.categories.empty() && !ui.machine_index.categories[0].machines.empty())
            ui.browser_sel = ui.machine_index.machines[(size_t)ui.machine_index.categories[0].machines[0]].file;
    }
    float u = ImGui::GetFontSize();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(58 * u, 38 * u), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(1.0f);
    if (!ImGui::Begin(tr_id("Choose a machine"), &ui.show_browser)) {
        ImGui::End();
        return;
    }
    const MachineIndex& idx = ui.machine_index;
    static char filter[64] = "";
    ImGui::BeginChild("list", ImVec2(22 * u, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", tr("Search"), filter, sizeof(filter));
    // recently started ones
    bool any_recent = false;
    for (const std::string& f : ui.recent)
        if (machine_index_find(idx, f)) any_recent = true;
    if (any_recent && !filter[0] && ImGui::CollapsingHeader(tr("Recently started"), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const std::string& f : ui.recent)
            if (const MachineEntry* e = machine_index_find(idx, f)) machine_item(ui, *e, "#recent");
    }
    // the categories; those of one group under one header
    std::string open_group;
    bool group_open = false;
    for (size_t c = 0; c < idx.categories.size(); c++) {
        const MachineCategory& cat = idx.categories[c];
        bool any = false;
        for (int k : cat.machines)
            if (matches(idx.machines[(size_t)k], filter)) any = true;
        if (!any) continue;
        std::string cname = pick(cat.name, cat.name_en) + "##cat" + cat.id;
        if (cat.group.empty()) {
            ImGuiTreeNodeFlags fl = filter[0] || cat.guides ? ImGuiTreeNodeFlags_DefaultOpen : 0;
            if (!ImGui::CollapsingHeader(cname.c_str(), fl)) continue;
            const std::string& text = pick(cat.text, cat.text_en);
            if (!text.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", text.c_str());
            for (int k : cat.machines)
                if (matches(idx.machines[(size_t)k], filter)) machine_item(ui, idx.machines[(size_t)k], "");
            continue;
        }
        if (cat.group != open_group) {   // a new group header
            open_group = cat.group;
            std::string g = pick(cat.group, cat.group_en) + "##group" + cat.group;
            group_open = ImGui::CollapsingHeader(g.c_str(), filter[0] ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        }
        if (!group_open) continue;
        if (filter[0]) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        if (ImGui::TreeNode(cname.c_str())) {
            for (int k : cat.machines)
                if (matches(idx.machines[(size_t)k], filter)) machine_item(ui, idx.machines[(size_t)k], "");
            ImGui::TreePop();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("details", ImVec2(0, 0), ImGuiChildFlags_None);
    if (const MachineEntry* e = machine_index_find(idx, ui.browser_sel)) details(ui, *e);
    else ImGui::TextDisabled("%s", tr("Choose a machine on the left."));
    ImGui::EndChild();
    ImGui::End();
}

// ---------------------------------------------------------------- the guide

// inline marks of Markdown left out: **bold**, `code`; [text](link) keeps the text, the links are collected
static std::string plain(const std::string& s, std::vector<std::pair<std::string, std::string>>* links)
{
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s.compare(i, 2, "**") == 0) {
            i++;
            continue;
        }
        if (s[i] == '`') continue;
        if (s[i] == '[') {
            size_t close = s.find("](", i);
            size_t end = close == std::string::npos ? close : s.find(')', close);
            if (end != std::string::npos) {
                std::string text = s.substr(i + 1, close - i - 1), url = s.substr(close + 2, end - close - 2);
                o += text;
                if (links) links->push_back({ text, url });
                i = end;
                continue;
            }
        }
        o += s[i];
    }
    return o;
}

static void open_link(const std::string& url)
{
    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        SDL_OpenURL(url.c_str());
        return;
    }
    std::string p = data_path(url);   // a file of the documentation, relative to the data root
    for (char& c : p)
        if (c == '\\') c = '/';
    SDL_OpenURL(("file:///" + p).c_str());
}

static void code_block(UiState& ui, const std::vector<std::string>& lines, int n)
{
    float u = ImGui::GetFontSize();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.11f, 0.13f, 1.0f));
    float h = (float)lines.size() * ImGui::GetTextLineHeightWithSpacing() + 0.8f * u;
    char id[32];
    snprintf(id, sizeof(id), "code%d", n);
    ImGui::BeginChild(id, ImVec2(-1, h), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    if (ui.mono) ImGui::PushFont(ui.mono);
    for (const std::string& l : lines) ImGui::TextUnformatted(l.c_str());
    if (ui.mono) ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    std::string text;
    for (const std::string& l : lines) text += l + "\r";
    char b[64];
    snprintf(b, sizeof(b), "%s##type%d", tr("Type into SAPI"), n);
    Machine* m = ui.host->machine();
    ImGui::BeginDisabled(!m);
    if (ImGui::SmallButton(b) && m) m->type_text(text);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Types the lines on the keyboard of SAPI, each ended by Enter."));
    ImGui::SameLine();
    snprintf(b, sizeof(b), "%s##copy%d", tr("Copy"), n);
    if (ImGui::SmallButton(b)) {
        std::string c;
        for (const std::string& l : lines) c += l + "\n";
        SDL_SetClipboardText(c.c_str());
    }
}

static void markdown(UiState& ui, const std::string& text)
{
    std::vector<std::string> lines;
    size_t a = 0;
    while (a <= text.size()) {
        size_t e = text.find('\n', a);
        if (e == std::string::npos) e = text.size();
        std::string l = text.substr(a, e - a);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
        a = e + 1;
    }
    // a line that only goes on with the text of the line above (a paragraph, a list item) joins it, as in Markdown
    auto starts_block = [](const std::string& l) {
        size_t p = l.find_first_not_of(' ');
        if (p == std::string::npos) return true;
        std::string b = l.substr(p);
        if (b[0] == '#' || b.rfind("```", 0) == 0 || b.rfind("- ", 0) == 0 || b.rfind("* ", 0) == 0) return true;
        size_t d = 0;
        while (d < b.size() && b[d] >= '0' && b[d] <= '9') d++;
        return d > 0 && d + 1 < b.size() && b[d] == '.' && b[d + 1] == ' ';
    };
    std::vector<std::string> joined;
    bool in_code = false;
    for (const std::string& l : lines) {
        if (l.rfind("```", 0) == 0) in_code = !in_code;
        bool prev_text = !joined.empty() && !joined.back().empty() && joined.back().rfind("```", 0) != 0 &&
                         joined.back()[0] != '#';
        if (!in_code && l.rfind("```", 0) != 0 && prev_text && !starts_block(l)) {
            size_t p = l.find_first_not_of(' ');
            joined.back() += " " + l.substr(p);
            continue;
        }
        joined.push_back(l);
    }
    lines.swap(joined);
    int ncode = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& l = lines[i];
        if (l.rfind("```", 0) == 0) {
            std::vector<std::string> code;
            for (i++; i < lines.size() && lines[i].rfind("```", 0) != 0; i++) code.push_back(lines[i]);
            code_block(ui, code, ncode++);
            continue;
        }
        if (l.find_first_not_of(' ') == std::string::npos) {   // empty or only spaces
            ImGui::Spacing();
            continue;
        }
        std::vector<std::pair<std::string, std::string>> links;
        size_t lead = l.find_first_not_of(' ');
        std::string body = l.substr(lead);
        float indent = (float)lead * 0.5f * ImGui::GetFontSize(), bullet = 0;
        if (indent > 0) ImGui::Indent(indent);
        if (body.rfind("# ", 0) == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.45f, 1.0f));
            ImGui::TextWrapped("%s", plain(body.substr(2), nullptr).c_str());
            ImGui::PopStyleColor();
        } else if (body.rfind("## ", 0) == 0 || body.rfind("### ", 0) == 0) {
            ImGui::Spacing();
            ImGui::SeparatorText(plain(body.substr(body.find(' ') + 1), nullptr).c_str());
        } else if (body.rfind("- ", 0) == 0 || body.rfind("* ", 0) == 0) {
            ImGui::Bullet();
            ImGui::TextWrapped("%s", plain(body.substr(2), &links).c_str());
            bullet = ImGui::GetTreeNodeToLabelSpacing();
        } else {
            ImGui::TextWrapped("%s", plain(body, &links).c_str());
        }
        // the links as buttons under the text (aligned with it), wrapped to the width of the window
        if (bullet > 0 && !links.empty()) ImGui::Indent(bullet);
        for (size_t k = 0; k < links.size(); k++) {
            char b[300];
            snprintf(b, sizeof(b), "%s \xE2\x86\x97##link%zu_%zu", links[k].first.c_str(), i, k);
            float w = ImGui::CalcTextSize(b, nullptr, true).x + 2.0f * ImGui::GetStyle().FramePadding.x;
            if (k > 0) {
                ImGui::SameLine();
                if (ImGui::GetContentRegionAvail().x < w) ImGui::NewLine();
            }
            if (ImGui::SmallButton(b)) open_link(links[k].second);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", links[k].second.c_str());
        }
        if (bullet > 0 && !links.empty()) ImGui::Unindent(bullet);
        if (indent > 0) ImGui::Unindent(indent);
    }
}

void draw_guide(UiState& ui)
{
    // a machine started: its guide opens itself (categories with guides), unless the user said no
    // (not while the window Choose a machine is open: the machine the user starts from it opens its guide)
    uint64_t gen = ui.host->machine_generation();
    if (gen != ui.guide_gen && !ui.show_browser) {
        ui.guide_gen = gen;
        const std::string& f = ui.host->machine_file();
        if (!f.empty()) {
            if (!ui.browser_loaded && ui.machine_index.machines.empty()) {
                std::string err;
                machine_index_load(ui.machine_index, err);
            }
            const MachineEntry* e = machine_index_find(ui.machine_index, f);
            bool hidden = std::find(ui.guides_hidden.begin(), ui.guides_hidden.end(), f) != ui.guides_hidden.end();
            if (e && !e->guide.empty() && e->category >= 0 && ui.machine_index.categories[(size_t)e->category].guides &&
                ui.guides_auto && !hidden) {
                ui.show_guide = true;
                ui.guide_machine = f;
            } else if (ui.show_guide) {
                ui.guide_machine = f;   // an open guide follows the machine
            }
        }
    }
    if (!ui.show_guide) return;
    float u = ImGui::GetFontSize();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(30 * u, 36 * u), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 30.5f * u, 2.0f * u), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(tr_id("Machine guide"), &ui.show_guide)) {
        ImGui::End();
        return;
    }
    if (ui.machine_index.machines.empty()) {
        std::string err;
        machine_index_load(ui.machine_index, err);
    }
    const std::string f = ui.guide_machine.empty() ? ui.host->machine_file() : ui.guide_machine;
    const MachineEntry* e = machine_index_find(ui.machine_index, f);
    std::string path = e ? pick(e->guide, e->guide_en) : std::string();
    static std::string s_path, s_text;
    if (path != s_path) {
        s_path = path;
        s_text.clear();
        if (!path.empty()) file_read_text(data_path(path), s_text);
    }
    ImGui::BeginChild("text", ImVec2(0, e && !e->guide.empty() ? -3.6f * u : 0));
    if (!e) ImGui::TextDisabled("%s", tr("No machine."));
    else if (s_text.empty()) {
        ImGui::TextWrapped("%s", entry_name(*e).c_str());
        ImGui::TextDisabled("%s", tr("This machine has no guide yet."));
    } else {
        markdown(ui, s_text);
    }
    ImGui::EndChild();
    if (e && !e->guide.empty()) {
        bool show = std::find(ui.guides_hidden.begin(), ui.guides_hidden.end(), f) == ui.guides_hidden.end();
        if (ImGui::Checkbox(tr("Show this guide when the machine starts"), &show)) {
            ui.guides_hidden.erase(std::remove(ui.guides_hidden.begin(), ui.guides_hidden.end(), f), ui.guides_hidden.end());
            if (!show) ui.guides_hidden.push_back(f);
        }
        ImGui::Checkbox(tr("Guides open by themselves"), &ui.guides_auto);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr("For the basic machines, the boards of Libor Lasota and the games and demos. "
                                       "Help -> Machine guide opens it any time."));
    }
    ImGui::End();
}
