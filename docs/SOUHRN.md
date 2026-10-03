# SAPIemu – souhrn a předávka

Tento soubor je vstupní bod pro pokračování na jiném počítači nebo v nové konverzaci. Udržovat ho aktuální.

## Stav k 2026-10-03

**Hotové: ZPS-1 s MIKROBASICem.** Nové karty JPR-1 (8080, porty v paměti 2400/2800/2C00, 3214), REM-1
(EPROM a RAM, propojky E/F/R) a AND-1 (40×24, režimy znaků, překreslený font MHB 2501/2502). ANK-1 s volbou
programu (MIKROBASIC/MIKOS). Sestavy `machines/zps/ZPS-1.sapi`, `ZPS-2.sapi` (MIKROBASIC 2.4) a
`MIKROBASIC_AR1983.sapi` (verze z AR/B 1983). Jádro 8080 `third_party/i8080`, test `test_i8080`, integrační test
`test_mikrobasic`. IDE-1 odpovídá na portech vždy. Popis v `docs/desky/JPR-1.md`, `REM-1.md`, `AND-1.md`,
`docs/software/MIKROBASIC.md`.

**Hotové: mapa portů a paměti s kolizemi** (konfigurátor, záložka „Mapa portů a paměti“; MCP `machine_map`;
popis v `docs/sestavy.md`). Vychází z dekodérů karet (PROM, jumpery). Kolize na čtení sestavu nezapne.

**Hotové: sestavy s příponou `.sapi`, NAPÁJENÍ jako vypínač s aretací, baterie DMR-1 (JP7, SRAM přežije
vypnutí).**

**Hotové: pamatování nastavení GUI** (`sapiemu-settings.json` vedle exe, popis v `docs/build.md`): poslední
sestava, okno, otevřená okna, zobrazení, terminál, rychlost. Opravené pády v menu (výběr sestavy, výměna
klávesnice) a ztráta psaní po použití textového pole ImGui. TURBO/F8 přepíná 2 / 4 MHz / max.

**Hotové: konfigurátor sestavy (Stroj → Sestava…), návod `docs/sestavy.md`.**
- Čelní pohled na rám (X1 vpravo, panely SPN a ZDR). Kliknutím na slot se karta vloží, vymění nebo nastaví
  (jumpery, ROM, obrazy).
- Katalog karet (`src/cards/catalog.cpp`) zná i karty, které zatím nejsou emulované. Ty jsou šedé a na sběrnici
  neodpovídají.
- Ukládání sestav jako komentovaný soubor `.sapi` kamkoli. Šablony ZPS-1 až ZPS-4 v `machines/zps/`.
- Změna se projeví po vypnutí a zapnutí (tlačítko NAPÁJENÍ na panelu ZDR).
- MCP: `card_catalog`, `get_machine_config`, `set_machine_config`. Sběrnice podle typu (ARB-1 8, ARB-1C 14,
  ARB-1C-8 8 pozic).
- `sapiemu --ui-snapshot` vykreslí GUI do souboru (kontrola vzhledu bez snímku plochy).
- Testy `test_machine_cfg` a rozšířený `test_mcp`, celkem 16 testů.
- **Do budoucna:** fotky panelů karet (PNG pro každou kartu, kreslený panel zůstane jako náhrada), mapa portů
  a paměti sestavy s kontrolou kolizí z PROM dekodérů, přesouvání karet tažením.

**Hotové: výběr klávesnice (F5).** EKL-1, Consul 262.3 (výchozí s úpravou 7474, volitelně původní pulz
1 ms), Consul 259.11 (KOI-7 s klávesou CLEAR, volitelně KOI-8 cs pro ECMON) a ANK-1 (matice 5×8).
- **Volba:** v sestavě (`keyboard` u JPR-1ZI), menu Stroj → Klávesnice, MCP `set_keyboard`,
  `sapiemu-cli --keyboard NAME`.
- **Klávesy PC** bez textu se mapují podle klávesnice (šipky, numerika B0–B9, F1 ROL, F2 COPY, F3 BREAK,
  F4/F6/F7 klávesy D0–D2, Pause CLEAR); tabulka je v `docs/desky/klavesnice.md`. MCP `press_key`.
- **Stav LED ANK-1** ve stavovém řádku a v `machine_info`.
- **EKL-1 posílá češtinu s prohozenou velikostí** jako jeho firmware, MikroMon proto po `ž` ukáže `Ž`
  (nejasnost 13).
- **Test** `test_keyboards`: porty všech klávesnic a MikroMon s pěti paralelními variantami. Celkem 15 testů.
- ANK-1 zatím nemá software pro JPR-1ZI (MIKOS a MIKROBASIC jsou pro paměťově mapovanou JPR-1).

**Hotové: sériová linka na straně PC (první část F5).** Ověřeno autorem 2026-10-03: TeraTerm přes TCP,
konzole na obrazovce (DCD vypnuté) a přenos souboru DOPC10 → XMODEM do TeraTermu (obsah souboru korektní).

- **TCP server pro každý sériový kanál** (`src/serial/bridge.*`):
  - TeraTerm se připojí přímo (`127.0.0.1:2323`), telnet nebo raw, rozpozná se automaticky;
  - DCD může sledovat připojení klienta, takže ECMON po resetu běží na TeraTermu;
  - ověřeno: menu ECMON, boot CP/M a `DIR D:` přes TCP.
- **COM port PC** (Windows `COMx`, Linux `/dev/tty*`), případně virtuální pár com0com.
- **Volby kanálu:**
  - „Bez přetečení“ (`pacing`, výchozí zapnuto): PC posílá až po přečtení předchozího znaku;
  - řízení toku RTS/CTS (`flow_control`).
- **Ovládání** v okně Sériový terminál (spustit/zastavit TCP, otevřít COM). Stav připojení je i v MCP
  `machine_info`.
- **Sestavy:** CP/M sestavy mají TCP 2323 zapnuté; u MikroMonu je příklad zakomentovaný.
- **DCD a CTS jsou samostatné** („Terminál připojen (DCD)“ volí konzoli, „Připraven (CTS)“ povoluje
  vysílání s řízením RTS/CTS). Přijímaný znak má dobu přenosu po lince.
- **XMODEM:** DOPC10.COM přenese soubor do PC (TeraTerm) i s konzolí na obrazovce.
- **Testy:** `test_serial_tcp` (raw i telnet přes IPv4/IPv6, DCD, MikroMon s USYNC) a `test_xmodem`
  (DOPC10 → přijímač XMODEM). Celkem 14 testů.
- **Návod:** `docs/seriova-linka.md`.

Fáze F4 (IDE-1 s IDE diskem a PMD32-SD, okno Média) je hotová a ověřená autorem.

Dřív hotové fáze:
- **F4:** IDE-1 (IDE disk, PMD32-SD), sestava `cpm_ide.sapi`, okno Média.
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
| F4 | IDE-1 (8255, IDE `.img`) + PMD32-SD (protokol, `images.cfg`), ROM ECMON IDE, boot volbou 3 | ✅ hotovo |
| F5+ | ~~Sériová linka přes TCP a COM~~ ✅; ~~Consul 262.3, Consul 259.11, ANK-1~~ ✅; DSM-1 s kazetou (WAV), RAM-1Z, ZRD-1V, JPR-1V; 8080, MIKOS, PKT-1; ukládání stavu; přepínání jazyka UI; zvuk bzučáku; CI | |

Po každé fázi následuje commit, shrnutí, aktualizace tohoto souboru a zastavení, aby autor mohl výsledek ověřit.

## Nejasnosti k ověření na reálném HW (sbírat a řešit dávkově)

1. **AND-1Z-89:** 20 nebo 24 řádků? (PROM a manuál říkají 20.) Délka řádku 107 nebo 108 znaků.
2. **DMR-1:** skutečné nastavení JP8 (baud rate) na autorově sestavě. Stav modemových bitů při odpojeném terminálu.
3. **Volná sběrnice:** čte se opravdu FFh?
4. **Consul 262.3:** polarita, délka strobe; má vaše JPR-1Z/ZI úpravu se 7474? (Kódy už jsou z manuálu.)
5. **ANK-1:** bity LED a bzučáku na P0-OUT (emulace podle MIKOS: D7 ERR, D6 bzučák, D5 SHIFT).
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
13. **EKL-1 a čeština v MikroMonu:** ukáže MikroMon na skutečném HW po napsání `ž` (Alt-lock) velké `Ž`?
    Firmware EKL-1 posílá české znaky s prohozenou velikostí, ECMON ji vrací, MikroMon ne.
15. **JPR-1 a ZPS-1:**
    - Které RST dává 3214 pro kterou úroveň (předpoklad RST 7 − úroveň) a význam bitů zápisu na 3000h.
    - Co CPU čte z prázdných patic JPR-1 a REM-1 (emulace: FFh).
    - AND-1: skládání sloupců 32–63 do 1 KB VRAM (D4C), frekvence blikání, fotka obrazovky pro kontrolu
      překresleného fontu MHB 2501.
    - Kterou verzi MIKROBASICu má tvoje ZPS-1 a v jakém pořadí jsou karty ve slotech.
14. **Sestavy ZPS:** v jakém pořadí byly karty ve slotech u ZPS-1 až ZPS-4 (šablony jsou odhad)? Kolik pozic
    mají ARB-1A/Au (odhad 13) a ARB-1B (odhad 14, ARB-1C má X1–X14)? Mají všechny karty čelní panel se zkratkou
    (DMR, ZRMD, IDE…)? Panely jsou kreslené podle Liborových štítků. AND-1Z-89 má zatím panel AND-1V (K2 VGA),
    jak vypadá panel originálních karet a DMR-1, ZRMD-1, IDE-1?

## Jak pokračovat

1. `git pull`.
2. Přečíst `CLAUDE.md`, tento soubor a `docs/rozhodnuti.md`.
3. Sestavit přes `tools\build.cmd debug test`. Na PC s pomalým síťovým diskem použít lokální preset `local-debug`
   (soubor `CMakeUserPresets.json` se necommituje, vzor je v `docs/build.md`).
4. Pokračovat další fází z tabulky.
