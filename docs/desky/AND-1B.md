# AND-1B – náhrada AND-1 / AND-1A s výstupem VGA (Libor Lasota)

**Stav (2026-10-06): emulovaná** (typ `AND-1B`, `src/cards/and1b.*`, test `test_and1b`). Rozbor podkladů rev 1.4,
nic neověřeno na HW. Podklady: `..\SAPI-karty\AND-1B\`.

## Zdroje

- [B1] `E:\SAPI_GIT\SAPI-karty\AND-1B\rev1.4\AND-1B.sch`: binární EAGLE (starý formát), rev 1.4, razítko listu
  06.06.2024 8:59 (soubor 6. 6. 2024). „Datum posledni upravy 10. 4. 2022“ v XML je jen popis knihovní součástky
  konektoru VGA, ne datum desky. Netlist se dá vytáhnout offline přes `"C:\EAGLE 9.5.1\eaglecon.exe" -C "EXPORT NETLIST n.txt; EXPORT PARTLIST
  p.txt; WRITE x.sch; QUIT;" kopie.sch` (na kopii; `eagle.exe` chce přihlášení).
  Všechny piny a sítě níže jsou z netlistu, ne z obrázku.
- [B2] `AND-1B_schema.pdf`: 6 listů (1 časová základna, 2 generování obrazu a paměť fontu, 3 paměťový podsystém,
  4 dekodér adresy, přerušení, blikání, 5 konektory a napájení, 6 časové průběhy a tabulky bank fontu).
- [B3] `Kusovník AND-1B rev. 1.4.xls`.
- [B4] `libor_info_AND-1B.txt` (UTF-8, ne cp1250): popis fontu.
- [B5] `FontAND1B11\main.c` (kompilátor fontu „AND-1B rev. 1.1“), `FontAND1B11.bmp` (448×160, 16 barev),
  `FontAND1B11.bmp.bin` (32 KB).
- [B6] `Foto\AND-1B_front.jpg`, `AND-1B_back.jpg` (deska s potiskem tabulek propojek).
- [B7] https://www.pmd85.cz/?page_id=2102. Odkazy „Popis AND-1B
  rev. 1.4“ a „Fonty“ jsou na webu šedé, **soubory ke stažení tam nejsou**.

## 0. Revize zdrojů (kontrola 2026-10-06)

| Zdroj | Revize / datum | Poznámka |
|---|---|---|
| `rev1.4\AND-1B.sch` | **rev 1.4**, DPLA0068ADB, 06.06.2024 | ze všech fakt v těchto poznámkách |
| `AND-1B_schema.pdf` | **rev 1.4** | vytištěno autorem 06.10.2026 14:21 z téhož `rev1.4\AND-1B.sch` (zápatí i metadata PDF: EAGLE 9.5.1), tedy totožné s [B1] |
| `Kusovník AND-1B rev. 1.4.xls` | **rev 1.4** (soubor 6. 6. 2024) | odpovídá partlistu, jen D1 BAT42 místo BAT41 a QG1 „DIL8/DIL14“; S3 (pájecí) v BOM chybí |
| `libor_info_AND-1B.txt` | revize neuvedena (soubor 19. 12. 2024) | popis 2× 16 KB a 16 B na znak (+0..+4, +8..+12) sedí na adresování rev 1.4 |
| `FontAND1B11\` | kompilátor „pro AND-1B rev. 1.1“ (soubory 15. 4. 2024) | rozložení bank (A13–A10) a řádků sedí přesně na tabulku listu 6 rev 1.4, `.bin` = výstup `main.c` z BMP. „1.1“ je nejspíš verze kompilátoru/fontu, ne desky; i kdyby byla deska 1.1, adresování fontu je stejné jako u 1.4 |
| `Foto\AND-1B_front.jpg`, `_back.jpg` | **rev 1.4** | zadní strana: „AND-1B rev. 1.4“, „DPLA0068ADB“, „TXT40x24VGA“; přední: kód výroby „2677188f_y108_240516“ (16. 5. 2024), osazení a potisk odpovídají partlistu (IC1–IC31 na místech, R5, R6, S1, S2, D1, QG1) |
| web [B7] `?page_id=2102` | **rev 1.4**, publikováno 1. 9. 2024 | text i odkazy „Popis AND-1B rev. 1.4“ (bez souboru) a „Fonty“ (bez souboru); **ZIP ani PDF ke stažení nejsou** |
| web `AND-1B-rev.-1.4.jpg` | **rev 1.4** | zmenšená stejná fotka jako `Foto\AND-1B_front.jpg` |
| web `mAND-1B-rev.-1.4-grn.jpg` | **rev 1.4** (EXIF 3. 6. 2024) | obrazovka „SAPI AND-1B REV. 1.4“, test atributů |
| web `mAND-1B-uB24.jpg` | **starší revize** (EXIF 9. 3. 2023, před výrobou PCB 1.4) | MIKROBASIC, dvojitá šířka a „A“ na 3900; chování shodné s 1.4 |

- **Všechna fakta v těchto poznámkách jsou z rev 1.4** (netlist [B1] = PDF = BOM = fotka desky). Starší revize
  (prototyp z 2023, případně „1.1“) nemáme ve schématu ani v BOM, takže rozdíly v propojkách, INT1, barvách nebo
  časování **nelze porovnat**. Jediný doklad starší desky (fotka obrazovky z 3/2023) ukazuje stejný formát
  40 × 24, stejné umístění 3800 + řádek·40h a stejnou dvojitou šířku.
- Webová stránka (1. 9. 2024) popisuje vlastnosti, které rev 1.4 přesně splňuje: adresy 2800/3800/E800/F800,
  15 barev, INT1 60 Hz nativně + 50 Hz, VGA 640×480/60.
- **Fotky desky ukazují nejnovější známou revizi (1.4).**
- S3 (H/L, vstup TS oscilátoru) je na fotce zadní strany **nepropojená**; oscilátor má TS s vnitřním pull-upem,
  běží tedy. Pro emulaci bez významu.

## 1. Co to je

- „Deska textové grafiky“, **přímá náhrada AND-1 / AND-1A s výstupem VGA** [B7]:
  - „100% kompatibilní s AND-1/AND-1A včetně mapování videoram“;
  - 40 × 24 znaků, znak 7 × 10 bodů, font 64 znaků (ASCII 32–95) s atributy blikání, podtržení, dvojitá šířka;
  - adresa 2800h, 3800h, E800h nebo F800h; 1 z 15 barev inkoustu na černém pozadí;
  - VGA 640×480/60 Hz; INT1 synchronní s obrazem (nativně 60 Hz, „s násobičem“ 50 Hz);
  - CPU přistupuje asynchronně, při přístupu „sněží“.
- Na fotce z MIKROBASICu [B7 `mAND-1B-uB24.jpg`] `POKE HEX(3900),1` dá „A“ na začátek 5. řádku (řádek 4):
  potvrzuje základ 3800 a 64 bajtů na řádek.
- Libor (pmd85.cz, 9. 5. 2024) ji zkoušel v sestavě odpovídající ZPS-1 (JPR-1/JPR-1S + MIKROBASIC).

### Obvody (rev 1.4, [B1] partlist)

| IC | Typ | Úloha |
|---|---|---|
| QG1 | oscilátor 25,175 MHz (DIL14/DIL8) | základní takt |
| IC2A | 74LS74 | dělič 2 → DOTCLK 12,5875 MHz |
| IC9 | 74LS175 | 7stavový čítač fáze znaku (QA=/Q1, QB, QC, QD, /F) |
| IC17 | 74LS393 | H čítač H1–H32, nuluje se při H = 57 |
| IC7, IC8, IC1 | 74LS20, 74LS00, 74LS02 | dekódování H (HZKR, HOR40, HOR48), SH/LD, NEXTCHAR |
| IC23 | 74LS74 | /HZAT (horizontální zatemnění), HSYNC |
| IC16 | 74LS138 | /HSON (H=45), /HSOFF (H=52) |
| IC12 | 74LS90 | čítač mikrořádků 0–9 (L0, L1, L2, L4) |
| IC14 | 74LS393 | L8 (polovina znaku), R1–R16 (textový řádek) |
| IC13, IC29 | 74LS30, 74LS00 | konec snímku (525 řádků), /R24, /SEL, /FLASH |
| IC21 | 74LS138 | VS_POSIT (VSYNC) |
| IC31 | 74LS74 | VSYNC (/IRQOFF), R24ZAT |
| IC19 | AS6C6264-55 (8 KB SRAM) | video RAM, **využitý jen 1 KB** |
| IC24, IC25, IC27 | 74LS157 | multiplexer adresy CPU/obraz (výběr /SEL) |
| IC18 | 74LS157 | „překlad“ adresy sloupců 32–63 (skládání do 1 KB) |
| IC26 | 74LS245 | budič dat CPU ↔ RAM |
| IC20, IC6 | 74LS32, 74LS37 | /READ, /WRITE, /OPEN, buzení synchronizace |
| IC28 | 74LS85 | dekodér adresy (komparátor A15–A11 s propojkami) |
| IC10, IC11 | 74LS175, 74LS174 | latch kódu znaku, /BLANK, /FLASH |
| IC4 | 27C256 (AT27C256R-70) | znakový generátor (font) |
| IC2B | 74LS74 | LEFT/RIGHT (dvojitá šířka) |
| IC3 | 74LS166 | posuvný registr bodů |
| IC5 | 74HCT08 | hradlování barev R, GH, GL, B |
| IC22 | 74LS193 | dělič přerušení 6 → „50 Hz“ |
| IC15 | 74LS74 | INT_ENA, výstup INT1 |
| IC30 | 74LS393 | čítač snímků pro blikání |
| D1 | BAT41 (BOM: BAT42) | INT1 na sběrnici (otevřený výstup, aktivní L) |
| S1, S2 | 2×4 kolíky | barva, adresa a přerušení |
| S3 | pájecí propojka SJ2W | vstup TS (tristate) oscilátoru: 1 = GND, 3 = VCC |

Žádná PROM ani GAL. Na sběrnici K1 jsou zapojené jen: /RES (8), /MR (9), /MW (11), A0–A15, D0–D7, /INT1 (59),
napájení. **RDY, /IOR, /IOW ani STSTB zapojené nejsou**, takže karta nemá porty a nečeká.

## 2. Dekodér adresy a propojka S2

IC28 74LS85: A3 = A12, B3 = SET_A12; A2 = A13, B2 = VCC; A1 = SET_QADR, B1 = A15; A0 = SET_QADR, B0 = A14;
kaskádní vstup A=B_I = **A11**, A<B_I = A>B_I = GND. Výstup A=B_O = SEL.

- SEL = (A15 = A14 = SET_QADR) ∧ A13 ∧ (A12 = SET_A12) ∧ A11.
- /SEL = NAND(SEL, /RES) (IC29 4,5 → 6): při /RES = L je karta odpojená.
- **Okno 2 KB** (A10–A0 jdou do multiplexeru), RAM 1 KB uvnitř zrcadlená (viz kap. 4).
- Propojky S2 (R6 4k7 na VCC, kolíky 2, 4, 6, 8 = GND, **osazená = 0**):

| S2 | Síť | Rozpojená | Osazená | Potisk |
|---|---|---|---|---|
| 1-2 | SET_INT_50HZ | INT deaktivováno | 50 Hz | „50Hz“ |
| 3-4 | SET_INT_60HZ | INT deaktivováno | 60 Hz | „60Hz“ |
| 5-6 | SET_A12 | A12 = 1 | A12 = 0 | „/A12“ |
| 7-8 | SET_QADR | A15:14 = 11 | A15:14 = 00 | „/A15:14“ |

- Adresa (A15–A11 podle listu 4: „111x1“ / „001x1“, „xxx1x“ / „xxx0x“):
  - F800–FFFF: 5-6 i 7-8 rozpojené;
  - E800–EFFF: 5-6 osazená (AND-1A pro ZPS-3);
  - 3800–3FFF: 7-8 osazená (AND-1, AND-1A s S1, MIKROBASIC);
  - 2800–2FFF: obě osazené (není na potisku, jen na webu).
- Potisk desky: tabulky „3800h / E800h / F800h“ a „INT1 OFF / INT1 50Hz / INT1 60Hz“.
- **Na fotce [B6] i [B7] je osazená jen 7-8 → 3800h, INT1 vypnuté.**
- SEL závisí jen na adrese (ne na /MR, /MW). Multiplexer se proto přepne na CPU v každém cyklu s adresou v okně
  (i fetch, IN/OUT s A15–A8 = port: port 38–3F při 3800, F8–FF při F800), data se ale přenesou jen s /MR nebo /MW.

## 3. Přístup CPU k video RAM

- /READ = /MR ∨ /SEL, /WRITE = /MW ∨ /SEL (IC20). RAM: /CS1 = GND, CS2 = VCC (vybraná stále), /WE = /WRITE,
  /OE = IC27 1Y (při /SEL = L je /OE = /READ, při zobrazení /OE = GND).
- IC26 74LS245: DIR = /READ, /G = /OPEN = NOT(NAND(/READ, /WRITE)) – budič je zapnutý jen při čtení nebo zápisu.
  Bity dat jsou 1:1 (D0 ↔ ID0 …). Fyzické zapojení datových a adresových vývodů 6264 je přeházené (kvůli spojům),
  na funkci to nemá vliv.
- **Čtení i zápis bez čekání**, čte se celých 8 bitů. RAM je jednobranová, CPU má přednost.
- **„Sněžení“:** IC10 D3 = /SEL se zachytí spolu s kódem znaku (signál /BLANK). Když CPU drží adresu v okně ve
  chvíli zachycení znaku, je /BLANK = L a SH/LD = NAND(/HZAT, /BLANK, QB, /QC) nedovolí nahrát 74LS166 → **ta buňka
  zůstane černá** (166 vysune A = GND a SER = GND). Žádné smetí z adresy CPU, jen černé buňky.

## 4. Mapování video RAM

### Adresa CPU (offset o = adresa − základ, 0–7FF)

- sloupec = o & 3Fh (A0–A5), řádek = o >> 6 (A6–A10, 0–31); řádek r začíná na základ + r·40h (jako AND-1).
- Multiplexery (výběr /SEL, L = adresa CPU, H = čítače obrazu):

| RAMn | CPU | obraz |
|---|---|---|
| RAM0–RAM5 | A0–A5 | H1, H2, H4, H8, H16, H32 (sloupec) |
| RAM6–RAM10 | A6–A10 | R1, R2, R4, R8, R16 (řádek) |

- IC18 74LS157, výběr = RAM5 (sloupec ≥ 32):
  - RAM5 = 0: TRANS_A = RAM4, TRANS_B = RAM3, TRANS_C = RAM9, TRANS_D = RAM10;
  - RAM5 = 1: TRANS_A = RAM10, TRANS_B = RAM9, TRANS_C = 1, TRANS_D = 1.
- Vývody 6264: A0 = RAM0, A2 = RAM2, A3 = RAM8, A4 = RAM7, A5 = RAM6, A10 = RAM1, A11 = TRANS_A, A12 = TRANS_B,
  A7 = TRANS_C, A9 = TRANS_D; **A1, A6, A8 = GND** → využitých 10 adresových bitů = 1 KB.

### Výsledný index do 1 KB (CPU i obraz jdou stejnou cestou, logický index)

```
col = o & 63; row = (o >> 6) & 31;
if (col < 32) idx = row * 32 + col;
else          idx = (24 + (row & 7)) * 32 + (row >> 3) * 8 + (col & 7);
```

- Sloupce 32–39 řádků 0–23 leží v místě „řádků 24–31, sloupců 0–23“ (300h + (row & 7)·32 + (row >> 3)·8 + col & 7).
  Viditelné buňky 40 × 24 se nepřekrývají.
- **Sloupce 40–63 zrcadlí 32–39** (A3, A4 se při A5 = 1 neuplatní).
- Řádky 24–31 jsou neviditelné. Jejich sloupce 0–23 jsou totožné se sloupci 32–39 viditelných řádků (řádek 24 + j,
  sloupec 8q + k ≡ řádek 8q + j, sloupec 32 + k). Sloupce 24–31 a 32–63 řádků 24–31 jsou společná neviditelná RAM.
- **Srovnání s emulátorem (`And1::vram_index`):** emulátor má sloupce 32–63 na 300h + řádek·8 + (sloupec & 7).
  Viditelné chování je stejné, liší se jen to, které neviditelné adresy se zrcadlí. Protože Libor píše
  „100% kompatibilní … včetně mapování videoram“, je vzorec výše **nejlepší dostupný odhad i pro původní AND-1
  (multiplexer D4C)**. Ověřit proti schématu AND-1, pak případně opravit i `And1::vram_index`.

## 5. Obraz a časování

### Bodový a znakový takt

- QG1 25,175 MHz → IC2A (T) → **DOTCLK 12,5875 MHz** (bod 79,4 ns = 2 body VGA).
- IC9 74LS175 jako 7stavový čítač: D1 = /F = Q3 ∨ Q4, D2 = /Q1, D3 = Q2, D4 = Q3. Stavy Q1Q2Q3Q4:
  0000 → 0100 → 0110 → 1111 → 1011 → 1001 → 1000 → 0000. **Znak = 7 bodů**, znakový takt 1,798 MHz (556 ns).
- Fáze (podle listu 6 „SHIFTER LOAD, CHAR LOAD, INC ADR“):
  - SH/LD = L ve stavu 0100 (QB = 1, QC = 0) → nahrání 166 na hraně 0100 → 0110;
  - NEXTCHAR = NOR(/QC, LEFT/RIGHT): náběh při QC 0 → 1 (tatáž hrana) → zachycení kódu znaku do IC10/IC11;
  - H se zvýší při náběhu Q1 (hrana 0110 → 1111).
  - Pipeline: znak zachycený na konci periody H = h se zobrazí až v další znakové periodě.
- 74LS166: B–H = D0–D6 EPROM, A = GND, SER = GND, výstup QH. **D6 je levý bod**, D0 pravý, 7 bodů bez mezery
  (mezera je ve fontu). D7 EPROM = příznak DOUBLE (nesvítí).

### Horizontálně (jednotka = znak = 7 bodů 12,5875 MHz)

- H čítač 0–56, při H = 57 (/HZKR = NAND(H1, H8, H16, H32)) asynchronní nulování → **57 znaků = 399 bodů =
  31,698 µs, 31,548 kHz** (VGA 800 bodů 25,175 MHz = 400 bodů).
- Viditelných **40 znaků** (H 0–39, /HZAT = vzorek HOR40+ = H ≥ 40) = 280 bodů = 560 bodů VGA.
- HSYNC: /HSON (Y7 IC16 = H 45) → HSYNC aktivní, /HSOFF (Y2 = H 52) → konec. Šířka 7 znaků = 49 bodů = 3,89 µs.
  **Záporná polarita** (IC6 NAND).
- Při 1,3znakovém zpoždění pipeline vychází obraz ~88 bodů VGA za koncem HSYNC (VGA standard 48) a končí ~40 bodů
  před standardním koncem: **obraz 560 × 480 je ve 640 × 480 vystředěný s okraji 40 bodů vlevo i vpravo**
  (odhad ±1 bod znaku).

### Vertikálně (jednotka = mikrořádek = 1 řádek VGA)

- IC12 74LS90 (BCD, takt H32 = konec řádku): počet c = 0–9, L0 = c & 1, (L1, L2, L4) = c >> 1 = 0–4.
- IC14: L8 (polovina znaku 0/1), R1–R16 (textový řádek 0–31).
- **Řádek fontu = L8·5 + (c >> 1), každý řádek fontu 2× (zdvojení)** → buňka 7 × 10 bodů fontu = 14 × 20 bodů VGA.
- Textový řádek = 20 řádků VGA, **24 viditelných řádků = 480 řádků**.
- /R24 = NAND(R8, R16) (řádky 24+) maže 166 (černo).
- Konec snímku (IC13): L0 ∧ L2 ∧ R2 ∧ R8 ∧ R16 → textový řádek 26, c = 5 → **525 řádků**, snímek 16,641 ms,
  **60,09 Hz** (poznámka ve schématu: „zakladni frekvence INT je 60.091Hz (60Hz+0,15%)“).
- VSYNC: Y0 IC21 (L1 = L2 = L4 = 0, L8 = 1, R1 = 0, /R24 = 0) → řádky 490–491, **2 řádky, záporná polarita**.
  Přesně VGA 640×480: zatemnění 480–524, přední okraj 10, synchro 2, zadní okraj 33.

### Barva (S1, R5 4k7 na VCC, kolíky 1, 3, 5, 7 = GND, osazená = složka vypnutá)

| S1 | Síť | Odpor | Potisk |
|---|---|---|---|
| 1-2 | SET_RED | R4 430R → R | „/R“ |
| 3-4 | SET_GRLO | R3 1k5 → G | „GL“ |
| 5-6 | SET_GRHI | R2 680R → G | „GH“ |
| 7-8 | SET_BLU | R1 430R → B | „/B“ |

- IC5 74HCT08: PIXEL ∧ SET_x. Úrovně do 75 Ω (výstup ~4,9 V): R a B 0,73 V (100 %), G jen GH 0,49 V (~67 %),
  jen GL 0,23 V (~33 %), obě 0,68 V (~97 %).
- 16 kombinací − 1 (vše osazené = černá) = **15 barev**. Potisk desky:
  - **WHT**: nic neosazeno;
  - **AMB**: osazené GL a /B → R 100 %, G ~67 %, B 0 (jantarová);
  - **GRN**: osazené /R a /B → zelená 100 %.
- Na fotce desky [B6] je GRN. Fotky obrazovky [B7] jsou zelené.
- Stejné schéma barev jako AND-1V (`docs/desky/AND-1V.md`), dá se použít stejný kód.

## 6. Znakový generátor (IC4 27C256)

- Adresa EPROM:
  - A0–A2 = L1, L2, L4 (řádek fontu v polovině 0–4), A3 = L8 (polovina);
  - A4–A9 = CHR0–CHR5 (D0–D5 bajtu VRAM), A10 = CHR6 (D6), A11 = CHR7 (D7);
  - A12 = LEFT/RIGHT, A13 = FLASH.VZORK (= zachycený /FLASH);
  - **A14 = VCC (vývod 27)** a vývod 1 (VPP u 27C256, A14 u 28C256) = VCC → čte se horní polovina 4000–7FFF.
    Proto Libor dodává „2× 16 kB (dvě zrcadla kvůli záměně 27C256/28C256)“ [B4].
- **Banka 1 KB = A13–A10 = {FLASH, LR, D7, D6}**, banka = D6 + 2·D7 + 4·LR + 8·A13. Znak = 16 bajtů, řádky fontu
  na +0..+4 a +8..+12 (+5..+7, +13..+15 = 0, nikdy se nečtou).
- Tabulka bank ze schématu (list 6) a obsah souboru:

| Banka [A14:A10] | Obsah | Banky v souboru |
|---|---|---|
| x0x01 | prázdná (blikání) | 1 (5) |
| x1x10 | podtržené znaky (podtržení na řádku fontu 8) | 10 (14) |
| xx111 | **levé** poloviny dvojitých znaků | 7, 15 |
| xx011 | **pravé** poloviny dvojitých znaků, D7 = 1 | 3, 11 |
| xxx00, x1x01, x0x10 | standardní znaky | 0, 8, 9, 2 (a 4, 6, 12, 13) |

- Banky 4, 5, 6, 12, 13, 14 (LR = 1 bez atributu 11) se nikdy nevyberou, jsou to kopie.
- `FontAND1B11.bmp.bin`: 32 KB, obě poloviny shodné (ověřeno), **přesně odpovídá `main.c` aplikovanému na
  `FontAND1B11.bmp`** (ověřeno přepisem do Pythonu). Bit 7 má 640 bajtů v bankách 3 a 11 (64 znaků × 10 řádků).
  Znaky jsou 5 × 7 v buňce 7 × 10 (sloupce 1–5, řádky 0–6, řádky 7–9 prázdné, podtržení na řádku 8).
- **Znaková sada = MHB 2501 (64 znaků):** 00–1F = `@A–Z[\]^_`, 20–3F = mezera až `?`, na 24h je `¤` (jako AND-1).
  Azbuka (MHB 2502) není, dala by se udělat vlastním fontem.
- Formát BMP pro `main.c`: 16 barev, 448 × 160 (64 znaků × 7 bodů, 16 bank × 10 řádků), bod svítí, když barva ≠ 0.
  Řádek obrázku = banka, všech 16 bank se musí udržovat (kombinací vznikají efekty) [B4].

### Atributy (D7–D6 bajtu VRAM)

| D7–D6 | Význam (list 6) | Banka A13 = 1 / A13 = 0 |
|---|---|---|
| 00 | standardní znak | 8 / 0, obě normální |
| 01 | blikající znak | 9 normální / 1 prázdná |
| 10 | blikající podtržení (kurzor) | 10 s podtržením / 2 bez |
| 11 | znak dvojité šířky | 3 + 7 / 11 + 15 (nebliká) |

- **Blikání:** IC30 74LS393 počítá snímky (takt /IRQOFF = začátek VSYNC). FLASH_A = bit 3, FLASH_B = bit 4,
  /FLASH = NAND(bit 3, bit 4) → **A13 = 0 jen 8 z 32 snímků**. Perioda 32 snímků = 532 ms (1,88 Hz), znak
  (podtržení) svítí 24 snímků (399 ms) a 8 nesvítí (133 ms). Střída 75 : 25, ne 50 : 50 jako předpoklad
  v `and1.cpp`.
- **Dvojitá šířka** (IC2B, D = D7 EPROM, takt /QC; NEXTCHAR = NOR(/QC, LR)):
  1. Znak n s atributem 11 se zachytí, EPROM (LR = 0) = banka 3/11 s D7 = 1 → LR := 1.
  2. Další buňka: NEXTCHAR je blokovaný, latch drží n, zobrazí se EPROM (n, LR = 1) = **banka 7/15 = levá
     polovina**, D7 = 0 → LR := 0.
  3. Další buňka: zobrazí se EPROM (n, LR = 0) = **banka 3/11 = pravá polovina**, zároveň se zachytí znak n + 2.
  - **Bajt na pozici n + 1 se nezobrazí** (stejně jako `span = 2` v `and1.cpp`). Ověřeno složením glyfů: banka 7
    a pak 3 dává správné „SAPI“, opačné pořadí nesmysl. Komentář v `main.c` („u leve casti … prechod na pravou“)
    je obráceně proti tabulce na listu 6, platí tabulka.
  - Široký znak ve sloupci 39: druhá buňka padne do zatemnění (/HZAT), zobrazí se **jen levá polovina**.
  - Řádek začíná s LR = 0: IC10 (D6, D7, /BLANK, /FLASH) se nuluje /HOR48 (H ≥ 48).
  - Emulace: řídit se bitem 7 bajtu fontu (ne atributem), pak funguje i vlastní font.
- Inverze ani kurzor v HW nejsou, „kurzor“ je atribut 10.

## 7. Přerušení INT1 (list 4)

- Výstup: IC15A /Q (INT1TTL) → D1 BAT41 (katoda) → **/INT1 sběrnice K1/59, aktivní L, otevřený výstup**
  (wired-OR).
- IC15A: D = INT_ENA, takt = R24ZAT (náběh = začátek zatemnění, řádek 480), /CLR = /IRQOFF (L během VSYNC,
  řádek 490).
  → **Pulz INT1 od řádku 480 do 490 = 10 řádků ≈ 317 µs** (poznámka ve schématu: „delka pulsu INT je 320us
  (original AND-1/AND-1A ma 640us)“). Začátek je na konci viditelného obrazu (po 24. textovém řádku).
- IC15B: D = INT_DIV_ABC, takt = /IRQOFF (náběh = konec VSYNC, řádek 492), /PRE = SET_INT_60HZ, /CLR = VCC,
  Q = INT_ENA.
- IC22 74LS193: DOWN = R24ZAT (−1 za snímek na řádku 480), UP = VCC, vstupy DCBA = 0110 (6), /LOAD = /BO (při 0
  a DOWN = L, tj. na začátku dalšího snímku se nahraje 6), **CLR = SET_INT_50HZ (aktivní H!)**.
  INT_DIV_ABC = QA ∨ QB ∨ QC (stav ≠ 0).
- Režimy:
  - **S2 1-2 i 3-4 rozpojené: INT1 vypnuto** (193 držený v nule → INT_ENA = 0);
  - **1-2 osazená („50 Hz“)**: čítač 6 → … → 0, ve snímku, kdy dojde na 0, je INT_ENA = 0 → v následujícím snímku
    pulz vynechá. **5 pulzů ze 6 snímků, průměrně 50,08 Hz**, intervaly 4× 16,64 ms a 1× 33,28 ms;
  - **3-4 osazená („60 Hz“)**: /PRE drží INT_ENA = 1 → pulz každý snímek, 60,09 Hz (platí i s 1-2 osazenou).
- Pro emulaci: pulz pro řádky 480–489 z 525 (z periody snímku 16,641 ms je to 15,215–15,532 ms od začátku 1. viditelného
  řádku), v 50Hz režimu čítač snímků mod 6.

## 8. Emulace (2026-10-06)

- Typ **`AND-1B`** (`src/cards/and1b.*`), parametry:
  - `base`: `3800h` (výchozí, jako autorova deska), `E800h`, `F800h`, `2800h` (S2 7-8, 5-6);
  - `int1`: `off` (výchozí), `50hz`, `60hz` (S2 1-2, 3-4): pulz řádky 480–490 z 525, v 50Hz režimu 5 snímků ze 6;
  - `color`: barva inkoustu S1 jako AND-1V (`white`, `green` – výchozí jako autorova deska, `amber`…);
  - `font`: `roms/and-1b/FontAND1B11.bin` (= `FontAND1B11.bmp.bin`, 32 KB; 16KB soubor se bere jako horní
    polovina).
- Obraz 640 × 480 (bod fontu 2 × 2 body VGA, obraz 560 bodů s okraji 40 vlevo i vpravo), snímek 525 × 31,698 µs.
- **Atributy řídí font**, jako v hardwaru: banka = {fáze blikání, LR, D7, D6}, dvojitá šířka podle bitu 7 bajtu
  fontu (levá polovina v buňce znaku, pravá v další, bajt další buňky se nezobrazí). Blikání: fáze vypnutá 8 snímků
  z 32 (75 : 25). Funguje tedy i s vlastním fontem.
- Rozložení video RAM podle vzorce v kap. 4 (`And1b::vram_index`).
- Neemulováno: „sněžení“ (černé buňky při přístupu CPU), přesná fáze obrazu proti CPU (obraz se kreslí celý).
- Test `test_and1b`: ZPS-1 s AND-1B místo AND-1 (MIKROBASIC), jedinečnost viditelných buněk, atributy z fontu
  (blikání, podtržení, dvojitá šířka), INT1 vypnuto / 60 Hz (36 pulzů za 600 ms) / 50 Hz (30).

## 9. Nejasnosti

1. **Font ve skutečných deskách:** je vypálený `FontAND1B11.bmp.bin` (kompilátor „rev. 1.1“, deska rev 1.4)? Na
   webu jsou „Fonty“ bez odkazu. Fotky obrazovky [B7] se s fontem shodují (`¤` na 24h, podtržení, široké znaky).
   Autorova deska rev 1.4 má EPROM, kterou si autor vypálil sám z `FontAND1B11` (2026-10-13), fotka obrazovky
   s fontem sedí. Jak je to u desek od Libora, zůstává otevřené.
2. ~~**Střída blikání 75 : 25**~~ Ověřeno na autorově desce 2026-10-13 (`tools/hwbench/and1b.asm`): blikající znak
   svítí déle, než je zhasnutý, za 10 s asi 19 bliknutí (perioda 32 snímků = 0,53 s), jako emulátor. Blikající
   podtržení (atribut 10) se ukazuje jen ve svítící fázi, jako ve fontu.
3. **Skládání sloupců 32–63:** na AND-1B ověřeno 2026-10-13 (`and1b.asm`, fotka obrazovky proti emulátoru):
   sloupce 40–47 i 56–63 zrcadlí 32–39 a řádky 24–31, sloupce 16–23 jsou sloupce 32–39 řádků 16–23, přesně podle
   vzorce v kap. 4. Otevřené zůstává, jestli se tak skládá i skutečná AND-1 (D4C), pak opravit i předpoklad
   v `And1::vram_index` (liší se jen neviditelné zrcadlení).
4. Polohu obrazu (okraje 40 bodů VGA) mám jen spočtenou z pipeline, mohou se lišit o pár bodů. Na monitoru se stejně
   automaticky upraví.
5. S3 (TS oscilátoru): na fotce rev 1.4 nepropojená (H/L). Pro emulaci nemá význam (L by oscilátor vypnula).
5a. **Starší revize desky** (prototyp 2023, fotka MIKROBASICu): schéma ani BOM nemáme. Jestli jsou v oběhu desky
   starší než 1.4 s jinými propojkami (adresa, INT1, barva), zeptat se Libora.
6. Výchozí barva sestavy: autorova deska je GRN, AND-1V má v emulátoru výchozí bílou. Rozhodne autor.
7. INT1 50Hz režim dává nepravidelné intervaly (4× 16,6 ms + 1× 33,3 ms). Programy počítající čas (MIKROBASIC,
   hodiny) to vidí jako 50 Hz v průměru. **Změřeno na autorově desce 2026-10-13** (`and1b_int.py`): impulz 317,0 µs,
   intervaly 4× 16,641 ms a 1× 33,283 ms, přesně jako schéma a emulátor. Zbývá potvrdit, že Libor nic jiného
   nezamýšlel.
8. BOM uvádí BAT42, schéma BAT41 (bez vlivu).
9. Žádná propojka 20/24 řádků ani volba MHB 2502 (na rozdíl od AND-1/AND-1A): pevně 24 řádků, font jen
   z EPROM.
