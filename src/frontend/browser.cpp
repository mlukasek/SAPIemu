// The window "Choose a machine": the machines in categories (core/machine_index.h) with a preview, a short text
// for newcomers and the technical description, Start; and the window "Guide": what to try in a machine, a small
// Markdown file (machines/guides/<id>.md, drawn by mdview.cpp) with code blocks that can be typed into SAPI.
// The guide opens itself when a machine of a category with guides starts, unless the user said no (settings).
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/theme.h"
#include "frontend/markdown.h"

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

void machine_index_refresh(UiState& ui)
{
    ui.machine_index_tried = true;
    std::string err;
    machine_index_load(ui.machine_index, err);
    if (!err.empty()) LOGD("%s", err.c_str());
}

std::string machine_title(UiState& ui, const std::string& file, const std::string& name)
{
    if (ui.machine_index.machines.empty() && !ui.machine_index_tried) machine_index_refresh(ui);
    const MachineEntry* e = file.empty() ? nullptr : machine_index_find(ui.machine_index, file);
    if (e && (name.empty() || name == e->name)) return entry_name(*e);
    return name.empty() ? file : name;
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

void recent_add(UiState& ui, const std::string& file)
{
    ui.recent.erase(std::remove(ui.recent.begin(), ui.recent.end(), file), ui.recent.end());
    ui.recent.insert(ui.recent.begin(), file);
    if (ui.recent.size() > 8) ui.recent.resize(8);
}

// Start (or restart: the running one is loaded again from its file) a machine; its guide may open itself (a load
// changes the machine generation, draw_guide sees it; a start waiting for the question about unsaved changes of
// the configurator does not open the guide of the old machine). The window closes when the start is done
// (browser_started), Cancel at the question keeps it open.
static void start(UiState& ui, const std::string& file)
{
    ui.browser_start = true;
    rack_start_machine(ui, file);
}

void browser_started(UiState& ui, bool ok)
{
    ui.show_browser = false;
    if (ok) {
        // the first machine chosen at the first start, no saved layout yet: the windows take the default places of
        // this machine (its main output, its instruments), not those of the machine running before it
        if (ui.browser_first && ui.layout_fresh) ui.layout_reset = true;
        ui.focus_display = 2;   // the keyboard goes to the display
    }
    ui.browser_first = false;
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

std::string machine_index_menus(UiState& ui)
{
    if (ui.machine_index.machines.empty() && !ui.machine_index_tried) machine_index_refresh(ui);
    const MachineIndex& idx = ui.machine_index;
    std::string chosen, open_group;
    bool group_open = false;
    for (const MachineCategory& cat : idx.categories) {
        if (cat.group != open_group) {   // leaving a group, coming to another one: the categories of a group in its menu
            if (group_open) ImGui::EndMenu();
            group_open = false;
            open_group = cat.group;
            if (!cat.group.empty()) group_open = ImGui::BeginMenu((pick(cat.group, cat.group_en) + "##g" + cat.group).c_str());
        }
        if (!cat.group.empty() && !group_open) continue;
        if (cat.machines.empty() || !ImGui::BeginMenu((pick(cat.name, cat.name_en) + "##c" + cat.id).c_str())) continue;
        for (size_t i = 0; i < cat.machines.size(); i++) {
            const MachineEntry& e = idx.machines[(size_t)cat.machines[i]];
            if (ImGui::MenuItem((item_name(cat, i, e) + "##" + e.file).c_str())) chosen = e.file;
            if (ImGui::IsItemHovered()) {   // which file it is, and what it is for a newcomer
                const std::string& sum = pick(e.summary, e.summary_en);
                tipf("%s%s%s", e.file.c_str(), sum.empty() ? "" : "\n\n", sum.c_str());
            }
        }
        ImGui::EndMenu();
    }
    if (group_open) ImGui::EndMenu();
    return chosen;
}

// A row of the list: the machine, its category ("recent" for Recent machines) and the title shown there.
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

static bool s_nav_on_row = false;   // the cursor of the keyboard navigation is on a machine row (this frame)

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
    if (ImGui::IsItemFocused()) s_nav_on_row = true;   // the keyboard navigation is on a machine: Enter starts it
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
        if (ImGui::IsItemHovered()) tip(text.c_str());
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

// My machines -> Delete: the copy asked about (its file, its title, its original), the question opens in the next frame.
static std::string s_delete_file, s_delete_title, s_delete_orig;
static bool s_delete_ask = false;

static bool is_copy(UiState& ui, const MachineEntry& e)
{
    const MachineIndex& idx = ui.machine_index;
    return e.category >= 0 && e.category < (int)idx.categories.size() && idx.categories[(size_t)e.category].id == MACHINE_MINE;
}

// A copy of My machines: "Modified from <the original>" (a click selects the original in the list).
static void copy_origin(UiState& ui, const MachineEntry& e)
{
    if (e.based_on.empty()) return;
    const MachineEntry* o = machine_index_find(ui.machine_index, e.based_on);
    ImGui::TextDisabled("%s", tr("Modified from"));
    ImGui::SameLine();
    if (!o) {
        ImGui::TextDisabled("%s", e.based_on.c_str());
        return;
    }
    std::string orig = entry_name(*o), file = o->file;   // the link may change the selection (and `e`)
    if (ImGui::TextLink((orig + "##orig").c_str())) {
        ui.browser_sel = file;
        select_first_row(ui, ui.machine_index);
    }
    if (ImGui::IsItemHovered())
        tipf("%s\n%s", file.c_str(), tr("The supplied machine as it came with SAPIemu: its category has it unchanged."));
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
    std::string name = pick(e.name, e.name_en);   // the long name of the file (in English from the index)
    if (e.copy) {   // a copy not renamed: the original's name with the mark in the UI language, as its title (the
                    // file has the mark of the language it was saved in)
        if (const MachineEntry* o = machine_index_find(ui.machine_index, e.based_on))
            name = pick(o->name, o->name_en) + machine_copy_name("", "", e.copy, !english());
    }
    if (entry_name(e) != name) text_dim_wrapped(name);
    bool copy = is_copy(ui, e);
    if (copy) copy_origin(ui, e);
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
    if (copy) {   // only this copy: the supplied machine stays in its category
        ImGui::SameLine();
        bool run = running(ui, e);
        ImGui::BeginDisabled(run);
        if (ImGui::Button(tr("Delete..."))) {
            s_delete_file = e.file;
            s_delete_title = entry_name(e);
            s_delete_orig = e.based_on;
            s_delete_ask = true;
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            tip(run ? tr("It is running: start another machine first.")
                    : tr("Deletes this copy of My machines. The supplied machine it was modified from stays."));
    }
    ImGui::Spacing();
    parts(e);
    if (!e.description.empty() && ImGui::CollapsingHeader(tr("Technical description"), sum.empty() ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        ImGui::TextWrapped("%s", e.description.c_str());
    ImGui::TextDisabled("%s", e.file.c_str());
}

// My machines -> Delete...: the question (Cancel is the default, Enter and Esc: deleting is not undone), then the copy
// goes and the original it was modified from is selected.
static void delete_question(UiState& ui)
{
    const char* title = tr_id("Delete the machine");
    if (s_delete_ask) {
        ImGui::OpenPopup(title);
        s_delete_ask = false;
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    float u = ImGui::GetFontSize();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 34.0f * u);
    ImGui::TextUnformatted(tr("Delete this machine of My machines?"));
    ImGui::Text("  %s", s_delete_title.c_str());
    ImGui::TextDisabled("  %s", s_delete_file.c_str());
    ImGui::TextUnformatted(tr("Its file is deleted, with the changes saved in it. The supplied machine it was modified "
                              "from stays as it is."));
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (ImGui::Button(tr("Delete"))) {
        std::string err;
        if (rack_delete_copy(ui, s_delete_file, err)) {
            ui_notice(ui, true, std::string(tr("Deleted")) + ": " + s_delete_file);
            // the original in its category, else the first row
            ui.browser_sel = machine_index_find(ui.machine_index, s_delete_orig) ? s_delete_orig : std::string();
            if (ui.browser_sel.empty() && !ui.machine_index.categories.empty() &&
                !ui.machine_index.categories[0].machines.empty())
                ui.browser_sel = ui.machine_index.machines[(size_t)ui.machine_index.categories[0].machines[0]].file;
            select_first_row(ui, ui.machine_index);
        } else {
            ui_notice(ui, false, err);
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    bool cancel = theme_accent_button(tr("Cancel"));
    ImGui::SetItemDefaultFocus();
    if (cancel || popup_enter() || popup_escape()) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
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
    // File -> Recent machines (started from the GUI, saved by Save as...), named the same here
    bool any_recent = false;
    for (const std::string& f : ui.recent)
        if (machine_index_find(idx, f)) any_recent = true;
    if (any_recent && !searching && ImGui::CollapsingHeader(tr("Recent machines"), ImGuiTreeNodeFlags_DefaultOpen)) {
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
            bool first_open = searching || cat.guides || cat.id == MACHINE_MINE;   // the user's own machines in sight
            open = ImGui::CollapsingHeader(cname.c_str(), first_open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
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
    // (the button does it with the mouse); at the first start nobody started it yet. The keyboard navigation on
    // another item (a category, a button, a link): Enter is that item's.
    bool nav_elsewhere = nav_cursor_on_item() && !s_nav_on_row;
    s_nav_on_row = false;
    if (ui.show_browser && !nav_elsewhere && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
        const MachineEntry* e = machine_index_find(idx, ui.browser_sel);
        if (searching && (!e || !machine_index_matches(*e, filter)) && first.e) {
            e = first.e;
            select_row(ui, *e, first.cat, first.title);
        }
        if (e && (!running(ui, *e) || ui.browser_first)) start(ui, e->file);
    }
    delete_question(ui);
    ImGui::End();
}

// ---------------------------------------------------------------- the guide

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
    static std::string s_path;
    static std::vector<MdBlock> s_doc;   // parsed when the guide changes
    if (path != s_path) {
        s_path = path;
        s_doc.clear();
        std::string text;
        if (!path.empty() && file_read_text(data_path(path), text)) {
            md_parse(text, s_doc);
            md_resolve(s_doc, path);   // the guides link "docs/..." from the data root
        }
    }
    // the text scrolls by itself above the two check boxes: room for them exactly (and a pixel), so that the window does
    // not scroll too (author 2026-10-29: two scroll bars, the inner one moving 2 pixels)
    float below = 2.0f * ImGui::GetFrameHeightWithSpacing() + 1.0f;
    ImGui::BeginChild("text", ImVec2(0, e && !e->guide.empty() ? -below : 0));
    if (!e) ImGui::TextDisabled("%s", tr("No machine."));
    else if (s_doc.empty()) {
        ImGui::TextWrapped("%s", entry_name(*e).c_str());
        ImGui::TextDisabled("%s", tr("This machine has no guide yet."));
    } else {
        MdView v;
        v.type_buttons = true;
        v.own_link = help_link;   // a user guide or a description of a board: in the window Help
        md_draw(ui, s_doc, v);
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
            tip(tr("For the machines of the first four groups: basics, instruments, boards of Libor "
                   "Lasota, games and demos. Help -> Machine guide opens it any time."));
    }
    ImGui::End();
}
