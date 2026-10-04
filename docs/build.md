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

Klávesy v GUI: F5 běh/pauza, F8 rychlost (takt CPU → 4 MHz → maximum, dokola), F10 krok přes, F11 krok, F12 nebo F9 reset (pod debuggerem jen F9).

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
```

Úplný seznam akcí vypíše `sapiemu-cli --help`.

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
- **`sapiemu-cli`:** `--timeout-ms` musí být **před** `--wait-text`, kterého se týká. `--load-hex` nahraje
  program, `--display DGD-1` určí obrazovku pro `--screenshot`.
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

