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
a `test_i18n_keys`, ten i po přidání nebo smazání `tr()` kdekoli v `src/frontend`); MCP → `mcp`; stav → `state`.

Testy smějí běžet souběžně: každý zapisuje jen do vlastních souborů ve `work/test*` (složky vytvoří `test_init()`),
pracovní kopie médií ve `work/` jen čte. Pracovní kopie vzniká pod dočasným jménem a přejmenuje se, takže
ji souběžně nevytvoří dva procesy napůl (dřív na čistém `work/` padaly `test_zps3`, `test_state`).

Klávesy v GUI: F5 běh/pauza, F8 rychlost emulace (100 % → 200 % → 400 % → maximum, dokola), F10 krok přes, F11 krok, F12 reset (pod debuggerem PC si F12 bere Windows, pak Stroj → Reset nebo tlačítko Reset v debuggeru), F9 breakpoint v debuggeru (do 2026-10-29 reset), Ctrl+F12 vypnout a zapnout, Alt+Enter celá obrazovka.

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

**Jazyk GUI** (čeština, angličtina): Zobrazení → Jazyk / Language, přepne se hned. Bez uložené volby podle jazyka Windows
(zobrazovací jazyk, ne místní formáty): čeština pro české nebo slovenské Windows, jinak angličtina. `sapiemu --lang cs|en` vybere jazyk jen pro jedno spuštění (neukládá se, hodí se pro
`--ui-snapshot`). Texty GUI jsou v `src/frontend/i18n.cpp` (klíč = anglický text, `tr()`), popisy karet a jumperů
v `src/cards/catalog.cpp` (anglicky i česky). Okna mají ID nezávislé na jazyku (`tr_id()`, „Média###Media“), aby si
po přepnutí držela polohu. Nepřekládají se: názvy a popisy sestav (`.sapi`), hlášky emulovaných programů, popisky
čelního panelu (potisk štítků modernizované skříně Libora Lasoty, podle které se panely kreslí; původní Tesla SAPI-1
měl holé karty za krycím panelem), MCP a `sapiemu-cli`. Test `test_i18n` hlídá i dvojí klíč v tabulce,
`test_i18n_keys` čte `src/` a hledá klíče `tr()` bez českého záznamu a záznamy, které nic nepoužívá (výjimky
s důvodem jsou v testu).
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

**Přístupnost** (od 2026-10-29, etapa 7 plánu vzhledu; důvody v `docs/rozhodnuti.md`, uživatelský přehled v README):
- **Navigace klávesnicí ImGui** (`NavEnableKeyboard`) je zapnutá s `io.ConfigNavCursorVisibleAuto = false`: rámeček
  ukáže až Tab (`nav_cursor_frame()` v `app.cpp`), kliknutí myší ho skryje, bez rámečku mezerník ani Enter nic
  nestisknou. Okna s klávesami SAPI (displeje, celá obrazovka, sériový terminál i jeho text) mají
  `ImGuiWindowFlags_NoNavInputs`, `handle_key` posílá klávesy podle fokusu. S rámečkem na prvku patří Enter jemu
  a `popup_enter()` ustoupí (`nav_cursor_on_item()`).
- **Stav nejen barvou:** kontrolky nakresleného hardwaru mají bublinu se stavem (`lamp_tooltip()`), rozsvícené
  tlačítko magnetofonu značku režimu, graf APS-5 čísla pecí, problémy sestavy ✖ a ⚠. Nová okna to mají dodržet.
- **Rozměry** oken a polí v jednotkách písma (výšky řádku, `combo_width()` podle nejdelší volby), pixely jen
  v kresbách hardwaru.
- **Náhled přístroje:** okno označí nakreslený hardware `picture_mark()`, `--ui-picture` s `--ui-crop` ořízne snímek
  jen na něj (`docs/sestavy.md`).

**Vzhled GUI** (od 2026-10-28, plán `docs/vydani/plan-vzhled.md`): Windows 11, světlý i tmavý, jako data
v `src/frontend/theme_data.cpp`. Mechanismus unese víc vzhledů: nový vzhled je dvojice palet, rozměry a řádek
`k_themes`. Varianty Windows klasický a SAPI průmyslový z galerie autor nevybral, jsou v commitu `1fc9a21`.
- **Vzhled:**
  - `theme.cpp` styl pokaždé postaví znovu z čistého `ImGuiStyle`;
  - okna berou sémantické barvy z `theme()` (ok, warn, error, pending, heading…), ne pevné hodnoty;
  - kresby hardwaru (rám, panely, displeje, papír) mají vlastní barvy a pevné písmo `panel_font()` (DejaVu Sans)
    v každém vzhledu.
- **Písma:** Segoe UI se bere ze systému (`%WINDIR%\Fonts`, nesmí se přibalit), bez něj a na Linuxu je to DejaVu Sans.
- **Volba:** Zobrazení → Nastavení → Vzhled (Podle systému / Světlý / Tmavý, velikost písma), uloží se jako
  `"appearance"`. `sapiemu --ui-theme light|dark` (také `win11-light`, `win11-dark`) vybere režim jen pro jedno
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

**Okna GUI** (od 2026-10-29, etapa 5 plánu vzhledu; důvody v `docs/rozhodnuti.md`, 2026-10-29 – etapa 5 plánu
vzhledu: okna):
- **Plovoucí rozmístění** (`src/frontend/layout.cpp`, výchozí): jeden pomocník pro všechna okna. Okno dostane místo
  a velikost jen při prvním použití (`ImGuiCond_FirstUseEver`), takže místo z `imgui.ini` má vždy přednost.
  - Tabulka `k_wins`: velikost, nejmenší velikost a role každého okna ve výškách písma.
  - Plocha je `GetBuildWorkRect` po kreslení lišty menu (`WorkPos` ji v prvním snímku ještě nemá).
  - Hlavní výstup (konzole, nebo displej z klíče sestavy `main_display`, `docs/sestavy.md`) plánuje `app.cpp`
    (`layout_main`) vlevo nahoře v největším celočíselném zvětšení do 65 % šířky. Ostatní displeje umisťuje
    `layout_display`, ostatní okna `layout_window` do volného místa (`find_place`, váhy rolí `weight_of`).
  - Pořadí kreslení v `app_run`: hlavní výstup, další displeje, přístroje, Média, průvodce, nástroje, dialogy.
    Okno dostane místo až po oknech, kterým se má vyhnout.
  - Plán se přepočítá jen po změně sestavy, plochy, písma nebo měřítka. `layout_hold` za celé obrazovky
    a minimalizace první místa odloží.
  - Zobrazení → Výchozí rozložení oken smaže i záznamy oken, která ještě nevznikla (`WantDelete`).
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
  v Nastavení), `--ui-fullscreen` (Alt+Enter, okno zůstane velikosti `--ui-size`), `--ui-zoom` a `--ui-reset-layout`.
  Popis voleb `--ui-*` je v `docs/sestavy.md`.

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
`test_tapegen` a `test_jpr1x`. Naposledy 2026-10-17: 58 testů (2026-10-16 před 0.6.0: 55, 2026-10-14 před 0.5.0: 53, 2026-10-09 před 0.4.0: 44; čisté
sestavení našlo dvě varování v kódu DPS-1, opravená; distribuce se ve WSL jmenuje `Ubuntu`,
`wsl -d Ubuntu`, klon aktualizuje `git pull /mnt/e/SAPI_GIT/SAPIemu main`). Čisté sestavení (`cmake --build --preset linux-debug --clean-first`) je bez varování
GCC ve vlastním kódu (varování v `third_party/` a `_deps/` se neřeší).
Testy `test_cassette` a `test_mikos` je hledají v `../SAPI_DOKU`, pak ve starém archivu na `H:`, jinak tu část
přeskočí. Co GCC na rozdíl od MSVC nepromine (pro další vývoj):
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

1. **Verze:** `project(SAPIemu VERSION ...)` a `SAPIEMU_VERSION_SUFFIX` (`-alpha`) v `CMakeLists.txt`.
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
4. **Obrázky do README** (`docs\img\`) se dělají skrytým oknem GUI:
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
- **Testy se skutečnými nahrávkami nebo disketami mimo repo** (`H:\SAPI-1\…`) se při chybějícím souboru
  přeskočí. Testy pracují s kopiemi ve `work/test/`.

