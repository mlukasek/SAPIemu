#pragma once
// The Markdown of the guides (machines/guides/*.md) and of the help in the program (Help -> First steps, Glossary:
// docs/prvni-kroky.md, docs/getting-started.md, docs/slovnicek.md, docs/glossary.md): the small part of Markdown these
// files use, parsed once into blocks that the windows draw (md_draw, frontend/mdview.cpp). No ImGui here (tested by
// tests/test_markdown.cpp).
// - Blocks: headings (# to ######), paragraphs, list items (- * and 1.), fenced code blocks (```), tables (a header
//   row, a row of dashes, the rows), a horizontal rule (---), empty lines. A line that only goes on with the text of a
//   paragraph or an item joins it, as in Markdown. Indented paragraphs, code blocks and tables belong to the list item
//   above them (level).
// - Inline: **bold**, `code`, [text](link), ![text](picture) as a link to the picture, bare http(s) addresses,
//   backslash escapes. `code` that names a .md file may become a link to it (MD_PATH, md_resolve in the viewer).
#include <stdint.h>
#include <string>
#include <vector>

enum {
    MD_BOLD = 1,
    MD_CODE = 2,
    MD_PATH = 4,   // `code` of a .md file: a link only when the file is there (the viewer decides, md_resolve)
};

struct MdSpan {
    std::string text;
    uint8_t style = 0;   // MD_BOLD | MD_CODE | MD_PATH
    std::string link;    // the target of a link, "" = none: http(s)://..., or a file relative to the document
};

enum MdKind { MD_HEADING, MD_PARAGRAPH, MD_ITEM, MD_CODEBLOCK, MD_TABLE, MD_RULE, MD_BLANK };

typedef std::vector<MdSpan> MdText;

struct MdBlock {
    MdKind kind = MD_PARAGRAPH;
    // heading: 1 for #, 2 for ##...; item: its depth in the list (0 = top); paragraph, code block, table: the list
    // level it belongs to (0 = none, 1 = the text of a top item...)
    int level = 0;
    std::string marker;              // item: "" = a bullet, "1." = numbered
    MdText text;                     // heading, paragraph, item
    std::vector<std::string> lines;  // code block
    std::vector<std::vector<MdText>> rows;   // table: rows of cells, the first one the header (all as wide)
};

// The blocks of a document (the old ones are replaced).
void md_parse(const std::string& text, std::vector<MdBlock>& out);
// The inline marks of one line of text (appended to out).
void md_inline(const std::string& s, MdText& out);
// The text without the marks (the label of a heading, tests).
std::string md_plain(const MdText& text);

// ---------------------------------------------------------------- drawing (frontend/mdview.cpp, ImGui)

struct UiState;

// How md_draw draws a document.
struct MdView {
    bool fold = false;          // ## headings as collapsing headers, the first one open at first (all_open: every one)
    bool all_open = false;
    int set_open = 0;           // this frame: 1 opens every section, -1 closes every one (Expand all, Collapse all)
    bool type_buttons = false;  // code blocks with Type into SAPI (the guides); else Copy only
    // a link to a file the window shows itself (another page of the help): true = done, else it opens outside
    bool (*own_link)(UiState& ui, const std::string& path) = nullptr;
};

// Once after md_parse of the document `file` (from the data root): the links to files become paths from the data root,
// tried in the folder of the file first ("docs/"), then from the data root (the guides link "docs/..."); `code` of a
// .md file that is not there is no link (GETTING-STARTED.md in the repository: it is at the top of the release zip).
void md_resolve(std::vector<MdBlock>& doc, const std::string& file);
// Draws the document at the cursor, as wide as the window (wrapped). The links open in the browser or in the program
// the system has for the file (help.cpp open_doc), unless own_link takes them.
void md_draw(UiState& ui, const std::vector<MdBlock>& doc, const MdView& view);
