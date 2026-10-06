// UI languages: codes, translation in both languages, window ids independent of the language
// ("<translated>###<key>"), no key twice in the table.
#include <string.h>
#include "frontend/i18n.h"
#include "test_util.h"

int main(void)
{
    UiLanguage l = LANG_EN;
    CHECK(i18n_parse("cs", &l) && l == LANG_CS);
    CHECK(i18n_parse("EN", &l) && l == LANG_EN);
    CHECK(!i18n_parse("de", &l));
    CHECK(!i18n_parse(nullptr, &l));
    CHECK(strcmp(i18n_code(LANG_CS), "cs") == 0 && strcmp(i18n_code(LANG_EN), "en") == 0);

    i18n_set_language(LANG_CS);
    CHECK(strcmp(tr("Media"), "Média") == 0);
    CHECK(strcmp(tr("no such text"), "no such text") == 0);
    CHECK(strcmp(tr_id("Media"), "Média###Media") == 0);
    const char* cs_id = tr_id("Printer");
    i18n_set_language(LANG_EN);
    CHECK(strcmp(tr("Media"), "Media") == 0);
    CHECK(strcmp(tr_id("Media"), "Media###Media") == 0);
    CHECK(strcmp(tr_lang("Media", LANG_CS), "Média") == 0);
    CHECK(strcmp(cs_id, "Tiskárna###Printer") == 0);   // cached ids stay valid

    UiLanguage o;
    CHECK(!i18n_override(&o));
    i18n_set_override(LANG_CS);
    CHECK(i18n_override(&o) && o == LANG_CS);

    const char* dup = i18n_duplicate_key();
    if (dup) printf("duplicate key: %s\n", dup);
    CHECK(dup == nullptr);
    printf("system language: %s\n", i18n_code(i18n_system_language()));
    return TEST_RESULT();
}
