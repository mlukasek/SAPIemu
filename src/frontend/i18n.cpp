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
    { "Smoothing (linear filtering)", "Vyhlazování (lineární filtrování)" },
    { "Paused", "Pozastaveno" },
    { "Breakpoint at", "Breakpoint na" },
    { "Error", "Chyba" },
    { "Close", "Zavřít" },
    { "About", "O programu" },
    { "No display card in this machine.", "Sestava nemá kartu displeje." },
    { "Click the screen to type on the SAPI keyboard.", "Klikněte na obrazovku a pište na klávesnici SAPI." },
    { "Keys: F5 run/pause, F8 turbo, F10 step over, F11 step, F12 reset",
      "Klávesy: F5 běh/pauza, F8 turbo, F10 krok přes, F11 krok, F12 reset" },
    { "Cannot load machine", "Sestavu nelze načíst" },
    { "Clear", "Vymazat" },
    { "Debug", "Ladění" },
    { "Debugger", "Debugger" },
    { "Memory", "Paměť" },
    { "I/O log", "Log I/O" },
    { "Step", "Krok" },
    { "Step over", "Krok přes" },
    { "Serial terminal", "Sériový terminál" },
    { "Local echo", "Lokální echo" },
    { "Terminal connected (DCD)", "Terminál připojen (DCD)" },
    { "SAPI (ECMON/AND-1Z)", "SAPI (ECMON/AND-1Z)" },
    { "No emulation (^X)", "Bez emulace (^X)" },
    { "CR = new line", "CR = nový řádek" },
    { "Media", "Média" },
    { "SD card:", "SD karta:" },
    { "(empty)", "(prázdná)" },
    { "Write protect", "Ochrana proti zápisu" },
    { "SD card locked", "SD karta zamčená" },
    { "IDE disk:", "IDE disk:" },
    { ", read only", ", jen pro čtení" },
    { "IDE disk: none", "IDE disk: žádný" },
    { "This machine has no removable media.", "Sestava nemá výměnná média." },
    { "This machine has no serial port.", "Sestava nemá sériový port." },
    { "Run (F5)", "Běh (F5)" },
    { "Pause (F5)", "Pauza (F5)" },
    { "Step (F11)", "Krok (F11)" },
    { "Step over (F10)", "Krok přes (F10)" },
    { "Cycles", "Takty" },
    { "Follow PC", "Sledovat PC" },
    { "address, click a line = breakpoint", "adresa, klik na řádek = breakpoint" },
    { "Breakpoints", "Breakpointy" },
    { "Stack", "Zásobník" },
    { "Add", "Přidat" },
    { "Go to", "Přejít na" },
    { "Write", "Zapsat" },
    { "Record", "Zaznamenávat" },
    { "MCP server is not running.", "MCP server neběží." },
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
