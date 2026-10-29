#pragma once
// UI translations. Keys are English texts, the table in i18n.cpp gives the Czech ones.
// Add new strings to the table in i18n.cpp.
#include <string>

enum UiLanguage { LANG_CS = 0, LANG_EN = 1 };

void i18n_set_language(UiLanguage lang);
UiLanguage i18n_language(void);
const char* i18n_code(UiLanguage lang);                   // "cs", "en"
bool i18n_parse(const char* code, UiLanguage* out);       // "cs" / "en" (case does not matter)
UiLanguage i18n_system_language(void);                    // Czech for a Czech or Slovak Windows display language, else English
void i18n_set_override(UiLanguage lang);                  // --lang: wins over the settings, not saved
bool i18n_override(UiLanguage* out);

const char* tr(const char* key);
const char* tr_lang(const char* key, UiLanguage lang);
// A window or popup title "<translated>###<key>": the ImGui id (and the saved position) does not
// change with the language.
const char* tr_id(const char* key);
const char* i18n_duplicate_key(void);   // a key twice in the table (test), else nullptr
const char* i18n_key_at(int i);         // the i-th key of the table (test), nullptr past the end

// A text of the core in the language of the UI, translated where the GUI shows it: the core (src/core, src/cards...)
// is shared with sapiemu-cli and MCP and speaks English (an error of a card, a machine file or a state, the reason
// the APS-5 demo stopped). Its fixed texts and the patterns of its composed ones are in the table k_core of i18n.cpp
// ("%s" = a part such as a file name or a number, put into the Czech text in the same order and translated itself
// when it is a text of the core too); then the table of tr(). "<who>: <text>" with <who> in neither (a card, a file)
// keeps <who> and translates <text>. A text found nowhere stays English.
std::string tr_core(const std::string& text);
std::string tr_core_lang(const std::string& text, UiLanguage lang);
// The i-th pattern of k_core (test): its English text, the Czech one into `cs`; nullptr past the end.
const char* i18n_core_at(int i, const char** cs);
