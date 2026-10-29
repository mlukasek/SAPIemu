// Markdown of the guides and the help in the program: the parser (markdown.h). The blocks are drawn by mdview.cpp.
#include "frontend/markdown.h"

#include <string.h>

// ---------------------------------------------------------------- inline

static bool is_alnum(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static void add_span(MdText& out, const std::string& text, uint8_t style, const std::string& link)
{
    if (text.empty()) return;
    if (!out.empty() && out.back().style == style && out.back().link == link && !(style & MD_CODE)) {
        out.back().text += text;   // one span per run of the same look (the code spans stay apart)
        return;
    }
    MdSpan s;
    s.text = text;
    s.style = style;
    s.link = link;
    out.push_back(s);
}

// `code` naming a .md file (docs/navod-kzd1.md, docs\desky\APS-5.md, GETTING-STARTED.md): the path with '/'
static bool md_file(const std::string& code, std::string& path)
{
    size_t n = code.size();
    if (n < 4 || code.find(' ') != std::string::npos) return false;
    const char* e = code.c_str() + n - 3;
    if (!(e[0] == '.' && (e[1] == 'm' || e[1] == 'M') && (e[2] == 'd' || e[2] == 'D'))) return false;
    path = code;
    for (char& c : path)
        if (c == '\\') c = '/';
    return true;
}

// The length of a bare address at s[i] (http:// or https://), 0 = none. It ends before a space and without the
// punctuation after it ("...issues)." -> "...issues"; a ')' stays when the address has its '(').
static size_t bare_url(const std::string& s, size_t i, size_t end)
{
    if (s.compare(i, 7, "http://") != 0 && s.compare(i, 8, "https://") != 0) return 0;
    size_t j = i;
    while (j < end && s[j] != ' ' && s[j] != '\t' && s[j] != '<' && s[j] != '>' && s[j] != '"' && s[j] != '`') j++;
    while (j > i) {
        char c = s[j - 1];
        if (c == ')') {
            size_t open = 0, close = 0;
            for (size_t k = i; k < j; k++) open += s[k] == '(', close += s[k] == ')';
            if (close <= open) break;
        } else if (!strchr(".,;:!?'*", c)) {
            break;
        }
        j--;
    }
    return j - i;
}

// s[a, b) with the style and link given (a link's text, a bold run) into out
static void inline_run(const std::string& s, size_t a, size_t b, uint8_t style, const std::string& link, MdText& out)
{
    std::string cur;
    for (size_t i = a; i < b; i++) {
        char c = s[i];
        if (c == '\\' && i + 1 < b && strchr("\\`*_[](){}#+-.!|<>~", s[i + 1])) {
            cur += s[++i];
            continue;
        }
        if (c == '`') {   // a code span: the same number of backticks closes it
            size_t n = 0;
            while (i + n < b && s[i + n] == '`') n++;
            size_t close = i + n;
            while (close < b) {
                close = s.find(std::string(n, '`'), close);
                if (close == std::string::npos || close >= b) {
                    close = b;
                    break;
                }
                size_t m = 0;
                while (close + m < b && s[close + m] == '`') m++;
                if (m == n) break;
                close += m;
            }
            if (close < b) {
                add_span(out, cur, style, link);
                cur.clear();
                std::string code = s.substr(i + n, close - i - n);
                if (code.size() >= 2 && code.front() == ' ' && code.back() == ' ')
                    code = code.substr(1, code.size() - 2);
                std::string path;
                if (link.empty() && md_file(code, path))
                    add_span(out, code, (uint8_t)(style | MD_CODE | MD_PATH), path);
                else add_span(out, code, (uint8_t)(style | MD_CODE), link);
                i = close + n - 1;
                continue;
            }
            cur.append(n, '`');
            i += n - 1;
            continue;
        }
        if (c == '*' && i + 1 < b && s[i + 1] == '*') {   // **bold**
            size_t close = s.find("**", i + 2);
            if (close != std::string::npos && close + 2 <= b && close > i + 2) {
                add_span(out, cur, style, link);
                cur.clear();
                inline_run(s, i + 2, close, (uint8_t)(style | MD_BOLD), link, out);
                i = close + 1;
                continue;
            }
        }
        // [text](link), ![text](picture)
        if ((c == '[' || (c == '!' && i + 1 < b && s[i + 1] == '[')) && link.empty()) {
            size_t t0 = c == '!' ? i + 2 : i + 1;
            size_t mid = s.find("](", t0);
            size_t end = mid == std::string::npos ? mid : s.find(')', mid + 2);
            if (end != std::string::npos && end < b && s.find('\n', t0) > end) {
                add_span(out, cur, style, link);
                cur.clear();
                std::string url = s.substr(mid + 2, end - mid - 2);
                size_t sp = url.find(' ');   // [text](link "title")
                if (sp != std::string::npos) url.erase(sp);
                if (mid == t0) add_span(out, url, style, url);   // no text: the address itself
                else inline_run(s, t0, mid, style, url, out);
                i = end;
                continue;
            }
        }
        if ((c == 'h' || c == 'H') && link.empty() && (i == a || !is_alnum(s[i - 1]))) {
            size_t n = bare_url(s, i, b);
            if (n > 0) {
                add_span(out, cur, style, link);
                cur.clear();
                std::string url = s.substr(i, n);
                add_span(out, url, style, url);
                i += n - 1;
                continue;
            }
        }
        cur += c;
    }
    add_span(out, cur, style, link);
}

void md_inline(const std::string& s, MdText& out)
{
    inline_run(s, 0, s.size(), 0, std::string(), out);
}

std::string md_plain(const MdText& text)
{
    std::string s;
    for (const MdSpan& p : text) s += p.text;
    return s;
}

// ---------------------------------------------------------------- blocks

static size_t indent_of(const std::string& l)
{
    size_t n = 0;
    while (n < l.size() && (l[n] == ' ' || l[n] == '\t')) n++;
    return n;
}

static bool fence(const std::string& body)
{
    return body.compare(0, 3, "```") == 0 || body.compare(0, 3, "~~~") == 0;
}

// # Heading: its level, 0 = none
static int heading(const std::string& body)
{
    int n = 0;
    while (n < (int)body.size() && body[(size_t)n] == '#') n++;
    return n >= 1 && n <= 6 && (size_t)n < body.size() && body[(size_t)n] == ' ' ? n : 0;
}

// ---, ***, ___ (spaces allowed): a horizontal rule
static bool rule(const std::string& body)
{
    if (body.empty() || !strchr("-*_", body[0])) return false;
    int n = 0;
    for (char c : body) {
        if (c == body[0]) n++;
        else if (c != ' ') return false;
    }
    return n >= 3;
}

// "- ", "* ", "+ ", "12. ", "3) ": the length of the marker with its space, 0 = none; numbered: marker = "12."
static size_t item(const std::string& body, std::string& marker)
{
    if (body.size() >= 2 && strchr("-*+", body[0]) && body[1] == ' ') {
        marker.clear();
        return 2;
    }
    size_t d = 0;
    while (d < body.size() && d < 9 && body[d] >= '0' && body[d] <= '9') d++;
    if (d > 0 && d + 1 < body.size() && (body[d] == '.' || body[d] == ')') && body[d + 1] == ' ') {
        marker = body.substr(0, d) + ".";
        return d + 2;
    }
    return 0;
}

static bool table_row(const std::string& body)
{
    return !body.empty() && body[0] == '|';
}

// |---|:--:| : the line under the header of a table
static bool table_rule(const std::string& body)
{
    if (!table_row(body)) return false;
    bool dash = false;
    for (char c : body) {
        if (c == '-') dash = true;
        else if (c != '|' && c != ':' && c != ' ' && c != '\t') return false;
    }
    return dash;
}

// The cells of a table row: split at '|' outside `code` and not escaped (\| stays for md_inline), trimmed.
static void row_cells(const std::string& body, std::vector<std::string>& cells)
{
    cells.clear();
    std::string b = body;
    while (!b.empty() && (b.back() == ' ' || b.back() == '\t')) b.pop_back();
    if (!b.empty() && b[0] == '|') b.erase(0, 1);
    if (!b.empty() && b.back() == '|' && !(b.size() >= 2 && b[b.size() - 2] == '\\')) b.pop_back();
    std::string cur;
    bool code = false;
    for (size_t i = 0; i < b.size(); i++) {
        char c = b[i];
        if (c == '\\' && i + 1 < b.size()) {
            cur += c;
            cur += b[++i];
            continue;
        }
        if (c == '`') code = !code;
        if (c == '|' && !code) {
            cells.push_back(cur);
            cur.clear();
            continue;
        }
        cur += c;
    }
    cells.push_back(cur);
    for (std::string& s : cells) {
        size_t p = s.find_first_not_of(" \t");
        size_t q = s.find_last_not_of(" \t");
        s = p == std::string::npos ? std::string() : s.substr(p, q - p + 1);
    }
}

namespace {
struct ListLevel {
    size_t marker_col;   // where its marker is
    size_t text_col;     // where its text starts
};
}

// The list level of a block indented by `ind` (not an item): the items whose text it is indented under (a little
// less is enough: two spaces under "1. "); not indented: out of the lists.
static int text_level(std::vector<ListLevel>& lists, size_t ind)
{
    if (ind == 0) lists.clear();
    while (!lists.empty() && lists.back().text_col > ind + 1) lists.pop_back();
    return (int)lists.size();
}

void md_parse(const std::string& text, std::vector<MdBlock>& out)
{
    out.clear();
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
    std::vector<std::string> raw;   // the text of the blocks before md_inline (joined lines)
    std::vector<ListLevel> lists;
    bool joinable = false;          // the last block is a paragraph or an item a text line may go on
    auto push = [&](MdKind kind, int level, const std::string& r) {
        MdBlock b;
        b.kind = kind;
        b.level = level;
        out.push_back(b);
        raw.push_back(r);
    };
    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& l = lines[i];
        size_t ind = indent_of(l);
        std::string body = l.substr(ind);
        if (body.empty()) {
            if (!out.empty() && out.back().kind != MD_BLANK) push(MD_BLANK, 0, std::string());
            joinable = false;
            continue;
        }
        if (body.compare(0, 4, "<!--") == 0) {   // an HTML comment: left out
            while (i < lines.size() && lines[i].find("-->") == std::string::npos) i++;
            joinable = false;
            continue;
        }
        if (fence(body)) {
            push(MD_CODEBLOCK, text_level(lists, ind), std::string());
            std::string close = body.substr(0, 3);
            for (i++; i < lines.size(); i++) {
                const std::string& c = lines[i];
                size_t ci = indent_of(c);
                if (c.compare(ci, 3, close) == 0) break;
                out.back().lines.push_back(c.substr(ci < ind ? ci : ind));
            }
            joinable = false;
            continue;
        }
        if (int h = ind < 4 ? heading(body) : 0) {
            std::string t = body.substr((size_t)h + 1);
            while (!t.empty() && (t.back() == '#' || t.back() == ' ')) t.pop_back();   // ## closing hashes ##
            push(MD_HEADING, h, t);
            lists.clear();
            joinable = false;
            continue;
        }
        if (table_row(body) && i + 1 < lines.size() && table_rule(lines[i + 1].substr(indent_of(lines[i + 1])))) {
            push(MD_TABLE, text_level(lists, ind), std::string());
            MdBlock& t = out.back();
            std::vector<std::string> cells;
            row_cells(body, cells);
            size_t ncol = cells.size();
            size_t r = i;
            for (; r < lines.size(); r++) {
                std::string rb = lines[r].substr(indent_of(lines[r]));
                if (!table_row(rb)) break;
                if (r == i + 1) continue;   // the line under the header
                row_cells(rb, cells);
                cells.resize(ncol);
                std::vector<MdText> row(ncol);
                for (size_t k = 0; k < ncol; k++) md_inline(cells[k], row[k]);
                t.rows.push_back(row);
            }
            i = r - 1;
            joinable = false;
            continue;
        }
        if (ind < 4 && rule(body)) {
            push(MD_RULE, 0, std::string());
            lists.clear();
            joinable = false;
            continue;
        }
        std::string marker;
        if (size_t m = item(body, marker)) {
            while (!lists.empty() && lists.back().marker_col >= ind) lists.pop_back();
            push(MD_ITEM, (int)lists.size(), body.substr(m));
            out.back().marker = marker;
            lists.push_back({ ind, ind + m });
            joinable = true;
            continue;
        }
        if (joinable) {   // goes on with the text above
            raw.back() += " " + body;
            continue;
        }
        push(MD_PARAGRAPH, text_level(lists, ind), body);
        joinable = true;
    }
    while (!out.empty() && out.back().kind == MD_BLANK) {
        out.pop_back();
        raw.pop_back();
    }
    for (size_t k = 0; k < out.size(); k++)
        if (out[k].kind == MD_HEADING || out[k].kind == MD_PARAGRAPH || out[k].kind == MD_ITEM)
            md_inline(raw[k], out[k].text);
}
