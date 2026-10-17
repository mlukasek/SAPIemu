# SAPIemu – kontext projektu

Emulátor počítače **Tesla SAPI-1** (Z80/8080, modulární karty na sběrnici ARB-1) pro Windows 11 (primární), sestaví se i na Linuxu (ověřeno ve WSL).
Má vestavěný **MCP server** pro vývoj softwaru pro SAPI s Claude.

Autor: Martin Lukášek (mlukasek, 8bity.cz). Komunikace s autorem probíhá **česky**.

## Kde co je

- **Začni tady:** `docs/SOUHRN.md` (stav, další kroky, předávka mezi počítači).
- **Rozhodnutí s důvody:** `docs/rozhodnuti.md`. Nové rozhodnutí vždy dopsat tam.
- **Změny podle měření na skutečném HW:** `docs/zmeny-podle-hw.md` (pracoviště s PicoROM a Logic Pro 16, doklady,
  potvrzení beze změny, podezření). Změnu podle HW nejdřív navrhnout a probrat s autorem. Programy a skripty
  měření jsou v `tools/hwbench/`. Otevřené otázky na HW podle důležitosti: `docs/hw-otazky-priorita.md`.
- **HW poznatky po deskách:** `docs/desky/*.md` (porty, PROM, jumpery, časování, **nejasnosti k ověření na HW**). Mapa portů je v `docs/porty.md`, software v `docs/software/` (SAPI-1.5, ZPS-6: `SAPI-1.5.md`), tiskárna (X3 karet JPR, DPP-1) v `docs/tiskarna.md`.
- **Návody k obsluze pro uživatele** (krok za krokem, výpisy opsané z emulátoru): `docs/navod-kzd1.md`,
  `docs/navod-minigraf.md`, `docs/navod-xy4130.md`, `docs/navod-rkd1.md`, `docs/navod-rmp1.md`, `docs/navod-ar1990.md`, `docs/navod-dps1.md`,
  `docs/aps5-ukazka.md`. Po změně chování přístroje je zkontrolovat.
- **ROM, PROM a média:** `roms/` (dumpy, popis v `roms/README.md`) a `media/` (nedotčené originály obrazů, emulátor pracuje s kopiemi ve `work/`).
- **Knihovny:** `third_party/` (vendorované, popis v `third_party/README.md`). SDL3 se stahuje přes CMake FetchContent.
- **Podklady k deskám** jsou mimo repo, o složku výš ve složce `SAPI-karty` (`..\SAPI-karty\JPR-1ZI`,
  `..\SAPI-karty\AND-1Z-89`, …), pokud jsou na daném PC naklonované. Jde o desky, které nejsou na GitHubu, nové
  karty přibývají tamtéž. Repa z GitHubu (`..\MikroMon`, `..\ZPS-3`, `..\SAPI_DPI-1`…) jsou přímo vedle SAPIemu.
  Do 2026-10-05 byly i desky přímo vedle (`..\<deska>`), testy proto zkoušejí obě místa. Podklady k sestavám
  (ROM, obrazy disket, které autor připravil) jsou v `..\SAPI-sestavy\<sestava>` (např. `ZPS-3`). **Aktuální kopie
  archivů (stažené 2026-10-05, používat přednostně před staršími na `H:`):** `..\SAPI_DOKU` (dokumentace SAPI_DOKU,
  např. `MFC-1A` s celým návodem a originálními skeny schémat) a `..\SAPI.CZ` (záloha webu sapi.cz: `sapi`, `pdf`…).
  `..\SAPI_DOKU_FTP` je kopie SAPI_DOKU z FTP (2026-10-05), liší se od té z Dropboxu (archiv EC1045 s disketami,
  obrazy ImageDisk, viz `docs/software/CPM_SAPIMAN.md`); autor je bude slučovat.
  Další archiv je v `H:\SAPI-1\` (lokální záloha sapi.cz: `sapi.cz_BKP_2023-02-22`). **Zdrojáky systému ECMON
  + CP/M ZRMD-1** (ověřené proti originálům) jsou v `..\SAPI-karty\ZRMD-1\SRC\` (`build.py`, M80/L80 pod
  `cpmrun`, viz `docs/software/ECMON.md`). **Systémy CP/M J. Bíby (SAPIMAN) a EC1045**, obrazy disket, formáty a otázky na
  EC1045: `docs/software/CPM_SAPIMAN.md`, soupis jeho disket a knihovna v repu: `docs/software/SAPIMAN_diskety.md`.
- **Desky Libora Lasoty** (`..\SAPI-karty\<deska>`) mají víc revizí: vycházet z poslední, schémata v obrázcích,
  PDF a EAGLE se mohou lišit. Binární `.sch` převede offline na netlist `"C:\EAGLE 9.5.1\eaglecon.exe" -C "EXPORT
  NETLIST n.txt; EXPORT PARTLIST p.txt; WRITE x.sch; QUIT;" kopie.sch` (na kopii, `eagle.exe` chce přihlášení).
  Názvy: ANK-1M5 = ANK-1M2 (předchůdce), BGP-1 = BGP-1S.
- **Původní zadání:** `zadani.md`.
- **Zdrojový kód** (`src/`):
  - `core/`: sběrnice, karta, stroj, `host` (běh a fronta příkazů), disasm, headless, util, charset, ihex,
    png, log, `xmodem` (vysílač a přijímač pro okno terminálu), `tapegen` (kazeta MIKOS a MIKROBASIC
    z programu, `docs/software/kazeta.md`), `relay` (spojení s relay pro síťové hry, TCP řádky
    i WebSocket, a spojení TCP a DNS Repository API, každé ve vlastním vlákně; karta DPI-1L, její SD karta nad
    složkou PC je `cards/dpi1l_fs`, `docs/desky/DPI-1L.md`), `state` (ukládání stavu, `docs/stav.md`:
    **každá karta musí mít `state()`**);
  - `cpu/`: implementace z80.h (8080 je vendorované v `third_party/i8080`, upravené o režim 8085 pro JPR-1X,
    `docs/8085.md`);
  - `chips/`: obvody (MHB1012, 8251, 8253, 8255, 8272, RTC 72421…) a přístroje (magnetofon, záznamník dat
    KZD-1 `kzd1`, zapisovače se společnou mechanikou `plotter`: Minigraf 0507 `minigraf` a XY 4130 `xy4130`,
    plotter z AR 1990 `ar1990` na portech JPR-1);
  - `cards/`: desky a `registry.cpp`, kde se registrují nové typy karet;
  - `input/`: klávesnice, `joyport` (konektor joysticku Atari: joystick a myš, MPH-1V a ANK-1M2);
  - `frontend/`: GUI (app, debugger, terminal, i18n, keymap, diag = okno desky diagnostiky na K3, `plotter` = okno
    zapisovače (Minigraf, XY 4130), `shot` = snímky obrazovek PNG a schránka, `video` = MP4 se zvukem přes ffmpeg);
  - `mcp/`: MCP server a nástroje (popis v `docs/mcp.md`);
  - `serial/`: připojení sériových kanálů k PC (TCP server, COM port; návod `docs/seriova-linka.md`);
  - `plant/`: technologie mimo počítač (`core/plant.h`, `Bus::plant`): pece a termočlánky APS-5
    (`docs/desky/APS-5.md`), `aps5_demo` = tlačítko DEMO (obsluha, která ťuká ukázku na panelu); měřicí pracoviště
    IMS-2 desky DPS-1 (`ims2_bench`, voltmetr `m1t330`, zdroj `bs525`; sběrnice `core/ims2`, `docs/desky/DPS-1.md`);
  - `main.cpp`.
- **Nástroje** (`tools/`): `build.cmd`, `package.py`, `asm8080.py` (malý assembler 8080), `cpmrun.cpp` (programy
  pro CP/M jako M80/L80 na PC, cíl `cpmrun`; popis v `docs/build.md`), `hw_otazky.py` (z Nejasností vygeneruje
  souhrn otázek na HW `docs/hw-otazky.md`; po změně Nejasností spustit znovu), `ckm_oprava.py` (z originálu
  `media/mikos/CKM.hex` vyrobí opravenou kopii `CKM_opraveny.hex`, `docs/desky/KZD-1.md`), `mtcopy_oprava.py`
  (opravená kopie MTCOPYO a disketa `media/rmp-1/a_lad_izo_opraveny.img`, `docs/desky/RMP-1.md`), `rkd5400_bios_src.py`
  a `rkd5400_bios.py` (vlastní BIOS CP/M J. Bíby pro RKD-1 s CM 5400 a jeho bootovací disketa, `docs/desky/RKD-1.md`),
  `ar1990_rom.py` (EPROM plotteru z AR 1990 z výpisu `roms/ar1990/PLOTTER.MAC`, `docs/desky/AR1990.md`),
  `dps1_kazeta.py` (kazeta BASIMS s programy BASIC nahraná v emulátoru přes MCP, `docs/navod-dps1.md`),
  `check_machines.py` (před vydáním: v každé sestavě něco spustí a zkontroluje, `--games` porty a dema sestavy V;
  nad čistou kopií dat v `out/check/`, `docs/build.md`).
- **Sestavy:** `machines/*.sapi` (JSON s komentáři, cesty relativně ke kořeni repa).
- **Testy:** `tests/`, ctest. Podrobnosti k sestavení a příklady jsou v `docs/build.md`.
- **MCP:** emulátor (GUI nebo `sapiemu-cli --mcp`) poskytuje MCP na `http://127.0.0.1:8580/mcp`.
  Projektový `.mcp.json` ho nabídne Claude Code. Při ladění softwaru pro SAPI ho používat.
- **Ověřování:** stav emulace si Claude ověřuje přes `sapiemu-cli` (`--print-screen`, `--screenshot`), ne snímky
  plochy uživatele. Vzhled GUI ověřuje přes `sapiemu --ui-snapshot soubor.png` (skryté okno, viz `docs/sestavy.md`;
  interakce myší simuluje např. `--ui-drag Z:DO:drop`).
- **Sestavy a katalog karet:** `docs/sestavy.md`. Každá karta má v `src/cards/catalog.cpp` popis parametrů
  (jumpery), ze kterého vychází konfigurátor, komentáře v uložených sestavách i MCP.

## Preference autora (dodržovat)

- **Kód:** C++20 v jednoduchém stylu „C s třídami“. Přednost mají C hlavičky (`<stdint.h>`, `<stdio.h>`, `<string.h>`), struktury, prosté funkce a virtuální metody tam, kde dávají smysl. Minimum šablon, výjimek a složitých C++ konstrukcí. Musí to být efektivní.
- **Jazyk:** identifikátory a komentáře v kódu **anglicky**, dokumentace **česky**, UI zatím **česky** (texty přes `tr()`, aby šly později přepínat).
- **Git:** **lokální commity průběžně, push jen na výslovné vyžádání.** **Před každým pushem** projít všechny
  commity od `origin/main` a zaktualizovat všechny dotčené `.md` (README, `docs/prvni-kroky.md`, `docs/SOUHRN.md`,
  tento soubor, `docs/rozhodnuti.md`, `docs/desky/*`, `docs/software/*`, `docs/mcp.md`…), ať je GitHub aktuální.
- **Zápis poznatků:** důležité věci zapisovat do `docs/`, na projektu se pracuje z více počítačů.
- **Postup:** „dvakrát měř, jednou řež“. Na cokoli nejasného se zeptat a doporučení dávat jako první možnost.
- **Reálný HW:** autor ho má, ale příprava chvíli trvá. Otázky k ověření na HW sbírat do „Nejasností“ a řešit dávkově.
- **Desky s jumpery** musí být v emulátoru stejně konfigurovatelné jako na skutečné desce. Dekodéry adres se dělají přímo podle PROM dumpů.
- **Zdrojové kódy MikroMonu** se do repa nedávají (binárka ano, se souhlasem autora).
- **Emulátor Stan6314** ignorovat, nanejvýš do něj nahlédnout při nejasnostech.

## Software pro SAPI v asm: pasmo

Assembler **pasmo 0.5.3** (u autora `E:\SAPI_GIT\Tools\pasmo-0.5.3\pasmo.exe`, používá ho SAPI-Bomberman).
Pasti ověřené 2026-10-05:

- **Unární minus na začátku výrazu neguje celý zbytek výrazu.**
  - Příklady: `-1*40+1` = −41 (ne −39), `-1+2` = −3.
  - Správně: `(-1)*40+1`, `0-1*40+1` nebo hotové číslo.
  - Za operátorem (`5+-1`, `2*-3`) se nepřeloží, pomohou závorky.
  - Tabulky posunů po překladu zkontrolovat v binárce. Takhle vznikla chyba ramene výbuchu v SAPI-Bomberman.
- **Operand začínající závorkou** je adresa: `ld a,(X+3) & 0FFh` nejde. Použít `ld a,0+(X+3) & 0FFh` nebo
  `low`/`high`.
- **Zarovnání:** ALIGN není, použít `defs (256 - ($ & 0FFh)) & 0FFh`. `DEFL` jako počitadlo v `REPT`
  nefunguje, tabulky generovat skriptem.
- **Návěští `.x`** jsou globální, musí být jedinečná. Při „Relative jump out of range“ použít `jp`.
- **MCP při ladění:** `read_memory` vrací nejvýš 4096 bajtů (víc = chybový text). `cycles` počítá takty
  4 MHz i při CPU 2 MHz.

## Sestavení

- **Visual Studio 2022:** Open Folder, CMake presety `windows-debug` / `windows-release`.
- **Příkazová řádka:** `tools\build.cmd debug test`. Skript sám najde VS přes vswhere (přednost má VS 2022) a spustí configure, build a ctest.
- **Kdy které testy** (autor nechce pouštět vše vždy, tabulka v `docs/build.md`): během práce jen dotčené
  (`tools\build.cmd debug test "zps3|rpd1"`), před commitem celá debug sada (paralelně asi 25 s), před pushem
  i `release test`, `release full` (ZEX, asi 3 min) jen po změně jader CPU nebo před vydáním, Linux ve WSL po změně
  kódu závislého na platformě nebo před vydáním.
- **Výstupy:** `out/build/<preset>/sapiemu.exe`, testy v `out/build/<preset>/tests/`.
- **Data hledá exe** od své složky nahoru, pak od aktuální. Záchranná cesta ke zdrojákům (`SAPIEMU_SOURCE_FALLBACK`)
  je jen v debug a lokálních buildech mimo repo, release presety ji vypínají a `package.py` to hlídá.
- **Linux:** `cmake --preset linux-debug && cmake --build --preset linux-debug`, ověřeno ve WSL 2 (Ubuntu 24.04,
  klon v `~/SAPIemu`, ne na `/mnt/e`; postup a pasti GCC v `docs/build.md`). Windows build má přednost, změny
  pro Linux nesmí nic rozbít na Windows (po nich vždy `tools\build.cmd debug test` i `release test`).
- **Umístění na autorově hlavním PC (od 2026-10-04):** repo je na lokálním disku v `E:\SAPI_GIT\SAPIemu`, podpůrné
  repozitáře desek v `E:\SAPI_GIT\SAPI-karty\<deska>` (od 2026-10-05, relativní odkazy `..\SAPI-karty\JPR-1V`). Archiv
  `H:\SAPI-1\` (záloha sapi.cz, `LiborLasota`, `_ROM`, nahrávky kazet…) zůstává na síťovém disku `H:`
  (`\\Luksrv1\history`). Dřív bylo repo v `H:\SAPI-1\GIT\SAPIemu` a VS na síťovém disku zlobilo (IntelliSense
  do `.vs\`, pomalé sestavení). Lokální preset `local-debug` proto na tomto PC není potřeba.
- **Po přesunu repa do jiné složky** neplatí `out\build\*` (CMakeCache má absolutní cesty): smazat ho, nebo ve VS
  Project → Delete Cache and Reconfigure. Nastavení GUI (`sapiemu-settings.json`, `imgui.ini`) z něj předem
  odložit.
- **VS a C1060 „compiler is out of heap space“:** presety mají `toolset host=x64`, jinak VS pouští 32bitový `cl.exe`,
  kterému na souborech s `json.hpp` dojde paměť. Po změně presetů ve VS: Project → Delete Cache and Reconfigure.
- **LNK1236 / LNK1163 („corrupt or invalid COFF sections“, „invalid selection for COMDAT“)** po přerušeném
  sestavení: smazat `out\build\<preset>\CMakeFiles\sapicore.dir`, `sapicore.lib` a `sapicore.pdb` a sestavit znovu.
  Samotné smazání jednoho `.obj` nestačilo (2026-10-05).
- **Pozor:** makra `Z80_SET_DATA` a podobná v `z80.h` se rozvinou na blok `{}`. Před `else` je proto nutné psát složené závorky.
