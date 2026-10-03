# SAPIemu – souhrn a předávka

Tento soubor je vstupní bod pro pokračování na jiném počítači nebo v nové konverzaci. Udržovat ho aktuální.

## Stav k 2026-10-03

**Hotová fáze F1: MikroMon 1.3 naběhne a reaguje na klávesnici.** Čeká se na ověření GUI autorem.

- **Jádro** (`src/core/`):
  - sběrnice ARB-1 (`bus`), rozhraní karet (`card`, `devices`), stroj načítaný z `machines/*.jsonc` (`machine`);
  - Intel HEX a načítání ROM, znaková sada KOI-8čs ↔ Unicode, PNG, log;
  - režim bez okna (`headless`).
- **Karty:**
  - `JPR-1ZI`: Z80, dekodéry D3C/D6B z PROM dumpů, BOOT, porty 00–03h, takt 2/4 MHz;
  - `DMR-1`: RAM 128 KB s dekodérem D2C a 8 konfiguracemi JP3, UART MHB1012 s časováním podle baud rate;
  - `AND-1Z-89`: VRAM přes PROM D1, znakový generátor, wait stavy;
  - klávesnice `EKL-1` jako protokol na portech JPR.
- **GUI** (`src/frontend/`):
  - SDL3 + ImGui s dockováním, české UI přes `tr()`;
  - okno Obrazovka (barva luminoforu, celočíselné zvětšení, poměr stran TV) a log;
  - klávesnice PC → kódy EKL-1 včetně češtiny a kurzorových kláves;
  - F5 pauza, F11 turbo, F12 reset;
  - výběr sestavy z `machines/`.
- **Spustitelné soubory:**
  - `sapiemu.exe` (GUI);
  - `sapiemu-cli.exe` (bez okna). Příklad:
    `sapiemu-cli --wait-text "V1.3" --type "M F000\r" --run-ms 500 --print-screen --screenshot s.png`.
- **Testy** (ctest):
  - `test_charset`: semigrafika proti ROM, KOI-8čs, Intel HEX;
  - `test_decoders`: JPR BOOT ROM, všech 8 konfigurací DMR-1, okno AND-1Z;
  - `test_uart`: časování a přepis vysílacího registru;
  - `test_mikromon`: boot, příkazy, česky psaný text, breakpoint;
  - `test_zexdoc` a `test_zexall` jen v release buildu. ZEXDOC ověřen 2026-10-03: všechny testy OK,
    46,7 mld. T-stavů za asi 1 min 45 s (holé jádro asi 440 MHz).

Fáze F0 (kostra, build, dokumentace z průzkumu, ROM a média) je hotová dříve.

## Plán fází

| Fáze | Obsah | Stav |
|---|---|---|
| F0 | Kostra repa, build, dokumentace z průzkumu, ROM a média | ✅ hotovo |
| F1 | Jádro, JPR-1ZI, ARB-1C, DMR-1, AND-1Z-89, EKL-1, Intel HEX, režim bez okna, minimální GUI. **MikroMon naběhne a reaguje** | ✅ hotovo (GUI ověřit) |
| F2 | MCP server (Streamable HTTP), okno terminálu, debugger (registry, disassembler, paměť, breakpointy, I/O log) | ⏳ další |
| F3 | ZRMD-1 + RTC, boot CP/M z RAM disku (ROM ZZRMD) | |
| F4 | IDE-1 (8255, IDE `.img`) + PMD32-SD (protokol, `images.cfg`) | |
| F5+ | Sériová linka přes TCP a COM; Consul 262.3, Consul 259.11, ANK-1; DSM-1 s kazetou (WAV), RAM-1Z, ZRD-1V, JPR-1V; 8080, MIKOS, PKT-1; ukládání stavu; přepínání jazyka UI; zvuk bzučáku; CI | |

Po každé fázi následuje commit, shrnutí, aktualizace tohoto souboru a zastavení, aby autor mohl výsledek ověřit.

## Nejasnosti k ověření na reálném HW (sbírat a řešit dávkově)

1. **AND-1Z-89:** 20 nebo 24 řádků? (PROM a manuál říkají 20.) Délka řádku 107 nebo 108 znaků.
2. **DMR-1:** skutečné nastavení JP8 (baud rate) na autorově sestavě. Stav modemových bitů při odpojeném terminálu.
3. **Volná sběrnice:** čte se opravdu FFh?
4. **Consul 262.3:** polarita, kódy, délka strobe.
5. **ANK-1:** bity LED a bzučáku na P0-OUT.
6. **ZRMD-1:** které adresní rozsahy chrání S1–S4. Jak MikroMon zamyká ZRD-1V.
7. **MikroMon 1.3:** posílá opravdu „sw i01“ a skenuje jen kanály 10, 14 a 18h?
8. **IDE-1:** stav IDE signálů po resetu, když je PC 8255 nastavený jako vstup.
9. **MikroMon bez USYNC:** ztrácí ozvěna na UART znaky i na skutečném HW? Emulace: po startu přijde „Z4kn 3“.
10. **MHB1012:** pořadí bitů délky slova (b3/b2) v řídicím registru.

## Jak pokračovat

1. `git pull`.
2. Přečíst `CLAUDE.md`, tento soubor a `docs/rozhodnuti.md`.
3. Sestavit přes `tools\build.cmd debug test`. Na PC s pomalým síťovým diskem použít lokální preset `local-debug`
   (soubor `CMakeUserPresets.json` se necommituje, vzor je v `docs/build.md`).
4. Pokračovat další fází z tabulky.
