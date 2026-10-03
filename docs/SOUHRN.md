# SAPIemu – souhrn a předávka

Tento soubor je vstupní bod pro pokračování na jiném počítači nebo v nové konverzaci. Udržovat ho aktuální.

## Stav k 2026-10-03

**Hotová fáze F3: RAM disk ZRMD-1 s RTC a boot CP/M 2.2.** Čeká se na ověření autorem.

- **Karty `ZRMD-1` a `ZRD-1V`:**
  - 4 MB, porty 60–63h, autoinkrement adresy se zalamuje v rámci 256 B;
  - ochrana zápisu S1–S4 podle GAL;
  - režimy `rw`, `nosave` a `readonly`, pracovní kopie ve `work/`, automatické ukládání;
  - MAP přes JP2.
- **RTC 72421** podle hodin PC.
- **Sestava `machines/cpm_zrmd.jsonc`:**
  - ECMON → volba 1 → „JPR-1Z 62K CP/M V2.2Z … A: ZRMD1/4MB BOOT SS256“ → `A>` → `DIR` funguje;
  - volby 0 a 2 skončí na HALT (chybí FDC-1, MFC-1), volba 3 po chvíli hlásí „INIT HDD - nenalezen!“
    a nabootuje do `A>`;
  - popis v `docs/software/ECMON.md`.
- **ZZRMD ROM** se ukázala jako přenos systému na RAM disk, ne boot.
- **„Terminál připojen (DCD)“** je teď vlastnost sériového kanálu. Přepíná se v okně terminálu, v MCP
  (`serial_set_connected`) a v konfiguraci (`connected`). ECMON i MikroMon podle DCD volí konzoli.
- **Testy:** `test_zrmd` (porty, zalamování, ochrana, režimy, ukládání, RTC, MAP) a `test_cpm`
  (boot a DIR, RAM disk jen pro čtení). Celkem 9 testů.
- **`sapiemu-cli --disasm ADDR N`.**
- **Sériový terminál** je znakový terminál 80×24 s kurzorem (`src/core/term.*`, test `test_term`).
  - Emulace: SAPI (kódy ECMON/AND-1Z, výchozí), ANSI/VT100 (základ) a bez emulace (`^X`).
  - Volba „CR = nový řádek“.
  - CR se už nebere jako nový řádek, takže zmizelo dvojité odřádkování a „paznaky“ `^]^_` (ECMON posílá
    1D 1F = domů a smazání obrazovky).

Dřív hotové fáze:
- **F2:** MCP server (`docs/mcp.md`), debugger, sériový terminál, `EmuHost`.
- **F0:** kostra, build a dokumentace z průzkumu.
- **F1:** jádro, karty JPR-1ZI, DMR-1, AND-1Z-89 a EKL-1, GUI, `sapiemu-cli`. ZEXDOC prošel.

## Plán fází

| Fáze | Obsah | Stav |
|---|---|---|
| F0 | Kostra repa, build, dokumentace z průzkumu, ROM a média | ✅ hotovo |
| F1 | Jádro, JPR-1ZI, ARB-1C, DMR-1, AND-1Z-89, EKL-1, Intel HEX, režim bez okna, minimální GUI. **MikroMon naběhne a reaguje** | ✅ hotovo |
| F2 | MCP server (Streamable HTTP), okno terminálu, debugger (registry, disassembler, paměť, breakpointy, I/O log) | ✅ hotovo |
| F3 | ZRMD-1 + RTC, boot CP/M z RAM disku (ECMON) | ✅ hotovo (ověřit) |
| F4 | IDE-1 (8255, IDE `.img`) + PMD32-SD (protokol, `images.cfg`), ROM ECMON IDE, boot volbou 3 | ⏳ další |
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

## Jak pokračovat

1. `git pull`.
2. Přečíst `CLAUDE.md`, tento soubor a `docs/rozhodnuti.md`.
3. Sestavit přes `tools\build.cmd debug test`. Na PC s pomalým síťovým diskem použít lokální preset `local-debug`
   (soubor `CMakeUserPresets.json` se necommituje, vzor je v `docs/build.md`).
4. Pokračovat další fází z tabulky.
