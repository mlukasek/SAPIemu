# JPR-1V – procesorová deska Z80 (Libor Lasota, upgrade JPR-1Z)

Stav: **emulovaná** (2026-10-04), sestava `machines/sapi1v.sapi`, test `tests/test_sapi1v.cpp`.
Typ `JPR-1V` třídy JPR-1ZI (`src/cards/jpr1zi.*`):
- parametry `rom` (8 KB), `bios` (`fitted` = banka 0, `open` = banka 1), `turbo`, `osc_hz`, `z80` (`cmos`
  výchozí, `nmos`);
- **Z80 je CMOS** (autorova deska: Zilog Z84C0020PEG): hned po `LD A,I` a `LD A,R` nepřijme přerušení, až po další
  instrukci (změřeno 2026-10-13, `docs/zmeny-podle-hw.md`). Parametr `z80` to přepne pro desku s NMOS čipem;
- výchozí obraz `roms/jpr-1v/JPR-1V_28C64_MikroBIOS_ECMON.bin`: banka 0 MikroBIOS 3.0, banka 1 ECMON IDE-1
  (autorovo rozhodnutí);
- na autorově desce (fotka `H:\SAPI-1\LiborLasota\JPR-1V\Foto\mJPR1V.jpg`) je propojka BIOS osazená a TURBO
  rozpojené (2 MHz).
- **TURBO a BIOS za chodu** (jako na skutečné desce vyvedené na tlačítka panelu ZDR, autor): tlačítka TURBO a
  BIOS na panelu ZDR, menu Stroj („TURBO karty CPU“, „Propojka BIOS rozpojená“), konfigurátor („platí hned“), MCP
  `cpu_turbo`. Banka EEPROM se čte jen při BOOT, změna se tedy projeví po RESETu.
  - Časová základna emulace je osc/2 (4 MHz). Při 2 MHz udělá CPU takt jen v každé druhé jednotce, ostatní
    karty se proto nemusí přepočítávat.
  - **Čekání karet při 2 MHz** (od 2026-10-09): Z80 vzorkuje /WAIT na svých hranách, CPU proto stojí vždy celé
    své takty (2 jednotky). Čekání, které karta hlásí v jednotkách 4 MHz, se zaokrouhlí na celé takty CPU
    (`whole_clocks` v `jpr1zi.cpp`: n = ⌈(w − ½ taktu) / takt⌉). Dřív mohl CPU stát půl taktu. Týká se hlavně
    CGA-1V, ověřeno programem ZRDBENCH proti skutečné desce (`docs/desky/ZRMD-1.md`).
  - Stavový řádek ukazuje skutečný takt CPU.
- **Neemulováno:** STSTB bez refreshe (týká se jen čekání AND-1Z), viz „Na promyšlení“ v SOUHRN.

Zdroje:
- [S1] schéma `..\SAPI-karty\JPR-1V\JPR-1V.pdf` (rev 1.1, 4 listy);
- [S2] `..\SAPI-karty\JPR-1V\DESCRIPTION`;
- [S3] monitor `H:\SAPI-1\LiborLasota\JPR-1V\@orig\JPR-1V_monitor.zip` (mimo GIT);
- [S4] https://www.pmd85.cz/?page_id=2120.

Vyrenderované výřezy schémat byly v dočasné složce rozboru, z PDF se dají vyrenderovat znovu (`docs/build.md`).

## Zapojení

- **Dekodér bez PROM a GAL:** pevná logika TTL. Kód karty ji provede přímo, soubor dekodéru neexistuje.
- **Paměť a BOOT:**
  - IC12 74LS688 porovnává A12, A14, A11, /BOOT, /AEN, A15, A13 s nulou. Jeho výstup P=Q budí /CE 28C64 i A12
    EEPROM. **Okno ROM je jen 0000–07FF při BOOT** a dosažitelné jsou jen dolní 4 KB EEPROM.
  - **A11 EEPROM = /BIOS:** propojka „BIOS“ na zem s pull-upem 4k7. Propojka osazená = banka 0 (EEPROM
    0000–07FF), rozpojená = banka 1 (0800–0FFF). Jen propojka, žádný port. [S4]: „8kB využívá pouze dvě banky
    2kB … přepínatelnými pomocí JUMPERu“, propojku lze vyvést na tlačítko na panelu.
  - Při čtení ROM je /MR potlačené (změřeno na desce 2026-10-11, `docs/zmeny-podle-hw.md`), **zápisy jdou vždy
    na sběrnici** (jako JPR-1ZI). MAP1/MAP2 na BOOT ROM
    nepůsobí [S4]. /WE EEPROM je odpojené, za provozu se nedá přepsat.
  - **BOOT klopný obvod** IC24B: RESET ho nastaví (červená LED), libovolný `OUT (00h)` ho shodí. Jako JPR-1ZI.
- **I/O:** IC9 74LS138 (A7, A6, A5, G2A = A4, G2B = A3) a IC7 (A = A2, B = A1, C = A0, obrácené pořadí). Port 00
  = vypnutí BOOT, 01/02/03 = P0/P1/P2. Plně dekódováno A0–A7, A8–A15 se ignorují. Funkčně **shodné s D6B**.
- **Porty P0–P2:** šest 3212.
  - P0-IN je transparentní (STB na H), na K2 tedy není P0-STB.
  - P1-IN se zachytává signálem P1-STB (K2-10).
  - P2-IN je transparentní.
  - Zapojení K2 je jako X2 u JPR-1ZI, současný model EKL-1/Consul sedí beze změny.
  - MikroBIOS čte klávesnici: čeká na `IN 01h` bit 0 = 0, pak `IN 02h` s CPL. ACK neposílá, čeká na uvolnění
    STROBE (rutina 0591, 500 čtení). Consul 262.3 proto bez úpravy 7474 (`latch_mod: false`).
- **Přerušení:** /INT0 → /INT, /INT1 → /NMI, RDY → /WAIT, /HOLD → /BUSRQ. /INTA = /IORQ OR /M1. Bez obvodu
  XINTA (jako JPR-1Z).
- **Takty:** oscilátor 8 MHz → 74HCT175.
  - **Takt sběrnice = 2 MHz vždy** (Johnsonův čítač ÷4).
  - **CPU 2 MHz nebo 4 MHz** podle vstupu TURBO (pin 3 třípinové lišty, pull-up 4k7): rozpojeno = 2 MHz,
    na zem = 4 MHz. Přepnutí je za chodu synchronní a bez zákmitů, software ho nevidí.
  - [S4]: s rychlejším oscilátorem stabilně sběrnice 5 MHz a CPU 10 MHz (osc/4, osc/2).
- **STSTB:** impuls na začátku každého cyklu paměti i I/O (včetně INTA), **kromě refreshe** (změřeno na desce
  autora 2026-10-11: 88–96 ns, i u čtení ROM, při INTA 0,66 µs po začátku /M1; `docs/zmeny-podle-hw.md`). U JPR-1ZI je STSTB
  jen z /MREQ. [S4] to popisuje jako opravu generování STSTB pro mapování.
  - **STSTB při INTA zablokuje sběrnici** (L. Lasota 2026-10-10, ověřeno pokusem): při potvrzení přerušení je na
    sběrnici PC. Když je v okně grafické karty (třeba ≥ C000h u CGA-1V, DGD-1V), karta na STSTB stáhne RDY,
    ale /MR ani /MW nepřijde, takže RDY už neuvolní a CPU stojí v čekacích taktech navždy. Stačí, aby přerušení
    přišlo, když program běží nad BFFFh, nebo když je v PC chvíli nesmyslná adresa ze zásobníku.
  - **Záplata** podle Libora jako u refreshe: od /IORQ STSTB negenerovat (jeden přeškrab a propojení pinů 3–4
    IC15). Zbývá ověřit kompatibilitu s ostatními kartami.
- **RDY a DGD-1:** „nelze použít DGD-1 jako prostou RAM – zůstane viset RDY při BOOT“ [S2]. DGD-1 začne čekání
  na STSTB s adresou, ale při čtení ROM 0000–07FF nedostane /MR.
- **Reset:** tlačítko je ošetřené proti zákmitům, synchronizované s /M1 a zkrácené monostabilním obvodem
  („zkrácený RESET pro součinnost s DRAM“), sečtené s power-on resetem MCP100.
  - **Důsledek** (L. Lasota 2026-10-10): tlačítko se vzorkuje jen během /M1. Když CPU stojí v čekacích taktech
    (zablokované RDY, viz STSTB při INTA), M1 nepřijde a RESET nezabere, pomůže jen vypnutí. Platí pro JPR-1Z
    i JPR-1V.
- /REF = /RFSH OR /MREQ jako JPR-1ZI.

## Software (EEPROM 28C64)

- **ECMON IDE-1:** `[S3]/JPR-1V/JPR1ZI_ECMON_IDE-1.bin` (2 KB) je shodný (MD5 87f3d36c…) s
  `roms/jpr-1zi/JPR-1ZI_ECMON_2k_IDE-1_Martin.bin`.
- **MikroBIOS 3.0 AllRAM (2024/03)** pro „JPR-1V/RAM-1V/AND-1V, Consul 262.3“: Intel HEX `[S3]/JPR-1V/jpr1v.txt`
  (org 0000, 0x719 B) a zdroje TASM.
  - Zkopíruje 0000–07FF do RAM pod ROM a provede `OUT (0)`.
  - Používá AND-1V na F800 (64×20), proměnné má na FD00 a SP = 0000, tedy v neviditelných řádcích VRAM 20–31.
- 8KB obraz = banka 0 + banka 1 + 4 KB FF. Který program je ve které bance, není popsané.

## Plán emulace

- Varianta `Jpr1zi` (typ `JPR-1V`) s pevnou logikou místo D3C/D6B:
  - okno ROM 0000–07FF, banka podle propojky BIOS, porty 00–03;
  - STSTB z MREQ i IORQ kromě refreshe;
  - sběrnice 2 MHz, CPU 2/4 MHz podle TURBO (přepínatelné za chodu), oscilátor jako parametr.
- Parametry: `rom` (8 KB) nebo `bank0`/`bank1`, `bios` (osazená/rozpojená), `turbo`, `osc_hz`.
- TURBO desky JPR-1V je jen takt CPU. Rychlost celé emulace je zvlášť (F8, menu Stroj → Rychlost emulace).
- **Emulováno od 2026-10-11** (změřeno na desce autora, `docs/zmeny-podle-hw.md`): přerušení přijaté s PC v okně
  CGA-1V, AND-1V nebo DGD-1V (se zapnutými čekacími stavy karty) zastaví CPU v cyklu INTA napořád; v logu je
  vysvětlení. RESET (tlačítko, Reset v GUI, MCP) nezabere, protože čeká na /M1; pomůže vypnout a zapnout.
  Program, který s povoleným přerušením zabloudí nad BFFFh, teď v emulátoru zamrzne jako na železe. Liborova
  záplata IC15 (STSTB jen od /MREQ) zatím jako volba není, čeká se na její přesné zapojení.

## Otázky pro autora

Jsou souhrnně v `docs/SOUHRN.md` (bod 21).

- **Čtení prázdné sběrnice** (změřeno 2026-10-11, `docs/zmeny-podle-hw.md`): když na čtení paměti nebo I/O
  nikdo neodpoví, dostane CPU poslední bajt své datové sběrnice: `IN A,(n)` → n, `IN A,(C)` → 78h,
  `LD A,(nn)` → horní bajt nn, `LD A,(HL)` → 7Eh. Platí i po výběru instrukce z ROM na kartě. Emulováno
  (`Bus::open_value`); varianty ECMONu bez svého řadiče pak čekají donekonečna, viz `docs/software/ECMON.md`.

## Nejasnosti k ověření

- **Záplata STSTB při INTA** (L. Lasota, 2026-10-10): přesné zapojení (přeškrab, IC15 piny 3–4) a které karty
  STSTB při I/O potřebují. Podle [S4] je STSTB z /IORQ oprava pro mapování, po záplatě se to musí ověřit
  s RAM-1V (registr MAP 63h), DSM-1V a ostatními.
- **FXS4 a přepisované definice znaků:** na Liborově sestavě se po čase přepisuje font FXS4 a nakonec se program
  zasekne. Podle Libora se v 1.0.1 přerušení přijalo s PC > BFFFh. V SAPIemu PC při INTA nad 904Fh nebylo
  (9. 10.). Zkouška fontu: viz `docs/SOUHRN.md` (2026-10-10).
