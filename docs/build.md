# Sestavení a spuštění

## Visual Studio 2022

1. Soubor → Otevřít → Složka, vybrat kořen repa.
2. Vybrat preset (`Windows x64 Debug`, případně lokální `Local x64 Debug`).
3. Jako Startup Item zvolit `sapiemu.exe` (GUI), nebo `sapiemu-cli.exe` (bez okna).

## Příkazová řádka

```
tools\build.cmd debug test          # preset windows-debug, výstup v out\build\windows-debug
tools\build.cmd local-debug test    # lokální preset (výstup mimo repo)
tools\build.cmd release             # release build, zahrnuje pomalé testy ZEXDOC/ZEXALL
```

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
`--machine`), velikost a poloha okna, otevřená okna, zobrazení obrazovky, nastavení terminálu a rychlost. Jiný
soubor nastavení: `sapiemu --settings soubor.json`.

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
  `cpmrun M80 =ECMONM/L`. L80 je spolehlivější s příkazem na stdin (`echo X,X/N/X/E | cpmrun L80`), protože
  s parametry v příkazové řádce nepřijme `/P:0`. Když program čeká na řádek z konzole a stdin skončil, `cpmrun`
  skončí. `CPMRUN_TRACE=1` vypisuje volání BDOS. Použití: `..\SAPI-karty\ZRMD-1\SRC\build.py`
  (`docs/software/ECMON.md`).
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

