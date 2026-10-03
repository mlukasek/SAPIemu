#include "frontend/i18n.h"

#include <string.h>
#include <string>
#include <unordered_map>

struct Translation {
    const char* key;   // English
    const char* cs;
};

static const Translation k_table[] = {
    { "File", "Soubor" },
    { "Machine", "Stroj" },
    { "View", "Zobrazení" },
    { "Help", "Nápověda" },
    { "Open machine", "Otevřít sestavu" },
    { "Exit", "Konec" },
    { "Reset", "Reset" },
    { "Pause", "Pauza" },
    { "Turbo", "Turbo" },
    { "Screen", "Obrazovka" },
    { "Log", "Log" },
    { "Phosphor", "Barva luminoforu" },
    { "White", "Bílá" },
    { "Green", "Zelená" },
    { "Amber", "Jantarová" },
    { "Integer scaling", "Celočíselné zvětšení" },
    { "TV aspect ratio", "Poměr stran jako na TV" },
    { "Paused", "Pozastaveno" },
    { "Breakpoint at", "Breakpoint na" },
    { "Error", "Chyba" },
    { "Close", "Zavřít" },
    { "About", "O programu" },
    { "No display card in this machine.", "Sestava nemá kartu displeje." },
    { "Click the screen to type on the SAPI keyboard.", "Klikněte na obrazovku a pište na klávesnici SAPI." },
    { "Keys: F5 pause, F11 turbo, F12 reset", "Klávesy: F5 pauza, F11 turbo, F12 reset" },
    { "Cannot load machine", "Sestavu nelze načíst" },
    { "Clear", "Vymazat" },
};

static UiLanguage g_lang = LANG_CS;

void i18n_set_language(UiLanguage lang) { g_lang = lang; }
UiLanguage i18n_language(void) { return g_lang; }

const char* tr(const char* key)
{
    if (g_lang == LANG_EN) return key;
    static std::unordered_map<std::string, const char*> map;
    if (map.empty())
        for (const Translation& t : k_table) map[t.key] = t.cs;
    auto it = map.find(key);
    return it != map.end() ? it->second : key;
}
