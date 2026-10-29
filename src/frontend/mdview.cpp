// Markdown drawn in a window (markdown.h): the guides of the machines (browser.cpp) and Help -> First steps, Glossary
// (help.cpp). The text is laid out here word by word, so that **bold**, `code` and links sit inside a wrapped
// paragraph (ImGui wraps one font and one colour only); a link is an item of ImGui (mouse, Tab), a word too long for
// the line is cut at characters. Tables get columns no narrower than their longest word where there is room, and
// as wide as their text where that fits.
#include "frontend/markdown.h"
#include "frontend/ui.h"
#include "frontend/i18n.h"
#include "frontend/theme.h"

#include <float.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_internal.h"   // ItemAdd, ButtonBehavior: the links inside a wrapped text
#include "core/host.h"
#include "core/machine.h"
#include "core/util.h"

// ---------------------------------------------------------------- links

static bool web(const std::string& u)
{
    return u.compare(0, 7, "http://") == 0 || u.compare(0, 8, "https://") == 0 || u.compare(0, 7, "mailto:") == 0;
}

static void resolve_text(MdText& t, const std::string& base)
{
    for (MdSpan& s : t) {
        if (s.link.empty() || web(s.link)) {
            s.style &= (uint8_t)~MD_PATH;
            continue;
        }
        std::string p = s.link;
        size_t hash = p.find('#');   // a place in a file: the file (a place in this page: no link)
        if (hash != std::string::npos) p.erase(hash);
        for (char& c : p)
            if (c == '\\') c = '/';
        while (p.compare(0, 2, "./") == 0) p.erase(0, 2);
        std::string found;
        if (!p.empty() && !base.empty() && file_exists(data_path(base + p))) found = base + p;
        else if (!p.empty() && file_exists(data_path(p))) found = p;
        if (!found.empty()) s.link = found;
        else if ((s.style & MD_PATH) || p.empty()) s.link.clear();
        else s.link = base + p;   // as written: opening it says nothing when it is not there
        s.style &= (uint8_t)~MD_PATH;
    }
}

void md_resolve(std::vector<MdBlock>& doc, const std::string& file)
{
    size_t slash = file.find_last_of("/\\");
    std::string base = slash == std::string::npos ? std::string() : file.substr(0, slash + 1);
    for (char& c : base)
        if (c == '\\') c = '/';
    for (MdBlock& b : doc) {
        resolve_text(b.text, base);
        for (std::vector<MdText>& row : b.rows)
            for (MdText& cell : row) resolve_text(cell, base);
    }
}

static void open_link(UiState& ui, const std::string& link, const MdView& v)
{
    if (web(link)) {
        SDL_OpenURL(link.c_str());
        return;
    }
    if (v.own_link && v.own_link(ui, link)) return;
    open_doc(link.c_str());
}

// ---------------------------------------------------------------- wrapped text with marks

namespace {

struct Fonts {
    ImFont* reg;
    ImFont* bold;
    ImFont* mono;
    float size;
    float dy_bold, dy_mono;   // the baseline of the other fonts on the one of the text
};

// a piece of a span on one line, at (x, y) from the top left corner of the text
struct Frag {
    int span;
    const char* b;
    const char* e;
    float x, y, w;
};

ImFont* font_of(const Fonts& f, uint8_t style)
{
    return (style & MD_CODE) ? f.mono : (style & MD_BOLD) ? f.bold : f.reg;
}

float width_of(ImFont* font, float size, const char* b, const char* e)
{
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, b, e).x;
}

int utf8_len(unsigned char c)
{
    return c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
}

Fonts fonts(UiState& ui)
{
    Fonts f;
    f.reg = ImGui::GetFont();
    f.size = ImGui::GetFontSize();
    f.bold = theme_fonts().bold[theme_variant()];
    if (!f.bold) f.bold = f.reg;
    f.mono = ui.mono ? ui.mono : theme_fonts().mono;
    if (!f.mono) f.mono = f.reg;
    float asc = f.reg->GetFontBaked(f.size)->Ascent;
    f.dy_bold = asc - f.bold->GetFontBaked(f.size)->Ascent;
    f.dy_mono = asc - f.mono->GetFontBaked(f.size)->Ascent;
    return f;
}

// A space in a short `code` (`10 FOR I=1 TO 5`) does not break the line: the code stays one piece.
const size_t CODE_KEEP = 30;

// Lays the text out in `width` (FLT_MAX: one line): the fragments with their places (frags may be null: only the
// sizes); returns the height. widest: the widest line, longest: the longest word (the least width of a column).
float layout(const MdText& t, const Fonts& f, bool bold, float width, std::vector<Frag>* frags, float* widest = nullptr,
             float* longest = nullptr)
{
    if (frags) frags->clear();
    float x = 0, y = 0, lh = f.size, wmax = 0, lmax = 0;
    struct Piece {
        int span;
        const char* b;
        const char* e;
        float w;
    };
    Piece word[64];   // a word = pieces without a space between them (a bold word and its comma), placed together
    int nword = 0;
    float word_w = 0, space = 0;   // the word so far, the space before it
    auto newline = [&]() {
        if (x > wmax) wmax = x;
        x = 0;
        y += lh;
    };
    auto emit = [&](int span, const char* b, const char* e, float w) {
        if (frags) frags->push_back({ span, b, e, x, y, w });
        x += w;
    };
    auto place = [&]() {
        if (nword == 0) return;
        if (word_w > lmax) lmax = word_w;
        if (x > 0 && x + space + word_w > width) newline();
        else if (x > 0) x += space;
        if (word_w <= width - x) {
            for (int k = 0; k < nword; k++) emit(word[k].span, word[k].b, word[k].e, word[k].w);
        } else {   // longer than the line: cut at characters
            for (int k = 0; k < nword; k++) {
                ImFont* fo = font_of(f, (uint8_t)(t[(size_t)word[k].span].style | (bold ? MD_BOLD : 0)));
                const char* p = word[k].b;
                while (p < word[k].e) {
                    const char* q = p;
                    float w = 0;
                    while (q < word[k].e) {
                        int n = utf8_len((unsigned char)*q);
                        float cw = width_of(fo, f.size, q, q + n);
                        if (x + w + cw > width && (q > p || x > 0)) break;
                        w += cw;
                        q += n;
                    }
                    if (q == p) {
                        newline();
                        continue;
                    }
                    emit(word[k].span, p, q, w);
                    p = q;
                    if (p < word[k].e) newline();
                }
            }
        }
        nword = 0;
        word_w = 0;
        space = 0;
    };
    for (size_t si = 0; si < t.size(); si++) {
        const MdSpan& s = t[si];
        ImFont* fo = font_of(f, (uint8_t)(s.style | (bold ? MD_BOLD : 0)));
        bool keep = (s.style & MD_CODE) && s.text.size() <= CODE_KEEP;
        const char* p = s.text.c_str();
        const char* end = p + s.text.size();
        while (p < end) {
            if ((*p == ' ' || *p == '\t') && !keep) {
                place();
                space = width_of(fo, f.size, " ", nullptr);
                while (p < end && (*p == ' ' || *p == '\t')) p++;
                continue;
            }
            const char* q = p;
            while (q < end && (keep || (*q != ' ' && *q != '\t'))) q++;
            if (nword == (int)(sizeof(word) / sizeof(word[0]))) place();
            float w = width_of(fo, f.size, p, q);
            word[nword++] = { (int)si, p, q, w };
            word_w += w;
            p = q;
        }
    }
    place();
    if (x > wmax) wmax = x;
    if (widest) *widest = wmax;
    if (longest) *longest = lmax;
    return y + lh;
}

}   // namespace

// The text at `pos` in `width` (wrapped): drawn and its links made items (ids in the scope of the caller); returns
// its height. bold: all of it bold (the header of a table).
static float rich(UiState& ui, const MdText& t, ImVec2 pos, float width, const Fonts& f, bool bold, const MdView& v)
{
    static std::vector<Frag> frags;   // reused: a frame draws many texts
    float h = layout(t, f, bold, width, &frags);
    if (!ImGui::IsRectVisible(pos, ImVec2(pos.x + width, pos.y + h))) return h;
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int hot = -1;          // the span of the link under the mouse (or pressed): all its pieces lit
    bool held_hot = false;
    std::string open;      // a link clicked: opened after the drawing (it may change the document)
    for (size_t k = 0; k < frags.size(); k++) {
        const Frag& fr = frags[k];
        const MdSpan& s = t[(size_t)fr.span];
        if (s.link.empty()) continue;
        ImRect bb(pos.x + fr.x, pos.y + fr.y, pos.x + fr.x + fr.w, pos.y + fr.y + f.size);
        ImGuiID id = win->GetID((int)k + 1);
        if (!ImGui::ItemAdd(bb, id)) continue;
        bool hovered = false, held = false;
        if (ImGui::ButtonBehavior(bb, id, &hovered, &held)) open = s.link;
        ImGui::RenderNavCursor(bb, id);
        if (hovered || held) {
            hot = fr.span;
            held_hot = held;
        }
        if (hovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            tip(s.link.c_str());
        }
    }
    ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
    ImVec4 lc = ImGui::GetStyleColorVec4(ImGuiCol_TextLink), lc_hot = lc;
    {   // as ImGui::TextLink: lighter under the mouse, the line under it darker
        float hh, ss, vv;
        ImGui::ColorConvertRGBtoHSV(lc.x, lc.y, lc.z, hh, ss, vv);
        ImGui::ColorConvertHSVtoRGB(ImFmod(hh + 0.02f, 1.0f), ss, ImSaturate(vv + (held_hot ? 0.4f : 0.3f)), lc_hot.x,
                                    lc_hot.y, lc_hot.z);
    }
    ImU32 code_bg = ImGui::ColorConvertFloat4ToU32(theme().code_bg);
    float u = f.size;
    for (const Frag& fr : frags) {
        const MdSpan& s = t[(size_t)fr.span];
        uint8_t style = (uint8_t)(s.style | (bold ? MD_BOLD : 0));
        ImFont* fo = font_of(f, style);
        ImVec2 p(pos.x + fr.x, pos.y + fr.y);
        if (style & MD_CODE)
            dl->AddRectFilled(ImVec2(p.x - 0.12f * u, p.y), ImVec2(p.x + fr.w + 0.12f * u, p.y + f.size), code_bg,
                              0.2f * u);
        ImU32 col = text;
        if (!s.link.empty()) {
            ImVec4 c = fr.span == hot ? lc_hot : lc;
            col = ImGui::ColorConvertFloat4ToU32(c);
            float ly = p.y + f.size - 1.0f;
            ImU32 line = ImGui::ColorConvertFloat4ToU32(ImVec4(c.x * 0.8f, c.y * 0.8f, c.z * 0.8f, c.w));
            dl->AddLine(ImVec2(p.x, ly), ImVec2(p.x + fr.w, ly), line);
        }
        float dy = fo == f.mono ? f.dy_mono : fo == f.bold ? f.dy_bold : 0.0f;
        dl->AddText(fo, f.size, ImVec2(p.x, p.y + dy), col, fr.b, fr.e);
    }
    if (!open.empty()) open_link(ui, open, v);
    return h;
}

// A paragraph or an item at the cursor, `indent` from the left edge; mark: the width of the marker of an item in front
// of the text (drawn by the caller). Moves the cursor under it.
static void text_block(UiState& ui, const MdText& t, float indent, float mark, const Fonts& f, const MdView& v,
                       bool bold = false)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x - indent - mark;
    if (w < 4 * f.size) w = 4 * f.size;
    float h = rich(ui, t, ImVec2(p.x + indent + mark, p.y), w, f, bold, v);
    ImGui::Dummy(ImVec2(indent + mark + w, h));
}

// ---------------------------------------------------------------- blocks

static void code_block(UiState& ui, const std::vector<std::string>& lines, int n, float indent, const MdView& v)
{
    float u = ImGui::GetFontSize();
    if (indent > 0) ImGui::Indent(indent);
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
    char b[64];
    if (v.type_buttons) {   // the guides: the lines typed on the keyboard of SAPI
        std::string text;
        for (const std::string& l : lines) text += l + "\r";
        snprintf(b, sizeof(b), "%s##type%d", tr("Type into SAPI"), n);
        Machine* m = ui.host->machine();
        ImGui::BeginDisabled(!m);
        if (theme_accent_button(b, ImVec2(0, 0), true) && m) {   // the action of the block
            m->type_text(text);
            ui.focus_display = 2;   // the next step of the guide is typed on the screen
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) tip(tr("Types the lines on the keyboard of SAPI, each ended by Enter."));
        ImGui::SameLine();
    }
    snprintf(b, sizeof(b), "%s##copy%d", tr("Copy"), n);
    if (ImGui::SmallButton(b)) {
        std::string c;
        for (const std::string& l : lines) c += l + "\n";
        SDL_SetClipboardText(c.c_str());
    }
    if (indent > 0) ImGui::Unindent(indent);
}

// The widths of the columns of a table in `avail`: each at least its longest word (at most 45 % of the table) and at
// most its text in one line; the room left over goes to the columns in the proportion of their text.
static void column_widths(const MdBlock& b, const Fonts& f, float avail, float pad, std::vector<float>& w)
{
    size_t n = b.rows[0].size();
    std::vector<float> lo(n, 0.0f), hi(n, 0.0f);
    for (size_t r = 0; r < b.rows.size(); r++)
        for (size_t c = 0; c < n; c++) {
            float widest = 0, longest = 0;
            layout(b.rows[r][c], f, r == 0, FLT_MAX, nullptr, &widest, &longest);
            if (longest + 2 * pad > lo[c]) lo[c] = longest + 2 * pad;
            if (widest + 2 * pad > hi[c]) hi[c] = widest + 2 * pad;
        }
    float sum_lo = 0, sum_hi = 0;
    for (size_t c = 0; c < n; c++) {
        if (lo[c] > 0.45f * avail) lo[c] = 0.45f * avail;
        if (hi[c] < lo[c]) hi[c] = lo[c];
        sum_lo += lo[c];
        sum_hi += hi[c];
    }
    w.assign(n, 0.0f);
    for (size_t c = 0; c < n; c++) {
        if (sum_hi <= avail) w[c] = hi[c] + (avail - sum_hi) * (sum_hi > 0 ? hi[c] / sum_hi : 1.0f / (float)n);
        else if (sum_lo >= avail) w[c] = lo[c] * avail / sum_lo;
        else w[c] = lo[c] + (avail - sum_lo) * (hi[c] - lo[c]) / (sum_hi - sum_lo);
    }
}

static void table(UiState& ui, const MdBlock& b, float indent, const Fonts& f, const MdView& v)
{
    if (b.rows.empty() || b.rows[0].empty()) return;
    float u = f.size, pad = 0.4f * u, pady = 0.2f * u;
    ImVec2 o = ImGui::GetCursorScreenPos();
    o.x += indent;
    float avail = ImGui::GetContentRegionAvail().x - indent;
    std::vector<float> w;
    column_widths(b, f, avail, pad, w);
    size_t n = w.size();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 light = ImGui::GetColorU32(ImGuiCol_TableBorderLight);
    ImU32 strong = ImGui::GetColorU32(ImGuiCol_TableBorderStrong);
    float y = 0;
    for (size_t r = 0; r < b.rows.size(); r++) {
        float rh = 0;
        for (size_t c = 0; c < n; c++) {
            float hc = layout(b.rows[r][c], f, r == 0, w[c] - 2 * pad, nullptr);
            if (hc > rh) rh = hc;
        }
        rh += 2 * pady;
        ImVec2 r0(o.x, o.y + y), r1(o.x + avail, o.y + y + rh);
        if (ImGui::IsRectVisible(r0, r1)) {
            if (r == 0) dl->AddRectFilled(r0, r1, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
            else if (r % 2 == 0) dl->AddRectFilled(r0, r1, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
            if (r > 0) dl->AddLine(r0, ImVec2(r1.x, r0.y), r == 1 ? strong : light);   // over the background
            float x = 0;
            for (size_t c = 0; c < n; c++) {
                ImGui::PushID((int)(r * n + c));
                rich(ui, b.rows[r][c], ImVec2(o.x + x + pad, o.y + y + pady), w[c] - 2 * pad, f, r == 0, v);
                ImGui::PopID();
                x += w[c];
            }
        }
        y += rh;
    }
    float x = 0;   // the lines between the columns and around the table
    for (size_t c = 0; c + 1 < n; c++) {
        x += w[c];
        dl->AddLine(ImVec2(o.x + x, o.y), ImVec2(o.x + x, o.y + y), light);
    }
    dl->AddRect(o, ImVec2(o.x + avail, o.y + y), strong);
    ImGui::Dummy(ImVec2(indent + avail, y));
}

void md_draw(UiState& ui, const std::vector<MdBlock>& doc, const MdView& v)
{
    Fonts f = fonts(ui);
    float u = f.size;
    // the step of a list level: the marker of an item ("10." fits) and the gap after it
    float step = width_of(f.reg, u, "00.", nullptr) + 0.5f * u;
    bool first_section = true, open = true;
    for (size_t k = 0; k < doc.size(); k++) {
        const MdBlock& b = doc[k];
        if (b.kind == MD_HEADING && b.level == 2 && v.fold) {   // a section that folds
            std::string label = md_plain(b.text) + "##sec" + std::to_string(k);
            if (v.set_open) ImGui::SetNextItemOpen(v.set_open > 0);
            ImGui::PushFont(f.bold, 0.0f);
            bool at_first = first_section || v.all_open;
            open = ImGui::CollapsingHeader(label.c_str(), at_first ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            ImGui::PopFont();
            first_section = false;
            continue;
        }
        if (!open) continue;   // the blocks of a closed section
        ImGui::PushID((int)k);
        switch (b.kind) {
        case MD_HEADING:
            if (b.level == 1) {
                theme_heading(md_plain(b.text).c_str(), 20.0f);
            } else {
                ImGui::Spacing();
                theme_section(md_plain(b.text).c_str());
            }
            break;
        case MD_PARAGRAPH:
            text_block(ui, b.text, (float)b.level * step, 0.0f, f, v);
            break;
        case MD_ITEM: {
            float indent = (float)b.level * step;
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
            if (b.marker.empty())
                dl->AddCircleFilled(ImVec2(p.x + indent + 0.45f * u, p.y + 0.55f * u), 0.17f * u, col);
            else
                dl->AddText(f.reg, u, ImVec2(p.x + indent, p.y), col, b.marker.c_str());
            text_block(ui, b.text, indent, step, f, v);
            break;
        }
        case MD_CODEBLOCK:
            code_block(ui, b.lines, (int)k, (float)b.level * step, v);
            break;
        case MD_TABLE:
            table(ui, b, (float)b.level * step, f, v);
            break;
        case MD_RULE:
            ImGui::Separator();
            break;
        case MD_BLANK:
            ImGui::Spacing();
            break;
        }
        ImGui::PopID();
    }
}
