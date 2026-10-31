# AND-30C – alfanumerický displej 80 × 21 s MC6845 (SAPI-IPS)

Zdroje:
- [sapi.cz/sapi/and-30c.php](https://sapi.cz/sapi/and-30c.php): schéma „AND-30Cu“, které ze skutečné desky metodou
  zpětného inženýrství překreslil P. Všijanský (EC1045, 25. 12. 2020), plošný spoj, fotka desky, obsah PROM dekodéru
  a obou EPROM znakového generátoru (Intel HEX). Dokumentace ani software k desce nejsou.
- Netlist schématu vznikl strojově z vektorového PDF (`and-30c.pdf`, export EAGLE) a byl ručně zkontrolovaný na
  výřezech schématu a proti zapojení vývodů obvodů v katalozích. Použité soubory: `roms/and-30c/`.
- Sestava SAPI-IPS (JPR-1Z, AND-30C, RAM-64, RFD-Z) na sapi.cz: `ram-64.php`, `rfd-z.php`, `jpr-1z.php`.

Stav (2026-10-31): emulované podle schématu, sestava `cpm_and30c.sapi` (Ostatní → CP/M s ECMONem) s nově
napsaným ovladačem v ECMONu (`docs/software/ECMON.md`, oddíl AND-30C).

## Shrnutí: má smysl replika?

**Krátce:** zapojení je promyšlené a skutečná deska nejspíš fungovala. Spoléhá ale na rychlosti součástek mimo
katalogové hodnoty a je naladěné propojkou. Replika podle schématu by proto mohla rozběhnout jen s rychlými
pamětmi a doladěním fáze. Na dvě místa schématu je potřeba se zeptat (Petr má originál), viz Nejasnosti.

| Riziko | Proč | Co s tím u repliky |
|---|---|---|
| **Řetěz MA → RAM → EPROM bez registru** | Adresa z 6845 jde přes multiplexer do 2114 a data z RAM přímo na adresu EPROM. Znak trvá 555 ns. Katalogové maximum řetězu (6845 160 ns, 74157 23 ns, 2114 450 ns, 2716 450 ns) je asi 1080 ns. 74165 bere data až druhým nahráním po adrese (zpoždění DE a CURSOR o 2 znaky v 74175 tomu odpovídá), takže řetěz musí být kratší než asi 740–930 ns a zároveň **delší** než asi 185–370 ns (podle JP17, níže). Funguje to jen v pásmu skutečných rychlostí součástek („wave pipelining“). | Rychlé 2114 (200 ns) a EPROM (350 ns, nebo EEPROM 28C16 150 ns), JP17 vyzkoušet. Spolehlivě: registr 74LS374 mezi RAM a EPROM a o takt delší zpoždění DE a CURSOR. |
| **JP17: fáze hodin 6845** | Hodiny 6845 jsou QB, QC nebo QD kruhového čítače 74164 (2, 3 nebo 4 body po nahrání 74165). Volba posouvá okno platných dat: QB snese pomalejší a QD rychlejší součástky. Je to ladicí propojka pro konkrétní čipy, ne volba režimu. | Nechat jako na originálu (QD), při chybách zkusit QC a QB. |
| **/CE1 PROM na /REF** | Podle schématu je /CE1 dekodéru 74S287 přímo na X1 špičce 7. Na JPR-1Z je tam /REF (L jen v cyklech refreshe), PROM by tedy povolila desku jen při refreshi a karta by neodpovídala. Na pájecím bodu JP10 je tentýž spoj. V SAPI-IPS byla RAM-64 statická a /REF nepotřebovala, nejspíš byl spoj přerušený nebo JP10 spojená se zemí. | Ověřit na originálu (otázka 1). Bez toho JP10 na GND a spoj na špičku 7 přerušit. |
| **MAP2 budí deska** | Výstup O3 PROM (výběr desky, L pro F800–FFFF už podle adresy) je spojený s MAP2 sběrnice (pull-up R5). RAM-64 přes propojku JP6–JP8 podle MAP2 uvolní okno F800–FFFF. JPR-1Z MAP1/MAP2 nebudí. | S procesorovou kartou nebo deskou, která MAP2 budí (ZRMD-1 s JP2, PKT-1, RAM-1V), spoj přerušit. Pozor, i bez toho musí RAM nechat F800–FFFF volné (62 KB). |
| **E 6845 = FI2 sběrnice** | 6845 dostává jako E hodiny sběrnice 2 MHz (střída 50 %, E v H 250 ns). MC6845 (1 MHz, jako CM607P na desce) chce aspoň 450 ns. Zápisy nejspíš projdou, je to ale mimo katalog. | MC68B45 nebo HD6845 (2 MHz). Se sběrnicí 4 MHz nepůjde ani ten. |
| **Krátké pulzy z kruhového čítače** | Nahrání 74165 (SH/LD) a hodiny 74175 dává zpětná vazba QF → 7404 → CLR 74164: pulz trvá jen zpoždění hradel (asi 20–35 ns). Závisí na řadě obvodů (TTL, LS, ALS). | Stejné řady jako originál (MH74164, MH7404), nebo pulz udělat synchronně. |
| **LC oscilátor** | Bodové hodiny 10,8 MHz dává tranzistor KSY71 s laditelnou cívkou, ne krystal. Kmitočet se ladí a ujíždí, televizor to ale snese. | Krystalový oscilátor 10,8 MHz, nebo blízký, např. 10,7386 MHz, s řádkem 15,56 kHz. |
| **Synchronizace na kompozitním výstupu** | HS a VS se slučují logickým součtem přes diody. Během VS (6845 má pevně 16 řádků) chybí řádkové pulzy, horní řádky se mohou ohnout. Synchro vychází asi 0,65 V pod černou, norma chce 0,3 V. | Většina televizorů to snese. Lepší je XOR, nebo samostatné HS a VS na X2, které tam jsou. |

**Plusy:**
- přístup procesoru se neblokuje a nejsou čekací stavy, „sníh“ místo toho zakryje zatemnění;
- 6845 umožňuje libovolný formát;
- výstupy TTL (VID, HS, VS i negované) i kompozitní video;
- font je stejný jako u AND-1Z.

## Deska

Fotka na sapi.cz ukazuje osazenou desku, na které byly opravy. Většinu z nich Petr do schématu zanesl, odtud „u“
v názvu (AND-30Cu). Součástky:
- CM607P z roku 1988 (bulharský klon MC6845);
- 2 × MHB2716C se štítky „AND 80 0“ a „AND 80 1“ (deska se původně asi jmenovala AND-80, tak se jmenují i konektory
  v Petrově knihovně);
- MH74S287 (dekodér), K555AP6 (= 74LS245), 74ALS244, 3 × DG74157, 4 × MHB2114, K555TM8 (= 74LS175), UCY74165;
- MH74164, MH7474, MH74ALS04, MH74ALS05, MH7404, K555LA2 (= 74LS30), 2 × K555LA3 (= 74LS00);
- laditelná cívka a tranzistory oscilátoru vlevo nahoře.

Jsou na ní i dlouhé propojovací drátky. 8286 (IC3A) je ve schématu paralelně s 74LS245 jako náhradní pozice, na
desce osazená není. Součástky bez hodnoty ve schématu (R10–R15, C15) osazené nebyly.

Podle Petra šlo o součást sestavy SAPI-IPS, kde se osazovalo jen to, co bylo potřeba. Vypadá spíš jako kus nebo
malá série než sériový výrobek Tesly.

## Adresace

- **PROM IC2 (74S287), obsah z desky:**
  - vstupy: A0–A4 = AB11–AB15, A5 = /MW, A6 = /MR, A7 = JP8;
  - /CE2 = RES: PROM je vypnutá během /RES, výstupy drží v H rezistory 3k57;
  - /CE1 = X1 špička 7 (/REF), viz výše.
- **Obsah:** všude 0Eh, kromě adres F800–FFFF pro A7 = 0:

  | Index | /MR /MW | Výstup | O2 (DIR 74LS245) | O3 (/SEL, MAP2) | O4 (/WE, R/W 6845) |
  |---|---|---|---|---|---|
  | 3Fh | čtení | 08h | 0: data na sběrnici | 0 | 1 |
  | 5Fh | zápis | 02h | 1 | 0 | 0 |
  | 7Fh | jen adresa | 0Ah | 1 | 0 | 1 |

  O1 je vždy 0 a vede jen na pájecí bod JP11.
- **Proč stav „jen adresa“:** deska se vybere (O3) už podle adresy, ještě před /MR a /MW. Multiplexer 74157 tak
  stihne přepnout RAM na adresu procesoru dřív, než přijde zápis.
- **6845:** 7430 (IC6) ho vybere, když je deska vybraná a A5–A10 = 1, tedy FFE0–FFFF. RS = A4:
  - adresový registr FFE0h (zrcadla FFE1–FFEF);
  - datový registr FFF0h (zrcadla FFF1–FFFF).

  Zapisuje se sestupnou hranou E (FI2) při R/W = O4 = L. Číst jdou R14–R17. R16 a R17 jsou světelné pero, LPSTB
  není zapojený.
- **RAM:**
  - 4 × 2114 = 2 KB na F800–FFDF. Banky po 1 KB vybírá RA10 (A10 nebo MA10), při výběru 6845 jsou obě vypnuté.
  - Buňky 7E0–7FF tedy procesor nedosáhne, displej je ale ukáže, když na ně 6845 adresuje.
  - Formát 80 × 25 (2000 B) by se ještě vešel, rolování posunem začátku (R12/R13) přes celé 2 KB ne.
  - Adresní vodiče 2114 jsou proházené (stejně u všech čtyř), pro procesor i 6845 je to tatáž buňka.
- **MAP2:** O3 je spojený s MAP2 sběrnice. V okně F800–FFFF je L, jinak H (pull-up). Na RAM-64 vede MAP2 podle
  čárkované propojky JP6–JP8 na hradlo 7438 ve výběru pamětí. Tak se RAM-64, která zabírá celých 64 KB, nejspíš
  uhne displeji (otázka 3).
  Emulátor MAP2 sběrnice touto deskou nebudí (paměť sestavy je nastavená na 62 KB), viz Emulace.
- **RDY se nepoužívá:** spoj z DE 6845 přes 74LS244 na pájecí bod JP6 je na desce přerušený (mezera ve
  schématu). Jinak by se RDY budil při každém cyklu a procesor by stál v každém zatemnění.

## Zobrazení

- **Bodový kmitočet:** 10,8 MHz (oscilátor KSY71 s laditelnou cívkou). Kruhový čítač 74164 + 7404 dělí šesti:
  znak = 6 bodů, 1,8 MHz, 555,6 ns. Hodiny 6845 bere JP17 z QB, QC nebo QD přes JP18 a 7400. Ve schématu je
  QD čárkovaně spojené s JP18.
- **Znakový generátor:**
  - 2 × 2716: IC9 kódy 00–7F, IC10 80–FF. D7 přepíná /CE přes JP13 a JP14, druhý /CE přes invertor.
  - A0–A3 = RA0–RA3, A4–A10 = bity 0–6 kódu, 16 B na znak, řádek 0 nahoře. Body jsou D7..D2 (D7 vlevo).
  - Podle propojek JP12–JP16 jde osadit i jednu 2732 (D7 na A11) nebo 2 × 2732 se dvěma sadami (A11 z MAP
    nebo S, Petr). Emulátor má 2 × 2716 jako originál.
- **Font:** je to font AND-1Z (`roms/and-1z-89/AND-1Z_CharGen.bin`), jen s řádky v obvyklém pořadí (u AND-1Z
  je řádek 11 nahoře). Liší se jen „ď“ (C4h) a „ů“ (EAh), každé o jeden bod.
- **Kurzor:** 6845 CURSOR i DE zpožďuje 74175 o dva znaky. Kurzor se k bodům přičte logickým součtem (7400), je
  to tedy plný blok na řádcích R10–R11. Blikání řídí 6845 (R10 bity 5–6).
- **Zatemnění při přístupu procesoru (IC12):**
  - O3 (výběr desky) nuluje IC12B (74LS74). Ten přes R7 330 Ω a C12 15 nF drží asi 4,5 µs předvolbu IC12A a ta
    nastaví video na černou.
  - Každý přístup procesoru do desky tak od svého začátku zatemní asi 8 znaků. Na displeji je místo „sněhu“
    krátká černá čárka.
  - Další přístup uvnitř zatemnění ho neprodlouží.
  - Při rolování (LDIR přes 1600 B) je zatemněná zhruba polovina obrazu, obrazovka během rolování bliká.
- **Video na X2:**
  - TTL: VID, /VID, HS, /HS, VS a /VS (74ALS04);
  - kompozitní CV přes otevřené kolektory 74ALS05, diody KA221 a emitorové sledovače KSY82 a KSY71. Na 75 Ω
    vychází přibližně synchro 0,5 V, černá 1,15 V a bílá 1,8 V.

### Časování pro televizor (ovladač v ECMONu)

Desku nastavuje program, žádné původní hodnoty registrů neznáme. Ovladač SAPIemu (`tools/ecmon_and30c.mac`, CRTTAB)
používá tyto:

| Registr | Hodnota | Význam |
|---|---|---|
| R0–R3 | 114, 80, 89, 8 | řádek 115 znaků = 63,89 µs (15,65 kHz), 80 zobrazených (44,4 µs), HS od znaku 89, šířka 8 znaků (4,4 µs) |
| R4–R7 | 25, 0, 21, 23 | 26 řádků znaků × 12 = 312 řádků (50,1 Hz), 21 zobrazených (252 řádků), VS od řádku 23 |
| R8–R11 | 0, 11, 4Ah, 11 | bez prokládání, 12 řádků na znak, kurzor řádky 10–11, bliká (1/16) |
| R12–R15 | 0 | začátek obrazovky a kurzor |

80 znaků zabírá 44,4 µs z viditelných asi 52 µs řádku PAL. 21 řádků po 12 (252) se vejde do asi 288 viditelných.
Proto Petr uvádí nejvýš 21 řádků.

## Emulace

`src/cards/and30c.cpp`, typ `AND-30C`, katalog `src/cards/catalog.cpp`. Parametry:

| Parametr | Význam |
|---|---|
| `eprom0`, `eprom1` | IC9 a IC10, výchozí `roms/and-30c/AND-30C_IC9_EPROM0.bin`, `AND-30C_IC10_EPROM1.bin` |
| `decoder` | PROM IC2, výchozí `roms/and-30c/AND-30C_IC2_74S287.bin`. Dekóduje se přímo podle ní. |
| `ce1` | `gnd` (JP10 na GND, výchozí, předpoklad pro SAPI-IPS) nebo `ref` (podle schématu: jen v cyklech refreshe, které emulátor nemá, karta tedy neodpovídá) |
| `prom_a7` | `gnd` (JP8 na GND, výchozí) nebo `map1` (deska jen při MAP1 = L) |
| `blank_on_access` | `true` jako na desce: přístup procesoru zatemní obraz na 4,5 µs. `false` je pohodlí emulátoru, obraz bez čárek. |
| `monitor` | barva luminoforu monitoru (není na desce) |

**Co se emuluje:**
- 6845 v registrech: formát, začátek, kurzor a blikání;
- 2 KB RAM včetně skrytých buněk pod 6845;
- čtení zápisových registrů vrací FFh (vnitřní sběrnice desky nikdo nebudí, předpoklad);
- obraz se kreslí po snímcích podle registrů;
- zatemnění podle času přístupu: kde byl paprsek, se počítá z registrů, od /RES (RESET 6845 nuluje čítače) a se
  zpožděním dvou znaků. Kreslí se přístupy posledního snímku.
- `describe` (MCP `machine_info`) vypisuje řádkový a snímkový kmitočet z registrů a varování, se kterými by
  televizor nesynchronizoval.

**Co se neemuluje:**
- časování řetězu MA → RAM → EPROM a JP17: obraz je vždy správný;
- prokládání (R8);
- buzení MAP2;
- světelné pero;
- RESET 6845 nemaže registry.

Test: `tests/test_and30c.cpp` (části `unit` a `cpm`).

## Nejasnosti (k ověření na originálu, otázky pro Petra)

1. **/CE1 dekodéru (IC2 vývod 13)**
   - Podle schématu je na X1 špičce 7 (/REF) a na pájecím bodu JP10.
   - S JPR-1Z by se deska vybrala jen v cyklech refreshe. Je na desce spoj na špičku 7 přerušený, nebo je JP10
     spojená se zemí? Jak to bylo na JPR-1Z v SAPI-IPS?
   - Emulátor předpokládá /CE1 = L (`ce1: gnd`).
2. **Propojky JP1–JP3, JP8, JP10:** čárkovaná čára ve schématu vede z JP8 (A7 PROM) k JP1 se zubem k JP2 (GND).
   Které body jsou na desce spojené? Kam vede JP3 (spojený s JP1)? Obsah PROM platí jen pro A7 = 0.
3. **MAP2:**
   - Je výstup O3 (výběr desky) na MAP2 sběrnice úmysl, kvůli RAM-64 přes JP6–JP8? A je na RAM-64 opravdu
     osazená propojka JP6–JP8?
   - Které úrovně MAP2 RAM-64 vypínají? Obsah PROM 74188 RAM-64 je na sapi.cz.
4. **JP17/JP18:** který výstup 74164 (QB, QC, QD) je na originálu spojený s JP18? Schéma kreslí QD.
5. **Mezera ve spoji pod IC11:** DE 6845 → 74LS244 → JP6. Je na desce přerušený plošný spoj, nebo jen chyba
   kresby? Je JP6 něčím propojená (RDY je na JP4)?
6. **Rychlosti součástek:** změřit na originálu zpoždění od sestupné hrany hodin 6845 po data na vstupech 74165
   (osciloskop nebo analyzátor). Ukáže, jak velká je rezerva řetězu MA → RAM → EPROM.
7. **Software:** existoval k SAPI-IPS ovladač, monitor nebo systém, který AND-30C nastavoval? Hodnoty registrů
   6845 by řekly, jaký formát se používal (21 řádků podle Petra).
8. **Kmitočet oscilátoru:** 10,8 MHz uvádí Petr. Na kolik je cívka naladěná, a jak stabilní je?
