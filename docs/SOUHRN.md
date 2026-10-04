# SAPIemu – souhrn a předávka

Tento soubor je vstupní bod pro pokračování na jiném počítači nebo v nové konverzaci. Udržovat ho aktuální.

## Stav k 2026-10-04

**Hotové: jedna barva obrazu a okna podle karet.**
- Barva jednobarevného obrazu je parametr karty v sestavě: AND-1V propojky S1 (`color`), ostatní parametr
  `monitor` (luminofor). Menu Zobrazení → Barva i konfigurátor mění totéž a hned, uloží se se sestavou. Globální
  „Luminofor“ v nastavení GUI zmizel. Sestava V má AND-1V jantarovou (autor).
- Okna obrazovek se jmenují podle karty („AND-1V“, „CGA-1V“, „DGD-1“), rozložení okna se převede.
- Popis v `docs/sestavy.md` („Okna obrazovek a barva obrazu“).

**Hotové: CGA-1V** (barevná grafika 320×200, VGA, Libor Lasota) v sestavě V na X7.
- Nastavení: C000h při MAP1 = MAP2 = H (`OUT 63h,C0h`), jumpery podle fotky.
- Registry jsou ze schématu (CONFIG/STATUS, Bt476), EGA i CGA, barvové domény, dvě stránky, VBI na /INT0
  a čekací stavy.
- Liborovy obrázky CASTLE, REMBRAN1 a REMBRAN2 se zobrazí, CGATEST projde. REMBRAN2 střídá stránky po snímcích,
  proto emulátor prolíná dva poslední snímky.
- **TURBO JPR-1V** jde přepínat za chodu (menu Rychlost, MCP `cpu_turbo`).
- Popis v `docs/desky/CGA-1V.md`, programy v `media/cga-1v/`.

**Hotové: desky V Libora Lasoty** (`machines/sapi1v.sapi`).
- JPR-1V (banky EEPROM MikroBIOS 3.0 / ECMON), AND-1V rev 2.3 (VGA 64x20), RAM-1V (generátor MAP na 63h,
  díry, volitelné RTC M48T35), DSM-1V (10h a 14h, 115200 Bd).
- ZRD-1V bez MAP, reset nenuluje adresu.
- ECMON + CP/M z RAM disku běží, MikroBIOS se ohlásí.
- Test `test_sapi1v`, celkem 24 testů. Propojky podle fotek autorových desek.

**Hotové: ZPS-4** (`machines/zps/ZPS-4.sapi`, `ZPS-4_ELIS.sapi`).
- Nové karty JPR-1Z (typ JPR-1ZI s 2716), RAM-1Z (dump D3B, přepínače S1) a RAM disk RMD-1 (256 KB / 1 MB).
- CP/M 2.2/P (Peřina) s RAM diskem E: i ELIS 61k CP/M s RAM diskem C: běží. ELIS v3.00 má nevyjasněnou
  klávesnici.
- Test `test_zps4`, celkem 23 testů. Popis v `docs/software/CPM_ZPS-4.md`.


**Hotové: ZPS-3** (`machines/zps/ZPS-3.sapi`).
- Nové karty JPR-1A (varianta JPR-1: BOOT, porty 01–03, D2C), AND-1A (D3A, E800) a RAM-1A (D1A,
  0000–BFFF).
- Originální 48K MON V1.0-C1 zavede z diskety CP/M 2.2/P (Peřina) a funguje DIR, TYPE, SAVE, ERA.
- Dekodéry RAM-1, RAM-1A, JPR-1A a AND-1A jsou soubory PROM v `roms/`, jiná adresace = jiný soubor
  (rozhodnutí autora).
- Test `test_zps3`, celkem 22 testů. Popis v `docs/software/CPM_ZPS-3.md`.

**Hotové: řadič pružných disků RPD-1 a MIKOS/D V5.0** (`machines/zps/ZPS-2_MIKOS_D.sapi`).
- Systém se zavede z diskety, funguje DIR, SAVE/LOAD a BASIC z diskety.
- Diskety 8" jako `.img` v okně Média (vložit, nová, vysunout, ochrana proti zápisu).
- Registry RPD-1 vycházejí z obrázků návodu a z rozboru EBIOS. Popis v `docs/desky/RPD-1.md`, test
  `test_rpd1`, celkem 21 testů.
- V záloze sapi.cz jsou i obrazy CP/M disket pro ZPS-3/4 (`data2/cpm48*`, `cpm62*`, `cpm64k*`).

**Hotové: BASIC EXP V5.0 a V5.0/G pod MIKOSem V5, nová karta DGD-1 (grafika 320×240).**
- `media/mikos/*.hex`, v GUI Soubor → Nahrát program do paměti (Intel HEX), `.B`.
- DGD-1 má vlastní okno „DGD-1“, snímek přes MCP `screenshot display=DGD-1` a `sapiemu-cli --display`.
- Popis v `docs/desky/DGD-1.md` a `docs/software/MIKOS.md`.

**Hotové: MIKOS V5.0 na ZPS-2 s klávesnicí Consul** (`machines/zps/ZPS-2_MIKOS_V5.sapi`, ROM Monitor V5.0F/E
+ MIKOS V5.0). Naběhne, klávesnice funguje, `.K_S`/`.K_L` funguje, BASIC EX V4.0 z kazety běží. BDK-1 je
prázdná karta bez funkce. Popis v `docs/software/MIKOS.md`.

**Hotové: ZPS-2 s MIKOSem V4.0F.** Nová karta RAM-1 (16/32/48 KB, S1/S2 podle tabulky 1, `docs/desky/RAM-1.md`),
ROM Monitor V4.0 + MIKOS 4.0F/4.0D a V4.1 pro Consul v `roms/mikos/`. `machines/zps/ZPS-2.sapi` naběhne
s `MIKOS-1 V4.0F +`. Z kazety se nahraje TOOL 80 (PEDIT) a BASIC EX (WAV i MP3) a spustí se `.J`. Ukládání `.K_A`
a `.K_S` funguje a jde zpět načíst. Magnetofon zapisuje při rozběhu motoru krátké ticho. Popis
v `docs/software/MIKOS.md`, test `test_mikos`, celkem 20 testů.

**Hotové: DSM-1 s magnetofonem.** Terminál jako u DMR-1 (TCP, COM), kazety WAV i MP3 odkudkoli, tlačítka v okně
Média a v MCP. MIKROBASIC SAVE a LOAD fungují. Fázový dekodér čte skutečné nahrávky TOOL 48, TOOL 80 a BASIC EX ve
WAV i MP3 bez chyby. Popis v `docs/desky/DSM-1.md`.

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
| F5 | Sériová linka přes TCP a COM; klávesnice Consul 262.3, Consul 259.11, ANK-1; konfigurátor sestav a mapa portů | ✅ hotovo |
| F6 | ZPS-1 (JPR-1 8080, REM-1, AND-1, MIKROBASIC), ZPS-2 (RAM-1, DSM-1 s kazetou, MIKOS V4 a V5, BASIC EX/EXP, DGD-1), RPD-1 + MIKOS/D | ✅ hotovo |
| F7 | **ZPS-3:** JPR-1A, AND-1A, RAM-1A, CP/M 2.2/P z diskety přes RPD-1. Zbývá originální IMA CP/M 48K (disketa od autora), varianty 62K/64K | ✅ základ hotový |
| F8 | **ZPS-4:** JPR-1Z, RAM-1Z, AND-1Z, RMD-1, CP/M 2.2/P (MOR62Z) a ELIS | ✅ hotovo (originál Tesla IMA chybí) |
| F9 ✅ | **Liborovy desky V (hotovo):** JPR-1V (Z80, rychlejší, až 4 MHz), RAM-1V (silně konfigurovatelná, mapování MAP), AND-1V (VGA), DSM-1V (dva sériové kanály, bez kazety), ZRD-1V (zálohování tužkovými AAA, bez RTC a bez mapování). Podklady v `..\JPR-1V` atd. (PDF schémata) a na https://www.pmd85.cz/?page_id=292 | ✅ hotovo |
| F10+ | PKT-1; dokončení ZPS-3 (IMA CP/M 48K, 62K/64K); ukládání stavu; přepínání jazyka UI; zvuk bzučáku; převod HEX → kazeta; CI a Linux | |

Po každé fázi následuje commit, shrnutí, aktualizace tohoto souboru a zastavení, aby autor mohl výsledek ověřit.

## Na promyšlení (nemodelované věci a vylepšení)

- **AND-1V:** čekací stavy (RDY, jedno okno CPU na znak, 0–1 takt při 2 MHz, žádné při I/O).
- **JPR-1V:** STSTB bez refreshe (u JPR-1ZI je STSTB jen z /MREQ, což u AND-1Z může při refreshi s I = F8–FF
  spustit čekání). Visení RDY u DGD-1 během BOOT.
- **ZRD-1V:** vedlejší účinky čtení 61h–63h (nedefinovaná hodnota, která se zapíše do registru adresy).
- **ZPS-4 / ELIS v3.00:** obsluha klávesnice (dvojité nebo nekonečné opakování kláves).
- **RMD-1:** časování (INIR, NOP po `OUT 43`).
- **CGA-1V:**
  - čekací stavy jsou přibližné (do konce okna CPU 318 ns);
  - prolínání snímků je náhrada za oko (REMBRAN2);
  - paleta a VRAM jsou po zapnutí černé, ne náhodné.

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

16. **ZPS-2 a MIKOS:**
    - Byl MIKOS v ZPS-2 ve 4× 2708 (dump 1. KB tomu odpovídá), nebo ve 2× 2716? Emulace: 2708.
    - Dump PROM D1A z RAM-1 (emulace generuje obsah z tabulky 1).
    - Jsou v ZPS-2 opravdu rozpojené propojky přerušení (AND-1 49–50, JPR-1 10–11), jak radí manuál?
    - Bere MIKOS na skutečném HW klávesu stisknutou hned po vypsání otázky? (Emulace: ztratí se.)
    - MIKOS V5: kterou verzi monitoru (V5.0E nebo F) a kterou klávesnici máš? Byl v EPROM i ovladač XY 4130?

17. **DGD-1:** zapojení vstupů PROM D1A, okno CPU (čekací stavy), mapování. Máš desku nebo fotku grafiky?

18. **RPD-1 a MIKOS/D:** čítač 2 a OVFL, doba ustálení hlavy, 4 nebo 5 nul před datovou značkou, TOUT.
    Jak rychle se na skutečném stroji nahraje BAS5C.SYS (emulace 17 s)? Funguje LOAD bez přípony?
    Máš 8" mechaniku a disketu MIKOS/D?

19. **ZPS-3:**
    - Máš bootovatelnou originální systémovou disketu IMA CP/M 48K pro ZPS-3 (k MON V1.0-C1)?
    - Ve které patici JPR-1A je boot EPROM (D3B nebo D4B)? Jaké propojky máš na JPR-1A, AND-1A a RAM-1A?
    - Jsou na AND-1A D3A vstupy A6/A7 (G/H) na +5 V?
    - Dumpy PROM D2C, D3A, D1A pro potvrzení přepsaných tabulek.
    - Má AND-1A 1 KB, nebo 2 KB video RAM?
    - Pro 62K a 64K: PROM D3A pro F800 a F000 a jak se dvě RAM-1A vyhnou displeji.

20. **ZPS-4** (odpovědi autora 2026-10-04):
    - **RMD-1 PROM:** bit 2 (O3) = port 40 (řada), bit 3 (O4) = 43 (příprava čtení)? Schéma má popisky
      obráceně. Autor desku nemá, zeptá se kamaráda. Emulace zatím: bit 2 řada, bit 3 příprava čtení.
    - **Originální boot EPROM Tesla IMA a systémová disketa ZPS-4:** autor je nemá, zeptá se po nich.
    - AND-1Z vstup A6 DA1: vyřešeno ze schématu, nastavuje ho přepínač S1 (`docs/desky/AND-1Z-89.md`).
    - Která ROM ELIS (v3.00/v7.01) patří k disketě: autor neví, vyzkoušet v emulátoru.
    - RMD-1 časové limity: autor neví.
    - Pořadí karet je celkem jedno. Klávesnice spíš **Consul 262.3** (ZPS-4 je novější).
    - **ELIS v3.00:** s Consulem 262.3 bere klávesy dvakrát (pulz STROBE) nebo donekonečna (7474). Jak
      monitor v3.00 čte klávesnici? (v7.01 funguje.)
    - **Pozorování autora (2026-10-04, GUI, ZPS-4):** jednou se systém zasekl a ve stavovém řádku přibývaly
      neobsloužené klávesy. Mohl to být i software. Zatím nevyhodnocovat, potřebuje to víc testování.

**Odpovědi autora k deskám V (2026-10-04)**, platí pro body 21 a 22:
- Do složek s podklady (`..\JPR-1V` atd.) doplnil **fotky hotových desek s výchozími propojkami** a do
  `..\ZRD-1V` **PDF schématu**.
- **JPR-1V:** 28C64 nemá, programuje si ho dynamicky (PicoROM). Rozhodnutí: **banka 0 = MikroBIOS 3.0,
  banka 1 = ECMON IDE-1**.
- **AND-1V:** rev 2.3, font neví. U něj je nastavená na **MAP1 = L** kvůli CGA-1V (deska přijde později).
- **RAM-1V:** rev 1.1, patice C prázdná. RTC Timekeeperu emulovat **volitelně** (parametr).
- **DSM-1V:** kanály na **10h a 14h, oba 115200 Bd**.
- **ECMON ani CP/M mapování nepoužívají.**
- **MikroBIOS (binárka) a fonty AND-1V smí do repa**, zdroje ne.
- Drobnosti (TURBO, oscilátor, bit 5 DSM-1V, polovina fontu rev 1.1) neví.

21. **Desky V – JPR-1V a AND-1V** (rozbor `docs/desky/JPR-1V.md`, `AND-1V.md`):
    - Který obraz EEPROM máš v JPR-1V a ve které bance (propojka BIOS osazená/rozpojená) je ECMON IDE-1 a
      MikroBIOS 3.0? Šel by dump 28C64?
    - Kterou revizi AND-1V má sestava V (fotky ukazují v1.1 i v2.3) a který font je vypálený (10×24 typ 1/typ 2,
      u rev 1.1 kterou sadu volí S1 9-10)?
    - Smí MikroBIOS 3.0 (HEX) a fonty AND-1V do `roms/`? Zdroje ne, jako u MikroMonu?
    - Lišta TURBO: tlačítko mezi piny 1 a 3 (pin 2 = LED)? Podle schématu dává 4 MHz pin 3 na zemi.
    - Oscilátor 8 MHz? Jede něco rychleji (10/5 MHz)?
    - Používá ECMON nebo CP/M na deskách V AND-1V mapovanou přes MAP1 (port a bit RAM-1V), nebo vždy F800
      s MAP1OFF?
    - Ověřit: s JPR-1V AND-1V při IN/OUT na porty F8–FF nečeká? Visí DGD-1 při BOOT opravdu?
    - Rev 1.1: která polovina fontu (A12 = 0/1) je standardní?

22. **Desky V – RAM-1V, DSM-1V, ZRD-1V** (rozbor `docs/desky/RAM-1V.md`, `DSM-1V.md`, `ZRMD-1.md`):
    - **ZRD-1V:** PDF schématu (vytisknout `ZRD-1V.sch` z Eagle jako u ostatních). Nuluje /RES čítače a
      registry adresy? Jsou 61h–63h opravdu nečitelné? Má hlídání MAX808 nějaký vliv viditelný pro software?
    - ZRD-1V: kterou revizi máš (fotka 1.1)? Co dělá zámek zápisu v rev 2.0 a souvisí s ním sekvence MikroMonu
      `OUT 61/62/63 = FF/FF/3F` + `OUT 60`?
    - RAM-1V: revize 1.1, nebo 1.2? Emulovat podle schématu 1.1 (1.2 nemá MAP2 u díry 16 KB)?
    - RAM-1V, patice C: je prázdná. Jaký Timekeeper se tam dává (M48T35/DS1644?) a emulovat jeho RTC?
    - DSM-1V: propojky na tvé desce (adresy A = 10h, B = 14h?, rychlosti obou kanálů).
    - DSM-1V b5 („START“): nemá funkci? Čte +3 FFh?
    - DGD-1/BGP-1S „paralelně s RAM-1V“: obě karty odpovídají na stejné adresy. Emulátor to dnes hlásí jako
      kolizi, bude potřeba výjimka.

23. **CGA-1V** (`docs/desky/CGA-1V.md`):
    - `..\CGA-1V\RADICROM.bin` k desce nepatří. Je tam omylem?
    - Je k dispozici dokument „Programování karty“ a novější CGATEST, který má i test CGA (volba 1)?
    - Liborovy programy smí do repa jako binárky (autor), jsou v `media/cga-1v/`.

## Jak pokračovat

1. `git pull`.
2. Přečíst `CLAUDE.md`, tento soubor a `docs/rozhodnuti.md`.
3. Sestavit přes `tools\build.cmd debug test`. Na PC s pomalým síťovým diskem použít lokální preset `local-debug`
   (soubor `CMakeUserPresets.json` se necommituje, vzor je v `docs/build.md`). Na autorově hlavním PC je repo od
   2026-10-04 na lokálním `E:\SAPI_GIT\SAPIemu` (dřív `H:\SAPI-1\GIT\SAPIemu`), stačí preset `windows-debug`.
   Po přesunu je sestavení čisté a všech 24 testů prochází. Autor ověřil překlad debug i release ve VS 2022.
4. Pokračovat další fází z tabulky. **Hotovo (2026-10-04): F9 desky V včetně CGA-1V.** Původní poznámky k F9:
   Výsledek
   a otázky pro autora se zapíšou do `docs/desky/JPR-1V.md`, `RAM-1V.md`, `AND-1V.md`, `DSM-1V.md` a
   `ZRMD-1.md` (ZRD-1V). ZPS-1 až ZPS-4 běží (2026-10-04).
   - Pokud se rozbor ztratí, zopakovat ho:
     - schémata `..\JPR-1V\JPR-1V.pdf`, `..\AND-1V\AND-1V_sch.pdf` (+ `rev2.3\Popis-AND-1V-rev.-2.3.pdf`,
       `FontROM\`), `..\RAM-1V\RAM-1V.pdf` (+ `Popis-RAM-1V-rev.-1.1.pdf`), `..\DSM-1V\DSM-1V.pdf` (+ xlsx
       s rychlostmi);
     - `..\ZRD-1V\DESCRIPTION` (PDF chybí, Eagle je binární);
     - stránka https://www.pmd85.cz/?page_id=292.
   - Porovnat s emulovanými JPR-1ZI/1Z, AND-1Z-89, RAM-1/1A, DSM-1 a ZRMD-1. Výsledek a otázky zapsat do
     `docs/desky/`.
   - Autor: na deskách V běží systém ze ZRMD-1 (ECMON + CP/M) a funguje s nimi boot ROM i IDE-1. Mapování
     dělá RAM-1V, ZRD-1V ho nemá.
   - **Rozbor je hotový** (`docs/desky/JPR-1V.md`, `AND-1V.md`, `RAM-1V.md`, `DSM-1V.md`, `ZRMD-1.md`) a autor
     odpověděl (viz „Odpovědi autora k deskám V“ v nejasnostech).
   - **Hotovo:** JPR-1V (typ JPR-1ZI, banky EEPROM propojkou BIOS, osc 8 MHz, TURBO), `roms/jpr-1v` (banka 0
     MikroBIOS 3.0, banka 1 ECMON IDE-1), `roms/and-1v` (fonty). Ověřeno: obě banky naběhnou se ZRMD-1.
   - **Další krok:**
     - z fotek desek přečíst výchozí propojky (běží agent), z PDF ZRD-1V ověřit reset a čitelnost 61h–63h;
     - implementovat JPR-1V, AND-1V (rev 2.3), RAM-1V (s generátorem MAP a volitelným RTC v C), DSM-1V,
       opravit ZRD-1V (pryč `map_jp2`);
     - sestava V s ECMON a CP/M ze ZRD-1V a IDE-1.
   - Zdroje MikroBIOSu jsou v `H:\SAPI-1\LiborLasota\JPR-1V\@orig\JPR-1V_monitor.zip` (do repa jen HEX/BIN). Otevřené otázky ke ZPS-3/4 jsou v bodech
   19 a 20. Zbývá originální IMA CP/M 48K, k tomu chybí
   disketa (bod 19), a varianty 62K/64K. Podklady jsou v `docs/software/CPM_ZPS-3.md`.
