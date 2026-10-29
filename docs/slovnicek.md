# Slovníček

Slova, která používají SAPIemu a jeho návody. Ve sloupci Anglicky je výraz anglického rozhraní (Zobrazení → Jazyk /
Language) a anglických návodů. V programu je slovníček v Nápověda → Slovníček, anglicky v `docs/glossary.md`.

## Počítač SAPI-1

| Pojem | Anglicky | Co znamená |
|---|---|---|
| SAPI-1 | SAPI-1 | Průmyslový řídicí mikropočítač Tesla (1983–1990) s procesorem 8080 nebo Z80. Karty jsou v rámu na sběrnici ARB-1. |
| sběrnice ARB-1 | ARB-1 bus | Společné vodiče rámu (adresy, data, řízení), do jejichž konektorů se zasouvají karty. Sběrnice ARB-1C a ImBUS Libora Lasoty mají jiný počet slotů. |
| rám | rack | Skříň se sběrnicí a sloty. Konfigurátor ho kreslí zepředu podle modernizované skříně Libora Lasoty. |
| slot | slot | Místo pro kartu v rámu, X1, X2… V X1 je procesorová karta. |
| karta, deska | card, board | Deska plošného spoje v rámu: procesor (JPR-1…), paměť, displej, řadič disket… Program říká karta, návody často deska („desky V“). |
| propojka | jumper | Spojka nebo přepínač na kartě, které nastavují adresy, paměť a volby. V konfigurátoru se nastavuje stejně jako na skutečné desce. V některých návodech jumper. |
| PROM, EPROM | PROM, EPROM | Paměti jen pro čtení. V PROM jsou dekodéry adres desek (emulátor je čte z dumpů skutečných obvodů), v EPROM programy: monitor, BASIC. |
| port | port | Adresa vstupu nebo výstupu (00h–FFh), přes kterou program mluví s kartou. Mapa portů a paměti je v konfigurátoru. |
| sestava ZPS | ZPS set | Základní počítačová sestava, jak ji Tesla dodávala: ZPS-1 až ZPS-4 a ZPS-6 (SAPI-1.5). |
| desky V | V boards | Novodobé desky Libora Lasoty s V v názvu (JPR-1V, CGA-1V, MPH-1V…). Na nich běží sestava SAPI-1 V s hrami a demy. |
| TURBO | TURBO | Přepínač procesorové karty na dvojnásobný takt (4 MHz). |

## Programy

| Pojem | Anglicky | Co znamená |
|---|---|---|
| monitor | monitor | Program v EPROM, který po zapnutí ovládá počítač z klávesnice: výpis a změna paměti, spuštění programu, kazeta. |
| MIKROBASIC | MIKROBASIC | BASIC v EPROM sestavy ZPS-1. Po zapnutí se v něm rovnou píše program. |
| MIKOS | MIKOS | Monitor a kazetový systém sestavy ZPS-2 (verze V4, V5, MIKOS/K pro záznamník KZD-1, MIKOS/D z diskety). |
| ECMON | ECMON | Monitor sestav s RAM diskem ZRMD-1 a sestavy V. Po zapnutí nabídne volby 0–3: CP/M z RAM disku podle mechanik sestavy (sestava V: `3`). |
| MikroMon, MIKROBIOS | MikroMon, MIKROBIOS | Monitory Libora Lasoty pro jeho procesorové karty. |
| CP/M | CP/M | Diskový operační systém: příkazy `DIR`, `TYPE`, programy `.COM` na disketách nebo RAM disku. |

## Média a data

| Pojem | Anglicky | Co znamená |
|---|---|---|
| médium | medium | Co se vkládá do mechaniky: disketa, kazeta, páska, kazetový disk, pevný disk nebo SD karta. V emulátoru je to soubor, vkládá se v okně Média. |
| obraz disku, diskety | disk image | Soubor s celým obsahem diskety nebo disku sektor po sektoru (`.img`, ImageDisk `.imd`). |
| RAM disk | RAM disk | Disk v paměti karty (ZRMD-1, RMD-1, ZRD-1V). Sestava V z něj zavádí CP/M jako disk A:. |
| pracovní kopie | working copy | Emulátor pracuje s kopií média ze složky `media\` ve složce `work\`, originál zůstane netknutý. Tlačítko … u média v okně Média → Obnovit z originálu ji vrátí. |
| stav, uložený stav | state, saved state | Soubor `.sapistate` s celým počítačem: paměť, karty, média i RAM disky. Soubor → Uložit stav… a Načíst stav… |

## Program SAPIemu

| Pojem | Anglicky | Co znamená |
|---|---|---|
| sestava | machine | Počítač složený z karet a příslušenství, popsaný souborem `.sapi` ve složce `machines\`. Anglicky machine, slovo set jen v historických názvech ZPS. |
| Vybrat sestavu | Choose a machine | Okno se sestavami po skupinách, s náhledem, popisem a tlačítkem Spustit (Soubor → Vybrat sestavu…). |
| konfigurátor sestavy | configurator | Okno s rámem, kartami, propojkami a mapou portů, kde se sestava upravuje (Zobrazení → Konfigurátor sestavy…). Změny se projeví po Restartovat s touto sestavou. |
| Ponechat pro příští spuštění | Keep for the next start | Změny z konfigurátoru, které se použijí při každém spuštění sestavy, i když nejsou uložené v jejím souboru. |
| Moje sestavy | My machines | Vaše vlastní sestavy ve složce `work\machines`, nahoře v okně Vybrat sestavu. Uložit v konfigurátoru zapíše změny dodané sestavy do kopie tam, dodaná sestava zůstane, jak byla. |
| Stroj (menu) | Machine (menu) | Běžící počítač: zapnout a vypnout, reset (F12), pauza, rychlost, klávesnice, zvuk a média. |
| průvodce sestavou | machine guide | Okno s tím, co v sestavě vyzkoušet. Tlačítko Napsat do SAPI napíše blok kódu na klávesnici SAPI (Nápověda → Průvodce sestavou). |
| displej, obrazovka | display, screen | Okno s obrazem karty displeje (AND-1, CGA-1V, BGP-1S…). Sestava jich může mít víc, Zobrazení → Obrazovky je skryje nebo ukáže. |
| konzole | console | Textový displej, na kterém běží monitor, BASIC nebo CP/M. |
| hlavní výstup | main output | Displej, na kterém sestava hlavně pracuje: konzole, u her grafika. Rozložení oken ho dá vlevo nahoru a po spuštění sestavy dostane klávesnici. |
| přístroj | instrument | Zařízení nakreslené ve vlastním okně: měřicí jednotka APS-5, zapisovače, měřicí pracoviště IMS-2. |
| rozložení oken | window layout | Místa oken: plovoucí, nebo zadokovaná ke krajům (Zobrazení → Nastavení → Vzhled). Zobrazení → Výchozí rozložení oken je vrátí. |
| režim pro hry | game mode | Klávesy ANK-1 podle polohy na klávesnici PC (Ctrl+F7), pro hry jako Manic Miner. |
| joystick z klávesnice | joystick from the keyboard | Šipky a mezerník jako joystick (Ctrl+F8). |
| log | log | Záznam toho, co emulátor dělal, s chybami a varováními (Ladění → Log, soubor `sapiemu.log`). Hodí se k Nápověda → Nahlásit chybu… |
| MCP | MCP | Server pro vývoj programů pro SAPI s Claude. Je v Nastavení pod Pokročilé (pro vývojáře), v nové instalaci vypnutý. |
