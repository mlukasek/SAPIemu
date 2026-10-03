# SAPIemu – souhrn a předávka

Tento soubor je vstupní bod pro pokračování na jiném počítači nebo v nové konverzaci. Udržovat ho aktuální.

## Stav k 2026-10-03

**Hotová fáze F0 (kostra a dokumentace):**
- CMake projekt s presety pro Windows a Linux. SDL3 se stahuje přes FetchContent, ImGui, httplib, json, Z80 a fonty jsou ve `third_party/`.
- Placeholder aplikace (okno SDL3/ImGui, `--version`) a smoke test Z80. Build i ctest procházejí ve VS 2022 (MSVC 14.44).
- ROM a PROM dumpy jsou v `roms/`, obrazy médií v `media/`.
- Kompletní poznatky z průzkumu podkladů jsou v `docs/desky/`, `docs/porty.md` a `docs/software/MikroMon.md`.

## Plán fází

| Fáze | Obsah | Stav |
|---|---|---|
| F0 | Kostra repa, build, dokumentace z průzkumu, ROM a média | ✅ hotovo |
| F1 | Jádro (bus, machine, plánovač), JPR-1ZI, ARB-1C, DMR-1, AND-1Z-89, EKL-1, Intel HEX, `--headless`, minimální GUI. Cíl: **MikroMon naběhne a reaguje** | ⏳ další |
| F2 | MCP server (Streamable HTTP), okno terminálu, debugger (registry, disassembler, paměť, breakpointy, I/O log) | |
| F3 | ZRMD-1 + RTC, boot CP/M z RAM disku (ROM ZZRMD) | |
| F4 | IDE-1 (8255, IDE `.img`) + PMD32-SD (protokol, `images.cfg`) | |
| F5+ | Sériová linka přes TCP a COM; Consul 262.3, Consul 259.11, ANK-1; DSM-1 s kazetou (WAV), RAM-1Z, ZRD-1V, JPR-1V; 8080, MIKOS, PKT-1; ukládání stavu; přepínání jazyka UI; CI | |

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

## Jak pokračovat

1. `git pull`.
2. Přečíst `CLAUDE.md`, tento soubor a `docs/rozhodnuti.md`.
3. Sestavit přes `tools\build.cmd debug test`.
4. Pokračovat další fází z tabulky.
