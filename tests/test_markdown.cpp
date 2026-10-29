// The Markdown of the guides and of the window Help (frontend/markdown.cpp): the inline marks, the blocks, and the
// documents the program shows (docs/prvni-kroky.md, getting-started.md, slovnicek.md, glossary.md): they are read,
// their links to files lead somewhere, and the Czech and English glossary have the same sections and rows.
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "frontend/markdown.h"
#include "test_util.h"

static void inline_marks(void)
{
    MdText t;
    md_inline("Run **`sapiemu.exe`** and `DIR B:`, see [MIKOS](docs/software/MIKOS.md) or https://x.cz/a_(b)).", t);
    // "Run ", code+bold, " and ", code, ", see ", link, " or ", url, ")."
    CHECK_EQ(t.size(), 9);
    if (t.size() == 9) {
        CHECK(t[0].text == "Run " && t[0].style == 0);
        CHECK(t[1].text == "sapiemu.exe" && t[1].style == (MD_BOLD | MD_CODE));
        CHECK(t[3].text == "DIR B:" && t[3].style == MD_CODE && t[3].link.empty());
        CHECK(t[5].text == "MIKOS" && t[5].link == "docs/software/MIKOS.md");
        CHECK(t[7].text == "https://x.cz/a_(b)" && t[7].link == t[7].text);
        CHECK(t[8].text == ").");
    }
    t.clear();
    md_inline("issues: https://github.com/mlukasek/SAPIemu/issues). Next", t);
    CHECK(t.size() == 3 && t[1].link == "https://github.com/mlukasek/SAPIemu/issues" && t[2].text == "). Next");
    t.clear();
    md_inline("guide `docs\\desky\\APS-5.md`, a \\*star\\* and \\| bar, 2 ** 3", t);
    CHECK(t.size() == 3);
    if (t.size() == 3) {
        CHECK(t[1].style == (MD_CODE | MD_PATH) && t[1].link == "docs/desky/APS-5.md");
        CHECK(t[1].text == "docs\\desky\\APS-5.md");
        CHECK(t[2].text == ", a *star* and | bar, 2 ** 3");
    }
    CHECK(md_plain(t) == "guide docs\\desky\\APS-5.md, a *star* and | bar, 2 ** 3");
}

static void blocks(void)
{
    const char* doc =
        "# Title\n"
        "\n"
        "Intro that goes\n"
        "on here.\n"
        "\n"
        "## Section\n"
        "\n"
        "1. First item\n"
        "   continued.\n"
        "2. Second\n"
        "  - nested **bold**\n"
        "\n"
        "  Paragraph of the second item.\n"
        "\n"
        "```\n"
        "10 PRINT 1\n"
        "```\n"
        "\n"
        "| A | B `x|y` | C |\n"
        "|---|:---:|---|\n"
        "| 1 | 2 \\| 3 |\n"
        "| 4 | 5 | 6 | 7 |\n"
        "\n"
        "---\n"
        "### Sub\n";
    std::vector<MdBlock> b;
    md_parse(doc, b);
    // title, blank, para, blank, h2, blank, item, item, item, blank, para, blank, code, blank, table, blank, rule, h3
    CHECK_EQ(b.size(), 18);
    if (b.size() != 18) return;
    CHECK(b[0].kind == MD_HEADING && b[0].level == 1 && md_plain(b[0].text) == "Title");
    CHECK(b[2].kind == MD_PARAGRAPH && md_plain(b[2].text) == "Intro that goes on here." && b[2].level == 0);
    CHECK(b[4].kind == MD_HEADING && b[4].level == 2);
    CHECK(b[6].kind == MD_ITEM && b[6].marker == "1." && b[6].level == 0);
    CHECK(md_plain(b[6].text) == "First item continued.");
    CHECK(b[7].kind == MD_ITEM && b[7].marker == "2." && b[7].level == 0);
    CHECK(b[8].kind == MD_ITEM && b[8].marker.empty() && b[8].level == 1 && b[8].text.size() == 2);
    CHECK(b[10].kind == MD_PARAGRAPH && b[10].level == 1);   // under the second item (two spaces under "2. ")
    CHECK(b[12].kind == MD_CODEBLOCK && b[12].lines.size() == 1 && b[12].lines[0] == "10 PRINT 1" && b[12].level == 0);
    CHECK(b[14].kind == MD_TABLE && b[14].rows.size() == 3);
    if (b[14].rows.size() == 3) {
        CHECK(b[14].rows[0].size() == 3 && b[14].rows[1].size() == 3 && b[14].rows[2].size() == 3);
        CHECK(md_plain(b[14].rows[0][1]) == "B x|y");          // a | in code does not split
        CHECK(md_plain(b[14].rows[1][1]) == "2 | 3");           // an escaped one neither
        CHECK(b[14].rows[1][2].empty());                        // a missing cell: empty
        CHECK(md_plain(b[14].rows[2][2]) == "6");               // one more: left out
    }
    CHECK(b[16].kind == MD_RULE);
    CHECK(b[17].kind == MD_HEADING && b[17].level == 3);
}

// A picture alone in its paragraph is a block (inside a text it stays a link), a quote gives marked blocks, a prompt
// in a code block is no quote, paths without "..".
static void pictures_quotes(void)
{
    const char* doc =
        "![A plotter](../img/x.png)\n"
        "\n"
        "See ![this](img/y.png) here.\n"
        "\n"
        "> **Note:** first\n"
        "> 1. one\n"
        ">    goes on\n"
        "\n"
        "After.\n"
        "\n"
        "```\n"
        ">RUN\n"
        "```\n";
    std::vector<MdBlock> b;
    md_parse(doc, b);
    CHECK_EQ(b.size(), 10);
    if (b.size() != 10) return;
    CHECK(b[0].kind == MD_IMAGE && b[0].image == "../img/x.png" && md_plain(b[0].text) == "A plotter");
    CHECK(b[2].kind == MD_PARAGRAPH && b[2].text.size() == 3 && b[2].text[1].link == "img/y.png");
    CHECK(b[4].kind == MD_PARAGRAPH && b[4].quote && md_plain(b[4].text) == "Note: first");
    CHECK(b[5].kind == MD_ITEM && b[5].quote && b[5].marker == "1." && md_plain(b[5].text) == "one goes on");
    CHECK(b[7].kind == MD_PARAGRAPH && !b[7].quote && md_plain(b[7].text) == "After.");
    CHECK(b[9].kind == MD_CODEBLOCK && !b[9].quote && b[9].lines.size() == 1 && b[9].lines[0] == ">RUN");
    md_parse("  ![A](a.png)\n  ![B](b.png \"title\")\n", b);   // two on the lines of one paragraph: two pictures
    CHECK(b.size() == 2 && b[0].kind == MD_IMAGE && b[1].kind == MD_IMAGE && b[1].image == "b.png" &&
          md_plain(b[1].text) == "B");
    // a quote under a list item stays under it, so does the text after it (docs/vydani/ux-hodnoceni.md)
    md_parse("1. Item\n\n   > Quoted\n\n   Text of the item.\n", b);
    CHECK(b.size() == 5 && b[2].quote && b[2].level == 1 && !b[4].quote && b[4].level == 1);
    // a code block in a quote ends with its fence or with the quote, the rest of the document stays
    md_parse("> Note:\n> ```\n> 10 PRINT\n> ```\n\nAfter.\n\n## Section\n", b);
    CHECK(b.size() == 6 && b[1].kind == MD_CODEBLOCK && b[1].quote && b[1].lines.size() == 1 &&
          b[1].lines[0] == "10 PRINT" && b[3].kind == MD_PARAGRAPH && !b[3].quote && b[5].kind == MD_HEADING);
    // a fence inside an HTML comment does not hide the quotes after it
    md_parse("<!--\n```\n-->\n> Quoted\n", b);
    CHECK(b.size() == 1 && b[0].quote && md_plain(b[0].text) == "Quoted");
    CHECK(md_norm_path("/home/u/../x.md") == "/home/x.md");
    CHECK(md_norm_path("\\\\server\\share\\a.md") == "//server/share/a.md");
    CHECK(md_norm_path("docs/desky/../img/x.png") == "docs/img/x.png");
    CHECK(md_norm_path("./docs\\navod-kzd1.md") == "docs/navod-kzd1.md");
    CHECK(md_norm_path("../a/./b") == "../a/b");
}

static bool web(const std::string& l)
{
    return l.compare(0, 7, "http://") == 0 || l.compare(0, 8, "https://") == 0;
}

static void check_links(const MdText& t, const char* file)
{
    for (const MdSpan& s : t) {
        if (s.link.empty() || web(s.link) || (s.style & MD_PATH)) continue;   // `code` may name a file of the zip only
        std::string p = s.link.substr(0, s.link.find('#'));
        if (p.empty()) continue;
        bool ok = file_exists(data_path("docs/" + p)) || file_exists(data_path(p));
        if (!ok) fprintf(stderr, "%s: the link %s leads nowhere\n", file, s.link.c_str());
        CHECK(ok);
    }
}

static bool load(const char* file, std::vector<MdBlock>& b)
{
    std::string text;
    if (!file_read_text(data_path(file), text)) {
        fprintf(stderr, "cannot read %s\n", file);
        CHECK(false);
        return false;
    }
    md_parse(text, b);
    for (const MdBlock& k : b) {
        check_links(k.text, file);
        for (const auto& row : k.rows)
            for (const MdText& cell : row) check_links(cell, file);
    }
    return true;
}

// The Czech and the English glossary: the same sections with as many terms, every row with three cells.
static void glossaries(void)
{
    std::vector<MdBlock> cs, en;
    if (!load("docs/slovnicek.md", cs) || !load("docs/glossary.md", en)) return;
    std::vector<size_t> rows_cs, rows_en;
    for (int k = 0; k < 2; k++) {
        const std::vector<MdBlock>& b = k ? en : cs;
        std::vector<size_t>& rows = k ? rows_en : rows_cs;
        for (const MdBlock& x : b) {
            if (x.kind == MD_HEADING && x.level == 2) rows.push_back(0);
            if (x.kind != MD_TABLE || rows.empty()) continue;
            rows.back() += x.rows.size() - 1;
            for (const auto& row : x.rows) {
                CHECK(row.size() == 3);
                for (const MdText& cell : row) CHECK(!md_plain(cell).empty());
            }
        }
    }
    CHECK(rows_cs.size() >= 3);
    CHECK(rows_cs == rows_en);
    if (rows_cs != rows_en)
        for (size_t i = 0; i < rows_cs.size() || i < rows_en.size(); i++)
            fprintf(stderr, "section %zu: %zu Czech terms, %zu English terms\n", i,
                    i < rows_cs.size() ? rows_cs[i] : 0, i < rows_en.size() ? rows_en[i] : 0);
}

// The first steps: read, sections to fold, the tables of the machines with their three columns.
static void first_steps(void)
{
    for (const char* f : { "docs/prvni-kroky.md", "docs/getting-started.md" }) {
        std::vector<MdBlock> b;
        if (!load(f, b)) continue;
        int sections = 0, tables = 0;
        for (const MdBlock& x : b) {
            if (x.kind == MD_HEADING && x.level == 2) sections++;
            if (x.kind == MD_TABLE) {
                tables++;
                CHECK(x.rows.size() >= 2 && x.rows[0].size() >= 2);
            }
        }
        CHECK(sections >= 4);
        CHECK(tables >= 1);
        CHECK(b.size() > 20 && b[0].kind == MD_HEADING && b[0].level == 1);
    }
}

int main()
{
    test_init();
    inline_marks();
    blocks();
    pictures_quotes();
    glossaries();
    first_steps();
    return TEST_RESULT();
}
