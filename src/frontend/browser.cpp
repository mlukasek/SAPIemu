// The window "Choose a machine": the machines in categories (core/machine_index.h) with a preview, a short text
// for newcomers and the technical description, Start; and the window "Guide": what to try in a machine, a small
// Markdown file (machines/guides/<id>.md) shown with headings, lists and code blocks that can be typed into SAPI.
// The guide opens itself when a machine of a category with guides starts, unless the user said no (settings).
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/theme.h"

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

// Start (or restart: the running one is loaded again from its file) a machine; its guide may open itself (a load
// changes the machine generation, draw_guide sees it; a start waiting for the question about unsaved changes of
// the configurator does not open the guide of the old machine). The keyboard goes to the display.
static void start(UiState& ui, const std::string& file)
{
    remember_recent(ui, file);
    // the first machine chosen at the first start, no saved layout yet: the windows take the default places of this
    // machine (its main output, its instruments), not those of the machine running before it
    if (ui.browser_first && ui.layout_fresh) ui.layout_reset = true;
    rack_start_machine(ui, file);
    ui.show_browser = false;
    ui.browser_first = false;
    ui.focus_display = 2;
}

static bool running(UiState& ui, const MachineEntry& e)
{
    return ui.host->machine() && e.file == ui.host->machine_file();   // switched off = not running
}

// The name of the i-th machine of a category in the list: its title in this category, else its own title.
static std::string item_name(const MachineCategory& cat, size_t i, const MachineEntry& e)
{
    if (i < cat.titles.size() && i < cat.titles_en.size()) {
        const std::string& t = pick(cat.titles[i], cat.titles_en[i]);
        if (!t.empty()) return t;
    }
    return entry_name(e);
}

// A row of the list: the machine, its category ("recent" for Recently started) and the title shown there.
struct BrowserRow {
    const MachineEntry* e = nullptr;
    std::string cat, title;
};

static void select_row(UiState& ui, const MachineEntry& e, const std::string& cat, const std::string& name)
{
    ui.browser_sel = e.file;
    ui.browser_sel_cat = cat;
    ui.browser_sel_title = name != entry_name(e) ? name : std::string();
}

// first: the first row the list shows (Enter while the search hides the selected one)
static void machine_item(UiState& ui, const MachineEntry& e, const std::string& name, const std::string& cat,
                         BrowserRow* first)
{
    if (!first->e) *first = { &e, cat, name };
    bool sel = ui.browser_sel == e.file && ui.browser_sel_cat == cat;   // one row lit, not every row of the machine
    // the running one in the colour "ok" and with the word (not a mark: arrows belong to the headers that fold);
    // the id without the name: the same item when the machine starts or the language changes
    bool run = running(ui, e);
    std::string label = name;
    if (run) label += std::string(" ") + tr("(running)");
    if (rack_has_kept(ui, e.file)) label += " *";   // unsaved changes kept from the configurator, as its title marks them
    label += "###" + e.file + "#" + cat;
    if (run) ImGui::PushStyleColor(ImGuiCol_Text, theme().ok);
    bool clicked = ImGui::Selectable(label.c_str(), sel, ImGuiSelectableFlags_AllowDoubleClick);
    if (sel) theme_selected_bar();
    if (run) ImGui::PopStyleColor();
    if (clicked) {
        select_row(ui, e, cat, name);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) start(ui, e.file);
    }
}

// The row selected when the window opens: the first category that has the machine.
static void select_first_row(UiState& ui, const MachineIndex& idx)
{
    ui.browser_sel_cat.clear();
    ui.browser_sel_title.clear();
    for (const MachineCategory& cat : idx.categories)
        for (size_t i = 0; i < cat.machines.size(); i++) {
            const MachineEntry& e = idx.machines[(size_t)cat.machines[i]];
            if (e.file != ui.browser_sel) continue;
            select_row(ui, e, cat.id, item_name(cat, i, e));
            return;
        }
}

static void text_dim_wrapped(const std::string& s)
{
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", s.c_str());
    ImGui::PopTextWrapPos();
}

// The text of a category: wrapped under its open header (not among the results of a search, they stay compact),
// a tooltip over the closed one. Right after the header.
static void category_text(const MachineCategory& cat, bool open, bool searching)
{
    const std::string& text = pick(cat.text, cat.text_en);
    if (text.empty()) return;
    if (!open) {
        if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos(30.0f * ImGui::GetFontSize());
            ImGui::TextUnformatted(text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        return;
    }
    if (searching) return;
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", text.c_str());
    ImGui::PopTextWrapPos();
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
            // in a frame of the colour Border, as thin as the borders of the windows (1 px at 100 %), with a dark
            // margin like a monitor: the text of a screen does not touch the frame
            float b = std::max(1.0f, ImGui::GetStyle().ChildBorderSize), m = 0.3f * u;
            float w = std::min(avail - 2.0f * (b + m), 35.0f * u), h = w * p->h / (float)p->w;
            if (h > 22.0f * u) {   // a tall picture (the sheet of a plotter) keeps the text in sight
                w *= 22.0f * u / h;
                h = 22.0f * u;
            }
            if (w > 1.0f) {
                ImVec2 o = ImGui::GetCursorScreenPos(), end(o.x + w + 2 * (b + m), o.y + h + 2 * (b + m));
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(o, end, IM_COL32(16, 16, 16, 255));   // the background of the displays
                dl->AddRect(o, end, ImGui::GetColorU32(ImGuiCol_Border), 0.0f, 0, b);
                ImGui::SetCursorScreenPos(ImVec2(o.x + b + m, o.y + b + m));
                ImGui::Image(ImTextureRef((ImTextureID)(intptr_t)p->tex), ImVec2(w, h));
                ImGui::SetCursorScreenPos(o);
                ImGui::Dummy(ImVec2(end.x - o.x, end.y - o.y));
            }
        }
    } else {
        ImGui::TextDisabled("%s", tr("No preview"));
    }
    ImGui::Spacing();
    // the title of the row clicked (Manic Miner among the games), the machine's own title under it
    std::string title = ui.browser_sel_title.empty() || ui.browser_sel != e.file ? entry_name(e) : ui.browser_sel_title;
    theme_heading(title.c_str());
    if (title != entry_name(e)) text_dim_wrapped(entry_name(e));
    if (entry_name(e) != e.name) text_dim_wrapped(e.name);
    const std::string& sum = pick(e.summary, e.summary_en);
    if (!sum.empty()) ImGui::TextWrapped("%s", sum.c_str());
    if (rack_has_kept(ui, e.file)) {   // the preview and the cards below show the file, the start does not
        ImGui::PushStyleColor(ImGuiCol_Text, theme().warn);
        ImGui::TextWrapped("* %s", tr("A start uses the changes kept from the configurator, not saved in its file. "
                                      "Open in the configurator, then Revert forgets them, Save writes them."));
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    // the running machine: Restart (the mouse only, Enter does not throw its memory away); at the first start the
    // machine behind the window was started by nobody, Start
    bool restart = running(ui, e) && !ui.browser_first;
    if (theme_accent_button(restart ? tr("Restart") : tr("Start"))) start(ui, e.file);
    ImGui::SameLine();
    if (!e.guide.empty()) {
        if (ImGui::Button(tr("Guide"))) {
            ui.show_guide = true;
            ui.guide_focus = true;
            ui.guide_machine = e.file;
        }
        ImGui::SameLine();
    }
    if (ImGui::Button(tr("Open in the configurator"))) rack_open_in_editor(ui, e.file);   // asks about unsaved changes
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
        // a machine the release leaves out (package.py REPO_ONLY) is skipped; test_machine_index
        // checks the index in the repository
        if (!err.empty()) LOGD("%s", err.c_str());
        ui.browser_loaded = true;
        if (ui.browser_sel.empty()) ui.browser_sel = ui.host->machine_file();
        if (!machine_index_find(ui.machine_index, ui.browser_sel) && !ui.machine_index.machines.empty() &&
            !ui.machine_index.categories.empty() && !ui.machine_index.categories[0].machines.empty())
            ui.browser_sel = ui.machine_index.machines[(size_t)ui.machine_index.categories[0].machines[0]].file;
        select_first_row(ui, ui.machine_index);
    }
    float u = ImGui::GetFontSize();
    layout_window(LW_BROWSER);   // in the middle, never over the menu bar
    bool visible = ImGui::Begin(tr_id("Choose a machine"), &ui.show_browser);
    if (!ui.show_browser) ui.focus_display = 2;   // closed: the keyboard back to the display
    if (!ui.show_browser && ui.browser_first) {   // the first start: closed without starting a machine from it
        ui.browser_first = false;
        // the guide of the running machine (ZPS-1) may open itself, once (draw_guide decides); a guide the user
        // opened from the window stays as it is
        if (!ui.show_guide) {
            ui.guide_gen = 0;
            ui.guide_start_gen = ui.host->machine_generation();
        }
    }
    if (!visible) {
        ImGui::End();
        return;
    }
    const MachineIndex& idx = ui.machine_index;
    // what SAPI-1 was and where to start, over the whole width
    const std::string& intro = pick(idx.intro, idx.intro_en);
    if (!intro.empty()) {
        ImGui::TextWrapped("%s", intro.c_str());
        ImGui::Spacing();
    }
    static char filter[64] = "";
    BrowserRow first;   // the first row the list shows
    ImGui::BeginChild("list", ImVec2(24 * u, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", tr("Search"), filter, sizeof(filter));
    bool searching = filter[0] != 0;
    // recently started ones
    bool any_recent = false;
    for (const std::string& f : ui.recent)
        if (machine_index_find(idx, f)) any_recent = true;
    if (any_recent && !searching && ImGui::CollapsingHeader(tr("Recently started"), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const std::string& f : std::vector<std::string>(ui.recent))   // a copy: a start reorders the list
            if (const MachineEntry* e = machine_index_find(idx, f)) machine_item(ui, *e, entry_name(*e), "recent", &first);
    }
    // the categories; those of one group under one header. While searching the headers have their own ids, open
    // at first: the results are in sight and the headers folded before stay as they were.
    const char* mode = searching ? "#search" : "";
    std::string open_group;
    bool group_open = false;
    for (size_t c = 0; c < idx.categories.size(); c++) {
        const MachineCategory& cat = idx.categories[c];
        bool any = false;
        for (int k : cat.machines)
            if (machine_index_matches(idx.machines[(size_t)k], filter)) any = true;
        if (!any) continue;
        std::string cname = pick(cat.name, cat.name_en) + "##cat" + cat.id + mode;
        bool open;
        if (cat.group.empty()) {
            open = ImGui::CollapsingHeader(cname.c_str(), searching || cat.guides ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            category_text(cat, open, searching);
        } else {
            if (cat.group != open_group) {   // a new group header
                open_group = cat.group;
                std::string g = pick(cat.group, cat.group_en) + "##group" + cat.group + mode;
                group_open = ImGui::CollapsingHeader(g.c_str(), searching ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            }
            if (!group_open) continue;
            open = ImGui::TreeNodeEx(cname.c_str(), searching ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            category_text(cat, open, searching);
        }
        if (!open) continue;
        for (size_t i = 0; i < cat.machines.size(); i++) {   // one machine may be in two categories: ids by category
            const MachineEntry& e = idx.machines[(size_t)cat.machines[i]];
            if (machine_index_matches(e, filter)) machine_item(ui, e, item_name(cat, i, e), cat.id, &first);
        }
        if (!cat.group.empty()) ImGui::TreePop();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("details", ImVec2(0, 0), ImGuiChildFlags_None);
    if (const MachineEntry* e = machine_index_find(idx, ui.browser_sel)) details(ui, *e);
    else ImGui::TextDisabled("%s", tr("Choose a machine on the left."));
    ImGui::EndChild();
    // Enter = Start while the window has the keyboard; when the search hides the selected machine, the first one
    // the list shows is selected and started. Not Restart: one key would throw away the memory of the running machine
    // (the button does it with the mouse); at the first start nobody started it yet.
    if (ui.show_browser && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
        const MachineEntry* e = machine_index_find(idx, ui.browser_sel);
        if (searching && (!e || !machine_index_matches(*e, filter)) && first.e) {
            e = first.e;
            select_row(ui, *e, first.cat, first.title);
        }
        if (e && (!running(ui, *e) || ui.browser_first)) start(ui, e->file);
    }
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
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme().code_bg);
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
    if (theme_accent_button(b, ImVec2(0, 0), true) && m) {   // the action of the block
        m->type_text(text);
        ui.focus_display = 2;   // the next step of the guide is typed on the screen
    }
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
            theme_heading(plain(body.substr(2), nullptr).c_str(), 20.0f);
        } else if (body.rfind("## ", 0) == 0 || body.rfind("### ", 0) == 0) {
            ImGui::Spacing();
            theme_section(plain(body.substr(body.find(' ') + 1), nullptr).c_str());
        } else if (body.rfind("- ", 0) == 0 || body.rfind("* ", 0) == 0) {
            ImGui::Bullet();
            ImGui::TextWrapped("%s", plain(body.substr(2), &links).c_str());
            bullet = ImGui::GetTreeNodeToLabelSpacing();
        } else {
            ImGui::TextWrapped("%s", plain(body, &links).c_str());
        }
        // the links under the text (aligned with it) in the colour of links, wrapped to the width of the window
        if (bullet > 0 && !links.empty()) ImGui::Indent(bullet);
        for (size_t k = 0; k < links.size(); k++) {
            char b[300];
            snprintf(b, sizeof(b), "%s \xE2\x86\x97##link%zu_%zu", links[k].first.c_str(), i, k);
            float w = ImGui::CalcTextSize(b, nullptr, true).x;
            if (k > 0) {
                ImGui::SameLine(0.0f, ImGui::GetFontSize());
                if (ImGui::GetContentRegionAvail().x < w) ImGui::NewLine();
            }
            if (ImGui::TextLink(b)) open_link(links[k].second);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", links[k].second.c_str());
        }
        if (bullet > 0 && !links.empty()) ImGui::Unindent(bullet);
        if (indent > 0) ImGui::Unindent(indent);
    }
}

void draw_guide(UiState& ui)
{
    // a machine the user started (Start, Restart, File -> Recent machines, Start a machine from a file): its guide
    // opens itself (categories with guides), unless the user said no; not after a power cycle, a restart from the
    // configurator, a state or MCP (decision 2026-10-17). Not while the window Choose a machine is open: the machine
    // the user starts from it opens its guide.
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
            if (gen == ui.guide_start_gen && e && !e->guide.empty() && e->category >= 0 &&
                ui.machine_index.categories[(size_t)e->category].guides && ui.guides_auto && !hidden) {
                ui.show_guide = true;
                ui.guide_machine = f;
            } else if (ui.show_guide) {
                ui.guide_machine = f;   // an open guide follows the machine
            }
        }
    }
    if (!ui.show_guide) return;
    float u = ImGui::GetFontSize();
    layout_window(LW_GUIDE);   // beside the instruments and Media, not over them
    if (ui.guide_focus) {   // asked for: in front of the windows that hide it (an instrument at the same place)
        ImGui::SetNextWindowFocus();
        ui.guide_focus = false;
    }
    bool visible = ImGui::Begin(tr_id("Machine guide"), &ui.show_guide);
    if (!ui.show_guide) ui.focus_display = 2;   // closed: the keyboard back to the display
    if (!visible) {
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
            ImGui::SetTooltip("%s", tr("For the machines of the first four groups: basics, instruments, boards of Libor "
                                       "Lasota, games and demos. Help -> Machine guide opens it any time."));
    }
    ImGui::End();
}
