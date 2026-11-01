# Sestavení a spuštění

## Visual Studio 2022

1. Soubor → Otevřít → Složka, vybrat kořen repa.
2. Vybrat preset (`Windows x64 Debug`, případně lokální `Local x64 Debug`).
3. Jako Startup Item zvolit `sapiemu.exe` (GUI), nebo `sapiemu-cli.exe` (bez okna).

Jde to i přes klasické řešení `.sln`: `cmake -G "Visual Studio 17 2022" -A x64 -S . -B out\sln` a otevřít
`out\sln\SAPIemu.sln`. Výchozí Startup Project je `sapiemu` (CMake `VS_STARTUP_PROJECT`), ne `ALL_BUILD`.

## Příkazová řádka

```
tools\build.cmd debug test          # preset windows-debug, výstup v out\build\windows-debug
tools\build.cmd debug test "zps3|rpd1"   # jen testy, jejichž jméno odpovídá
tools\build.cmd release test        # release, bez pomalých testů procesoru
tools\build.cmd release full        # release se všemi testy včetně ZEXDOC/ZEXALL/8080EXM/8085EXM
tools\build.cmd local-debug test    # lokální preset (výstup mimo repo)
```

### Kdy které testy (2026-10-06)

Testy běží paralelně (8 najednou, `execution.jobs` v `CMakePresets.json`). Časy na autorově PC:

| Kdy | Příkaz | Trvá |
|---|---|---|
| během práce, po každé úpravě | `tools\build.cmd debug test "<testy dotčené změnou>"` | sekundy |
| před každým commitem | `tools\build.cmd debug test` (všech 33) | asi 25 s |
| před pushem, po změně časování nebo výkonu | navíc `tools\build.cmd release test` | asi 10 s + překlad |
| změna jader CPU (`src/cpu`, `third_party/i8080`, `z80.h`), před vydáním zipu | `tools\build.cmd release full` | asi 3 min |
| změna kódu závislého na platformě (`#ifdef _WIN32`, soubory, sériová linka, CMake, nové knihovny), před vydáním | Linux ve WSL (níže) | asi 1 min + překlad |
| před vydáním zipu (po `release test`) | `python tools\check_machines.py --games` | asi 5 min |

**Kontrola všech sestav** (`tools/check_machines.py`, od 2026-10-14): v každé sestavě z `machines/` spustí něco
konkrétního (`DIR` na různé disky, `PRINT 6*7` v MIKROBASICu, `D0 7` v MIKOSu, přihlášení v shellu RKD-1, `DIRMT`
pásky RMP-1, `.K_I` a `.K_D` na KZD-1, Manic Miner na BGP-1S, READY a TEST plotteru z AR 1990…) a zkontroluje text
obrazovky, snímek grafiky, sériovou linku, kresbu zapisovače (`strokes:N`, PNG kresby vedle výpisu), varování v logu
a zbylé dočasné soubory ukládání médií (`*.tmpNNN` ve `work/`, od 2026-10-29). U `cpm_zrmd` uloží soubor na RAM
disk, vypne a zapne stroj (`--power-cycle`) a zkontroluje, že ECMON znovu nabídne volbu a soubor na disku zůstal.
`--games` přidá porty a dema sestavy V: Bad Apple!! celé až do návratu do CP/M,
všech 26 skladeb FXS4 s kontrolou zvuku, Prince of Persia, Bomberman, Flappy na CGA-1V a DGD-1V. Pracuje nad čistou
kopií dat z gitu (`roms`, `media`, `machines` a IDE disk z `work/ide`) v `out/check/<čas>/`, takže `work/` (pracovní
kopie, baterie) zůstane netknuté; výstupy a snímky jsou tamtéž. Používá release `sapiemu-cli` (aktuální sestavení!),
`--debug` debug. Nová sestava potřebuje svůj scénář v tabulce `MACHINES`, jinak ji nástroj ohlásí.

Dotčené testy podle oblasti: deska nebo sestava → testy, které ji načítají (`grep machines/… tests/*.cpp`); diskové
řadiče → `fdc_ecmon|floppy_imd|sapi_sys|drive_letters|rpd1|zps3|zps4`; GUI texty → `i18n` (`test_i18n`
a `test_i18n_keys`, ten i po přidání nebo smazání `tr()` kdekoli v `src/frontend`); Markdown průvodců a okna Nápověda
(`markdown.cpp`, `docs/prvni-kroky.md`, `getting-started.md`, `slovnicek.md`, `glossary.md`) → `markdown`; MCP → `mcp`;
stav → `state`;
přepínání sestav (`EmuHost`: nepovedené načtení, zkušební sestavení, Stroj → Klávesnice, zámek GUI) → `host`.

Testy smějí běžet souběžně: každý zapisuje jen do vlastních souborů ve `work/test*` (složky vytvoří `test_init()`),
pracovní kopie médií ve `work/` jen čte. Pracovní kopie vzniká pod dočasným jménem a přejmenuje se, takže
ji souběžně nevytvoří dva procesy napůl (dřív na čistém `work/` padaly `test_zps3`, `test_state`).

Klávesy v GUI: F5 běh/pauza, F8 rychlost emulace (100 % → 200 % → 400 % → maximum, dokola), F10 krok přes, F11 krok, F12 reset (pod debuggerem PC si F12 bere Windows, pak Stroj → Reset nebo tlačítko Reset v debuggeru), F9 breakpoint v debuggeru (do 2026-10-29 reset), Ctrl+F12 vypnout a zapnout, Alt+Enter celá obrazovka.

**Debugger** (`debugger.cpp`, od etapy 8) ukazuje registry podle `CpuCard::cpu_isa()`: 8080 a 8085 jmény Intel
(8085 navíc `CpuRegs::rim` a `sod` jen pro čtení, plní je jen `Jpr1::get_regs`; ostatní karty je neplní, debugger
`CpuRegs` nejdřív nuluje), Z80 s druhou sadou, IX, IY, I, R, IM, IFF. Disassembler má jen mnemoniky Zilog. Okno
Paměť ukáže „.“ za znak, který písmo GUI nemá (`mem_char`, `IsGlyphInFont`; v jádře by to záviselo na GUI).

**Rychlost a takt** jsou dvě různé věci:
- **Takt CPU** je vlastnost karty: u JPR-1ZI propojky JP2/JP3 v sestavě, u JPR-1V přepínač TURBO (2/4 MHz) i za
  chodu (tlačítko TURBO na panelu ZDR, menu Stroj, MCP `cpu_turbo`).
- **Rychlost emulace** zrychlí celý emulovaný počítač proti reálnému času (F8, menu Stroj → Rychlost emulace, MCP
  `resume speed_percent`/`max_speed`), tedy i UART, disketu a časování karet.
- Stavový řádek ukazuje obojí („CPU 4.00 MHz  rychlost 100 %“), displej FREKVENCE CPU na panelu ZDR jejich součin,
  tedy efektivní takt CPU (od 100 MHz bez desetin).

Nastavení GUI se ukládá do `sapiemu-settings.json` a místa oken do `imgui.ini` (zadokovaná okna do `imgui-docked.ini`)
ve složce s exe (přenosné). Když tam nejde zapisovat, použije se `%APPDATA%\SAPIemu\SAPIemu`. Pamatuje se poslední
sestava (spustí se bez
`--machine`), velikost a poloha okna, otevřená okna, zobrazení obrazovky, nastavení terminálu, rychlost a jazyk
(`"language": "cs"` nebo `"en"`, jen když si ho uživatel zvolil v menu). Jiný soubor nastavení:
`sapiemu --settings soubor.json`.

**Okno Nastavení** (`draw_settings` v `app.cpp`, od etapy 8 po sekcích): jedna stránka s nadpisy `theme_section`
a formulářem (popisek ve sloupci `settings_row`, nejširší popisek z `k_settings_labels` určí sloupec): Obecné, Vzhled,
Obraz, Zvuk, Ovládání, Záznam, sbalitelné Pokročilé (pro vývojáře) se serverem MCP (otevře se samo, když je server
zapnutý nebo hlásí chybu). Klíče souboru nastavení se nezměnily. Výchozí velikost 40 × 56 výšek písma. ffmpeg hledá
`video_ffmpeg()` jen při otevření okna a po změně pole. Složky snímků a videí dávají `shot_dir()` a `video_dir()`,
otevírá je `open_folder()` (`help.cpp`, složku nejdřív vytvoří; i Nahlásit chybu a Zobrazení → Otevřít složku
snímků a videí).

**Druhé okno GUI** nad stejným nastavením nebo stejnou pracovní složkou (`work/`: RAM disky, diskety, zálohované
paměti) se spustí, ale ukáže varování „Už běží jiné SAPIemu“ (Pokračovat / Ukončit). Dokud uživatel neodpoví, do
sdílených souborů nic nezapíše: sestava neběží (`host.frame` ani příkazy MCP), ImGui neukládá rozložení oken
(`io.IniFilename` po načtení v prvním snímku odložené v `App::ini_held`, Pokračovat ho vrátí). Ukončit odejde bez
uložení nastavení, rozložení oken i sestavy (`EmuHost::drop`: bez výpadku napájení, RAM disky a zálohované paměti
zůstanou, jak je má první okno). GUI drží po celý běh zámek `<soubor nastavení>.lock` a `work/sapiemu-gui.lock`
(`file_lock_try` v `util.cpp`). Na Windows je to soubor otevřený bez sdílení, který systém po skončení procesu
smaže i po pádu. Na Linuxu je to `flock`, soubor zůstane. Druhé GUI nad stejným nastavením píše log do
`sapiemu.2.log`, aby nepřepsalo log prvního. `sapiemu-cli` a `--ui-snapshot` zámek neberou ani nehlídají, protože
testy a galerie jich spouští mnoho najednou. Snímek varování: `--ui-second`.

**Přepnutí sestavy** (od 2026-10-29, etapa 8 plánu vzhledu; `EmuHost::load_config`, `load_state` v `host.cpp`): nová
sestava se nejdřív postaví nanečisto (`EmuHost::trial`: `Machine::trial`, `load_json`, `check_bridges` bez otevření
portů, u stavu i `load_state`), starý stroj zatím běží. Když zkouška selže, starý stroj zůstane nedotčený (objekt,
generace, paměť, breakpointy, pauza, spojení sériových linek, bez `power_down`). Po dobré zkoušce se starý vypne jako
dřív a nový se postaví znovu (`build`), aby četl RAM disky a zálohované paměti až po jejich zápisu. Zkušební stroj
a stroj, který se nepostavil, nic neukládá (`Machine::trial`, `Machine::discard()`, `Card::trial` hlídají `~Machine`
a destruktory karet, které zapisují; nová karta, která v destruktoru zapisuje soubor, musí `trial` hlídat také).
Hlášky zkoušky drží `log_hold` a zapíše `log_replay` (pro každé vlákno zvlášť). Výjimka JSON při stavbě je chyba,
neuteče. Test `test_host`.

**Poslední sestavy** (`ui.recent`, v nastavení `browser.recent`): `recent_add` po každém úspěšném startu z GUI
(`start_machine` v `rack.cpp`, kudy jdou Vybrat sestavu, Poslední sestavy, Spustit ze souboru i přetažení), po
Restartovat s touto sestavou se souborem a po Uložit jako. MCP a `sapiemu-cli` seznam nemění. Vybrat sestavu se zavře
až po startu (`ui.browser_start`).

**Stroj → Klávesnice** jde přes popis sestavy: `rack_set_keyboard` → `rack_set_live_param` → `Machine::set_live_param`
klávesnice karty CPU (`Machine::plug_keyboard`). Popis staví `rack_keyboard_cfg` jako konfigurátor. Klávesnice
odmítnutá kartou CPU vrátí klávesnici z popisu. MCP `set_keyboard` mění jen běžící klávesnici.

**Moje sestavy** (od 2026-10-29, rozhodl autor): v buildu pro vydání (`NDEBUG`, `save_copies()` v `rack.cpp`) zapíše
Uložit v konfigurátoru dodanou sestavu jako kopii do `work/machines/` (`MACHINE_MINE_DIR`, přes `work_root`). Dodaná =
jmenovaná v `machines/_vyber.json` (`machine_supplied`, `MachineEntry::listed`; bez rejstříku každý soubor pod
`machines/`). Ladicí build dodanou sestavu přepíše, `--ui-save-copy` vynutí chování vydání.
- Kopie: jméno souboru originálu (obsazené `-2`, `-3`…), ke jménu „ (moje)“ nebo „ (mine)“ podle jazyka (druhá
  „(moje 2)“), `based_on` = cesta originálu (`cfg_hw` ho nebere jako změnu hardwaru). Běžící sestavu na kopii
  přejmenuje `EmuHost::set_file` bez nového sestavení. Rychlý slot kopie `work/state/mine_<jméno>.sapistate`
  (`quick_state_path`).
- Uložit jako v buildu pro vydání začne u nové a dodané sestavy ve `work/machines`.
- Rejstřík (`core/machine_index`) dá kopie do kategorie `MACHINE_MINE` („Moje sestavy“) nahoru, jen když nějaké jsou
  (soubory s `_` přeskočí). Kopie bere náhled, průvodce a titulek originálu se značkou (`MachineEntry::copy`).
  Okno Vybrat sestavu: „Vychází z“, Smazat… (ne u běžící). MCP `list_machines` je vypíše s `based_on`.
- Testy: `test_mine` v `test_machine_index` (vlastní kořen dat), `host_copy` v `test_state`; `test_index` hlídá, že
  každá sestava v `machines/` je v rejstříku (jinak by ji Uložit nebralo jako dodanou).

**Přetažení souborů** (`SDL_EVENT_DROP_FILE`): fronta v `App`, `drop_frame` v `app.cpp` bere jeden soubor za snímek
po liště menu, při modálním okně (`GetTopMostPopupModal`) ho odmítne, když má otázka nebo Chyba teprve vyskočit,
počká snímek. `file_drop` v `media.cpp` volá kód menu: `rack_start_machine` (`.sapi`, `.jsonc`), načtení stavu,
`file_chosen` (odpověď souborového dialogu, sdílená s menu) pro programy a média. Disketa, kazeta RKD-1 a páska
RMP-1 jen do prázdné mechaniky, kazeta DSM-1 a KZD-1 i do plného magnetofonu (jako Vložit…). Snímek: `--ui-drop`.

**Jazyk GUI** (čeština, angličtina): Zobrazení → Jazyk / Language, přepne se hned. Bez uložené volby podle jazyka Windows
(zobrazovací jazyk, ne místní formáty): čeština pro české nebo slovenské Windows, jinak angličtina. `sapiemu --lang cs|en` vybere jazyk jen pro jedno spuštění (neukládá se, hodí se pro
`--ui-snapshot`). Texty GUI jsou v `src/frontend/i18n.cpp` (klíč = anglický text, `tr()`), popisy karet a jumperů
v `src/cards/catalog.cpp` (anglicky i česky). Okna mají ID nezávislé na jazyku (`tr_id()`, „Média###Media“), aby si
po přepnutí držela polohu. Nepřekládají se: názvy a popisy sestav (`.sapi`), hlášky emulovaných programů, popisky
čelního panelu (potisk štítků modernizované skříně Libora Lasoty, podle které se panely kreslí; původní Tesla SAPI-1
měl holé karty za krycím panelem), MCP a `sapiemu-cli`. Test `test_i18n` hlídá i dvojí klíč v tabulce,
`test_i18n_keys` čte `src/` a hledá klíče `tr()` bez českého záznamu a záznamy, které nic nepoužívá (výjimky
s důvodem jsou v testu).
- **Hlášky jádra** (chyby karet, souboru sestavy, ROM, médií a stavu, důvod zastavení dema APS-5) jsou anglicky, jádro
  je sdílené se `sapiemu-cli` a MCP. GUI je překládá až při zobrazení: `tr_core()` podle tabulky `k_core`
  v `i18n.cpp`. Jsou v ní pevné texty a vzory s `%s` (jméno souboru, číslo, další hláška jádra, která se přeloží
  také). Bere se první vzor, který sedí, proto jsou zvláštní vzory před obecnými („cannot create working copy %s: %s“
  před „cannot create %s“). Pak se hledá v tabulce `tr()`. Na „<karta nebo soubor>: <text>“ se přeloží jen text.
  Hláška, která v tabulce není, zůstane anglicky. `test_i18n_keys` hlídá, že každý pevný kus vzoru je v literálech
  `src/` (jinak je záznam mrtvý), `test_i18n` hlídá, že každý vzor přeloží jeho vlastní záznam.
- **Potisk přístrojů** se také nepřekládá (štítky pecí „Pec 1“ APS-5, OCHRANA, NAPĚTÍ… IMS-2). V anglickém rozhraní
  má překlad vedle: legenda grafu a bublina pece („Furnace 1“), bublina nad potiskem IMS-2. Jméno pece ze sestavy
  (`furnaces[].name`) zůstává, jak je napsané.
- **Jméno sestavy** v titulku okna, v Soubor → Poslední sestavy, v konfigurátoru a v otázce Neuložené změny bere
  `machine_title()` z rejstříku `machines/_vyber.json` (`title_en` v angličtině), jen když jméno odpovídá souboru.
  Sestava mimo rejstřík nebo přejmenovaná v konfigurátoru ukáže jméno ze `.sapi`.
- **Texty samotného ImGui** (seznam oken Ctrl+Tab, bublina a menu odkazu, ukotvená okna) registruje
  `imgui_localize()` v `app.cpp` při každé změně jazyka, překlady jsou v `i18n.cpp`.
- **`loc(en, cs)`** jen pro data se dvěma jazyky (katalog karet, problémy sestavy). Hlášení mapy portů a paměti dává
  jádro v obou jazycích, české názvy funkcí karet bere přes překladač z GUI (`machine_map_problems(m, cs_what)`).

**Přístupnost** (od 2026-10-29, etapa 7 plánu vzhledu; důvody v [docs/rozhodnuti.md](https://github.com/mlukasek/SAPIemu/blob/main/docs/rozhodnuti.md), uživatelský přehled v README):
- **Navigace klávesnicí ImGui** (`NavEnableKeyboard`) je zapnutá s `io.ConfigNavCursorVisibleAuto = false`: rámeček
  ukáže až Tab (`nav_cursor_frame()` v `app.cpp`), kliknutí myší ho skryje, bez rámečku mezerník ani Enter nic
  nestisknou. Okna s klávesami SAPI (displeje, celá obrazovka, sériový terminál i jeho text) mají
  `ImGuiWindowFlags_NoNavInputs`, `handle_key` posílá klávesy podle fokusu. S rámečkem na prvku patří Enter jemu
  a `popup_enter()` ustoupí (`nav_cursor_on_item()`).
- **Stav nejen barvou:** kontrolky nakresleného hardwaru mají bublinu se stavem (`lamp_tooltip()`), rozsvícené
  tlačítko magnetofonu značku režimu, graf APS-5 čísla pecí, problémy sestavy ✖ a ⚠. Nová okna to mají dodržet.
- **Rozměry** oken a polí v jednotkách písma (výšky řádku, `combo_width()` podle nejdelší volby), pixely jen
  v kresbách hardwaru.
- **Bubliny** (tooltipy) jen přes `tip(text)` a `tipf(formát, …)` z `theme.h`, ne `ImGui::SetTooltip`: zalomí text
  na asi 35 šířek písma (dřív byla bublina až 1470 px na jednom řádku). Podmínku `IsItemHovered()` píše volající.
- **Nápověda v programu** (etapa 8, prohlížeč `.md` od 2026-10-29): okno Nápověda (`help.cpp`) ukazuje každý `.md`
  z dat. Stránky První kroky a Slovníček (`docs/prvni-kroky.md`, `docs/slovnicek.md`, v angličtině
  `docs/getting-started.md` a `docs/glossary.md`; chybí-li anglický, český; v rozbaleném zipu i `PRVNI-KROKY.md`
  nahoře) mají tlačítka, Návody k obsluze (`k_docs`) seznam; ostatní dokumenty otevřou odkazy, i z průvodců sestavami
  (`MdView::own_link` = `help_link`: `.md` do okna, ostatní soubory programem systému).
  - Historie jako v prohlížeči (`UiState::help_history`, `help_pos`; položka je `first`, `glossary` nebo cesta `.md`
    od kořene dat): odkaz nebo menu nastaví `help_go`, `help_navigate` ji vloží za zobrazenou a zahodí ty před ní
    (nejvýš 50). Zpět a Vpřed: šipky, Alt+šipka vlevo a vpravo (`ImGui::Shortcut`, okno s klávesnicí), boční
    tlačítka myši nad oknem. Každý dokument má vlastní podřízené okno (`text##<položka>`), takže Zpět vrátí i místo
    v textu; sbalené kapitoly si drží ImGui. Stránka je soubor v jazyce rozhraní, odkaz na soubor druhého jazyka
    (anglický slovníček → `docs/slovnicek.md`) otevře ten soubor. Titulek okna: „Nápověda – <nadpis #>“ (`###Help`).
  - Markdown parsuje `markdown.cpp` (bez ImGui, test `test_markdown`) a kreslí `mdview.cpp`, stejně jako průvodce
    sestavami: odstavce s **tučným**, `kódem` a odkazy uvnitř zalomeného textu, odrážky a čísla s odsazením, tabulky
    (sloupec nejméně tak široký jako jeho nejdelší slovo; když se nevejdou, úzké sloupce si je nechají a široké se
    dělí o zbytek; půl bodu tolerance proti zaokrouhlení), kapitoly `##` jako sbalitelné sekce (v Prvních krocích
    první otevřená, jinde všechny), citace `>` se svislou čarou, obrázek sám v odstavci (`MD_IMAGE`, i víc pod sebou)
    jako textura PNG (`SDL_LoadPNG`, načtená jednou) zmenšená na šířku okna, s rámečkem, klik ho otevře v plné
    velikosti; obrázek, který nejde načíst, je odkaz. Odkazy: http(s) v prohlížeči, soubory relativně ke složce
    dokumentu nebo ke kořeni dat (`md_norm_path`: `docs/desky/../img/x.png` → `docs/img/x.png`), `kód` s cestou
    k existujícímu `.md` je odkaz také. Odkaz na soubor, který v kopii není (vývojářské poznámky v zipu), ukáže
    „Soubor … chybí.“.
  - Dlouhé dokumenty: `MdCache` si pamatuje výšky bloků pro šířku a velikost písma, bloky mimo pohled jen zaberou
    místo (rozvrhují se jen viditelné).
  - `package.py` hlídá, že čtyři dokumenty stránek jsou v gitu a v zipu.
  - Snímky: `--ui-help first|glossary|SOUBOR.md`, `--ui-help-open all|none`, `--ui-help-scroll 0..1`, klik na odkaz
    nebo šipku `--ui-click X,Y`.
- **Náhled přístroje:** okno označí nakreslený hardware `picture_mark()`, `--ui-picture` s `--ui-crop` ořízne snímek
  jen na něj (`docs/sestavy.md`).

**Vzhled GUI** (od 2026-10-28, plán [docs/vydani/plan-vzhled.md](https://github.com/mlukasek/SAPIemu/blob/main/docs/vydani/plan-vzhled.md)): Windows 11, světlý i tmavý, jako data
v `src/frontend/theme_data.cpp`. Mechanismus unese víc vzhledů: nový vzhled je dvojice palet, rozměry a řádek
`k_themes`. Varianty Windows klasický a SAPI průmyslový z galerie autor nevybral, jsou v commitu `1fc9a21`.
- **Vzhled:**
  - `theme.cpp` styl pokaždé postaví znovu z čistého `ImGuiStyle`;
  - okna berou sémantické barvy z `theme()` (ok, warn, error, pending, heading…), ne pevné hodnoty;
  - kresby hardwaru (rám, panely, displeje, papír) mají vlastní barvy a pevné písmo `panel_font()` (DejaVu Sans)
    v každém vzhledu.
- **Písma:** Segoe UI se bere ze systému (`%WINDIR%\Fonts`, nesmí se přibalit), bez něj a na Linuxu je to DejaVu Sans.
- **Volba:** Zobrazení → Nastavení → Vzhled → Režim (Podle systému / Světlý / Tmavý; velikost písma je v sekci
  Obecné), uloží se jako `"appearance"`. `sapiemu --ui-theme light|dark` (také `win11-light`, `win11-dark`) vybere režim jen pro jedno
  spuštění. `--ui-snapshot` bez něj kreslí světlý, aby obrázky nezávisely na nastavení Windows. `--ui-scale 1.25`
  napodobí měřítko monitoru.
- **Okna:** plovoucí (výchozí) nebo zadokovaná, popis níže v Oknech GUI.
- **Titulek okna Windows** (`theme_titlebar()` v `theme.cpp`, jen `_WIN32`, na Linuxu ho kreslí správce oken):
  - `DwmSetWindowAttribute`: 20 = světlý nebo tmavý režim (před Windows 10 20H1 číslo 19), ve Windows 11 navíc
    35 = barva titulku jako lišta menu (#F3F3F3 / #202020) a 36 = barva textu. Titulek a lišta menu tak tvoří jeden
    pruh. Neaktivní okno má ztlumený text (`text_dim`), jako ho tlumí Windows. Na Windows 10 35 a 36 neškodně selžou.
    Barvu zvýraznění v záhlaví, pokud ji má uživatel ve Windows zapnutou, vzhled přebije.
  - `dwmapi.dll` se načítá za běhu (`GetProcAddress`), CMakeLists se nemění: `dwmapi.h` by do `theme.cpp` zavlekl
    `rpcndr.h` s makrem `small`. SDL ji načítá stejně, takže je obvykle už v paměti.
  - Nastavuje se po každém `theme_apply()` a při získání a ztrátě fokusu. SDL titulku nastavuje tmavý režim podle
    systému při vytvoření okna a při každé změně barev Windows (`WM_SETTINGCHANGE` „ImmersiveColorSet“,
    `WIN_UpdateDarkModeForHWND`). Přepnutí světlý/tmavý přinese hned potom `SDL_EVENT_SYSTEM_THEME_CHANGED`, po něm se
    nastaví znovu náš. Jinou změnu barev (barva zvýraznění) SDL neohlásí, opraví ji další změna fokusu.
  - `sapiemu --verbose` vypíše výsledky (HRESULT, `0x00000000` = OK) při startu a po každé změně vzhledu, ne při
    změně fokusu, např. `title bar: dark mode 1 0x00000000, caption 0x202020 0x00000000, text 0xA8A8A8 0x00000000`
    (barvy jako COLORREF 0xBBGGRR, text neaktivního okna). Ve Windows 11 (build 26300) vracejí všechna tři volání OK.
- **Měřítko** podle monitoru, na kterém okno je (`SDL_GetDisplayForWindow`). Nové okno dostane velikost podle hlavního
  monitoru, kde ho SDL vycentruje. Po přesunu na monitor s jiným měřítkem nebo po změně měřítka ve Windows
  (`SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED`, `SDL_EVENT_WINDOW_DISPLAY_CHANGED`,
  `SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED`) se styl postaví znovu a v logu je `display scale 150 % (was 100 %)`.
  Okno Windows si drží velikost v pixelech (tak to dělá SDL) a okna ImGui také, zvětší se písmo a rozestupy.
  `--ui-scale` má přednost.
- **Kontrola:** test `test_theme` hlídá kontrast všech palet (WCAG 2.2: text 4,5:1, prvky 3:1).
- **Ladění:** v debug buildu je Ladění → Editor stylu (ImGui `ShowStyleEditor`).
- **Logo v O programu** je maska `docs/img/logo-alpha.png` (z `tools/make_icon.py`) v barvě textu: tmavé písmo na
  světlém okně, bílé na tmavém. `docs/img/logo.png` s černým pruhem zůstává pro README.
- **Galerie vzhledů:** `python tools\ui_gallery.py [--themes win11-light,win11-dark] [--screens 01,16]
  [--ref out\ux-review-20261028]` udělá 16 obrazovek v každém vzhledu (stavy z `sapiemu-cli`, každý snímek s novým
  nastavením). Výsledek je v `out\ui-gallery\<čas>\index.html`. Data jsou z HEAD, sestavy, písma a obrázky
  z pracovního stromu (nové klíče sestav, např. `main_display`).
- **Galerie rozložení oken:** `python tools\ui_gallery.py --layouts [floating,docked] [--sizes 1280x720,1920x1080]
  [--scale 1] [--debug]` udělá 8 obrazovek (první start, ZPS-1 s průvodcem, hry, APS-5, IMS-2, XY 4130 s průvodcem,
  RMP-1 s Médii, ZPS-1 s debuggerem a pamětí) ve světlém vzhledu Windows 11. Sloupce jsou velikost × rozložení,
  rozložení je v nastavení snímku (`appearance.layout`) i v `--ui-layout`. Výsledek je
  v `out\ui-gallery\<čas>-layouts\index.html`.
- **Obrázky do dokumentace:** `python tools\docs_images.py [--out SLOŽKA] [--keep] [jméno…]` znovu udělá snímky GUI
  v `docs/img` (návody, desky, README) ve tmavém vzhledu Windows 11 při 100 %. Každý obrázek má v `PICTURES` recept:
  sestavu, akce `sapiemu-cli` až do stavu, který popisuje text u obrázku, okna GUI a ořez (okna bez lišty menu, nebo
  horní část jednoho okna). Co by člověk nastavil myší (knoflík zdroje BS 525, zátěž), zapíše skript do uloženého
  stavu mezi dvěma běhy cli. Data jsou čistá kopie z HEAD v `out\docs-images\<čas>`, každý snímek má nové nastavení.
  PNG se ukládá s paletou, když ji obrázek snese. Po změně GUI skript spustit s `--out` do jiné složky, obrázky
  prohlédnout a dobré zkopírovat do `docs/img`. Kresby zapisovačů a logo nedělá.
- **GIF nahoře v README:** `python tools\readme_gif.py [cs|en] [--install]` udělá krátkou ukázku (Vybrat sestavu →
  Spustit ZPS-1 → průvodce → Napsat do SAPI → program běží), česky `docs/img/readme/sapiemu-ukazka.gif`, anglicky
  `sapiemu-demo-en.gif`. Snímky jsou skutečné snímky GUI z release buildu nad čistou kopií dat
  v `out\readme-gif\<čas>`, dokreslený je jen kurzor myši a kroužek kliknutí; položka rychlosti ve stavovém řádku
  je vynechaná (snímek by ukázal „--“). Jedna paleta 128 barev, asi 0,85 MB. Bez `--install` zůstane GIF jen
  v pracovní složce, `--install` ho zkopíruje do `docs/img/readme`.

**Okna GUI** (od 2026-10-29, etapa 5 plánu vzhledu; důvody v [docs/rozhodnuti.md](https://github.com/mlukasek/SAPIemu/blob/main/docs/rozhodnuti.md), 2026-10-29 – etapa 5 plánu
vzhledu: okna):
- **Plovoucí rozmístění** (`src/frontend/layout.cpp`, výchozí): jeden pomocník pro všechna okna. Okno dostane místo
  a velikost jen při prvním použití (`ImGuiCond_FirstUseEver`), takže místo z `imgui.ini` má vždy přednost.
  - Tabulka `k_wins`: velikost, nejmenší velikost a role každého okna ve výškách písma.
  - Plocha je `GetBuildWorkRect` po kreslení lišty menu (`WorkPos` ji v prvním snímku ještě nemá).
  - Hlavní výstup (konzole, nebo displej z klíče sestavy `main_display`, `docs/sestavy.md`) plánuje `app.cpp`
    (`layout_main`) vlevo nahoře v největším celočíselném zvětšení do 65 % šířky. Ostatní displeje umisťuje
    `layout_display`, ostatní okna `layout_window` do volného místa (`find_place`, váhy rolí `weight_of`).
  - Samo rozmístění dá displeji nejvýš `display_zoom_cap` = 200 % při každém měřítku Windows (od 2026-10-29 večer,
    autor po galerii všech sestav; ráno round(2 × měřítko), 300 % při 125 a 150 %; 400 % je obludné a kostičkované).
    Zoom zvolený v menu a uložená místa platí dál. Ostatní displeje zkouší zvětšení od stropu dolů po 1:1 (`layout_display`), okno má vždy velikost obrazu
    s malým okrajem (`display_window_size`), bez černé plochy. Pod 1:1 největší volné místo do 3/4. Další displej
    má obraz nejvýš 1,5× širší a vyšší než hlavní výstup (`place_display`; konzole AND-1V her 100 % vedle CGA-1V
    200 %, DGD-1 vedle AND-1 200 %). Displej se zoomem zvoleným v menu (`zs.fixed`, po Přeskládat otevřená okna)
    dostane místo přesně v tom zvětšení (`layout_display(..., exact)`, hlavní výstup i v plánu, klíč plánu ho má).
    Zvětšení dalšího displeje počítá `display_layout_zoom` (i pro změnu sestavy).
  - Pořadí míst stejně dobrých (`Want.order`): displeje vlevo, pak nahoře (pod hlavní výstup, jinak vedle něj),
    průvodce nahoře, pak vlevo (vedle displejů), ostatní vpravo, pak nahoře. Pravý kraj tak zůstane přístrojům,
    Médiím a nástrojům.
  - **Sestava bez displeje** (od 2026-10-29, autor: AR 1990 dostala velké prázdné okno obrazovky, zapisovač byl malý
    a konfigurátor se nevešel): okno `###screen` se kreslí jen u sestavy s displejem konzole (`screen_shown`; vypnutá
    sestava podle té, která běžela naposledy, `UiState::console_last`). Sestava bez jakéhokoli displeje má hlavní
    výstup v prvním okně svého `windows` (`machine_main_window`): `plan_layout` ho ohlásí `layout_main_window`, okno
    dostane při prvním použití místo vlevo nahoře (`layout_window`), zapisovač na výšku plochy a tak široký, aby se
    vešel list A4 (`main_window_size`: 230 × 297 mm pod ovládáním, jehož výšku okno změří při kreslení,
    `UiState::plotter_head`, do té doby 19 výšek písma), jiné okno ve své velikosti. Okna displejů, která sestava
    nekreslí (`###screen` bez displeje konzole, `###gfxN` nad počet jejích grafických displejů), nejsou překážkou
    (`obstacles`), ani když v minulém snímku ještě byla (změna sestavy). Fokus po startu jde do hlavního okna. V doku
    je hlavní okno uprostřed (`dock_layout`, `dock_follow`).
  - Konfigurátor patří do výchozího rozložení s nejnižší prioritou: Výchozí rozložení oken a první start (bez
    `imgui.ini` a bez uloženého `windows.configuration`) ho otevřou jen do volného místa aspoň 44 × 44 výšek písma
    (`layout_window_if_room`, rozhoduje `draw_rack` po ostatních oknech). Při 1280 × 720 se nevejde. Otevřený
    rozložením (`ui.rack_by_layout`, uložený `windows.configuration_auto`) se příštím Výchozím rozložením rozhodne
    znovu. Za „použitý“ ho mají jen kliknutí do něj, klávesa, když má fokus, nebo vyžádání z menu (položka okna,
    změny čekající na restart, Otevřít v konfigurátoru); samotný fokus ne, ten ImGui dává i bez uživatele. Do té
    doby: `NoFocusOnAppearing`, při objevení (i obnovený při dalším startu) za ostatní okna (`layout_to_back`),
    nejnižší priorita (`layout_rack_low` každý snímek: `obstacles` ho vynechá, okna otevřená později ho zakryjí)
    a chyba Zapnout jde i do okna Chyba (`editor_apply`, také když je zakrytý). Zadokované rozložení ho samo
    neotevírá.
  - Pořadí kreslení v `app_run`: hlavní výstup, další displeje, přístroje, Média, průvodce, nástroje, dialogy.
    Okno dostane místo až po oknech, kterým se má vyhnout.
  - Okno Vybrat sestavu má poprvé asi 75 × 80 % plochy, aspoň 58 × 38 výšek písma (od 2026-10-29). Uložená velikost
    platí dál.
  - **Změna sestavy** (`refit_displays` v `app.cpp`, po `plan_layout`, před okny; od 2026-10-29, autor: AND-1Z
    100 % v okně mnohem větším než obraz): okna displejů nové sestavy (`###screen` a `###gfxN` jsou stejná okna pro
    všechny sestavy) si nechají místo a dostanou velikost nového obrazu ve zvětšení, které by dalo rozložení
    (`display_layout_zoom`: hlavní výstup `main_k`, další displej podle stropu, plochy a 1,5× hlavního výstupu),
    s malým okrajem, uvnitř plochy (`layout_refit`, nevejde-li se, menší zvětšení). Okno se zvětšením zvoleným v menu
    (`ZoomState::fixed`) dostane velikost nového obrazu v tomto zvětšení (`zs.size` pro `zoom_track`); nevejde-li se
    na plochu, přizpůsobí se oknu (`fixed` = 0). Ne okno, které uživatel zvětšil rukou
    (`ZoomState::user`: držený úchyt nebo okraj okna po `Begin` v `zoom_track`; volba zvětšení `zoom_resize`, Výchozí
    rozložení oken a Přeskládat otevřená okna příznak smažou), zadokované okno a okno, které teď dostává první místo.
    První sestava běhu nechá velikosti z `imgui.ini`.
  - **Co uživatel nastaví, zůstane** (od 2026-10-29, autor: „když si to někdo nějak nastaví, tak bych mu to
    dovolil“): nastavení ukládá u každého okna displeje (`"displays": { "screen": { "zoom": 200, "by_hand": false },
    "gfx0": … }`) zvětšení zvolené v menu (0 = žádné) a příznak ručně zvětšeného okna. Při startu (`settings_apply`,
    jen plovoucí rozložení, zadokovaný displej řídí dok) dostane okno se zvoleným zvětšením v prvním snímku přesně
    velikost obrazu v něm (`zs.request` → `zoom_resize`, místo z `imgui.ini`), ručně zvětšené okno si velikost
    nechá i při změně sestavy. Ručně roztažené okno se k obrazu nedorovnává (autor nechce uživatele omezovat).
    Přepnutí rozložení stav displejů smaže (druhé rozložení má své velikosti). Restart stejné sestavy a vypnutí a zapnutí nic nemění (podpis `app.fit_sig` = soubor
    a karty displejů s rozměry obrazu). Sestava spuštěná v menu uprostřed snímku se přizpůsobí až v dalším snímku,
    po otevření jejích oken (`machine_side_width`).
  - **Okna mimo plochu** (`layout_keep_inside` po liště menu, od 2026-10-29): když má plocha novou velikost (start,
    zmenšené okno, menší monitor) a vydrží ji 3 snímky, každé plovoucí okno (otevřené i zavřené v tomto běhu)
    a každé uložené místo okna, které ještě nevzniklo (`ImGuiWindowSettings`), víc než napůl mimo plochu se vrátí
    dovnitř s mezerou rozložení, posunuté co nejméně; dialog (`ROLE_DIALOG`: Vybrat sestavu, Nastavení…) i jen
    částečně mimo (`to_bring_inside`; uložený velký výběr sestav by na menší ploše skryl křížek), displej i jen
    částečně mimo, když se vejde celý (uříznutý obraz). Velikost zůstane, jen co se nevejde, se zmenší; displej,
    který se vejde jen bez mezery, jde na kraj plochy ve své velikosti (`back_inside`, obraz si nechá zvětšení).
    Dialog částečně mimo plochu se vrátí dovnitř i při každém otevření (`dialog_inside` v `layout_window`; autor:
    dolní hrana výběru sestav mimo okno, nešel chytit a zvětšit). Ne zadokovaná okna, záznamy podřízených oken (`IsChild`, seznam ve Vybrat sestavu
    má jen velikost), okna s prvním místem a za celé obrazovky nebo minimalizace (rozhodne se po návratu).
  - Plán se přepočítá jen po změně sestavy, plochy, písma nebo měřítka. `layout_hold` za celé obrazovky
    a minimalizace první místa odloží.
  - Zobrazení → Výchozí rozložení oken smaže i záznamy oken, která ještě nevznikla (`WantDelete`).
  - Zobrazení → Přeskládat otevřená okna (`reset_window_layout(app, true)`, příznak `layout_arrange`): totéž jen pro
    otevřená okna (`Active || WasActive`). Nic nezapomíná, zoom zvolený v menu nechá, konfigurátor neotevírá ani
    nezavírá. Otevřené okno přilepené v plovoucím rozložení odlepí jen ono (`SetWindowDock(w, 0)`), ne celý dok.
    V zadokovaném rozložení postaví dok znovu jako Výchozí rozložení, i se zavřenými okny (tooltip to říká).
- **Hledání oken** (od 2026-10-29, autor: „okno se snadno schová za jiné“; `layout.cpp`): okno je **zakryté**, když
  je sbalené, je to nevybraná záložka doku, nebo ho překrývá okno před ním (`g.Windows` je pořadí odzadu dopředu,
  počítají se jen kořenová okna za kořenem toho okna, ne nabídky, popupy, lišta menu, celá obrazovka, plocha doku
  a skryté okno ImGui „Debug“). Okno zadokované do plochy doku (kořen `RootWindowDockTree` má
  `NoBringToFrontOnFocus`) je zakryté jen sbalené nebo jako nevybraná záložka: plovoucí okno před ním nejde dát
  dozadu a položka menu by ho jinak nikdy nezavřela. Zadokované okno se pozná podle `DockNode` a příznaku
  `ChildWindow` z minulého snímku: `DockIsActive` ImGui jedinému oknu uzlu maže od aktualizace uzlu do jeho `Begin`,
  a menu se kreslí mezi tím.
  `layout_to_front` okno rozbalí, v doku vybere jeho záložku, plovoucí okno částečně mimo plochu posune dovnitř
  a dá mu fokus (`FocusWindow` až po `MenuItem`, které při zavření menu vrací fokus předchozímu oknu).
  - Zobrazení → Okna (`layout_open_windows`): otevřená okna v pevném pořadí (displeje, pak podle `LayoutWin`,
    ostatní), displej bez zvětšení v názvu, u zakrytého „zakryté“. Kliknutí: `layout_to_front`.
  - Položky oken v menu Zobrazení a Ladění (`window_item`): zavřené okno otevřou, zakryté dají dopředu (vpravo
    „zakryté“ a tooltip), okno vpředu zavřou. Fajfka dál znamená otevřené. Nastavení…, Klávesové zkratky
    a O programu otevřené okno dají dopředu (`open_window`). Okna přístrojů (`device_item`) jsou povolená, jen když
    sestava přístroj má (`machine_has_device`), nebo když je okno otevřené (položka ho zavře). Vypnutá sestava se
    řídí přístroji sestavy, jak naposledy běžela (`machine_devices_seen` po každé změně sestavy, Zapnout postaví
    stejný popis), před první běžící sestavou jsou povolené všechny. Zakázaná položka
    v zalomeném tooltipu řekne, co chybí a kde je sestava s přístrojem (`device_missing`: kategorie a název
    z rejstříku `_vyber.json`, místo v konfigurátoru). Stejnou větu ukáže i otevřené okno sestavy bez přístroje,
    okno při vypnuté sestavě řekne „Sestava je vypnutá“ (`device_window_off`).
  - Zobrazení → Obrazovky: okna displejů běžící sestavy (`###screen`, `###gfxN`), fajfka = ukázané. Skryté okno se
    nekreslí (`App::display_hidden`), emulace běží dál. Displej použitý naposledy se přesune na ukázaný, a když měl
    před otevřením menu klávesnici (`App::screen_keys`: `screen_focused` posledního snímku bez popupu), i ona. Znovu
    ukázané okno přijde dopředu s klávesnicí. Jiná sestava (jiný soubor nebo jiné displeje, `displays_sig`) ukáže
    všechny, restart a vypnutí a zapnutí téže ne. Neukládá se. Celá obrazovka ukáže i skrytý displej vybraný v jejím
    menu.
- **Zadokované rozložení:** Zobrazení → Nastavení → Vzhled → Okna, uloží se jako `"appearance": { "layout":
  "docked" }`. Staví ho `layout_dock()` v `layout.cpp` přes DockBuilder: hlavní výstup uprostřed, sloupec u pravého
  kraje (nahoře ostatní displeje, pod nimi okna stroje, dole průvodce a ladicí okna jako záložky). Postaví se jen bez
  svého souboru míst, volbou a Výchozím rozložením oken. Po změně sestavy jen `layout_dock_main()` vymění hlavní
  výstup nové sestavy s displejem uprostřed, ostatní okna a velikosti částí zůstanou. Sloupec nevezme hlavnímu
  výstupu šířku, kterou má v plovoucím rozložení; otevřený průvodce má aspoň 12 výšek písma.
- **Soubory míst oken:** plovoucí `imgui.ini`, zadokované `imgui-docked.ini` (u `--settings SOUBOR` je to
  `SOUBOR.imgui.ini` a `SOUBOR.imgui-docked.ini`). Volba v Nastavení mezi dvěma snímky uloží místa opouštěného
  rozložení do jeho souboru, smaže nastavení ImGui (`ClearIniSettings`) a načte soubor zvoleného
  (`LoadIniSettingsFromDisk`, okna bez místa v něm dostanou výchozí). Bez souboru se rozložení postaví jako
  Zobrazení → Výchozí rozložení oken.
- **Okno programu:** při prvním startu 88 % použitelné plochy hlavního monitoru (`usable_area` v `app.cpp`), uložená
  velikost se zmenší do použitelné plochy svého monitoru.
- **Celá obrazovka** (Alt+Enter, zachycený v `handle_key` dřív, než Enter dojde do SAPI): `SDL_SetWindowFullscreen`
  (okno bez rámu přes plochu). `draw_fullscreen` kreslí displej použitý naposledy. Ostatní okna i lišta menu se dál
  kreslí a před `Render` dostanou příznak Hidden: jejich práce před `Begin` běží dál, místa a dok zůstanou. Neukládá
  se.
- **Displej menší než 1:1** se kreslí s lineárním filtrem, od 1:1 ostře (pokud není zapnuté Vyhlazování).
- **Volby:** `--ui-layout floating|docked` vybere rozložení jen pro jedno spuštění. Když se liší od uloženého, začne
  z výchozích míst a soubory míst nečte ani nezapisuje. Pro snímky jsou `--ui-layout-switch` (přepne jako volba
  v Nastavení), `--ui-fullscreen` (Alt+Enter, okno zůstane velikosti `--ui-size`), `--ui-zoom`, `--ui-reset-layout`,
  `--ui-arrange` (Přeskládat otevřená okna), `--ui-window-item KLÍČ` (kliknutí na položku okna `###KLÍČ` v menu
  Zobrazení nebo Ladění, opakovaně po sobě), `--ui-front KLÍČ` (to okno v Zobrazení → Okna), `--ui-switch SESTAVA`
  (spuštění další sestavy za běhu, opakovaně po sobě), `--ui-hide-display KLÍČ` (Zobrazení → Obrazovky skryje okno
  `screen`, `gfx0`…), `--ui-second` (varování druhého okna nad stejným nastavením) a `--ui-menu View/Windows`
  (otevřená podnabídka). `--ui-key` u znakové klávesy pošle i její znak (událost textu jako klávesnice). Etapa 8:
  `--ui-drop SOUBOR` (soubor přetažený na okno jako z Průzkumníka: `SDL_EVENT_DROP_FILE`, opakovaně po sobě),
  `--ui-click X,Y` (kliknutí levým tlačítkem, body okna, opakovaně po sobě), `--ui-save` (Uložit v konfigurátoru po
  `--ui-set`) a `--ui-save-copy` (Uložit jako build pro vydání i v ladicím buildu: dodaná sestava jako kopie do Mých
  sestav, výše). Popis voleb `--ui-*` je v `docs/sestavy.md`. **Pozor:** snímek běží nad skutečným `work/`; přepnutí
  ze sestavy se zálohovanou pamětí (APS-5, DMR-1, sestavy V) ji zapíše (2026-10-29 tak snímek přepsal
  `work/aps-5/crm.bin`). Takové snímky dělat nad kopií dat (`--data-root`).

## Linux (ověřeno ve WSL 2, 2026-10-06)

Primární je Windows build. Linux se zatím zkouší ve WSL 2 s Ubuntu 24.04 (WSLg zobrazí i okno GUI a zvuk):

1. Balíčky (jednou, vyžaduje `sudo`): `sudo apt install -y build-essential cmake ninja-build pkg-config
   libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev
   libwayland-dev libdecor-0-dev libgl1-mesa-dev libegl1-mesa-dev libpulse-dev libasound2-dev libudev-dev
   libdbus-1-dev`. SDL3 se stáhne přes FetchContent jako na Windows.
2. **Repo na linuxovém disku**, ne na `/mnt/e` (tam je překlad ve WSL velmi pomalý): např.
   `git clone /mnt/e/SAPI_GIT/SAPIemu ~/SAPIemu`. Podklady vedle repa jako u autora: `ln -s
   /mnt/e/SAPI_GIT/SAPI-karty ~/SAPI-karty` a `ln -s /mnt/e/SAPI_GIT/SAPI_DOKU ~/SAPI_DOKU` (testy MPH-1V hledají
   `../SAPI-karty`, testy kazet nahrávky v `../SAPI_DOKU/Kazetopáskové vybavení AUDIO WAV`).
3. `cmake --preset linux-debug && cmake --build --preset linux-debug`, testy `ctest --preset linux-debug`
   (paralelně jako na Windows, asi 16 s).
4. GUI: `out/build/linux-debug/sapiemu` (ve WSL se okno ukáže na ploše Windows). Nastavení se ukládá vedle exe,
   jinak do `~/.local/share/SAPIemu/SAPIemu`. COM port je na Linuxu zařízení (`/dev/ttyUSB0`), ve WSL fyzické porty
   nejsou. Jazyk GUI podle `LANG` (`cs_*`, `sk_*` = čeština).

Výsledek 2026-10-06: všech 37 testů prošlo, včetně skutečných nahrávek kazet (TOOL 48, TOOL 80, BASIC EX),
`test_tapegen` a `test_jpr1x`. Naposledy 2026-11-01 před vydáním 0.7.1-beta: 70 testů, překlad bez varování GCC. 2026-10-31 po DSE-1 68 testů,
předtím 2026-10-30
po betě 0.7.0: 62 testů, překlad 248 souborů od nuly; sedm nových
varování GCC ve vlastním kódu opravených (`snprintf` do malých bufferů, `|` dvou typů enum ImGui, nepoužitá
proměnná). Dřív 2026-10-17: 58 testů (2026-10-16 před 0.6.0: 55, 2026-10-14 před 0.5.0: 53, 2026-10-09 před 0.4.0: 44; čisté
sestavení našlo dvě varování v kódu DPS-1, opravená; distribuce se ve WSL jmenuje `Ubuntu`,
`wsl -d Ubuntu`, klon aktualizuje `git pull /mnt/e/SAPI_GIT/SAPIemu main`). Čisté sestavení (`cmake --build --preset linux-debug --clean-first`) je bez varování
GCC ve vlastním kódu (varování v `third_party/` a `_deps/` se neřeší).
Testy `test_cassette` a `test_mikos` je hledají v `../SAPI_DOKU`, jinak tu část
přeskočí. Co GCC na rozdíl od MSVC nepromine (pro další vývoj):
- `snprintf` do bufferu, do kterého by se celý výstup nemusel vejít (`"###gfx%d"` do 16 bajtů, `uname` do 160):
  buffer dimenzovat na nejhorší případ (`-Wformat-truncation`);
- `|` mezi dvěma různými typy enum (veřejné a privátní příznaky ImGui): jeden z nich přetypovat na typ příznaků
  (`(ImGuiDockNodeFlags)`), v C++20 je to zastaralé;
- statická konstanta třídy použitá odkazem (`std::min`, JSON) bez definice: psát `static constexpr int`, ne
  `static const int` (linker hlásí `undefined reference to And1::COLS`);
- záměrný průchod do dalšího `case` (makro protothreadu v `pmd32sd.cpp`) označit `[[fallthrough]]`, pomocné
  proměnné testů `[[maybe_unused]]`;
- doslovný znak CR uvnitř řetězce (`"…^M"` místo `"\r"`): GCC ho bere jako konec řádku. Git navíc takový soubor
  považuje za binární a nenormalizuje mu konce řádků.

## Lokální preset (necommituje se)

Když repo leží na síťovém disku, je rychlejší ukládat výstupy sestavení na lokální disk. Stačí v kořeni repa vytvořit
`CMakeUserPresets.json`:

```json
{
  "version": 6,
  "configurePresets": [
    { "name": "local-debug", "displayName": "Local x64 Debug (build on C:)", "inherits": "windows-debug",
      "binaryDir": "C:/SAPIemu-build/${presetName}" },
    { "name": "local-release", "displayName": "Local x64 Release (build on C:)", "inherits": "windows-release",
      "binaryDir": "C:/SAPIemu-build/${presetName}", "cacheVariables": { "SAPIEMU_SOURCE_FALLBACK": "ON" } }
  ],
  "buildPresets": [
    { "name": "local-debug", "configurePreset": "local-debug" },
    { "name": "local-release", "configurePreset": "local-release" }
  ],
  "testPresets": [
    { "name": "local-debug", "configurePreset": "local-debug", "output": { "outputOnFailure": true } },
    { "name": "local-release", "configurePreset": "local-release", "output": { "outputOnFailure": true } }
  ]
}
```

**Kde emulátor hledá data** (`roms/`, `machines/`, `media/`): od složky s exe nahoru, pak od aktuální složky
nahoru. Build mimo repo (lokální preset na C:) by je nenašel, proto má CMake volbu `SAPIEMU_SOURCE_FALLBACK`:
zapnutá zakompiluje cestu ke zdrojákům jako poslední možnost. Debug ji má zapnutou, presety release vypnutou
(exe v `out\build` data najde i bez ní). Zabalené exe se tak nikdy nevrátí k datům autorovy pracovní kopie
(2026-10-07; ověřeno kopií release a debug cli mimo repo). `local-release` výše ji proto zapíná.

## Přesun repa do jiné složky

CMake si do `CMakeCache.txt` ukládá absolutní cesty ke zdrojům, takže po přesunu repa (nebo jeho kopii jinam)
sestavení ve staré výstupní složce selže. Postup:

1. Zavřít Visual Studio.
2. Odložit nastavení GUI ze složky s exe (`out\build\<preset>\sapiemu-settings.json`, `imgui.ini`).
3. Smazat `out\build\<preset>` (u lokálního presetu jeho složku mimo repo), případně ve VS Project → Delete Cache
   and Reconfigure.
4. Sestavit znovu (`tools\build.cmd debug test`) a nastavení GUI vrátit. V `sapiemu-settings.json` může být
   absolutní cesta (`cassette.last_dir`), ta se opraví ručně nebo při dalším otevření kazety.

Složka `.vs\` je jen mezipaměť VS a znovu se vytvoří. Pracovní kopie médií ve `work\` se přesouvají s repem.

## Datová složka

Emulátor hledá složku s `roms/` a `machines/`: nejdřív od složky s exe směrem nahoru, pak od aktuální složky
a nakonec ve zdrojové složce, ze které byl sestaven. Jinou složku lze zadat parametrem `--data-root`.

## Příklady pro režim bez okna

```
sapiemu-cli --wait-text "V1.3" --type "?\r" --run-ms 300 --print-screen
sapiemu-cli --machine machines/mikromon.sapi --run-ms 500 --screenshot obrazovka.png
sapiemu-cli --keyboard "Consul 262.3 (bez 7474)" --wait-text "V1.3" --type "M F000\r" --run-ms 500 --print-screen
sapiemu-cli --machine machines/zps/ZPS-2_MIKOS_V5.sapi --run-ms 1000 --load-hex media/mikos/BASIC_EXP_V5.0G.hex \
    --type B --run-ms 2000 --type "CLSG:MOVE 0,0:PLOT 319,239\r" --run-ms 3000 --display DGD-1 --screenshot grafika.png
sapiemu-cli --machine machines/sapi1v.sapi --wait-text "Zadej 0-3" --type 3 --wait-text "A>" \
    --load-hex media/mph-1v/splay.hex --type "SAVE 4 SPLAY.COM\r" --run-ms 1500 \
    --load-hex ../SAPI-karty/MPH-1V/SPLAY/hellfire.hex --type "SAVE 28 HELLFIRE.VGM\r" --run-ms 1500 \
    --audio-wav hellfire.wav --type "SPLAY HELLFIRE.VGM\r" --run-ms 30000
sapiemu-cli --machine machines/plotter_ar1990.sapi --run-ms 1000 --plotter ready --run-ms 6000 \
    --plotter test --run-ms 20000 --print-plotter --plotter-png test_per.png
sapiemu-cli --machine machines/zps/ZPS-2_MIKOS_V5_XY4130.sapi --timeout-ms 10000 --wait-text L=XY4130 --run-ms 5000 \
    --plotter pen --load-hex media/mikos/XY4130_DEMO.hex --type "G8000\r" --run-ms 25000 --print-plotter
```

`--plotter AKCE [ARG]` mačká tlačítka zapisovačů jako okno Minigraf (kód `src/core/plotter_ctl.cpp` sdílí okno, MCP
`plotter` i cli): `pen` (PEN na XY 4130), `new-sheet` (nový papír), u plotteru z AR 1990 `ready`, `test`, `stop`
(drženo 150 ms emulovaného času), `whole-test` (TEST držené přes reset JPR-1: celá znaková sada), `demo`
(`media/ar1990/ukazka.txt`), `send TEXT` (řádek povelů jako v okně: `\xNN` = bajt, na konci CR) a `send-file SOUBOR`.
Plotter z AR 1990 po zapnutí pípne a čeká na READY (asi 1 s), počátek pak hledá asi 5 s, TEST nakreslí 28 tahů
62 x 12 mm za asi 16,5 s. `--print-plotter` vypíše kontrolky (RDY, BUFF / READY, PEN), bajty čekající a převzaté,
pero, počet tahů, rozměr kresby a kroky, `--plotter-png` a `--plotter-svg SOUBOR` uloží kresbu s okrajem 5 mm.

`--audio-wav` nahrává zvuk zvukové karty (MPH-1V) od svého místa v akcích do konce (WAV mono, 49716 Hz).
`--joystick` drží joystick (konektor K4 MPH-1V nebo ANK-1M2): `--joystick fire1 --run-ms 300 --joystick -`
zmáčkne a pustí fire, směry `down,up,right,left`, druhý joystick `1:` na začátku (`1:left,fire1`).
Pozor: sestava V zapisuje změny RAM disku do `work/`, na zkoušky je lepší kopie sestavy s ZRD-1V `"mode": "nosave"`.

Úplný seznam akcí vypíše `sapiemu-cli --help`.

## Vydání (release)

1. **Verze:** `project(SAPIemu VERSION ...)` a `SAPIEMU_VERSION_SUFFIX` (`-beta`, dřív `-alpha`) v `CMakeLists.txt`.
   `sapiemu --version` ji ukáže, na Windows je i ve vlastnostech exe (VERSIONINFO z `src/res/sapiemu.rc.in`, ikona
   `src/res/sapiemu.ico` z `tools/make_icon.py`).
2. **Build:** `tools\build.cmd release test`. Runtime MSVC je přilinkovaný (`CMAKE_MSVC_RUNTIME_LIBRARY`), takže exe
   nepotřebuje Visual C++ Redistributable.
3. **Balík:** `python tools\package.py` vytvoří `out\package\SAPIemu-<verze>-win64.zip`:
   - exe, `roms\`, `media\`, `machines\`, `docs\`, fonty, licence knihoven, `PRVNI-KROKY.md` (česky,
     z `docs\prvni-kroky.md`) a `GETTING-STARTED.md` (anglicky, z `docs\getting-started.md`);
   - vedle zipu zapíše `SAPIemu-<verze>-win64.zip.sha256` s jeho SHA-256 ve formátu `sha256sum` (`<hex>  <jméno>`)
     a vypíše ho; ověří se `sha256sum -c …zip.sha256`, na Windows `certutil -hashfile …zip SHA256`;
   - jen soubory, které zná git, takže nové soubory nejdřív `git add`;
   - dokumenty okna Nápověda (`HELP_DOCS`: `docs/prvni-kroky.md`, `getting-started.md`, `slovnicek.md`,
     `glossary.md`) musí být v gitu a v `docs\` zipu, jinak zip není hotový;
   - exe jsou vždy z `out\build\windows-release`: tam Uložit v konfigurátoru dodanou sestavu nepřepíše, ale uloží
     kopii do Mých sestav (`work\machines`, `save_copies()`), ladicí build ji přepisuje;
   - skript zip rozbalí do čisté složky a ověří, že z ní naběhne MikroMon;
   - odmítne build se zapnutým `SAPIEMU_SOURCE_FALLBACK` (balík nesmí sahat do repa);
   - přidá i IDE disk tohoto PC (`work\ide\*.img`) jako `work\ide\`. Sestava V a
     `cpm_ide.sapi` pak mají na C: připravené programy. Na disku je i hudba: pro veřejný release
     `--no-ide`;
   - **`--public`** (beta a veřejné vydání): bez IDE disku, bez souborů `REPO_ONLY` (Prince of Persia) a bez
     vývojářských poznámek `DEV_DOCS` (`docs/SOUHRN.md`, `docs/rozhodnuti.md`, `docs/vydani/`), zip se na to
     zkontroluje;
   - přibalí `THIRD-PARTY-NOTICES.txt`. Po aktualizaci knihovny ho přegenerovat: `python tools\third_party_notices.py`
     (licenci SDL3 zkopírovat z `out\build\<preset>\_deps\sdl3-src\LICENSE.txt` do `third_party\sdl3\`).
4. **Obrázky do dokumentace a README** (`docs\img\`) po změně GUI: `python tools\docs_images.py --out <složka>`
   a GIF nahoře v README `python tools\readme_gif.py` (oba popsané výše u galerie vzhledů). Výsledek prohlédnout
   a dobré zkopírovat do `docs\img`, GIF přes `--install`. Fotky a snímky autora v `docs\img\readme\` se nedělají
   skriptem. Ručně jde snímek udělat skrytým oknem GUI:
   - připravit stav přes `sapiemu-cli ... --save-state scena.sapistate`;
   - `sapiemu --settings s.json --ui-snapshot obr.png --ui-state scena.sapistate --ui-size 1540x572`;
   - rozložení oken dává `s.json.imgui.ini`, třeba `[Window][x###screen]` a `[Window][x###gfx0]` s `Pos=` a `Size=`;
   - naše PNG nejsou komprimovaná, před commitem je přeuložit se zlib.
5. **Poznámky k vydání:** `docs\release-notes\v<verze>.md`.
6. **GitHub:** tag `v<verze>`, Releases → Draft a new release, Pre-release, vložit poznámky, zip a soubor `.sha256`.

## Tipy pro vývoj (postupy, které se osvědčily)

- **Nová sestava z JSON:** `test_machine_cfg <soubor.json> > machines/....sapi` převede prostý JSON na
  komentovaný `.sapi`. Komentáře se doplní z katalogu karet.
- **Skriptování přes MCP:** `sapiemu-cli --machine X --mcp --mcp-port 8591` (na pozadí). Nástroje se volají
  JSON-RPC POSTem na `http://127.0.0.1:8591/mcp`:
  - `pause` a `run_for` pro deterministický běh;
  - `type_text` s `run_ms`, `run_until`, `screen_text`;
  - `tape_insert`, `tape_control`, `load_hex`;
  - `io_log`: záznam I/O, kruhový buffer 4096 přístupů, takže polling ho rychle zaplní;
  - `io_read`, `io_write`: přímý přístup na porty karet, např. obsah RAM disku.
- **Testovací programy pro SAPI:** `python tools/asm8080.py ZDROJ.ASM VYSTUP.HEX [VYSTUP.COM]` je malý
  assembler 8080 (syntaxe M80: ORG, EQU, DB, DW, DS, výrazy). Píše Intel HEX a vypíše počet stránek pro `SAVE`.
  Použitý pro `media/zrd-1v/ZRDTEST.ASM`.
- **Programy pro CP/M na PC (`cpmrun`):** `out/build/<preset>/cpmrun.exe [-d SLOŽKA] PROGRAM [parametry]` spustí
  `.COM` pro CP/M 2.2 nad Z80 jádrem emulátoru (`tools/cpmrun.cpp`). Konzole jde na stdout/stdin, soubory jsou ve
  složce pro všechny jednotky. BDOS umí funkce 0–40, které potřebují překladače (sekvenční a náhodné soubory,
  hledání s `?`, mazání, přejmenování). Slouží k překladu zdrojáků SAPI v M80/L80, třeba
  `cpmrun M80 =ECMONM/L`, `cpmrun L80 /P:0,X,X/N/X/E`. stdin jsou klávesy napsané dopředu: stav konzole
  (BDOS 11, 6, BIOS CONST) je hlásí. Když program čeká na řádek z konzole a stdin skončil, `cpmrun` skončí.
  `CPMRUN_TRACE=1` vypisuje volání BDOS (soubory a vstup z konzole) a BIOS. Test `test_cpmrun`. Použití:
  `..\SAPI-karty\ZRMD-1\SRC\build.py` (`docs/software/ECMON.md`).
  - **L80 s ROM na 0000h** (`/P:0`) se ptá „Origin below loader memory, move anyway (Y or N)?“. HEX je už
    zapsaný, odpovědět **N** (stdin `N`). Y přesune kód přes nultou stránku CP/M a L80 se pak zacyklí.
  - **Z Git Bash** se parametr začínající `/` (třeba `/P:0,...`) přepíše na cestu Windows. Pomůže
    `MSYS2_ARG_CONV_EXCL='*'` před příkazem, z cmd, PowerShellu nebo Pythonu to nevadí.
- **`sapiemu-cli`:** `--timeout-ms` musí být **před** `--wait-text`, kterého se týká. `--load-hex` nahraje
  program, `--load-bin SOUBOR ADRESA` binární soubor (adresa hex, třeba `100`), `--display DGD-1` určí obrazovku
  pro `--screenshot`. `--save-state` / `--load-state SOUBOR` uloží a načte stav stroje (`docs/stav.md`).
  `--power-cycle` vypne a zapne stroj jako Stroj → Vypnout a zapnout (Ctrl+F12): paměť se ztratí, média a paměti
  s baterií se uloží a načtou znovu (od 2026-10-29).
- **Pracovní kopie médií** (`work/rpd-1/…`, `work/…`) po zkouškách z příkazové řádky smazat. Zápisy na disketu
  se do nich uloží a příště by se načetly místo originálu.
- **Průběžné ukládání médií** (od 2026-10-29): GUI a `sapiemu-cli --mcp` (oba přes `EmuHost::autosave`) zapíšou
  zapsaná média do souborů 3 s po posledním zápisu, při trvalém zápisu nejpozději po minutě, hned při pauze
  a GUI i při ztrátě fokusu okna. Akce samotného `sapiemu-cli` (bez `--mcp`) ukládají jako dřív až při konci,
  vysunutí nebo `--power-cycle`. Zápis jde přes dočasný soubor `jméno.tmpNNN` vedle obrazu a přejmenování
  (`file_write_safe` v `core/util`); zbylé `*.tmpNNN` hlásí `check_machines.py`. Na zkoušky s MCP proto kopie
  sestavy s médii `nosave` (RAM disk `"mode": "nosave"`), jinak se `work/` přepíše už za běhu.
- **Úpravy souborů skriptem:** na Windows v Bash heredocu se `\n`, `\r` a `\\` v Python řetězcích mění na
  skutečné znaky. Skripty proto psát nástrojem pro zápis souborů do dočasného `.py`, pro cesty používat raw
  řetězce, nebo upravovat soubory přímo editorem.
- **Rozbor schémat v PDF:** Python PyMuPDF (`pymupdf.open`, `page.get_pixmap(dpi=..., clip=...)`)
  vyrenderuje výřez schématu do PNG. Eagle `.sch` z let kolem 2011 je binární a nečte se.
- **RAR archivy ze sapi.cz** rozbalí `C:\Windows\System32\tar.exe -xf soubor.rar`.
- **Testy se skutečnými nahrávkami nebo disketami mimo repo** (archiv `..\SAPI_DOKU`, desky v `..\SAPI-karty`) se při chybějícím souboru
  přeskočí. Testy pracují s kopiemi ve `work/test/`.

