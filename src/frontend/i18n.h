#pragma once
// UI translations. Keys are English texts; the current language is Czech.
// Add new strings to the table in i18n.cpp.

enum UiLanguage { LANG_CS = 0, LANG_EN = 1 };

void i18n_set_language(UiLanguage lang);
UiLanguage i18n_language(void);
const char* tr(const char* key);
