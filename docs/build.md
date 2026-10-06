# Sestavení a spuštění

## Visual Studio 2022

1. Soubor → Otevřít → Složka, vybrat kořen repa.
2. Vybrat preset (`Windows x64 Debug`, případně lokální `Local x64 Debug`).
3. Jako Startup Item zvolit `sapiemu.exe` (GUI), nebo `sapiemu-cli.exe` (bez okna).

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

Dotčené testy podle oblasti: deska nebo sestava → testy, které ji načítají (`grep machines/… tests/*.cpp`); diskové
řadiče → `fdc_ecmon|floppy_imd|sapi_sys|drive_letters|rpd1|zps3|zps4`; GUI texty → `i18n`; MCP → `mcp`; stav → `state`.

Testy smějí běžet souběžně: každý zapisuje jen do vlastních souborů ve `work/test*` (složky vytvoří `test_init()`),
pracovní kopie médií ve `work/` jen čte. Pracovní kopie vzniká pod dočasným jménem a přejmenuje se, takže
ji souběžně nevytvoří dva procesy napůl (dřív na čistém `work/` padaly `test_zps3`, `test_state`).

Klávesy v GUI: F5 běh/pauza, F8 rychlost emulace (100 % → 200 % → 400 % → maximum, dokola), F10 krok přes, F11 krok, F12 nebo F9 reset (pod debuggerem jen F9).

**Rychlost a takt** jsou dvě různé věci:
- **Takt CPU** je vlastnost karty: u JPR-1ZI propojky JP2/JP3 v sestavě, u JPR-1V přepínač TURBO (2/4 MHz) i za
  chodu (tlačítko TURBO na panelu ZDR, menu Stroj, MCP `cpu_turbo`).
- **Rychlost emulace** zrychlí celý emulovaný počítač proti reálnému času (F8, menu Stroj → Rychlost emulace, MCP
  `resume speed_percent`/`max_speed`), tedy i UART, disketu a časování karet.
- Stavový řádek ukazuje obojí („CPU 4.00 MHz  emulace 100 %“), displej FREKVENCE CPU na panelu ZDR jejich součin,
  tedy efektivní takt CPU (od 100 MHz bez desetin).

Nastavení GUI se ukládá do `sapiemu-settings.json` a rozložení oken do `imgui.ini` ve složce s exe (přenosné). Když
tam nejde zapisovat, použije se `%APPDATA%\SAPIemu\SAPIemu`. Pamatuje se poslední sestava (spustí se bez
`--machine`), velikost a poloha okna, otevřená okna, zobrazení obrazovky, nastavení terminálu, rychlost a jazyk
(`"language": "cs"` nebo `"en"`, jen když si ho uživatel zvolil v menu). Jiný soubor nastavení:
`sapiemu --settings soubor.json`.

**Jazyk GUI** (čeština, angličtina): Zobrazení → Jazyk / Language, přepne se hned. Bez uložené volby podle jazyka Windows
(zobrazovací jazyk, ne místní formáty): čeština pro české nebo slovenské Windows, jinak angličtina. `sapiemu --lang cs|en` vybere jazyk jen pro jedno spuštění (neukládá se, hodí se pro
`--ui-snapshot`). Texty GUI jsou v `src/frontend/i18n.cpp` (klíč = anglický text, `tr()`), popisy karet a jumperů
v `src/cards/catalog.cpp` (anglicky i česky). Okna mají ID nezávislé na jazyku (`tr_id()`, „Média###Media“), aby si
po přepnutí držela polohu. Nepřekládají se: názvy a popisy sestav (`.sapi`), hlášky emulovaných programů, popisky
čelního panelu (potisk skutečného stroje), MCP a `sapiemu-cli`. Test `test_i18n` hlídá i dvojí klíč v tabulce.

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
`test_tapegen` a `test_jpr1x`. Čisté sestavení (`cmake --build --preset linux-debug --clean-first`) je bez varování
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
      "binaryDir": "C:/SAPIemu-build/${presetName}" }
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
sapiemu-cli --machine machines/sapi1v.sapi --wait-text "Zadej 0-3" --type 1 --wait-text "A>" \
    --load-hex media/mph-1v/splay.hex --type "SAVE 4 SPLAY.COM\r" --run-ms 1500 \
    --load-hex ../SAPI-karty/MPH-1V/SPLAY/hellfire.hex --type "SAVE 28 HELLFIRE.VGM\r" --run-ms 1500 \
    --audio-wav hellfire.wav --type "SPLAY HELLFIRE.VGM\r" --run-ms 30000
```

`--audio-wav` nahrává zvuk zvukové karty (MPH-1V) od svého místa v akcích do konce (WAV mono, 49716 Hz).
Pozor: sestava V zapisuje změny RAM disku do `work/`, na zkoušky je lepší kopie sestavy s ZRD-1V `"mode": "nosave"`.

Úplný seznam akcí vypíše `sapiemu-cli --help`.

## Vydání (release)

1. **Verze:** `project(SAPIemu VERSION ...)` a `SAPIEMU_VERSION_SUFFIX` (`-alpha`) v `CMakeLists.txt`.
   `sapiemu --version` ji ukáže.
2. **Build:** `tools\build.cmd release test`. Runtime MSVC je přilinkovaný (`CMAKE_MSVC_RUNTIME_LIBRARY`), takže exe
   nepotřebuje Visual C++ Redistributable.
3. **Balík:** `python tools\package.py` vytvoří `out\package\SAPIemu-<verze>-win64.zip`:
   - exe, `roms\`, `media\`, `machines\`, `docs\`, fonty, licence knihoven a `PRVNI-KROKY.md`;
   - jen soubory, které zná git, takže nové soubory nejdřív `git add`;
   - skript zip rozbalí do čisté složky a ověří, že z ní naběhne MikroMon;
   - přidá i IDE disk tohoto PC (`work\ide\*.img`) jako `work\ide\`. Sestava V a
     `cpm_ide.sapi` pak mají na C: připravené programy. Na disku je i hudba: pro veřejný release
     `--no-ide`.
4. **Obrázky do README** (`docs\img\`) se dělají skrytým oknem GUI:
   - připravit stav přes `sapiemu-cli ... --save-state scena.sapistate`;
   - `sapiemu --settings s.json --ui-snapshot obr.png --ui-state scena.sapistate --ui-size 1540x572`;
   - rozložení oken dává `s.json.imgui.ini`, třeba `[Window][x###screen]` a `[Window][x###gfx0]` s `Pos=` a `Size=`;
   - naše PNG nejsou komprimovaná, před commitem je přeuložit se zlib.
5. **Poznámky k vydání:** `docs\release-notes\v<verze>.md`.
6. **GitHub:** tag `v<verze>`, Releases → Draft a new release, Pre-release, vložit poznámky a zip.

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
- **Pracovní kopie médií** (`work/rpd-1/…`, `work/…`) po zkouškách z příkazové řádky smazat. Zápisy na disketu
  se do nich uloží a příště by se načetly místo originálu.
- **Úpravy souborů skriptem:** na Windows v Bash heredocu se `\n`, `\r` a `\\` v Python řetězcích mění na
  skutečné znaky. Skripty proto psát nástrojem pro zápis souborů do dočasného `.py`, pro cesty používat raw
  řetězce, nebo upravovat soubory přímo editorem.
- **Rozbor schémat v PDF:** Python PyMuPDF (`pymupdf.open`, `page.get_pixmap(dpi=..., clip=...)`)
  vyrenderuje výřez schématu do PNG. Eagle `.sch` z let kolem 2011 je binární a nečte se.
- **RAR archivy ze sapi.cz** rozbalí `C:\Windows\System32\tar.exe -xf soubor.rar`.
- **Testy se skutečnými nahrávkami nebo disketami mimo repo** (`H:\SAPI-1\…`) se při chybějícím souboru
  přeskočí. Testy pracují s kopiemi ve `work/test/`.

