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

    // the texts of the core: fixed ones, patterns, "<who>: <text>", parts that are texts of the core themselves
    struct { const char* en; const char* cs; } core[] = {
        { "not a SAPIemu state file", "není to soubor stavu SAPIemu" },
        { "unknown card type 'XYZ-9'", "neznámý typ karty „XYZ-9“" },
        { "cannot read machine file E:\\x\\machines/a.sapi", "nelze přečíst soubor sestavy E:\\x\\machines/a.sapi" },
        { "JPR-1ZI: cannot read E:\\x\\roms/none.txt", "JPR-1ZI: nelze přečíst E:\\x\\roms/none.txt" },
        { "RAM-1: slot 3 is already occupied", "RAM-1: slot 3 je už obsazený" },
        { "Z80: this CPU card is not emulated yet", "Z80: tato procesorová karta zatím není emulovaná" },
        { "state: block 'card X3 B' where 'card X4 C' was expected",
          "stav: blok „card X3 B“ tam, kde se čekal „card X4 C“" },
        { "state: keyboard ANK-1: unknown keyboard 'Q'", "stav: klávesnice ANK-1: neznámá klávesnice „Q“" },
        { "cannot create working copy work/a.img: Access is denied.",
          "nelze vytvořit pracovní kopii work/a.img: Access is denied." },
        { "media/a.img: 1000 bytes; a disk image is ImageDisk (.imd) or has 256256 (8\" SS SD), 1474560 (3.5\" HD), "
          "1228800 (5.25\" HD), 737280 or 368640 (DD), 327680 or 163840 (SAPI-1.5 DD)",
          "media/a.img: 1000 bajtů; obraz diskety je ImageDisk (.imd) nebo má 256256 (8\" SS SD), 1474560 (3,5\" HD), "
          "1228800 (5,25\" HD), 737280 nebo 368640 (DD), 327680 nebo 163840 (SAPI-1.5 DD) bajtů" },
        { "the state was saved by a newer SAPIemu (9.9.9, state format 3); this SAPIemu 0.7.0 reads state formats 1 to 2",
          "stav uložila novější verze SAPIemu (9.9.9, formát stavu 3); tato verze SAPIemu 0.7.0 čte formáty stavu 1 až 2" },
        { "no \"MENU\" on the screen", "na obrazovce není „MENU“" },
        { "the panel is gone", "panel zmizel" },   // in the table of tr()
        { "something new: 12", "something new: 12" },
        { "", "" },
    };
    for (auto& c : core) {
        std::string cs = tr_core_lang(c.en, LANG_CS);
        if (cs != c.cs) printf("tr_core: \"%s\" -> \"%s\"\n", c.en, cs.c_str());
        CHECK(cs == c.cs);
        CHECK(tr_core_lang(c.en, LANG_EN) == c.en);
    }
    i18n_set_language(LANG_CS);
    CHECK(tr_core("write protected") == "chráněno proti zápisu");
    i18n_set_language(LANG_EN);
    CHECK(tr_core("write protected") == "write protected");
    // the patterns: as many parts in Czech as in English; each one translated by its own entry (not taken for an
    // earlier, more general one: "cannot create %s" before "cannot create working copy %s: %s")
    auto fill = [](const char* pattern) {
        std::string s;
        for (const char* p = pattern; *p; p++) {
            if (p[0] == '%' && p[1] == 's') {
                s += "X1";
                p++;
            } else {
                s += *p;
            }
        }
        return s;
    };
    for (int i = 0; i18n_core_at(i, nullptr); i++) {
        const char* cs = nullptr;
        const char* en = i18n_core_at(i, &cs);
        int ne = 0, nc = 0;
        for (const char* p = en; (p = strstr(p, "%s")) != nullptr; p += 2) ne++;
        for (const char* p = cs; (p = strstr(p, "%s")) != nullptr; p += 2) nc++;
        if (ne != nc) printf("k_core: %d / %d parts: %s\n", ne, nc, en);
        CHECK_EQ(ne, nc);
        std::string got = tr_core_lang(fill(en), LANG_CS);
        if (got != fill(cs)) printf("k_core: \"%s\" -> \"%s\"\n", fill(en).c_str(), got.c_str());
        CHECK(got == fill(cs));
    }
    printf("system language: %s\n", i18n_code(i18n_system_language()));
    return TEST_RESULT();
}
