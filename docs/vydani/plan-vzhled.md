# Plán: nový vzhled a použitelnost GUI SAPIemu

Schválený autorem 2026-10-28 (Martin = autor, plán ho oslovuje přímo). Je to živý plán: stav etap a rozhodnutí
z checkpointů se doplňují sem a do `docs/rozhodnuti.md`. Vychází z hodnocení `docs/vydani/ux-hodnoceni.md`.

## Kontext

**Proč.** Hodnocení z 2026-10-28 (`docs/vydani/ux-hodnoceni.md`, také Claude Doc
https://claude.ai/artifact/CjtG1T9QnfyPh8wFtbU4Ba) zjistilo, že obsah emulátoru je nad úrovní běžných emulátorů,
ale povrch působí jako vývojářský nástroj:
- výchozí tmavý vzhled ImGui a průsvitná okna;
- ovládací prvky s kontrastem 1,41:1;
- okna rozmístěná podle pevných pixelů;
- ztracené klávesy a slepé uličky v ECMONu.

Martin s hodnocením souhlasí a chce udělat v zásadě všechno, s těmito úpravami:
- **SAPI-1 byl průmyslový řídicí počítač v rámu, ne počítač pro lidi.**
  - Hlavní jsou základní sestavy ZPS.
  - Lidi, kteří SAPI neznají, přitáhnou přístroje: kotelna APS-5, měřák IMS-2 se zdrojem, zapisovače, KZD-1,
    RKD-1, RMP-1.
- **Hry na SAPI nikdy nebyly.**
  - Libor Lasota udělal moderní karty CGA-1V a MPH-1V a dema.
  - Martin převedl Flappy, Bad Apple, PoP a FXS4 a udělal DPI-1L s BomberNetem.
  - Hry jsou zábavný novodobý doplněk, takže skupina „Začněte zde“ s PoP z hodnocení odpadá.
- **Rám v konfigurátoru je záměrná odchylka.**
  - Kreslí se podle Liborovy modernizované skříně s popsanými panely.
  - Původní Tesla měla holé karty za krycím panelem (http://www.osmibitovemuzeum.cz/fotogalerie/sapi-1_2/1.jpg),
    což je nezajímavé.
  - Odchylka dnes není nikde zapsaná a čtyři místa v dokumentaci tvrdí opak.
- **Vzhled jako aplikace Windows, světlý i tmavý.** Martin chce nejdřív vidět varianty, ladit se dá i za chodu.

**Rozhodnuto 2026-10-28:**
- Beta pro Libora, Petra a Michala vyjde až s novým vzhledem. Zip až na Martinův pokyn.
- Kategorie ve výběru sestav: Základ → Přístroje a technologie → Desky Libora Lasoty → Hry a dema → Ostatní.
  Nahoře bude úvod o průmyslovém SAPI-1. XY 4130 a AR 1990 přejdou do přístrojů s průvodci, CM 5400 zůstane
  v Ostatních a Manic Miner bude i mezi hrami.

**Martinova odpověď na první verzi plánu:**
- Plán je výchozí bod. Hodně věcí se vyřeší a změní, až je uvidí, proto checkpointy a ladění za chodu.
- Docking ani F9 = reset nejsou dogma.
  - Výchozí zadokované rozložení je v pořádku, když ho většina uživatelů preferuje a GUI s ním bude hezké.
  - F9 vznikla kvůli ladění.
- WSL nemusí být zelené, pouští se tam, kde to má smysl.

**Pořád platí:**
- kód „C s třídami“, vendorované knihovny se neupravují;
- uložené `imgui.ini` má přednost před výchozím rozložením;
- Windows má přednost; Linux ve WSL jen podle smyslu (písma, DWM, kód závislý na platformě), není podmínkou;
- testy podle `docs/build.md`;
- commity lokálně, push jen na vyžádání a před ním aktualizovat `.md`.

## Vzhled

### Jak to bude udělané

**Styl jako data**
- Nové soubory `src/frontend/theme.h`, `theme.cpp` a `theme_data.cpp`. Každá varianta má světlou a tmavou paletu
  všech `ImGuiCol` a k tomu rozměry.
- `theme_apply()` vždy začne z čistého `ImGuiStyle()`. Pak nastaví paletu a rozměry, `ScaleAllSizes(dpi)`
  a `FontScaleDpi/FontScaleMain`.
  - Postupně to nejde, protože `ScaleAllSizes` je ztrátová.
  - Volá se jen mezi snímky před `NewFrame`, přes příznak `restyle`.
- Nahradí `StyleColorsDark` v `app.cpp:1431-1438` a pevnou barvu plochy na `:1614`.

**Sémantické tokeny**
- Asi 12 tokenů nahradí 42 barev zadaných v kódu: heading, ok, warn, error, pending, pc_bg, bp_bg, code_bg,
  akcentní tlačítko, focus a další.
- Bez nich světlý vzhled nejde. Dnešní zelená a žlutá mají na světlém pozadí kontrast 1,3–1,6:1.
- Mapa míst je v příloze D.

**Barvy hardwaru (asi 222) zůstanou pevné**
- Rám, panely, displeje a papír tiskárny vypadají ve všech vzhledech stejně.
- Okna displejů dostanou tmavé okolí #101010 jako monitor. Jejich titulek se řídí vzhledem.
- Popisky panelů kreslí vlastní pevné písmo `ui.panel` (DejaVu Sans + Bold), nezávislé na písmu UI.
- Na téma se převede jen to, co se kreslí přímo na pozadí okna: legenda APS-5, osa mapy, značky RMP-1, PLAY/REC.

**Písma**
- Ve Windows variantách bude UI v Segoe UI ze systému (`%WINDIR%\Fonts`), přibalit ho nesmíme.
  - Tučné je Semibold nebo Bold.
  - Záloha a Linux: DejaVu Sans. Varianta SAPI používá DejaVu Sans vždy.
- Terminál a debugger zůstanou v DejaVu Sans Mono kvůli celé znakové sadě SAPI.
- Segoe UI dostane `ImFontConfig::ExtraSizeScale ≈ 1,16`.
  - Tělo textu pak má 14 px jako ve Windows 11.
  - Jednotka rozvržení `GetFontSize()` se nemění, žádné okno se proto nezvětší.
  - V galerii zkontrolovat, že háčky nad Č a Ř a ocásky g a y nepřesahují řádek.

**Fokus a výběr**
- Aktivní okno má 2px rámeček v barvě akcentu, kreslený po všech oknech před `Render`. Ukazuje zároveň, kam jde
  klávesnice.
- Vybraná položka seznamu má tónované pozadí a 3px proužek vlevo jako ve Win11. Výběr se tak nepozná jen podle
  barvy.

**Titulek okna Windows**
- Nastaví se přes `DwmSetWindowAttribute`: 20 = světlý nebo tmavý, ve Win11 navíc 35 a 36 = barva titulku podle
  lišty menu.
- Nastavuje se znovu po `SDL_EVENT_SYSTEM_THEME_CHANGED`, protože SDL titulek přepisuje.
- HWND se získá stejně jako v `shot.cpp`.

**Volby**
- Nastavení → Vzhled, vše se projeví hned:
  - varianta;
  - režim Podle systému, Světlý nebo Tmavý (`SDL_GetSystemTheme()`; když systém režim neurčí, použije se světlý);
  - velikost písma 100–200 %.
- Změna měřítka monitoru přes `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED`.
- Logo v O programu přes `ImageWithBg` v barvě textu. Nový soubor `docs/img/logo-alpha.png` vyrobí
  `tools/make_icon.py`.

### Tři varianty do galerie

Plné palety a kontrasty jsou v příloze.

| Varianta | Světlý | Tmavý | Charakter |
|---|---|---|---|
| **Windows 11 (Fluent)**, doporučená jako výchozí | okna #F9F9F9, plocha #EBEBEB, akcent #005FB8 | okna #2B2B2B, plocha #1C1C1C, akcent #4CC2FF | zaoblení 4–8 px, vzdušné rozestupy, Segoe UI + Semibold, titulky vlevo |
| **Windows klasický (10)** | dialog #F0F0F0 na šedé ploše #808080, akcent #0078D4 | okna #202020, plocha #141414 | hranatý, kompaktní, vyšší kontrast, posuvník 16 px, Segoe UI + Bold |
| **SAPI průmyslový** | hliník #ECEEF1 na oceli rámu #708498, modrá #2F5F8F | „velín“: paleta 5.2 z hodnocení s opravami | barvy z kresby rámu, DejaVu Sans jako popisky panelů, titulky na střed jako štítek |

Všechny páry splňují WCAG 2.2 AA: text 4,5:1, prvky 3:1. Hlídá to nový test `tests/test_theme.cpp`.
Nejtěsnější jsou:
- klasická modrá s bílým textem, 4,53:1;
- okraje SAPI tmavého, 3,1:1.

### Jak vybereš (checkpoint A)

1. Po etapě 1 spustím `tools/ui_gallery.py`. Vyrobí asi 15 obrazovek v šesti vzhledech plus sloupec „dnes“:
   - první start ve dvou velikostech, ZPS-1 s průvodcem, menu obrazovky, Flappy;
   - konfigurátor, mapa, Média, terminál, Nastavení, O programu;
   - APS-5, IMS-2, Minigraf, tiskárna a debugger.

   Před každým snímkem se nastavení a `imgui.ini` vrátí na výchozí. Výstup je
   `out/ui-gallery/<datum>/index.html` a k tomu soukromá stránka na claude.ai.
2. Vybereš variantu a připíšeš poznámky. Doladit jde i živě:
   - v Nastavení → Vzhled;
   - v debug buildu v menu Ladění → Editor stylu (`ImGui::ShowStyleEditor`). Jeho export do schránky mi pošleš
     a hodnoty přenesu do tabulky.
3. Vybraná varianta bude výchozí. U checkpointu rozhodneš, jestli ostatní zůstanou jako volba.

### Co Dear ImGui neumí

- Nemá nativní prvky Windows, animace, Mica, Acrylic, stíny oken ani ClearType. Všechno kreslí ImGui, vzhled
  Windows se jen napodobí.
- Okna nemohou vyjet z hlavního okna jako samostatná okna Windows. Multi-viewport s rendererem SDL_Renderer
  nejde.
- Jedna barva okraje platí pro všechno.
- Nemá 3D zkosení jako Win95 a posuvníky nemají šipky.
- U titulku okna Windows jde nastavit jen světlý nebo tmavý režim a barvu.

## Etapy

**Pravidla pro každou etapu**
- Commity po logických celcích.
- Během práce jen dotčené testy, před commitem celá debug sada.
- Před pushem `release test` a aktualizace `.md`. WSL jen podle smyslu, hlavně u kódu závislého na platformě.
- Rozhodnutí z checkpointů se zapíší do `docs/rozhodnuti.md`.
- Náročnost (práce Clauda): S ≤ ½ dne, M 1–2 dny, L 3 a více dní.

**Krok 0.**
- Schválený plán uložit do `docs/vydani/plan-vzhled.md`, protože se pracuje na více PC.
- Odkaz dát do `SOUHRN.md` a do memory `public-release-plan`.
- Do memory zapsat: WSL jen podle smyslu (`test-tiers`); docking a F9 jsou otevřené.

### Etapa 1: mechanismus vzhledu, tři varianty, galerie · L

**Stav 2026-10-28:**
- Mechanismus vzhledu, šest palet, tokeny ve všech oknech, písma a `test_theme` jsou hotové.
- `tools/ui_gallery.py` je hotový. Galerie pro autora je na claude.ai
  (https://claude.ai/artifact/BRYQjq7Rc8sHZvSVgf4xC4).
- Zbývá:
  - logo v O programu nechat, nebo udělat verzi do světlého vzhledu (podle galerie);
  - checkpoint A.

**Nové soubory**
- `theme.h`, `theme.cpp`, `theme_data.cpp`;
- `tests/test_theme.cpp`: WCAG kontrasty všech palet, podle vzoru `test_i18n`;
- `tools/ui_gallery.py`, podle `out/ux-review-20261028/ux_gallery.py`;
- `third_party/fonts/DejaVuSans-Bold.ttf` (licence DejaVu); doplnit `third_party/README.md`
  a `tools/third_party_notices.py`.

**Písma a barvy**
- Písma `ui.bold`, `ui.panel` a `ui.panel_bold` v `ui.h`.
- `PushFont(ui.panel)` kolem kreslení panelů v `rack.cpp`, `ims2.cpp`, `aps5.cpp`, `media.cpp` a `diag.cpp`.
- Tokeny místo barev v kódu (příloha D), okna displejů s tmavým okolím, rámeček fokusu, výběr s proužkem.
- Pryč s `SetNextWindowBgAlpha` (`browser.cpp`, `help.cpp`) a `SetWindowFontScale` (`help.cpp`).
- `url_link` (`help.cpp`) nahradí `TextLinkOpenURL`.

**Volby**
- `--ui-theme win11|classic|sapi[-light|-dark]` a `--ui-scale`: `UiSnapshot` v `app.h`, `main.cpp` vedle `--lang`.
  Neukládají se.
- Nastavení → Vzhled podle vzoru `set_ui_language`, `settings_json` a `settings_apply`.
- Debug Editor stylu.

**Ověření**
- `tools\build.cmd debug test "theme|i18n"`, pak celá debug sada.
- Galerie 15 obrazovek × 6 vzhledů.
- Podle smyslu build a jeden snímek ve WSL kvůli záložnímu DejaVu.

**→ Checkpoint A:** vybereš variantu. Mezitím poběží etapa 2.

### Etapa 2: výběr sestav, průvodci a texty podle historie SAPI · M

**Kategorie v `machines/_vyber.json`**
- Pořadí `zaklad` → `pristroje` → `libor` → `hry` → Ostatní.
- `pristroje` („Přístroje a technologie“):
  - vyjmout ze skupiny Ostatní, dostane `guides: true`;
  - obsah: APS-5, IMS-2, Minigraf, XY 4130, AR 1990, KZD-1 (`ZPS-2_MIKOS_K`), RKD-1 Tesla, RMP-1.
- RKD-1 s CM 5400 přejde do Ostatních.
- `hry` („Hry a dema (novodobé)“):
  - obsah: `sapi1v_hry`, PoP, BomberNet a Manic Miner;
  - `jpr1s_bgp1s` zůstane i v `libor`; v hrách bude pod titulkem „Manic Miner (JPR-1S + BGP-1S)“, k tomu malé
    rozšíření `machine_index.cpp` o titulek podle kategorie.

**Úvod nad seznamem**
- Nové klíče `intro` a `intro_en` (`machine_index.cpp/.h`, `browser.cpp`).
- Obsah: SAPI-1 byl průmyslový řídicí počítač v rámu, začněte ZPS-1, pak zkuste přístroje, hry jsou novodobý
  doplněk.
- Texty kategorií budou vidět pod nadpisem, ne jen v tooltipu.

**Průvodci, titulky a souhrny**
- XY 4130 a AR 1990, česky i anglicky, z `docs/navod-xy4130.md` a `docs/navod-ar1990.md`.
- Náhled AR 1990 a okna XY 4130 přes `tools/make_previews.py`.

**Testy**
- `tests/test_machine_index.cpp` nově kontroluje pořadí kategorií a APS-5 v `pristroje`.
- `mcp_tools.cpp` vrací první kategorii, takže APS-5 se přesune do přístrojů. Doplnit `docs/mcp.md`.
- Texty `i18n`.

**K čemu SAPI-1 sloužil**
- O programu (`help.cpp`).
- První odstavec README, česky i anglicky.
- `docs/prvni-kroky.md`: tabulka a oddíl „Vyzkoušejte“ v pořadí ZPS → přístroje → Libor → hry.
- `docs/vydani/sestavy.md`.

**Odchylka rámu**
- Tooltip u „Typy karet na panelech“ v konfigurátoru.
- Opravit README, `prvni-kroky.md`, `build.md` a `rozhodnuti.md`, které tvrdí „jako na skutečném stroji“.
- Nový záznam v `rozhodnuti.md` s odkazem na fotku původního rámu.
- Zodpovědět otázku 14 v `SOUHRN.md` a `hw-otazky.md`.

**Výběr sestav**
- Spustit jako akcentní tlačítko, Enter = Spustit.
- Náhled v rámečku.
- U běžící sestavy „Restartovat“ a „(běží)“.
- Hledání bez diakritiky.
- Po zavření výběru při prvním startu otevřít průvodce ZPS-1.

**Ověření:** `test_machine_index`, `test_i18n`, `test_mcp`, `check_machines.py`, `make_previews.py` a snímky
výběru česky i anglicky.

**→ Checkpoint B:** schválíš texty o historii. Návrh napíšu já, ty ho upravíš.

### Etapa 3: dokončení vzhledu · M

- Vybraná varianta bude výchozí, k tomu:
  - režim podle systému;
  - velikost písma;
  - změna měřítka monitoru;
  - titulek přes DWM (`dwmapi` jen pro WIN32 v `CMakeLists.txt`, nebo `LoadLibrary`).
- Náhledy sestav přegenerovat s pevným `--ui-theme <výchozí>-dark` (`make_previews.py`).
- Doplnit `docs/build.md` (písma, volby, galerie) a CLAUDE.md.

**Ověření**
- Snímky při 100, 125 a 150 %.
- Přepnutí vzhledu za běhu bez assertů ImGui.
- WSL podle smyslu.
- Martin ručně: přesun okna mezi monitory a přepnutí Windows mezi světlým a tmavým režimem.

### Etapa 4: první dojem a ovládání · M

1. **Klávesnice:**
   - fokus se vrátí na naposled použitý displej (vzor `focus_screen_window` v `app.cpp`), a to po těchto akcích:
     - „Napsat do SAPI“;
     - Spustit;
     - Nahrát program;
     - Vložit;
     - zavření průvodce nebo výběru;
   - nápis „Klikněte sem a pište“ v okně;
   - jednou upozornit na zahozenou klávesu.
2. **Stroj → Vypnout a zapnout (Ctrl+F12):** stejnou cestou jako Restartovat, doplnit do tabulky zkratek.
3. **Průvodci her a PoP:**
   - volba „3“ v ECMONu jako blok kódu s varováním;
   - názvy her jako bloky kódu;
   - ovládání;
   - souhrny ECMON sestav v Ostatních.
4. **APS-5:** F12, F9 a RESET na panelu ZDR se nejdřív zeptají.
5. **Konfigurátor:**
   - měřítko i podle výšky;
   - po kliknutí na slot posunout nastavení do pohledu;
   - cestu média potvrdit až Enterem nebo opuštěním pole (vzor `PK_HEX` v `rack.cpp`).
6. **Chyby nahlas:**
   - `state_notice` (`media.cpp`) zobecnit na chyby médií a FDC;
   - selhané Zapnout ukázat v okně Chyba;
   - Log barevně a „Chyby: N“ ve stavovém řádku.
7. **Média:** průběžně zapisovat přes dočasný soubor a přejmenování (`machine.cpp`, `zrmd1.cpp`, `floppy.cpp`):
   - chvíli po posledním zápisu;
   - při pauze;
   - při ztrátě fokusu.
8. **Stavový řádek:**
   - nepřekrývá menu;
   - „rychlost“ místo „emulace“;
   - upozornění 6–8 s a zároveň do Logu;
   - při F5/F8 nápis přes obraz, „[pauza]“ v titulku.
9. **Drobnosti:**
   - Otevřít v konfigurátoru přes `request(PEND_OPEN)`;
   - průvodce se sám otevře jen po výslovném spuštění;
   - první dialog Vložit/Nahrát začne v `media/`.

**Ověření**
- `check_machines.py` (Vypnout a zapnout, zápis médií).
- Testy zrmd, kzd1 a rkd1.
- Snímky.
- Martin ručně vyzkouší psaní a průvodce.

### Etapa 5: okna, celá obrazovka, obraz podle okna · L

**Dvě výchozí rozložení k porovnání (checkpoint C)**
- **Plovoucí okna** s chytrým rozmístěním, popsaná níže.
- **Zadokované rozložení:**
  - `DockSpaceOverViewport` a při prvním spuštění bez `imgui.ini` rozvržení přes DockBuilder (`imgui_internal.h`);
  - displeje ve středu;
  - ladicí a nástrojová okna vpravo jako záložky;
  - velké kresby přístrojů (APS-5, IMS-2, zapisovač) podle prototypu zadokované, nebo plovoucí.
- Zadokované okno řídí dok, ne zoom (rozhodnutí 2026-10-10), takže displej v doku jede v režimu Přizpůsobit
  oknu.
- Galerie ukáže obě rozložení při 1280×720 a 1920×1080. Vybereš ty, doporučení dám až podle prototypu.
- „Výchozí rozložení oken“ pak vrátí to vybrané. Plovoucí rozmístění je potřeba v obou případech, protože okna jde
  odtrhnout.

**Jeden pomocník rozmístění** nahradí `first_use_right` (`help.cpp`) a jednotlivá volání
`SetNextWindowPos/Size`:
- hlavní výstup vlevo nahoře v největším celočíselném zvětšení, které se vejde do asi 65 % šířky;
- klíč `main_display` v `.sapi` her (CGA-1V, BGP-1S);
- ostatní okna v pravém sloupci;
- vše oříznout do plochy, IMS-2 nejvýš na výšku plochy.

**Hlavní okno**
- Velikost 85–90 % `SDL_GetDisplayUsableBounds`, stejné omezení i pro obnovenou velikost.
- „Výchozí rozložení oken“ smaže i záznamy oken, která ještě nevznikla (`ClearWindowSettings`).
- Smazat mrtvý kód `rack_behind`.

**Celá obrazovka Alt+Enter** (`SDL_SetWindowFullscreen`)
- Jen displej s fokusem, ostatní okna skrytá.
- Položka v menu, v kontextovém menu a v tabulce zkratek.
- Zachytit dřív, než Enter dojde do SAPI.

**Zoom**
- „Přizpůsobit oknu“ jako první položka.
- Zoom pro aktivní displej.
- `NoCollapse` a `ConfigWindowsMoveFromTitleBarOnly`.

**Ověření**
- `--ui-size` 1100×760, 1280×720 a 1920×1080, každá při 100, 125 a 150 %.
- Galerie.
- Martin na notebooku.
- Do poznámek k vydání: alfa testeři ať jednou použijí Výchozí rozložení oken.

### Etapa 6: zbytek před betou · M

**Menu**
- Nápověda → Nahlásit chybu…: verze, sestava, cesty k datům a logu, Kopírovat diagnostiku, Otevřít složku logu.
- Přejmenovat „Sestava…“ na „Konfigurátor sestavy…“ a „Otevřít soubor sestavy…“ na „Spustit sestavu ze
  souboru…“, i se zhruba 77 citacemi v dokumentaci.
- Kopírovat text obrazovky, Ctrl+Shift+C (`screen_text` v `src/core/headless.cpp`).
- Enter a Esc v modálních oknech.
- **Zkratky ladění jako ve Visual Studiu** (návrh, schválíš):
  - F5 běh/pauza, F10 krok přes, F11 krok už sedí;
  - F9 přepne breakpoint na řádku debuggeru;
  - reset zůstane jen na F12, na tabulku zkratek se odkáže i z debuggeru;
  - F9 dnes zmiňuje asi 19 míst v 11 souborech (`help.cpp`, `i18n.cpp`, `app.cpp`, `docs/`); opravit všechna.

**Bezpečí dat**
- Dotaz na neuložené změny vyjmenuje skutečné rozdíly a nabídne „Ponechat pro příští spuštění“ a „Zapomenout“.
- Verze v hlavičce stavu; nekompatibilní stav odmítnout ještě před `load_config` (`src/core/host.cpp`).

**Pro testery**
- Anglický Getting started v zipu (`tools/package.py`).
- MCP pryč z prvního odstavce README.
- Kam hlásit chyby.
- 5 úkolů pro testery.
- SHA-256 k zipu.

**→ BETA zip** (`package.py --public`) až na Martinův pokyn.

### Etapa 7: přístupnost a jazyky · M

- **Překlady přes `tr()`:**
  - APS-5 klíč, PORUCHA, Pec %d;
  - filtry dialogů;
  - hlášení mapy;
  - název sestavy v titulku a v Posledních sestavách z rejstříku (`machine_index_find`).
- **IMS-2:** potisk jako na přístroji, k tomu anglický tooltip.
- **Ctest** na chybějící a nepoužité klíče.
- **Stav nejen barvou:** slovo nebo ikona, čísla pecí v grafu APS-5, PLAY a REC textem.
- **Rozměry v jednotkách písma** (`debugger.cpp`, `terminal.cpp`).
- **Upozornění:** na blikání v demech; v README věta o čtečkách obrazovky.
- **Náhledy** oříznout jen na obraz.
- **Vyzkoušet** `NavEnableKeyboard`.

### Etapa 8: zbytek menu, nastavení a chování · M–L

**Menu podle hodnocení 5.1**
- Média… ve Stroji, Obrazovky ▸, Otevřít složku snímků a videí.
- Okna přístrojů povolená podle typu sestavy a srozumitelná hláška místo JSONu.

**Nastavení po sekcích:** Obecné, Vzhled, Obraz, Zvuk, Ovládání, Záznam a ▸ Pokročilé s MCP.

**Nápověda v programu**
- První kroky a Getting started v okně programu.
- Pomocník `tip()` se zalomením.
- Sbalitelné sekce.
- Slovníček.

**Debugger a konfigurátor**
- Debugger podle CPU, port 63h šedě.
- Jména souborů místo plných cest.
- Konfigurátor: pevné záložky a nabídka Nová ▸ z rejstříku.

**Chování**
- „Moje sestavy“ v release a „Obnovit původní“.
- Starý stroj vypnout až po postavení nového.
- Upozornění při druhé instanci.
- Poslední sestavy po každém startu.
- Stroj → Klávesnice za běhu (`rack_set_live_param`).
- Přetahování souborů na okno (`SDL_EVENT_DROP_FILE`).

### Etapa 9: prezentace a zveřejnění · M + Martin

**README**
- Zhruba 110 řádků.
- Odstavec tvým hlasem (napíšeš sám).
- Přiznání AI.
- „Měřeno na skutečném SAPI-1 V“ s fotkou, snímkem z Logic 2 a čísly.

**Další materiály**
- GIF a video, `README.en.md`, `docs/funkce.md`.
- Obrázky v `docs/img` v novém vzhledu, zmenšit `minigraf-ukazka.png`.
- Úklid repa, šablona issue.

**Ověření před zveřejněním**
- `release full` a WSL.
- `check_machines.py` 60/60 i `--games`.
- Zkušební spuštění rozbaleného balíčku.

## Klíčové soubory

- **Nové:** `src/frontend/theme.h`, `theme.cpp`, `theme_data.cpp`, `tools/ui_gallery.py`, `tests/test_theme.cpp`.
- **Frontend:** `src/frontend/app.cpp`, `rack.cpp`, `browser.cpp`, `help.cpp`, `media.cpp`.
- **Jádro:** `src/core/machine_index.cpp`.
- **Data, nástroje a testy:** `machines/_vyber.json`, `tools/make_previews.py`, `tests/test_machine_index.cpp`.

## Co se použije z hotového

**Nastavení a volby**
- `set_ui_language`, `settings_json` a `settings_apply` (`app.cpp`) jako vzor pro vzhled.
- `UiSnapshot` a volby v `main.cpp` (`--ui-size`, `--settings`, `--lang`).
- `test_i18n` jako vzor testu.

**Fokus a chyby**
- `focus_screen_window` (`app.cpp`).
- `state_notice` (`media.cpp`).

**Konfigurátor a stroj**
- Vzor `PK_HEX` a `rack_set_live_param` (`rack.cpp`).
- `screen_text` (`src/core/headless.cpp`).

**Snímky a náhledy**
- Získání HWND v `shot.cpp`.
- Scénáře v `tools/check_machines.py` a `INSTRUMENTS` v `tools/make_previews.py`.
- `ImGui::ShowStyleEditor`: `imgui_demo.cpp` se už překládá.

## Ověření

- **Každá etapa:**
  - dotčené testy;
  - před commitem celá debug sada;
  - snímky přes `sapiemu --ui-snapshot`, česky i anglicky (ne snímky plochy).
- **Po etapách 1, 3 a 5:** galerie a soukromá stránka pro Martina.
- **Před pushem:** `release test`; WSL podle smyslu.
- **Před betou a zveřejněním:** `check_machines.py` 60/60 a `--games`, zkouška rozbaleného zipu.

---

## Příloha A: palety

W11 = Windows 11, KL = klasický, SAPI = průmyslový; sv = světlý, tm = tmavý. Hodnoty jsou výchozí návrh,
doladí se v galerii.

### Windows 11 (Fluent)

| Slot | sv | tm |
|---|---|---|
| Plocha (clear) | #EBEBEB | #1C1C1C |
| MenuBarBg (= titulek OS přes DWM) | #F3F3F3 | #202020 |
| WindowBg / ChildBg / PopupBg | #F9F9F9 / průhl. / #FCFCFC | #2B2B2B / průhl. / #2C2C2C |
| TitleBg / TitleBgActive | #F3F3F3 / #CCE4F7 | #2B2B2B / #2D4A60 |
| Border | #868686 | #7A7A7A |
| Text / TextDisabled | #1B1B1B / #616161 | #FFFFFF / #A8A8A8 |
| FrameBg / Hov / Act | #FFFFFF / #F3F3F3 / #EBEBEB | #333333 / #3A3A3A / #1F1F1F |
| Button / Hov / Act | #FDFDFD / #F3F3F3 / #EBEBEB | #373737 / #3F3F3F / #313131 |
| Akcentní tlačítko (text) | #005FB8 / #1A6FBF / #00529E (bílý) | #4CC2FF / #47B1E8 / #42A1D2 (černý) |
| Header / Hov / Act | #CCE4F7 / #E5F1FB / #B9D8F3 | #2D4A60 / #3A3A3A / #36586F |
| Tab / TabSelected / Overline | #E6E6E6 / #F9F9F9 / #005FB8 | #232323 / #2B2B2B / #4CC2FF |
| CheckMark / CheckboxSelectedBg | #005FB8 / #CCE4F7 | #4CC2FF / #1F3F57 |
| ScrollbarGrab / Hov / Act (dráha průhl.) | #868686 / #6B6B6B / #505050 | #808080 / #9A9A9A / #4CC2FF |
| Separator / TextSelectedBg | #D6D6D6 / #99C9EF | #404040 / #2D5F80 |
| TextLink, NavCursor, focus | #005FB8 | #4CC2FF |
| ok / warn / error | #0F7B0F / #8C6500 / #C42B1C | #6CCB5F / #FCE100 / #FF99A4 |
| pending: text / pole / tlačítko (bílý) | #B84A00 / #FFE3CC / #B84A00, #9E4000 | #FF9F45 / #4A2A0A / #A85400, #954A00 |
| řádek PC / breakpoint | #FFF4CE / #FDE7E9 | #433519 / #442726 |
| blok kódu / osa grafu | #F0F0F0 / = TextDisabled | #1C1C1C / = TextDisabled |

### Windows klasický (10)

| Slot | sv | tm |
|---|---|---|
| Plocha | #808080 | #141414 |
| MenuBarBg | #F0F0F0 | #2B2B2B |
| WindowBg / ChildBg / PopupBg | #F0F0F0 / průhl. / #F2F2F2 | #202020 / průhl. / #2B2B2B |
| TitleBg / TitleBgActive | #E3E3E3 / #99C9EF | #2B2B2B / #1E4F7A |
| Border | #7A7A7A | #8C8C8C |
| Text / TextDisabled | #000000 / #666666 | #FFFFFF / #A6A6A6 |
| FrameBg / Hov / Act | #FFFFFF / #F5FAFE / #CCE4F7 | #121212 / #1C1C1C / #2A2A2A |
| Button / Hov / Act | #E1E1E1 / #E5F1FB / #CCE4F7 | #333333 / #454545 / #555555 |
| Akcentní tlačítko (bílý text) | #0078D4 / #006CBE / #005A9E | #0078D4 / #006CBE / #005A9E |
| Header / Hov / Act | #CCE8FF / #E5F3FF / #99D1FF | #0063B1 / #3E3E3E / #005A9E |
| Tab / TabSelected / Overline | #DADADA / #F0F0F0 / #0078D4 | #2B2B2B / #202020 / #4CA0E0 |
| CheckMark / CheckboxSelectedBg | #000000 / #FFFFFF | #FFFFFF / #0063B1 |
| ScrollbarBg / Grab / Hov / Act | #E8E8E8 / #7A7A7A / #5F5F5F / #4D4D4D | #171717 / #7A7A7A / #9A9A9A / #4CA0E0 |
| Separator / TextSelectedBg | #A0A0A0 / #99C9EF | #4A4A4A / #264F78 |
| TextLink / focus | #0066CC / #0078D4 | #66B3FF / #4CA0E0 |
| ok / warn / error | #107C10 / #8A6100 / #C50F1F | #6CCB5F / #F2C94C / #FF7B72 |
| pending: text / pole / tlačítko | #B84A00 / #FFE3CC / #B84A00, #9E4000 | #FF9F45 / #4A2A0A / #A85400, #954A00 |
| řádek PC / breakpoint / blok kódu | #FFF29D / #F7D3D6 / #FFFFFF s okrajem | #4D4400 / #4A1F1F / #121212 |

### SAPI průmyslový

| Slot | sv („hliník a ocel“) | tm („velín“, paleta 5.2 s opravami) |
|---|---|---|
| Plocha | #708498 (hrana rámu, `rack.cpp`) | #121417 |
| MenuBarBg | #D6DAE2 (hliník panelu) | #16181C |
| WindowBg / ChildBg / PopupBg | #ECEEF1 / průhl. / #F5F6F8 | #1B1E22 / průhl. / #23272D |
| TitleBg / TitleBgActive | #D6DAE2 / #B0C4D6 | #1B1E22 / #3D6A99 |
| Border | #6E7C8A | #646C77 |
| Text / TextDisabled | #14191E / #57606B | #E6E9ED / #9AA3AE |
| FrameBg / Hov / Act | #FFFFFF / #F2F5F8 / #E2E9F0 | #2A2F36 / #333A43 / #3B444F |
| Button / Hov / Act | #DDE2E8 / #CFD8E1 / #BCC9D6 | #2F353D / #3A424C / #46505C |
| Akcentní tlačítko (bílý text) | #2F5F8F / #3A6C9E / #264E76 | #3D6A99 / #44739F / #335C85 |
| Header / Hov / Act | #B0C4D6 / #D0DCE7 / #9DB4C9 | #3D6A99 / #34536F / #335C85 |
| Tab / TabSelected / Overline | #D6DAE2 / #ECEEF1 / #2F5F8F | #23272D / #2C4763 / #8FB8E0 |
| CheckMark / CheckboxSelectedBg | #2F5F8F / #D9E4EE | #8FB8E0 / #2C4763 |
| ScrollbarGrab / Hov / Act | #7D8996 / #66727F / #2F5F8F | #646C77 / #7A838F / #8FB8E0 |
| Separator | #B4BCC6 | #3A4048 |
| TextLink / focus / nadpis | #245A8C / #2F5F8F / #2F5F8F | #8FB8E0 / #8FB8E0 / #8FB8E0 |
| ok / warn / error | #256D29 / #7A5A00 / #B3261E | #4CC38A / #E3B341 / #F47A70 |
| pending: text / pole / tlačítko | #B04C00 / #FFE3CC / #B04C00, #963F00 | #F0883E / #4A2A0A / #A85400, #954A00 |
| řádek PC / breakpoint / blok kódu | #FFF1B8 / #F9DEDC / #F7F8FA | #3A3418 / #4A2424 / #15171A |

Opravy palety 5.2 z hodnocení:
- hover a stisk akcentu: #44739F a #335C85, dřív bílý text 4,49 a 3,67;
- HeaderActive: #335C85, dřív 3,7;
- chyba: #F47A70, dřív 4,45.

## Příloha B: rozměry a písma

**Rozměry**

| | W11 | KL | SAPI |
|---|---|---|---|
| WindowPadding | 12,10 | 8,8 | 10,10 |
| FramePadding | 10,5 | 6,3 | 8,4 |
| ItemSpacing | 8,7 | 6,4 | 8,6 |
| ItemInnerSpacing | 6,5 | 4,4 | 6,4 |
| CellPadding | 8,4 | 4,2 | 6,3 |
| Indent | 20 | 16 | 18 |
| Scrollbar | 12 | 16 | 12 |
| GrabMin | 12 | 10 | 10 |

- **Zaoblení:**
  - W11: Window a Popup 8, Child 6, Frame, Grab, Tab, MenuItem, Selectable a Image 4, Scrollbar 6;
  - KL: všude 0;
  - SAPI: Window a Popup 4, Child 3, ostatní 2.
- **Okraje:**
  - W11: Window, Child, Popup a Frame 1, Tab 0, TabBar 1, Overline 2, ImageBorder 1;
  - KL: 1 včetně TabBorder, Overline 2;
  - SAPI: SeparatorTextBorderSize 2.
- **Titulek a šipka sbalení:**
  - W11 a KL: titulek vlevo, `WindowMenuButtonPosition = None`;
  - SAPI: titulky na střed.

**Písma**
- Velikosti (ImGui je bere jako výšku řádku): drobné 14, tělo 16, sekce 16 tučně, nadpis 23 tučně, O programu
  32 tučně.
- Jednotka `u = GetFontSize()` = 16 px × DPI × velikost písma.
- Náhradní znaky (U+26A0, U+2716…) se k Segoe UI sloučí z DejaVu Sans (MergeMode).

## Příloha C: kontrasty WCAG 2.2

| Dvojice (požadavek) | W11 sv | W11 tm | KL sv | KL tm | SAPI sv | SAPI tm |
|---|---|---|---|---|---|---|
| Text / okno (4,5) | 16,4 | 14,2 | 18,4 | 16,3 | 15,2 | 13,7 |
| Sekundární text / okno · pole (4,5) | 5,9 · 6,2 | 6,0 · 5,3 | 5,0 · 5,7 | 6,7 · 7,7 | 5,5 · 6,4 | 6,6 · 5,3 |
| Text na výběru (4,5) | 13,1 | 9,3 | 16,6 | 6,1 | 9,9 | 4,6 |
| Okraj prvku / okno (3) | 3,5 | 3,3 | 3,8 | 4,8 | 3,7 | 3,1 |
| Fajfka / pole (3) | 6,3 | 6,3 | 21 | 18,7 | 6,7 | 6,5 |
| Posuvník / dráha (3) | 3,5 | 3,6 | 3,5 | 4,2 | 3,1 | 3,1 |
| Akcentní tlačítko: klid · hover · stisk (4,5) | 6,3 · 5,2 · 7,8 | 10,5 · 8,7 · 7,3 | 4,53 · 5,4 · 7,1 | 4,53 · 5,4 · 7,1 | 6,7 · 5,5 · 8,6 | 5,6 · 5,0 · 7,0 |
| Rámeček fokusu / okno (3) | 6,0 | 7,1 | 4,0 | 5,8 | 5,7 | 8,0 |
| ok · warn · error · pending / okno (4,5) | 5,2 · 5,0 · 5,4 · 5,0 | 7,0 · 10,7 · 7,0 · 6,9 | 4,7 · 4,9 · 5,3 · 4,6 | 8,0 · 10,3 · 6,5 · 8,0 | 5,5 · 5,5 · 5,6 · 4,7 | 7,6 · 8,6 · 6,3 · 6,6 |

Aktivní a neaktivní titulek se liší jen o 1,2–3,0. Aktivní okno proto ukáže rámeček fokusu.

## Příloha D: mapa tokenů a míst

Podle stavu 9b36e49, řádky se mohou posunout.

**Tokeny**
- `heading`: `browser.cpp:168, 400`, `help.cpp:146`, vybraný MAP `rack.cpp:1983`.
- `ok`: `browser.cpp:101`, `terminal.cpp:316`, `media.cpp:456, 526`, `aps5.cpp:318`, `rack.cpp:63`.
- `warn`: `debugger.cpp:50`, `plotter.cpp:246, 310`, `media.cpp:530`, `aps5.cpp:327-332, 382`, `rack.cpp:62`.
- `error`: `app.cpp:868`, `terminal.cpp:273, 294, 357`, `media.cpp:671`, `aps5.cpp:320`, `rack.cpp:61`.
- `pending`: `app.cpp:1130`, `rack.cpp:52-53, 1469, 2198-2200`.
- `pc_bg` a `bp_bg`: `debugger.cpp:123-124`.
- `code_bg`: `browser.cpp:317`.
- Akcentní tlačítko: Spustit `browser.cpp:175`, DEMO `aps5.cpp:312`.

**Na téma převést** (kreslí se na pozadí okna)
- Osa a popisky mapy, stavy MAP: `rack.cpp:1880, 1888, 1983`.
- Legenda APS-5 `aps5.cpp:262`: barevná čárka, název v barvě `Text`.
- Fólie BOT a značka hlavy RMP-1: `media.cpp:1300, 1342`.
- PLAY/REC: `media.cpp:434`.

**Písmo panelů** (`PushFont(ui.panel)`)
- `rack.cpp`:
  - `draw_frame` a `pt_text` na `757-771`; tučné písmo nahradí dvojí tisk na `:770`;
  - dále `559-562, 901-902, 999, 1116-1119`.
- `ims2.cpp:68-74` a přímé `AddText` na řádcích 191–343.
- `aps5.cpp:71-72, 109-110, 157-164`.
- `media.cpp:480-482, 515, 1131-1134, 1185, 1205`.
- `diag.cpp:69-70`.

Tooltipy zůstanou v písmu UI.

**Pevné**
- Papír tiskárny #F4F1E8 s textem #222222 (`printer.cpp:46-47`).
- Okna displejů: WindowBg #101010 kolem `Begin` (`app.cpp:1210, 1259`).

**Rozmístění oken (etapa 5)**

| Okno | Kde se dnes umisťuje |
|---|---|
| Konzole | `app.cpp:1201` |
| Grafika | `app.cpp:1254-1257` |
| Diagnostika | `diag.cpp:53` |
| IMS-2 | `ims2.cpp:470-471` |
| APS-5 | `aps5.cpp:364-366` |
| Zapisovač | `plotter.cpp:263-264` |
| Průvodce | `browser.cpp:458-459` |
| Konfigurátor | `rack.cpp:2150` |
| Hlavní okno | `app.cpp:1387-1405` |
| Výchozí rozložení | `app.cpp:633-659` |
