// The translation table against the sources (src/ read at run time):
// - missing: every literal key of tr(), tr_id(), tr_lang() and the functions that translate their argument
//   (k_key_calls) in src/frontend has an entry in frontend/i18n.cpp (also when the Czech text is the same: the
//   entry says it was looked at);
// - unused: every entry is a literal somewhere in src/ (keys also come from the core: the function names of the
//   decoders "BOOT off", the labels of the APS-5 demo, the motion of KZD-1...), else it is dead and goes away.
// Keys passed through a variable are not checked for an entry (help.cpp's tables, the core's texts).
// The sources are read as C++: comments, character literals and raw strings handled, adjacent literals joined,
// escapes decoded. A short list of exceptions with their reasons is below.
#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#include "frontend/i18n.h"
#include "test_util.h"

#ifndef SAPIEMU_SRC_DIR
#error "SAPIEMU_SRC_DIR (the src folder of the repository) comes from tests/CMakeLists.txt"
#endif

// functions whose literal argument is a key: tx is help.cpp's ReportText (tr, or the English text for the report),
// begin_menu translates the title of its menu (app.cpp)
static const char* const k_key_calls[] = { "tr", "tr_id", "tr_lang", "tx", "begin_menu" };

// Entries no source has as a literal, but still used, and keys of tr() without an entry on purpose:
// { key, why }. Keep it short; nullptr ends it.
static const struct { const char* key; const char* why; } k_exceptions[] = {
    { nullptr, nullptr },
};

static bool excepted(const std::string& key)
{
    for (int i = 0; k_exceptions[i].key; i++)
        if (key == k_exceptions[i].key) return true;
    return false;
}

struct Lit {
    std::string text;   // decoded, adjacent literals joined
    int line;
    std::string call;   // the function of the innermost parenthesis around it ("" = none)
};

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// The body of an ordinary literal (between the quotes) with its escapes decoded.
static std::string unescape(const char* p, const char* end)
{
    std::string out;
    while (p < end) {
        if (*p != '\\' || p + 1 >= end) {
            out += *p++;
            continue;
        }
        char c = p[1];
        p += 2;
        switch (c) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
            int v = c - '0';
            for (int k = 0; k < 2 && p < end && *p >= '0' && *p <= '7'; k++) v = v * 8 + (*p++ - '0');
            out += (char)v;
            break;
        }
        case 'x': {
            int v = 0;
            while (p < end && hexval(*p) >= 0) v = v * 16 + hexval(*p++);
            out += (char)v;
            break;
        }
        default: out += c; break;   // \" \\ \' \?
        }
    }
    return out;
}

static bool ident_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

// The string literals of a C++ source.
static void scan(const std::string& src, std::vector<Lit>& out)
{
    const char* s = src.c_str();
    size_t n = src.size(), i = 0;
    int line = 1;
    std::vector<std::string> calls;   // per open parenthesis: the identifier before it
    std::string last_id;              // the identifier just before (a call when "(" follows)
    bool joining = false;             // the last token was a literal: the next one is joined to it
    auto literal = [&](const std::string& text, int at) {
        if (joining) out.back().text += text;
        else out.push_back({ text, at, calls.empty() ? std::string() : calls.back() });
        joining = true;
        last_id.clear();
    };
    while (i < n) {
        char c = s[i];
        if (c == '\n') { line++; i++; continue; }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') { i++; continue; }
        if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            while (i < n && s[i] != '\n') i++;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) line += s[i++] == '\n';
            i += 2;
            continue;
        }
        if (c == '"') {
            size_t j = i + 1;
            while (j < n && s[j] != '"' && s[j] != '\n') j += s[j] == '\\' ? 2 : 1;
            literal(unescape(s + i + 1, s + (j < n ? j : n)), line);
            i = j + 1;
            continue;
        }
        if (c == '\'') {   // a character literal ('"' is no string)
            size_t j = i + 1;
            while (j < n && s[j] != '\'' && s[j] != '\n') j += s[j] == '\\' ? 2 : 1;
            i = j + 1;
            joining = false;
            last_id.clear();
            continue;
        }
        if (ident_char(c)) {   // an identifier or a number (1'000, 0x1F, 1.5e3 in one piece)
            size_t j = i;
            bool number = c >= '0' && c <= '9';
            while (j < n && (ident_char(s[j]) || (number && (s[j] == '.' || s[j] == '\'')))) j++;
            std::string id(s + i, j - i);
            i = j;
            bool raw = j < n && s[j] == '"' && (id == "R" || id == "u8R" || id == "LR" || id == "uR" || id == "UR");
            if (raw) {   // R"delim( ... )delim"
                size_t open = src.find('(', j);
                if (open == std::string::npos) break;
                std::string close = ")" + src.substr(j + 1, open - j - 1) + "\"";
                size_t end = src.find(close, open + 1);
                if (end == std::string::npos) break;
                for (size_t k = j; k < end; k++) line += s[k] == '\n';
                literal(src.substr(open + 1, end - open - 1), line);
                i = end + close.size();
                continue;
            }
            if (id == "u8" || id == "L" || id == "u" || id == "U") {
                if (j < n && s[j] == '"') continue;   // the prefix of the literal after it
            }
            last_id = number ? std::string() : id;
            joining = false;
            continue;
        }
        if (c == '(') calls.push_back(last_id);
        else if (c == ')' && !calls.empty()) calls.pop_back();
        // "::" keeps the name of a call (ImGui::Text): only the identifier right before "(" counts
        if (!(c == ':' && i + 1 < n && s[i + 1] == ':')) last_id.clear();
        else i++;
        joining = false;
        i++;
    }
}

static bool key_call(const std::string& call)
{
    for (const char* k : k_key_calls)
        if (call == k) return true;
    return false;
}

// The scanner on a small sample: joins, comments, characters, raw strings, escapes, the innermost call.
static void self_test(void)
{
    const char* sample =
        "// tr(\"in a comment\")\n"
        "/* tr(\"in a block\") */\n"
        "char q = '\"'; ImGui::Text(\"%s\", tr(\"Two \" /* x */ \"parts\"));\n"
        "x = tr(a ? \"yes\" : \"no\"); y = f(tr_id(\"Id\"), \"other\");\n"
        "z = R\"(raw \"quoted\" text)\" ; w = u8\"esc\\x41\\n\\\"\";\n"
        "n = 1'000; v = tr(\"after\");\n";
    std::vector<Lit> v;
    scan(sample, v);
    const char* texts[] = { "%s", "Two parts", "yes", "no", "Id", "other", "raw \"quoted\" text", "escA\n\"", "after" };
    const char* calls[] = { "Text", "tr", "tr", "tr", "tr_id", "f", "", "", "tr" };
    CHECK_EQ(v.size(), 9);
    for (size_t k = 0; k < v.size() && k < 9; k++) {
        if (v[k].text != texts[k] || v[k].call != calls[k])
            fprintf(stderr, "self test %zu: \"%s\" in %s(), line %d\n", k, v[k].text.c_str(), v[k].call.c_str(), v[k].line);
        CHECK(v[k].text == texts[k]);
        CHECK(v[k].call == calls[k]);
    }
    CHECK(v.size() == 9 && v[1].line == 3 && v[8].line == 6);
}

static std::string u8s(const std::filesystem::path& p)
{
    std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

int main(void)
{
    self_test();
    std::set<std::string> table;
    for (int i = 0; i18n_key_at(i); i++) table.insert(i18n_key_at(i));
    CHECK(table.size() > 100);

    std::string src_dir = SAPIEMU_SRC_DIR;
    std::filesystem::path root(std::u8string(src_dir.begin(), src_dir.end()));
    std::error_code ec;
    std::set<std::string> used;
    int files = 0, keys = 0, missing = 0;
    for (std::filesystem::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        const std::filesystem::path& p = it->path();
        std::string ext = u8s(p.extension());
        if (!it->is_regular_file() || (ext != ".cpp" && ext != ".h")) continue;
        bool frontend = u8s(p.parent_path().filename()) == "frontend";
        if (frontend && u8s(p.filename()) == "i18n.cpp") continue;   // the table itself
        std::string text;
        if (!file_read_text(u8s(p), text)) {
            fprintf(stderr, "cannot read %s\n", u8s(p).c_str());
            CHECK(false);
            continue;
        }
        files++;
        std::vector<Lit> lits;
        scan(text, lits);
        for (const Lit& l : lits) {
            used.insert(l.text);
            if (!frontend || !key_call(l.call)) continue;
            keys++;
            if (table.count(l.text) || excepted(l.text)) continue;
            std::u8string rel = std::filesystem::relative(p, root).generic_u8string();
            fprintf(stderr, "src/%s:%d: no Czech entry for %s(\"%s\")\n", std::string(rel.begin(), rel.end()).c_str(),
                    l.line, l.call.c_str(), l.text.c_str());
            missing++;
        }
    }
    CHECK(!ec);
    CHECK(files > 100);
    CHECK(keys > 300);
    CHECK_EQ(missing, 0);

    int unused = 0;
    for (int i = 0; i18n_key_at(i); i++) {
        const char* k = i18n_key_at(i);
        if (used.count(k) || excepted(k)) continue;
        fprintf(stderr, "src/frontend/i18n.cpp: the entry \"%s\" is used nowhere (remove it)\n", k);
        unused++;
    }
    CHECK_EQ(unused, 0);
    for (int i = 0; k_exceptions[i].key; i++) {   // an exception no longer needed goes away too
        bool needed = !table.count(k_exceptions[i].key) || !used.count(k_exceptions[i].key);
        if (!needed) fprintf(stderr, "exception \"%s\" is not needed any more\n", k_exceptions[i].key);
        CHECK(needed);
    }
    printf("%d files, %d keys of tr() in src/frontend, %d entries\n", files, keys, (int)table.size());
    return TEST_RESULT();
}
