# BGP-1S – grafika kompatibilní s PMD-85 (Libor Lasota)

**Stav (2026-10-06): emulovaná** (typ `BGP-1S`, soubory `src/cards/bgp1s.*`, test `test_bgp1s`). Rozbor podkladů
rev 4.2, nic neověřeno na HW. Podklady: `..\SAPI-karty\BGP-1S\`.

Oficiální název karty je **BGP-1S**. V podkladech (schéma, BIOS) se objevuje i „BGP-1“, jde o tutéž kartu.
Procesorová karta k ní je JPR-1S (`JPR-1S.md`).

## Zdroje

- `..\SAPI-karty\BGP-1S\`:
  - `BGP-1rev42_schema.pdf`: 6 listů (1 časová základna, 2 generování obrazu, 3 videoram a multiplexery,
    4 synchronizace sběrnicových operací, 5 konektory a napájení, 6 časové průběhy). Rev. 4.2, 09.02.2022.
    Vektorové PDF, ale bez jmen sítí v textu.
  - `Eagle\BGP-1rev42.sch`: binární formát EAGLE (< 6.0). Netlist se dá vytáhnout offline přes
    `"C:\EAGLE 9.5.1\eaglecon.exe" -C "EXPORT NETLIST n.txt; EXPORT PARTLIST p.txt; WRITE x.sch; QUIT;" kopie.sch`
    (na kopii; `eagle.exe` chce přihlášení). Sítě níže jsou ověřené proti netlistu.
  - `Eagle\schema1-6.png` (listy schématu), `Eagle\Kusovník.xlsx`, `popis\Popis BGP-1S.docx`, `fotky\mBGP-1S.jpg`
    (deska s propojkami), `fotky\mBGP-1SaJPR1S.jpg` (sestava).
  - `jpr1s_sw\`: MIKROBIOS 1.02 (04/2022) + Falcon Monitor 6.1L pro JPR-1S/BGP-1S/ANK-1 (zdroje TASM, `jpr1s.bin`,
    `jpr1s.hex`, `jpr1s.lst`). Shodné s `..\SAPI-karty\JPR-1S\jpr1s_sw\` i se ZIPem `jpr1s_sw.zip` na webu.
- `..\SAPI-karty\JPR-1S\`:
  - `jpr1s_sw\`: totéž + **nové soubory `manic_sapi.txt` (Manic Miner), `auto.txt` (autíčka), `m.asm/m.txt/m.lst`,
    `jpr1s.txt` (= `jpr1s.hex`), `manic_main.jpg` (fotka obrazovky skutečné hry)**, `schov\` (záloha starší verze).
  - `jpr1s_mikrobios\`: MikroBIOS 2.1 (2024/11) pro JPR-1S/AND-1V/ANK-1, má příkaz `TEST BGP-1S [adr]`
    (`test_bgp.asm`).
  - `BIOS\bios_jpr1s.txt`: popis testovacího BIOSu („JPR-1S, BGP-1, ANK-1M5“; ANK-1M5 = ANK-1M2).
- Web <https://www.pmd85.cz/?page_id=1710> (článek a komentáře). ZIPy:
  - `BGP-1S.zip`: totéž co složka, **navíc `Eagle/Časování obrazu.xls`** (rozbor řádku pro rev 4.1 a 4.2; v lokální
    složce chybí);
  - `jpr1s_sw.zip`: shodné s `BGP-1S\jpr1s_sw` (binárka bajt po bajtu stejná).

## 0. Revize zdrojů (ověřeno 2026-10-06)

Libor dělá desky v několika iteracích, proto je u každého zdroje revize/datum:

| Zdroj | Revize / datum | Poznámka |
|---|---|---|
| `Eagle\BGP-1rev42.sch` | **rev 4.2**, rámeček „REV 4.2“, uloženo 09.02.2022 18:57 | knihovna dílů se jmenuje `BGP-1rev41` (zbytek z rev 4.1), K1 a IC10 z `BGP-1rev42`. Shodný (MD5) se souborem ve webovém ZIPu. |
| `BGP-1rev42_schema.pdf` | rev 4.2, rámeček 09.02.2022 18:57 | **vytištěno 06.10.2026 z téhož `.sch`** (patička s cestou E:\SAPI_GIT\…) → totožné se `.sch`. |
| `Eagle\schema1–6.png` | rev 4.2, rámeček **05.03.2022 10:07:55** | **pozdější uložení rev 4.2** než `.sch` v repu. Porovnáno pixelově s exportem listů z `.sch`: listy 1–4 se liší jen písmem a datem, **zapojení je stejné**; list 5 má navíc **C15 = 100n** (v `.sch` 47n); list 6 (časové průběhy) je **přepracovaný**: doplněné průběhy /BUF a /OE a poznámka o RC členu na /BUF (popisek „143 ns (1/7 MHz)“ zůstal). |
| `Eagle\Kusovník.xlsx` | „kusovník BGP-1S rev. 4.2“, ZIP 06.03.2022 | C15 100nF → odpovídá PNG (novější stav). Q1 6,5536 MHz. |
| `popis\Popis BGP-1S.docx` | bez čísla revize, ZIP 06.03.2022 | odpovídá rev 4.2 (IC30A, IC9A/B, IC13, RC člen na /BUF jako na listu 6 PNG). |
| `Časování obrazu.xls` (jen web ZIP) | dvě tabulky: **rev 4.1** a **rev 4.2** | liší se jen polohou řádkového synchra: 4.1 sync 52–56 (bez front porch), **4.2 front 52–53, sync 54–58**. Netlist rev 4.2 (IC7 Y1 = H 54, Y6 = H 59) odpovídá 4.2. |
| Fotky `mBGP-1S.jpg` (EXIF 25.02.2022), `mBGP-1SaJPR1S.jpg` (24.02.2022) | osazená deska, potisk „BGP-1S“, číslo revize na potisku není (vlevo nahoře je jen kód výrobce DPS „2677188F_Y63“) | rozmístění a číslování IO, potisk S1 „RGBM/ACE“, S2 „INT1/INT0“ a tabulka S3 souhlasí s netlistem rev 4.2. Kusovník uvádí DPS „DPLA0037BG4“. |
| Web page_id=1710 a `BGP-1S.zip` | ZIP: `.sch` 09.02.2022, PNG 05.03.2022, docx/xlsx 06.03.2022; komentáře Libora 12.3.2022 a 9.3.2023 mluví o **rev 4.2** | ZIP = lokální složka (MD5 všech souborů stejné) + xls navíc. |

**Závěr:** nejnovější známá revize je **4.2**, žádná novější (4.3…) se nikde neobjevila. `.sch`/PDF (09.02.)
a PNG/kusovník (05.–06.03.) jsou dvě uložení téže revize 4.2. Rozdíly mezi nimi (C15 47n/100n, kresba listu 6)
**nemají vliv na logiku ani na emulaci**. Rev 4.1 se podle xls lišila polohou řádkového synchra, což emulace
nepotřebuje. Všechna fakta v tomto popisu odpovídají rev 4.2.

### Ověření klíčových sítí proti netlistu

Vše, co bylo odečteno z obrázků, netlist potvrzuje (jména sítí jsou Liborova):

- **Výběr S3:** S3-1 = MAP1LEV, S3-3 = MAP1DIS, S3-5 = AD15, S3-7 = AD14 (pull-up R20), sudé kolíky GND.
  IC30D: MAP1OK = MAP1LEV ⊕ MAP1; IC30C: AD15OK = A15 ⊕ AD15; IC30B: AD14OK = A14 ⊕ AD14;
  IC29B: MAP1FED = NAND(MAP1OK, MAP1DIS); IC28A: **SEL = MAP1FED · /RES · AD14OK · AD15OK**. MAP1 = K1/21.
- **Čekání:** IC29C: /ACCESS = NAND(SEL, STSTB) → IC20A PRE; IC20A D = GND, CLK = /WE, CLR = WAIT_DIS =
  IC28B AND(/IOW, /IOR, /MR, /RES); IC20A /Q = RDY_TTL → D7 (BAT42) → RDY (K1/3).
- **Zápis:** IC29D: /SEL; IC13B: REQUEST = NOR(/MW, /SEL); IC29A: E_REQUEST = NAND(REQUEST, /SLOT_END);
  IC21B: D = E_REQUEST, CLK = /QB, PRE = /SLOT_END, Q = **/WE** (→ SRAM vývod 27, IC20A CLK), /Q = BUF_DELAYED;
  IC21A: D = /WE, CLK = /QC, PRE = REQUEST, CLR = Q = /SLOT_END; IC13A: /BUF = NOR(BUF_DELAYED, BUF_DELAYED).
- **Přerušení:** IC13C: VIDFETCH = NOR(/ACCESS, /M1); IC13D: /VIDFETCH → IC14A PRE; IC14A D = GND, CLK = V256,
  CLR = LOG1E (= /Q IC14B, který má PRE = CLR = GND), Q = /INT_TTL → D6 → /INT → S2-2; S2-1 = /INT0 (K1/62),
  S2-3 = /INT1 (K1/59).
- **Barvy a S1:** S1-1 = GND, S1-2 = ACE/RGBM, S1-3 = LOG1A. IC6C: SETXOR64 = NAND(/QB, QA, ACE/RGBM) → IC20B PRE;
  IC20B D = GND, CLK = C_ATR, Q = XOR64; IC30A: XA6 = MA6 ⊕ XOR64 → SRAM A2 (vývod 8). IC9A/B: MD8 ← MD6,
  MD9 ← MD7 při C_ATR. IC3 (CLK LD_ATR): /Q1 = L_DD6 (/MD6), /Q2 = L_DD7, /Q3 = L_DD9, /Q4 = L_DD8.
  IC2A: SUM_RED = NAND(L_DD6, L_DD8); IC2B: SUM_BLUE = NAND(L_DD7, L_DD9); IC4A: SUM_GRN1 = NAND(L_DD6, L_DD7,
  PIXMOD); IC4C: SUM_GRN2 = NAND(L_DD9, L_DD8, PIXMOD); IC4B: GRN_B = NAND(SUM_GRN1, SUM_GRN2);
  IC5B: RED_B = SUM_RED · PIXMOD; IC5C: BLU_B = SUM_BLUE · PIXMOD; PIXMOD = IC5D(PIX, PIX).
  RED_B → T2 → RED (K2/4), GRN_B → T1 → GREEN (K2/8), BLU_B → T3 → BLUE (K2/12). Mono: MOD1 = IC5A(PIX, PIX)
  → diody.
- **Časování:** IC16C: LD_ATR = QC · QA · DOTCLK; IC16A: C_ATR = QB · DOTCLK · /QC; IC6A: SH/LD = NAND(/HZAT, QA,
  QC); IC16B: ZKRH = H64 · H2 · H4; IC19A/B: V256 · V16 · V32 · V8; IC7 Y1 = HS_ON, Y6 = HS_OFF; IC12 Y1 = VSYNC.
  **Upřesnění:** Johnsonův čítač IC15 má hodiny ze sítě **7M** = výstup IC22C = invertovaný DOTCLK (IC22F →
  DOTCLK → IC22C). Stav se tedy mění na sestupné hraně DOTCLK, posuvný registr IC23 posouvá na náběžné. Pro
  emulaci bez vlivu.
- **Adresy:** IC11/IC27/IC8/IC24 (A/B = QA) → MA0–MA13 přesně podle tabulky v kap. 2. SRAM A6 i A7 jsou obě na MA8
  (proto je z 32 KB využito 16 KB), /CS = GND, /OE = /QA.

## 1. Co to je

Grafická karta pro SAPI-1 (sběrnice ARB-1), **programátorsky kompatibilní s grafikou PMD-85**: 288 × 256 bodů,
videoram 16 KB, řádek = 64 bajtů (48 viditelných × 6 bodů + 2 atributové bity). Iniciátor Petr (EC1045.01),
účel: převádění her z PMD-85. Testovaná s JPR-1 a JPR-1S, předpokládá se kompatibilita s JPR-1A/JPR-1Z.

Hlavní vlastnosti (popis + schéma):

- **Rozlišení:** 288 × 256, jediný režim. Žádný textový režim, žádný generátor znaků.
- **Barvy:** dva režimy volené propojkou **S1** (potisk „RGBM | ACE“):
  - **RGBM** (4 barvy, „PMD-85 RGBM“): atribut b7b6 bajtu = barva šestice bodů: 00 zelená, 01 červená, 10 modrá,
    11 fialová, pozadí černé.
  - **ACE = Colorace** (7 barev): atributy dvou šestic nad sebou (sudý + lichý mikrořádek) dávají jednu ze sedmi
    barev pro obě šestice (viz kap. 2).
- **Výstup:** **TV**, ne VGA. Kompozitní černobílý signál (VID) + digitální R, G, B pro SCART + /ZAT (trvale H přes
  100 Ω = přepnutí TV do RGB) + +12 V (AV režim TV). Konektor **K2 = FRB 30 (TY 513 3011)**, „aby šly použít
  stávající videokabely SAPI“.
- **Krystal Q1 6,5536 MHz** (kusovník; ve schématu „6,553MHz“) = bodový kmitočet. List 6 schématu uvádí
  „143 ns (1/7 MHz)“. To je zastaralý popisek ze starší revize, tabulka časování i kusovník mají 6,5536 MHz.
- **Videoram:** SRAM 62256 (32 KB, využito 16 KB; dva adresové vývody SRAM jsou spojené), doba přístupu ≤ 90 ns
  (Libor 12. 3. 2022: raději ≤ 70 ns).
- **CPU do videoram jen zapisuje.** Čtení není implementované (D-buffery 74LS125 IC25/IC26 jsou jen ve směru
  sběrnice → MD). Pro čtení musí být na stejné adrese RAM jiné karty (RAM-1, RAM-1Z, RAM-32…, nebo RAM na JPR-1S).
- **Okno:** 16 KB, kvadrant 0000h / 4000h / 8000h / C000h (propojky S3), volitelně podmíněné signálem MAP1.
- **Žádné I/O porty, žádné registry.** Karta se ovládá jen zápisem do paměti.
- **Synchronizace bez „sněžení“:** zápis CPU se provede v okně CPU každé bajtové periody, CPU mezitím čeká přes RDY.
- **Přerušení** 1× za snímek (50 Hz) na začátku mikrořádku 256, na /INT0 nebo /INT1 (S2). Nuluje ho
  **libovolné čtení operačního kódu (M1) z adresového okna karty**.
- Napájení jen +5 V (240 mA 74ALS, 250 mA 74LS, 130 mA 74HCT); +12 V jen průchozí na K2.

## 2. Formát obrazu (přesně podle schématu)

### Adresa

Multiplexery 74LS157 IC11, IC27, IC8, IC24 (list 3), A/B = QA (QA = 1 → adresa videa, QA = 0 → adresa CPU):

| bit offsetu | CPU | video |
|---|---|---|
| 0–5 | A0–A5 | H1, H2, H4, H8, H16, H32 (bajt v řádku) |
| 6 | A6 (přes XOR IC30A s IC20B Q = inverze pro Colorace) | V1 |
| 7–13 | A7–A13 | V2, V4, V8, V16, V32, V64, V128 |

Tedy **offset = 64 · y + x_bajt**, y = 0–255 (0 nahoře), x_bajt = 0–47 viditelné, 48–63 neviditelné (do SRAM se
zapíšou, nezobrazí se). Přesně jako PMD-85 (C000h + 64·y + x).

### Bajt

- Posuvný registr 74LS166 IC23 (list 2): SER, A, B = GND, C..H = MD5..MD0, výstup QH. Plní se v Johnsonově stavu
  s3 (SH/LD = NAND(okno, QA, QC)), pak se 6× posune. QH vydá **H = MD0 jako první** → **bit 0 je bod vlevo**,
  bit 5 vpravo, 1 = bod svítí. Potvrzuje to `bresenham_l6.asm` (`GetPixelAdr`: x mod 6 → maska 01h, 02h, 04h,
  08h, 10h, 20h) i font (`font.asm` = font PMD-85).
- CLR registru = /V256 → mikrořádky 256–311 černé. Mimo viditelných 48 bajtů se registr nenaplní, posouvá nuly.
- b6, b7 = atributy. Zachytává je IC3 (74LS175) při LD_ATR (stejný stav s3 jako načtení bodů), takže platí pro
  šestici bodů téhož bajtu.

### Barvy (list 2, IC2, IC4, IC5 → tranzistory T2/T1/T3 → RED/GREEN/BLUE)

Značení: P = bod (QH), a = b6, b = b7 vlastního bajtu; d, c = b6, b7 „pomocného“ bajtu (MD8 = IC9A ← MD6,
MD9 = IC9B ← MD7, zachycené při C_ATR):

- RED   = P · (a + d)          (IC2A NAND(/a,/d) → IC5B)
- BLUE  = P · (b + c)          (IC2B NAND(/c,/b) → IC5C)
- GREEN = P · (/a·/b + /c·/d)  (IC4A, IC4C, IC4B)

Pomocný bajt se čte v Johnsonově stavu s1–s2 z adresy s **invertovaným A6/V1** (IC30A XOR s IC20B Q), tj. ze
stejného sloupce **sousedního mikrořádku y ^ 1**, ale jen když je S1 = ACE. IC20B: PRE = NAND(/QB, QA, S1) (stav
s1), D = GND, CLK = C_ATR (stav s2) → inverze trvá jen během čtení pomocného bajtu.

- **S1 = RGBM** (vstup IC6C na GND): inverze nikdy, C_ATR zachytí atributy téhož bajtu (d = a, c = b) →
  R = P·a, B = P·b, G = P·/a·/b:

| b7 b6 | barva |
|---|---|
| 00 | zelená |
| 01 | červená |
| 10 | modrá |
| 11 | fialová (R+B) |

- **S1 = ACE (Colorace)**: pro sudý řádek 2k i lichý 2k+1 vychází stejný výsledek (vzorce jsou symetrické).
  Barva platí pro buňku 6 × 2 body, každý řádek ale kreslí své vlastní body P. Atribut A = b7b6 sudého,
  B = b7b6 lichého řádku:

| sudý \ lichý | 00 | 01 | 10 | 11 |
|---|---|---|---|---|
| **00** | zelená | žlutá | azurová | bílá |
| **01** | žlutá | červená | fialová | fialová |
| **10** | azurová | fialová | modrá | fialová |
| **11** | bílá | fialová | fialová | fialová |

  Kontrola proti softwaru: `tbColors` v `grafika.asm` (dw: nižší bajt = sudé řádky, vyšší = liché):
  White 00C0h (C0/00 → bílá), Red 4040h, Green 0000h, Yellow 0040h (40/00), Blue 8080h, Magenta C0C0h,
  Cyan 0080h (80/00). Vše sedí. Stejně `_tbColorace` v `test_bgp.asm`. Manic Miner kreslí vzduch střídavě 3Fh/BFh
  (00/10 → azurová), na `manic_main.jpg` je azurový.
- RGB výstupy jsou jen zap/vyp (emitorové sledovače, děliče 270/75 Ω): čisté barvy 0/255. Pozadí, okraj
  i zatemnění jsou černé. Žádné blikání, žádné poloviční jasy.
- **Černobílý výstup VID:** jas = jen P (IC5A → diody D1, D3, D5), atributy se neuplatní. Na ČB TV jsou tedy všechny
  body stejně bílé.

### Rozdíly proti PMD-85

- Žádné blikání a žádné úrovně jasu (PMD-85 má na ČB výstupu atributy jako jas/blikání). Barvy jen v RGB/SCART.
- Videoram jen pro zápis (PMD-85 čte), čte se RAM pod ní.
- Adresa volitelná (0000/4000/8000/C000h), u PMD-85 pevně C000h.
- Colorace (= ColorACE, rozšíření PMD-85 se dvěma mikrořádky) je přímo na desce. RGBM je jako PMD-85 s barevným
  adaptérem (zelená/červená/modrá/fialová).

## 3. Časování

Krystal 6,5536 MHz (oscilátor IC22D/E, IC22F → DOTCLK, IC22C → 7M = /DOTCLK pro IC15). IC15 74LS175 je Johnsonův
čítač ÷6 (D1 ← /QC, D2 ← QA, D3 ← QB): stavy (QA,QB,QC) s0 000 → s1 100 → s2 110 → s3 111 → s4 011 → s5 001.

- QA = 1 (s1, s2, s3) = **okno videa**, QA = 0 (s4, s5, s0) = **okno CPU** (/OE SRAM = /QA).
- s1: IC20B nastaví inverzi A6 (jen ACE); s2: C_ATR = QB·/QC·DOTCLK zachytí pomocné atributy, zruší inverzi;
  s3: SH/LD + LD_ATR = QA·QC·DOTCLK, načtení bodů a atributů.
- Bajt = 6 bodů = 915,5 ns (1,0923 MHz).
- **Řádek:** H čítač IC17 0–69 (nuluje IC16B při H64·H4·H2 = 70) → **70 bajtů = 420 bodů = 64,087 µs**
  (15,604 kHz). Viditelné H 0–47 (latch IC2C/D: konec při /ZAT48 = H16·H32, začátek po vynulování řádku).
  - Tabulka `Časování obrazu.xls` (rev 4.2): viditelné 0–47 (43,95 µs), pravý okraj 48–51, front 52–53,
    **sync 54–58** (4,58 µs), back 59–65, levý okraj 66–69. Ze schématu: IC7 74LS138 (A = H4, B = H1, C = H8,
    G1 = H2, /G2A = /ZAT48, /G2B = QC) Y1 (H = 54) nastaví IC1B, Y6 (H = 59) nuluje → shoduje se.
- **Snímek:** IC1A + V čítač IC18 0–311 (nuluje IC19A při V256·V32·V16·V8 = 312) → **312 mikrořádků,
  19,995 ms, 50,01 Hz, neprokládaně.** Viditelné 0–255, 256–311 zatemněno.
  - Snímkový synchro: IC12 74LS138 (A = V16, B = C = V8, G1 = V256, /G2A = V32, /G2B = V4), Y1 → mikrořádky
    **272–275** (4 řádky, 256 µs), přes D4 do VID. Řádkový synchro přes D2. Jen pro úplnost, emulace to nepotřebuje.
- Poměr stran: bod 152,6 ns. Při čtvercovém bodu PAL 14,75 MHz je bod 2,25 jednotky široký a 2 vysoký
  (neprokládaný řádek) → bod ≈ 1,125 : 1, obraz 288 × 256 ≈ 1,27 : 1. Tabulka v xls počítá šířku viditelné části
  262 mm.

## 4. Přístup CPU, čekání, výběr (list 4)

### Výběr SEL (IC28A 74LS21)

SEL = (A15 ⊕ J_AD15) · (A14 ⊕ J_AD14) · MAPOK · /RES

- S3 = 2 × 4 kolíky, liché kolíky přes R20 (4× 1 kΩ) na +5 V, sudé na GND: propojka nasazená = 0, sundaná = 1.
  - S3 1-2 = **MLEV** → IC30D (XOR s MAP1)
  - S3 3-4 = **MDIS** → IC29B
  - S3 5-6 = **AD15** → IC30C (XOR s A15)
  - S3 7-8 = **AD14** → IC30B (XOR s A14)
- MAPOK = IC29B = NAND(MAP1 ⊕ J_MLEV, J_MDIS).
- Potisk desky (fotka) a odvození souhlasí:

| Adresa | AD15 | AD14 |
|---|---|---|
| 0000h | – | – |
| 4000h | – | nasazená |
| 8000h | nasazená | – |
| C000h | nasazená | nasazená |

| MAP1 MODE | MDIS | MLEV | karta vybraná při |
|---|---|---|---|
| HIGH | – | – | MAP1 = H |
| LOW | – | nasazená | MAP1 = L |
| IGNORE | nasazená | libovolně | vždy |

- **Výchozí (fotka autorovy desky, Liborův komentář 19. 4. 2022): C000h, MAP1 IGNORE** (nasazené AD14, AD15, MDIS).
- /RES je ve výběru: během resetu karta nic nezapíše a neuvolní přerušení.
- Pozor: u 8080 je při IN/OUT číslo portu i na A15–A8, takže IN/OUT na porty C0–FFh (při C000h) dá SEL. Nic se ale
  nestane (zápis jen při /MW, RDY se hned nuluje přes /IOR, /IOW).

### Zápis a RDY

- IC13B: SEL·MW (MW = /MW aktivní). IC29A = NAND(SEL·MW, IC21A.Q); IC21B (D = IC29A, CLK = /QB, PRE = IC21A.Q)
  → Q = **/WE**; IC21A (D = /WE, CLK = /QC, PRE = /(SEL·MW), CLR = vlastní Q) zajistí **jeden** pulz WE na cyklus.
  → **/WE = L přesně ve stavu s5** (od sestupné hrany QB do sestupné hrany QC), tj. uprostřed okna CPU, jeden bod
  (152,6 ns). /BUF (IC13A) otvírá 74LS125 sběrnice → MD.
- **RDY:** IC20A: PRE = NAND(SEL, STSTB) → na začátku **každého** strojového cyklu se SEL je /Q = 0 → RDY = L
  (přes BAT42 D7, otevřený kolektor). CLK = /WE (náběžná hrana = konec zápisu, D = GND) → RDY uvolní.
  CLR = IC28B AND(/IOW, /IOR, /MR, /RES) → **čtení (včetně M1) a I/O cyklus RDY ihned uvolní**, čeká se jen při
  zápisu.
- Doba čekání při zápisu: od /MW do konce nejbližšího stavu s5 = 1–7 bodů (0,15–1,07 µs). Při CPU 2 MHz
  (T = 500 ns) zhruba 0–2 čekací takty podle fáze. Fáze bodu = (čas · 6,5536 MHz / takt CPU) mod 6; pevné jedno
  čekání by bylo jen zjednodušení, fázově přesný výpočet je levný (emulace ho dělá, kap. 8).

### Přerušení

- IC14A 74LS74: D = GND, CLK = V256 (náběh = **začátek mikrořádku 256**, 16,406 ms po začátku snímku) → Q = L →
  přes BAT42 D6 na /INT0 nebo /INT1 (S2: kolík 1 = /INT0, 2 = společný, 3 = /INT1; potisk „INT1 INT0“).
  Úrovňový požadavek, drží až do uvolnění.
- Uvolnění: PRE = IC13D = SEL · STSTB · M1 (IC29C, IC13C, IC13D) → **jakékoli čtení operačního kódu (M1) z okna
  karty** nastaví Q = H. Čtení dat ani zápis přerušení neuvolní. CLR je trvale neaktivní (IC14B je „mrtvý“
  klopný obvod s PRE = CLR = GND, jeho /Q = H). /RES IC14A nenuluje → stav po zapnutí je neurčitý.
- Liborův příklad (komentář 19. 4. 2022): JPR-1S s RAM 4000h–FFFFh, RST 7 (0038h) přes 8228, BGP-1S na C000h,
  MAP1 vypnuté, stejný kanál INT na obou deskách. Do RAM pod oknem `C030: EI / C031: RET`, obsluha na 4000h
  (počítadlo s předděličkou 32h = 50) končí `JMP C030` → čtení operačního kódu v okně uvolní požadavek. Spuštění
  `JUMP C030`. BIOS: 0038h `JMP INTadr` (2000h), výchozí obsah `JMP 4000h`.
- Známý software přerušení nepoužívá (MIKROBIOS má `DI`, v `test3.asm` „;ei – nepovolovat, to je špatně!“,
  Manic Miner a auto nemají EI).
- Propojka S2 na fotce: kolíky u potisku „INT0“ (pravděpodobně /INT0, ověřit).

## 5. Konektory

**K1 (FRB 62, TY 517 6211):** RDY 3, STSTB 4, /M1 6, /RES 8, /MR 9, /IOR 10, /MW 11, /IOW 12, MAP1 21,
A0 40, A1 39, A2 42, A3 41, A4 44, A5 43, A6 46, A7 45, A8 30, A9 23, A10 29, A11 25, A12 28, A13 27, A14 26,
A15 24, D0 37, D1 38, D2 35, D3 32, D4 31, D5 34, D6 33, D7 36, /INT0 62, /INT1 59, +5 V 15–18, GND 19, 20, 53, 54,
+12 V 51, 52.

**K2 (FRB 30, TY 513 3011):** RED 4, GREEN 8, BLUE 12, +12 V 26, /ZAT 28 (100 Ω na +5 V), VID 30;
GND 1, 3, 5, 7, 9, 11, 13, 25, 27, 29.

Propojky: S1 3 kolíky (RGBM 1-2 / ACE 2-3; na fotce **ACE**), S2 3 kolíky (INT0 / INT1), S3 2×4 (výše).
Žádné PROM ani GAL, celá logika je TTL, dekodér je XOR/AND (nic k dumpování).

## 6. Text

Karta nemá generátor znaků. Text kreslí software: `font.asm` = font z PMD-85, 64 znaků (32–95, bez malých písmen),
8 bajtů na znak, šířka 6 bodů (bit 0 vlevo; komentáře `; ..#.#.` jsou kreslené zrcadlově). `ShowChar`
v `grafika.asm` kreslí 8 mikrořádků a ORuje atribut střídavě C (sudé) a B (liché) z `Color`. Řádek textu má
10 mikrořádků (`LineHeight = 10*64`), 48 sloupců × 25 řádků. Kurzor XORem (`Cursor`). Scroll a CLS čtou a píšou
videoram, **čtení jde z RAM pod kartou** (proto MIKROBIOS vyžaduje stínovou RAM).

## 7. Software

| Co | Kde | Poznámka |
|---|---|---|
| MIKROBIOS 1.02 (04/2022) + Falcon Monitor 6.1L | `..\SAPI-karty\BGP-1S\jpr1s_sw\jpr1s.bin` (5658 B od 0000h, pro 28C64), `.hex`, zdroje TASM | BGP-1S jako jediný displej, ANK-1 (ANK-1M5 = ANK-1M2) na 2400h, sériový download na 2800h b7. Pro JPR-1S (mapa jako JPR-1: ROM 0000–1FFF, RAM 2000–23FF, P0 2400h, P1 2800h, P2 2C00h; `definice.asm` má ještě 3214 na 3000h, JPR-1S ho nemá). |
| totéž | `..\SAPI-karty\JPR-1S\jpr1s_sw\jpr1s.hex` = `jpr1s.txt` | shodné |
| MikroBIOS 2.1 (2024/11) JPR-1S/AND-1V/ANK-1 | `..\SAPI-karty\JPR-1S\jpr1s_mikrobios\jpr1s.hex` | displej AND-1V; příkaz `TEST BGP-1S [adr]` (= `TestBGP`: CLS barvou, motýl čarami, XORem smaže, barvy 7→1 dokola, ESC+SHIFT konec) |
| **Manic Miner** (česká verze „Hlavní sluj“, „Vzduch“, „Věcí“) | `..\SAPI-karty\JPR-1S\jpr1s_sw\manic_sapi.txt` | viz níže |
| „auto“ (závodní autíčka, „HAVAROVAL SI 003 KRAT“, „CIEL“) | `..\SAPI-karty\JPR-1S\jpr1s_sw\auto.txt` | viz níže |
| `m.asm` / `m.txt` | `..\SAPI-karty\JPR-1S\jpr1s_sw\` | „MARTINUV PRVNI POKUS (06/06/2022)“: na 4400h jen `CALL PrintText (0BC4h)` + RET |

### MIKROBIOS 1.02 – start

`DetectVideo` (`grafika.asm`):

1. když je RAM jen v jednom kvadrantu 4000/8000/C000h (test zápisu a čtení `BGPWriteTest`), je to videoram
   (stínová RAM);
2. klávesa 1/2/3 držená při startu → 4000/8000/C000h (čte řádek klávesnice přes 2400h);
3. jinak dříve platná `VideoBase` z RAM;
4. jinak C000h.

Pak `SystemInit`, výpis „SAPI MIKROBIOS 1.02 (04/2022) / JPR-1S/BGP-1S/ANK-1“, mapa RAM po 1 KB (`#` RAM, `.` nic),
„VIDEO BASE: C000h“, „FALCON MONITOR V6.1L / TYPE ? TO HELP“. Příkazy: A, D, M, SCAN, >, ., IN, OUT, FILL, CLS,
COLOR n (1–7), JUMP adr, BOOT (Intel HEX 19200 Bd, 8N2, na 2800h bit 7 přes redukci na K2, viz `JPR-1S.md`), TEST
(motýl `Test3`), ?.

### Manic Miner (`manic_sapi.txt`)

- **Formát:** Intel HEX, záznamy po 24 bajtech, jeden souvislý blok **4400h–989Eh** (21 663 B), EOF bez startovací
  adresy.
- **Načtení:** v MIKROBIOS/Falcon `BOOT` + poslat soubor sériovou linkou (19200 Bd), pak **`JUMP 4400`**.
  V emulátoru stačí nahrát HEX do RAM (menu, `--load-hex`, MCP) a skočit na 4400h.
- **Vstup:** 4400h `JMP 9565h` → `JMP 95E5h` → `LXI SP,95E5h`, `CALL 89AEh`, `CALL 77C8h`, `STA 2400h`…
- **Videoram natvrdo C000h–FFFFh** (např. vzduch E7AAh, texty EAC2h, ED1Ch, F0A5h, F442h, F742h, FC02h, kreslení
  přes `SPHL` + `PUSH B` od C030h). Kreslí ve stylu Colorace (střídá atributy B/C po mikrořádcích, XOR sprity
  z tabulek). **Vyžaduje S1 = ACE** (na fotce azurová a bílá).
- **Žádné IN/OUT, žádné EI**, nepoužívá ROM BIOSu (žádné CALL pod 4400h). Klávesnice ANK-1 přímo přes P0 2400h
  (řádky ANI EFh = D4: P, SHIFT; ANI F7h = D3: O, S; ANI FBh = D2: M…), zvuk přes bity masky 979Ah zapisované
  do 2400h (bzučák P0-OUT D6). **Joystick (2800h) nečte.**
- Potřebuje RAM 4400h–~98FFh (+ stínovou RAM pod C000h, jestli videoram čte; staticky nepotvrzeno). V kódu
  4800–48FF jsou zbytky loaderu z PMD-85 s `IN 4Ch`, mimo kód, který se při sledování toku provádí.
- **Referenční snímek `manic_main.jpg`** (fotka obrazovky skutečné hry na CRT): první jeskyně „Hlavní sluj“,
  fialové zdi a plošiny, azurový pruh „Vzduch“, bílý název, bílí panáčci (7 životů), „Věcí: 5“, skóre „000“.
  Vhodné k porovnání barev Colorace a polohy.

### auto (`auto.txt`)

- Intel HEX **4400h–4BABh** (1 964 B), start **4400h** (`LXI H,46CCh`…), `JUMP 4400`.
- Videoram C000h natvrdo, **čte videoram kvůli kolizím** (4598h–45B2h: `MVI A,3Fh; CMP M` na adrese ve videoram)
  → nutná stínová RAM pod BGP-1S. Používá RST 3 a RST 6 z BIOSu (RST 6 = HL += 64).
- Vstup: klávesnice přes 2400h a **P1-IN 2800h bity 2 a 3** (u ANK-1M2 joystick vpravo/vlevo).

## 8. Emulace (2026-10-06)

- Typ **`BGP-1S`** (`src/cards/bgp1s.*`), při načítání sestavy se přijme i alias `BGP-1`. Test `test_bgp1s`.
- Parametry:
  - `base`: `C000h` (výchozí), `8000h`, `4000h`, `0000h` (S3 AD15/AD14);
  - `map1`: `ignore` (výchozí, = MDIS), `high`, `low`;
  - `colors`: `ace` (výchozí), `rgbm` (S1);
  - `int`: `int0` (výchozí), `int1`, `none` (S2);
  - `output`: `rgb` (barevná TV/SCART, výchozí) nebo `mono` (ČB TV, kompozit). Přepíná se i za chodu v menu
    Zobrazení → Barva;
  - `wait_states`: výchozí zapnuto.
- Zápis do okna jde do 16 KB videoram. Čtení karta neobsluhuje (`mem_read` vrací „nedrží sběrnici“), data čte RAM
  pod ní. Mapa portů a paměti ji hlásí jen pro zápis (`DEC_WRITE`), takže nekoliduje s RAM.
- Čekání při zápisu: do konce stavu s5 nejbližší bajtové periody. Fáze bodu se počítá z času sběrnice, 1–7 bodů
  se převede na celé takty CPU.
- Přerušení: požadavek při každém začátku mikrořádku 256 (snímek 312 × 420 bodů / 6,5536 MHz = 19,995 ms). Drží
  se, dokud CPU nečte operační kód (M1) z okna karty (adresa = PC instrukce). Čtení dat ho neruší. Test:
  50 požadavků za 1 s.
- Obraz 288 × 256, bod 6,5536 MHz (poměr stran podle TV). Barvy podle vzorců R = P(a+d), B = P(b+c),
  G = P(/a/b + /c/d), v ACE je pomocný bajt z řádku y^1. Neviditelné bajty 48–63 se nevykreslují. Okraj a fáze
  obrazu proti CPU se nemodelují (kreslí se celý snímek z videoram).
- Konfigurátor: panel BGP s FRB K2 (dřív se mylně kreslil D-sub VGA).
- Ověřeno v emulátoru:
  - MIKROBIOS 1.02 (text kreslený softwarem);
  - Manic Miner odpovídá fotce `..\SAPI-karty\JPR-1S\jpr1s_sw\manic_main.jpg` (fialové zdi, azurový vzduch,
    bílé texty);
  - auto reaguje na joystick (ANK-1M2, P1-IN D3/D2).
- Hry v repu (`media/jpr-1s/`):
  - `manic_miner.hex` (Vít Libovický & Daniel Jenne, ViLiSoft & DaJeSoft 1985, port z PMD-85);
  - `auto.hex`;
  - `martinuv_prvni_pokus.hex` (jen s BIOSem 1.02).
- Sestava: `machines/jpr1s_bgp1s.sapi` (JPR-1S, MIKROBIOS 1.02, BGP-1S, ANK-1M2, redukce na K2).

## 9. Nejasnosti

1. Výchozí S1 (fotka: ACE) a S2 (fotka: kolíky u „INT0“): potvrdit.
2. Stav klopného obvodu přerušení IC14A po zapnutí (není nulovaný /RES). Emulace začíná bez požadavku.
3. Čte Manic Miner videoram (je stínová RAM C000–FFFF povinná)? Pro „auto“ ano.
4. Přesné vzorkování RDY u 8080 vs. okno s5 (počet čekacích taktů při 2 MHz): stačí model „čekat do konce s5“?
5. Je `.sch` v repu poslední uložení? PNG jsou z 05.03.2022 (C15 100n, nový list 6), `.sch` z 09.02.2022. Hodilo
   by se nahrát novější `.sch`.
6. List 6 schématu: popisek „143 ns (1/7 MHz)“ je ze starší revize (krystal je 6,5536 MHz), zůstal i v PNG
   z 05.03.2022.
7. Kdo napsal hru „auto“?
