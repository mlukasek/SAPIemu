# SAPIemu – souhrn a předávka

Tento soubor je vstupní bod pro pokračování na jiném počítači nebo v nové konverzaci. Udržovat ho aktuální.

## Stav k 2026-10-03

**Hotová fáze F4: IDE-1 s IDE diskem a PMD32-SD.** Čeká se na ověření autorem.

- **Karta `IDE-1`:**
  - dvě 8255 (`src/chips/i8255.*`, módy 0, 1 a 2);
  - IDE disk s obrazem `.img` (`ide_disk.*`, LBA a CHS), vytvoří se automaticky jako 128 MB vyplněných E5h;
  - PMD32-SD (`pmd32sd.*`): protokol přepsaný z firmwaru v3.2 jako stavový automat v emulovaném čase,
    SD karta = adresář s `images.cfg`.
- **Sestava `machines/cpm_ide.jsonc`:**
  - ECMON IDE → volba 3 → „PMD32-SD OK“, „INIT HDD SAPIemu IDE disk“, `A>`;
  - `DIR D:` (VZOR_2M.2MB), `DIR E:` (8" obraz), `PIP B:=D:STAT.COM`, `B:STAT B:` fungují.
- **Okno Média** (Zobrazení → Média): výběr obrazů PMD32-SD A–D, ochrana zápisu, zamčení karty.
  MCP: `pmd32_images`, `pmd32_mount`.
- **Pracovní kopie** (`media_path`) umí i celé adresáře (SD karta → `work/pmd32sd`).
- **Testy:** `test_ide1` (8255, IDE přes porty jako BIOS, protokol PMD32-SD) a `test_cpm_ide` (boot, DIR D:,
  PIP na B:). Celkem 12 testů.
- **Zjištění z firmwaru PMD32-SD:** NAK po chybném CRC hned přepíše prezentace AAh (viz `docs/desky/PMD32-SD.md`).

Dřív hotové fáze:
- **F3:** ZRMD-1 + RTC, boot CP/M z RAM disku, sériový terminál s emulací SAPI/ANSI.
- **F2:** MCP server (`docs/mcp.md`), debugger, sériový terminál, `EmuHost`.
- **F0:** kostra, build a dokumentace z průzkumu.
- **F1:** jádro, karty JPR-1ZI, DMR-1, AND-1Z-89 a EKL-1, GUI, `sapiemu-cli`. ZEXDOC prošel.

## Plán fází

| Fáze | Obsah | Stav |
|---|---|---|
| F0 | Kostra repa, build, dokumentace z průzkumu, ROM a média | ✅ hotovo |
| F1 | Jádro, JPR-1ZI, ARB-1C, DMR-1, AND-1Z-89, EKL-1, Intel HEX, režim bez okna, minimální GUI. **MikroMon naběhne a reaguje** | ✅ hotovo |
| F2 | MCP server (Streamable HTTP), okno terminálu, debugger (registry, disassembler, paměť, breakpointy, I/O log) | ✅ hotovo |
| F3 | ZRMD-1 + RTC, boot CP/M z RAM disku (ECMON) | ✅ hotovo |
| F4 | IDE-1 (8255, IDE `.img`) + PMD32-SD (protokol, `images.cfg`), ROM ECMON IDE, boot volbou 3 | ✅ hotovo (ověřit) |
| F5+ | Sériová linka přes TCP a COM; Consul 262.3, Consul 259.11, ANK-1; DSM-1 s kazetou (WAV), RAM-1Z, ZRD-1V, JPR-1V; 8080, MIKOS, PKT-1; ukládání stavu; přepínání jazyka UI; zvuk bzučáku; CI | |

Po každé fázi následuje commit, shrnutí, aktualizace tohoto souboru a zastavení, aby autor mohl výsledek ověřit.

## Nejasnosti k ověření na reálném HW (sbírat a řešit dávkově)

1. **AND-1Z-89:** 20 nebo 24 řádků? (PROM a manuál říkají 20.) Délka řádku 107 nebo 108 znaků.
2. **DMR-1:** skutečné nastavení JP8 (baud rate) na autorově sestavě. Stav modemových bitů při odpojeném terminálu.
3. **Volná sběrnice:** čte se opravdu FFh?
4. **Consul 262.3:** polarita, kódy, délka strobe.
5. **ANK-1:** bity LED a bzučáku na P0-OUT.
6. **ZRMD-1:** ~~rozsahy S1–S4~~ (vyřešeno z GAL). Jak MikroMon „zamyká“ ZRD-1V? Nuluje /RES adresní čítače?
   Které bity 63h jdou na MAP1/MAP2?
7. **MikroMon 1.3:** posílá opravdu „sw i01“ a skenuje jen kanály 10, 14 a 18h?
8. **IDE-1:** stav IDE signálů po resetu, když je PC 8255 nastavený jako vstup.
9. **MikroMon bez USYNC:** ztrácí ozvěna na UART znaky i na skutečném HW? Emulace: po startu přijde „Z4kn 3“.
10. **MHB1012:** pořadí bitů délky slova (b3/b2) v řídicím registru.
11. **Reálná sestava CP/M:** je na DMR-1 připojený terminál (DCD), nebo se používá obrazovka? Kterou volbu
    0–3 v ECMON používáte (předpoklad: 3 = HDD)? Kterou EPROM máte v JPR-1ZI (ECMON s IDE-1)?
12. **IDE disk:** máte obraz (dump) skutečného DOM/CF z vaší sestavy? Emulátor zatím začíná s prázdným
    diskem vyplněným E5h.

## Jak pokračovat

1. `git pull`.
2. Přečíst `CLAUDE.md`, tento soubor a `docs/rozhodnuti.md`.
3. Sestavit přes `tools\build.cmd debug test`. Na PC s pomalým síťovým diskem použít lokální preset `local-debug`
   (soubor `CMakeUserPresets.json` se necommituje, vzor je v `docs/build.md`).
4. Pokračovat další fází z tabulky.
