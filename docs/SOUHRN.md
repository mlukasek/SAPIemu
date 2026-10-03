# SAPIemu – souhrn a předávka

Tento soubor je vstupní bod pro pokračování na jiném počítači nebo v nové konverzaci. Udržovat ho aktuální.

## Stav k 2026-10-03

**Hotová fáze F2: MCP server, sériový terminál a debugger.** Čeká se na ověření autorem.

- **MCP** (`src/mcp/`):
  - Streamable HTTP na `127.0.0.1:8580/mcp`, 28 nástrojů, popis v `docs/mcp.md`;
  - spouští se v GUI, nebo bez okna přes `sapiemu-cli --mcp`;
  - projektový `.mcp.json` pro Claude Code;
  - ověřeno curlem proti GUI i proti režimu bez okna a testem `test_mcp`.
- **`EmuHost`** (`src/core/host.*`) řídí běh (pauza, turbo) a frontu příkazů z MCP vlákna. Používá ho GUI
  i režim bez okna.
- **Debugger:**
  - disassembler Z80 (`src/core/disasm.*`, test `test_disasm`);
  - breakpointy na PC, watchpointy na paměť a I/O porty;
  - krok a krok přes;
  - záznam I/O přístupů (`Bus::io_log`).
- **GUI:**
  - okna Debugger (registry s editací, disassembler s breakpointy po kliknutí, zásobník), Paměť,
    Log I/O a Sériový terminál;
  - klávesy F5 běh/pauza, F8 turbo, F10 krok přes, F11 krok, F12 reset.
- **Sériový kanál:** má historii s nezávislými čtenáři (terminál, MCP).
- **Screenshot:** PNG s 1bitovou paletou (malé obrázky pro MCP).

Dřív hotové fáze:
- **F0:** kostra, build a dokumentace z průzkumu.
- **F1:** jádro, karty JPR-1ZI, DMR-1, AND-1Z-89 a EKL-1, GUI, `sapiemu-cli`. ZEXDOC prošel.

## Plán fází

| Fáze | Obsah | Stav |
|---|---|---|
| F0 | Kostra repa, build, dokumentace z průzkumu, ROM a média | ✅ hotovo |
| F1 | Jádro, JPR-1ZI, ARB-1C, DMR-1, AND-1Z-89, EKL-1, Intel HEX, režim bez okna, minimální GUI. **MikroMon naběhne a reaguje** | ✅ hotovo |
| F2 | MCP server (Streamable HTTP), okno terminálu, debugger (registry, disassembler, paměť, breakpointy, I/O log) | ✅ hotovo (ověřit) |
| F3 | ZRMD-1 + RTC, boot CP/M z RAM disku (ROM ZZRMD) | ⏳ další |
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
