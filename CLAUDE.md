# SAPIemu – kontext projektu

Emulátor počítače **Tesla SAPI-1** (Z80/8080, modulární karty na sběrnici ARB-1) pro Windows 11, později i Linux.
Má vestavěný **MCP server** pro vývoj softwaru pro SAPI s Claude.

Autor: Martin Lukášek (mlukasek, 8bity.cz). Komunikace s autorem probíhá **česky**.

## Kde co je

- **Začni tady:** `docs/SOUHRN.md` (stav, další kroky, předávka mezi počítači).
- **Rozhodnutí s důvody:** `docs/rozhodnuti.md`. Nové rozhodnutí vždy dopsat tam.
- **HW poznatky po deskách:** `docs/desky/*.md` (porty, PROM, jumpery, časování, **nejasnosti k ověření na HW**). Mapa portů je v `docs/porty.md`, software v `docs/software/`.
- **ROM, PROM a média:** `roms/` (dumpy, popis v `roms/README.md`) a `media/` (nedotčené originály obrazů, emulátor pracuje s kopiemi ve `work/`).
- **Knihovny:** `third_party/` (vendorované, popis v `third_party/README.md`). SDL3 se stahuje přes CMake FetchContent.
- **Podklady k deskám** jsou mimo repo, o složku výš (`..\JPR-1ZI`, `..\AND-1Z-89`, …), pokud jsou na daném PC naklonované. Další archiv je v `H:\SAPI-1\` (lokální záloha sapi.cz: `sapi.cz_BKP_2023-02-22`).
- **Původní zadání:** `zadani.md`.
- **Zdrojový kód** (`src/`):
  - `core/`: sběrnice, karta, stroj, `host` (běh a fronta příkazů), disasm, headless, util, charset, ihex,
    png, log;
  - `cpu/`: implementace z80.h;
  - `chips/`: obvody (MHB1012…);
  - `cards/`: desky a `registry.cpp`, kde se registrují nové typy karet;
  - `input/`: klávesnice;
  - `frontend/`: GUI (app, debugger, terminal, i18n, keymap);
  - `mcp/`: MCP server a nástroje (popis v `docs/mcp.md`);
  - `serial/`: připojení sériových kanálů k PC (TCP server, COM port; návod `docs/seriova-linka.md`);
  - `main.cpp`.
- **Sestavy:** `machines/*.sapi` (JSON s komentáři, cesty relativně ke kořeni repa).
- **Testy:** `tests/`, ctest. Podrobnosti k sestavení a příklady jsou v `docs/build.md`.
- **MCP:** emulátor (GUI nebo `sapiemu-cli --mcp`) poskytuje MCP na `http://127.0.0.1:8580/mcp`.
  Projektový `.mcp.json` ho nabídne Claude Code. Při ladění softwaru pro SAPI ho používat.
- **Ověřování:** stav emulace si Claude ověřuje přes `sapiemu-cli` (`--print-screen`, `--screenshot`), ne snímky
  plochy uživatele. Vzhled GUI ověřuje přes `sapiemu --ui-snapshot soubor.png` (skryté okno, viz `docs/sestavy.md`).
- **Sestavy a katalog karet:** `docs/sestavy.md`. Každá karta má v `src/cards/catalog.cpp` popis parametrů
  (jumpery), ze kterého vychází konfigurátor, komentáře v uložených sestavách i MCP.

## Preference autora (dodržovat)

- **Kód:** C++20 v jednoduchém stylu „C s třídami“. Přednost mají C hlavičky (`<stdint.h>`, `<stdio.h>`, `<string.h>`), struktury, prosté funkce a virtuální metody tam, kde dávají smysl. Minimum šablon, výjimek a složitých C++ konstrukcí. Musí to být efektivní.
- **Jazyk:** identifikátory a komentáře v kódu **anglicky**, dokumentace **česky**, UI zatím **česky** (texty přes `tr()`, aby šly později přepínat).
- **Git:** **lokální commity průběžně, push jen na výslovné vyžádání.**
- **Zápis poznatků:** důležité věci zapisovat do `docs/`, na projektu se pracuje z více počítačů.
- **Postup:** „dvakrát měř, jednou řež“. Na cokoli nejasného se zeptat a doporučení dávat jako první možnost.
- **Reálný HW:** autor ho má, ale příprava chvíli trvá. Otázky k ověření na HW sbírat do „Nejasností“ a řešit dávkově.
- **Desky s jumpery** musí být v emulátoru stejně konfigurovatelné jako na skutečné desce. Dekodéry adres se dělají přímo podle PROM dumpů.
- **Zdrojové kódy MikroMonu** se do repa nedávají (binárka ano, se souhlasem autora).
- **Emulátor Stan6314** ignorovat, nanejvýš do něj nahlédnout při nejasnostech.

## Sestavení

- **Visual Studio 2022:** Open Folder, CMake presety `windows-debug` / `windows-release`.
- **Příkazová řádka:** `tools\build.cmd debug test`. Skript sám najde VS přes vswhere (přednost má VS 2022) a spustí configure, build a ctest.
- **Výstupy:** `out/build/<preset>/sapiemu.exe`, testy v `out/build/<preset>/tests/`.
- **Linux (později):** `cmake --preset linux-debug && cmake --build --preset linux-debug`.
- Na autorově hlavním PC je `H:` síťový disk (`\\Luksrv1\history`). VS proto hlásí, že databázi IntelliSense nemůže
  uložit do `.vs\`. Hláška je neškodná, stačí potvrdit OK nebo nastavit Fallback Location (C/C++ → Advanced).
  Sestavení do `out\` na síťovém disku funguje, jen je pomalejší.
- **VS a C1060 „compiler is out of heap space“:** presety mají `toolset host=x64`, jinak VS pouští 32bitový `cl.exe`,
  kterému na souborech s `json.hpp` dojde paměť. Po změně presetů ve VS: Project → Delete Cache and Reconfigure.
- **Pozor:** makra `Z80_SET_DATA` a podobná v `z80.h` se rozvinou na blok `{}`. Před `else` je proto nutné psát složené závorky.
