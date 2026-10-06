#pragma once
// UI translations. Keys are English texts, the table in i18n.cpp gives the Czech ones.
// Add new strings to the table in i18n.cpp.

enum UiLanguage { LANG_CS = 0, LANG_EN = 1 };

void i18n_set_language(UiLanguage lang);
UiLanguage i18n_language(void);
const char* i18n_code(UiLanguage lang);                   // "cs", "en"
bool i18n_parse(const char* code, UiLanguage* out);       // "cs" / "en" (case does not matter)
UiLanguage i18n_system_language(void);                    // Czech for a Czech or Slovak UI or format, else English
void i18n_set_override(UiLanguage lang);                  // --lang: wins over the settings, not saved
bool i18n_override(UiLanguage* out);

const char* tr(const char* key);
const char* tr_lang(const char* key, UiLanguage lang);
// A window or popup title "<translated>###<key>": the ImGui id (and the saved position) does not
// change with the language.
const char* tr_id(const char* key);
const char* i18n_duplicate_key(void);   // a key twice in the table (test), else nullptr
