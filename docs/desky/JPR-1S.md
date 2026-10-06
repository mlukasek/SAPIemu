# JPR-1S – moderní reinkarnace procesorové karty JPR-1 (Libor Lasota)

**Stav (2026-10-06): emulovaná** (typ `JPR-1S`, třída `Jpr1`, soubory `src/cards/jpr1.*`, test `test_bgp1s`).
Rozbor podkladů rev 2.2, nic neověřeno na HW. Podklady: `..\SAPI-karty\JPR-1S\`.

Grafická karta k ní se oficiálně jmenuje **BGP-1S** (`BGP-1S.md`). V BIOSu 1.0 a v textech se píše „BGP-1“, jde
o tutéž kartu. Klávesnice ANK-1M5 v podkladech je ANK-1M2 (`klavesnice.md`), píše se tu „ANK-1M5 (= ANK-1M2)“.

## Zdroje

Cesty jsou relativně k `..\SAPI-karty\JPR-1S\`, pokud není uvedeno jinak.

- [SCH] `Eagle\JPR-1S.sch` (2024-04-08, EAGLE 9.6.2 XML, potisk desky „rev. 2.2“) a `JPR-1S_schema.pdf`
  (vytištěno 2026-10-06 ze stejného `.sch`, 3 listy: „SBERNICE A OBVODY PROCESORU“, „DEKODERY ADRES A PAMETI“,
  „V/V REGISTRY, PRERUSENI, KONEKTORY K2/K3“, DPLA0034JPS).
- [DOC] `popis\Popis JPR-1S.docx` (2021-09-18).
- [WEB] https://www.pmd85.cz/?page_id=1544. ZIP `jpr1s.zip` = stav 2021 (fotky, popis, BIOS 1.0, Eagle `.sch`
  2021-09-09 ve starém binárním formátu, `JPR-1S.png` s osazovákem). ZIP `jpr1s_sw.zip` = `jpr1s_sw\` bez
  pozdějších souborů, BIOS 1.02.
- [B10] `BIOS\bios_jpr1s.hex` + `.txt` (MikroBIOS 1.0, 08/2021, shodný se ZIPem z webu).
- [SW] `jpr1s_sw\` (MikroBIOS 1.02, 04/2022, zdroje TASM). Autor sem 2026-10-06 doplnil `manic_sapi.txt`,
  `auto.txt`, `m.asm/.lst/.txt/.bat`, `jpr1s.txt`, `manic_main.jpg` a `schov\`.
- [MB] `jpr1s_mikrobios\` (MikroBIOS 2.1, 2024/11, zdroje TASM + `jpr1s.lst`).
- Fotky `Foto\`: `mJPR-1S.jpg` (2021, starší revize s DIP přepínačem SW1), `JPR-1S_front.jpg`/`_back.jpg`
  (rev. 2.2, v patici ROM je **PicoROM**, emulátor ROM s RP2040), `diagnostika.jpg`, `jpr-1.jpg` (originál).
- Popis klávesnice ANK-1M5 (= ANK-1M2) s redukcemi: `..\SAPI-karty\ANK-1M5\popis\Popis.docx`, fotka
  `..\SAPI-karty\ANK-1M5\fotky\redukce34REV.jpg`. Schéma redukce: `..\SAPI-karty\Redukce34\Redukce34KB01-rev.2.0.png`.
- Popis BGP-1S: `..\SAPI-karty\BGP-1S\popis\Popis BGP-1S.docx`.
- Zdroje BIOSů jsou v cp1250.
- Netlisty (rev 2.2, binární `.sch` 2021 z webového ZIPu a deska diagnostiky `Eagle\JPR1X_Diagnostika.*`) se dají
  vytáhnout offline přes `"C:\EAGLE 9.5.1\eaglecon.exe" -C "EXPORT NETLIST n.txt; EXPORT PARTLIST p.txt; WRITE
  x.sch; QUIT;" kopie.sch` (na kopii; `eagle.exe` chce přihlášení). Netlist z EAGLE souhlasí s vlastním rozborem
  XML. Všechny piny a sítě níže jsou z netlistu.

## 0. Revize zdrojů (emulace vychází z nejnovější rev. 2.2)

Existují dvě revize desky: **rev. 2.1** (2021, schéma „2.01“, DPLA0025JPS) a **rev. 2.2** (vyrobena a otestována
k 20. 1. 2022, schéma „2.2“, DPLA0034JPS). Libor to popsal v komentáři na webu 20. 1. 2022, článek a ZIP na webu
ale zůstaly na 2.1.

| Zdroj | Revize | Jak určeno |
|---|---|---|
| `Eagle\JPR-1S.sch` (2024-04-08, XML) | **2.2** | rohová razítka „2.2“, DPLA0034JPS; součástky RAM_EN, R9, R10, C15 |
| `Eagle\JPR-1S.brd` (2024-04-08) | **2.2** | potisk „rev. 2.2“, DPLA0034JPS, popisky RST7/RSTx, 4000-7FFF… |
| `JPR-1S_schema.pdf` | **2.2** | vytištěn 2026-10-06 14:13 přímo z `Eagle\JPR-1S.sch` (cesta v záhlaví) |
| `Foto\JPR-1S_front.jpg`, `_back.jpg` | **2.2** | potisk „JPR-1S rev. 2.2“, propojky RAM_EN, PicoROM v patici ROM |
| `Foto\mJPR-1S.jpg` (2021-08-11), web foto | **2.1** | DIP SW1 „RAM ENA“, „RST× / 38h“ |
| `popis\Popis JPR-1S.docx` (2021-09-18) | **2.1** | 74HCT04 doporučen, pull-up „jako u JPR-1“ jen doporučený, chyba LED D1 |
| web článek + `jpr1s.zip` (sch 2021-09-09 binární, `JPR-1S.png`) | **2.1** | razítka „2.01“, DPLA0025JPS, SW1, IC4 = 74HCT04 |
| web komentář 20. 1. 2022 | popis změn 2.1 → 2.2 | viz níže |
| `Eagle\JPR1X_Diagnostika.*` | diagnostika 1.00 | DPLA0016DG2/2021 (pro obě revize) |
| BIOS 1.0 / 1.02 / 2.1 | nezávislé na revizi | žádný rozdíl v hardwaru, který by software viděl |

Změny 2.1 → 2.2 (web komentář 20. 1. 2022, potvrzeno porovnáním netlistů z eaglecon):

1. **Diagnostická LED má správnou polaritu.** V 2.1 má D1 prohozené vývody (známá chyba z [DOC]), LED svítila jen
   při obráceném osazení. **Pro emulaci: rev. 2.2 → P1-OUT7 = 1 svítí.**
2. Širší napájecí spoje (nepodstatné).
3. **DIP přepínač SW1 nahrazen propojkami RAM_EN.** Logika je stejná (sepnuto/osazeno = banka zapnutá).
4. Potisk C15 (4u7 tantal paralelně s C8). V 2.1 to byla „druhá chyba“, neoznačené místo.
5. **R9** pull-up na INT pro 8080A (pak není nutný 74HCT04; ve schématu 2.2 je IC4 = 74LS04).
6. **R10** pull-up na /INT: klidová úroveň, když není osazená žádná propojka INT_SRC (v 2.1 vstup hradla „visel“,
   na 74HCT04 nedefinovaně). Emulace: bez propojky žádné přerušení.

- Navíc podle netlistu: R1 (pull-up sběrnicového /INTA) 1k → 10k (nepodstatné).
- Dekódování adres, porty, konektory K1–K3, přerušení a takt jsou v obou revizích **shodné**.

## 1. Co je JPR-1S

- „Moderní reinkarnace“ původní JPR-1 (ARB 1/1983) [WEB]. Účel: hlavně testování ostatních desek s procesorem
  8080 na mikrosběrnici. Jinak malá samostatná sestava JPR-1S + AND-1x/DGD-1/BGP-1S + ANK-1M5 (= ANK-1M2)
  + redukce pro sériový download [WEB].
- **CPU MHB8080A** (IC1), **8224** (IC3) s krystalem **18 MHz** (Q1, `value=18M`) → **2,000 MHz**, **8228** (IC2).
  Není to 8085 (TASM se spouští s `-85`, ale kód je čistá instrukční sada 8080).
- Rozdíly proti JPR-1 [DOC, WEB, SCH]:
  - ROM **8 KB v jednom pouzdře** (28C64, IC6) od 0000h, lineárně („bez přehozených bloků“);
  - RAM 1 KB na 2000h vždy + **tři nezávisle připínatelné banky po 16 KB** na 4000h, 8000h, C000h (jedna SRAM
    628128, IC5);
  - **bez DMA** (HOLD na GND), **bez 3214** (žádné víceúrovňové přerušení, 3000h se na desce nedekóduje);
  - zdroj přerušení volitelný propojkou (INT0, INT1 ze sběrnice, P2-IN0, P2-IN1 z K3), odezva RST 7 nebo /INTA
    na sběrnici;
  - 6× MH3212 jako porty P0–P2, **paměťově mapované na 24xxh/28xxh/2Cxxh stejně jako JPR-1**;
  - diagnostická LED na P1-OUT7; K2 má +5 V na kolíku 22 (zúžená cesta = „tavná pojistka“);
  - konektor K1 bez nevyužitých kolíků.
- Kompatibilita: BIOS 1.0 „měl by být použitelný i pro JPR-1“, klávesnice „i pro ANK-1“ [B10 txt]. `definice.asm`
  má stále `ROM_0..ROM_3 = 0000/0800/1000/1800` (4× 2716 jako JPR-1) a `i3214 .equ 3000h` (nepoužito).
- Úrovně: [DOC] doporučuje na IC4 74HCT04 (kvůli úrovním pro MHB8080A), schéma 2021 má 74HCT04, rev 2.2 74LS04
  + pull-upy R9/R10, [WEB] píše 74HCT14. Pro emulaci nepodstatné.
- Známá chyba rev. 2.1: LED D1 má prohozené vývody [DOC]. V rev. 2.2 opraveno (web komentář 20. 1. 2022).

## 2. Mapa paměti (z netlistu, potvrzeno BIOSem)

Dekódování: **IC7 74ALS138** + **IC8 74ALS153**, bez PROM/GAL. Signály MR/MW hradlují jen porty.

IC7 74ALS138: A = A10 (p1), B = A11 (p2), C = A12 (p3), G1 = A13 (p6), /G2A = A14 (p4), /G2B = A15 (p5).
Aktivní je tedy jen pro 2000–3FFF, výstupy po 1 KB:

| Výstup | Rozsah | Síť | Kam |
|---|---|---|---|
| Y0 (p15) | 2000–23FF | N$17 | IC8 2C0 → /RAMCS (RAM 1 KB) |
| Y1 (p14) | 2400–27FF | /S0 | /DS1 IC13 (P0 OUT) a IC14 (P0 IN) |
| Y2 (p13) | 2800–2BFF | /S1 | /DS1 IC11 (P1 OUT) a IC12 (P1 IN) |
| Y3 (p12) | 2C00–2FFF | /S2 | /DS1 IC9 (P2 OUT) a IC10 (P2 IN) |
| Y4–Y7 | 3000–3FFF | nezapojeno | → jen sběrnice (AND-1/AND-1V na 3800 apod.) |

IC8 74ALS153 (oba /G na GND), výběr A = A14 (p14), B = A15 (p2):

| A15 A14 | 1Y = /ROMCS (p7) | 2Y = /RAMCS (p9) |
|---|---|---|
| 0 0 (0000–3FFF) | 1C0 = **A13** → ROM při A13 = 0, tj. **0000–1FFF** | 2C0 = Y0 → **2000–23FF** |
| 0 1 (4000–7FFF) | 1C1 = 1 (pull-up RN1) | 2C1 = **BANK1** |
| 1 0 (8000–BFFF) | 1C2 = 1 | 2C2 = **BANK2** |
| 1 1 (C000–FFFF) | 1C3 = 1 | 2C3 = **BANK3** |

- **ROM IC6 28C64:** A0–A12, /CE = /ROMCS, /OE = /MR, **/WE = L1 = pull-up RN1** (zápis zakázán). 8 KB
  0000–1FFF bez zrcadlení. Na fotce rev. 2.2 je v patici PicoROM.
- **RAM IC5 628128 (128 KB):** A0–A15 = CPU, **A16 = CS2 = L2 = pull-up**. Používá se jen horní polovina (64 KB),
  adresa v čipu = adresa CPU. /CS1 = /RAMCS, /OE = /MR, /WE = /MW. Bez zrcadlení.
- **RAM_EN** (JP3Q; v rev. 2.2 tři propojky, ve starší revizi 2021 DIP přepínač SW1 „RAM ENA“): kolíky 1/3/5 = GND,
  2/4/6 = BANK1/2/3 s pull-upy RN1. **Propojka osazená = banka zapnutá**, rozpojená = prostor patří sběrnici.
  Potisk: „1: 4000-7FFF, 2: 8000-BFFF, 3: C000-FFFF“. Fotka rev. 2.2: **všechny tři osazené** (RAM 1 + 48 KB).
- Celkem RAM 1 KB + 0–48 KB [DOC, WEB: „RAM 1+48kB“].
- **Zápis do místa na desce jde vždy i na sběrnici.** Paměť a 3212 jsou na systémové straně 8228, /BUSEN 8228 = GND
  (budiče vždy povolené), nic neblokuje ostatní karty (žádný /INH). Důležité pro **BGP-1S**: ta má videoram jen
  pro zápis a „pro zpětné čtení je nutno osadit RAM na stejné adrese“ [popis BGP-1S]. Na JPR-1S to dělá banka 3
  na C000 (proto všechny banky osazené). Čtení z adres na desce má přednost (na skutečné desce by čtení
  kolidovalo s kartou na stejné adrese).
- Zápis do ROM (0000–1FFF) jde jen na sběrnici, ROM se nemění.

Potisk desky: „0000h ROM 8kB, 2000h RAM 1kB, 24XXh PORT0, 28XXh PORT1, 2CXXh PORT2“.

## 3. Porty P0–P2 (MH3212, paměťově mapované, zrcadlené po celém 1 KB)

Výběr: /DS1 (p1) = /S0../S2 z IC7, DS2 (p13) = **MW** (výstupní) nebo **MR** (vstupní) z invertorů IC4
(/MR p1→p2 MR, /MW p3→p4 MW). Instrukce IN/OUT porty na desce **nevidí** (na desce není žádné I/O, vše jde na
sběrnici). /CLR (p14) všech šesti = **/RES** → po resetu jsou výstupní latche 00h.

| Port | Adresa | Obvod | MD | STB | Konektor | /INT |
|---|---|---|---|---|---|---|
| P0 OUT | 2400 (W) | IC13 | 1 (LOG1, RN4) | 1 | K2 | K2-12 (P0-/INT) |
| P0 IN | 2400 (R) | IC14 | 0 | 1 (LOG1) → průchozí | K2 | nezapojeno |
| P1 OUT | 2800 (W) | IC11 | 1 (N$11, RN3) | K3-19 (P1-STB, pull-up RN2) | K3 | K3-22 (P1-/INT) |
| P1 IN | 2800 (R) | IC12 | 0 | **K2-10 (P1-STB, pull-up RN3)** | K2 | K2-1 (P1-/INT) |
| P2 OUT | 2C00 (W) | IC9 | 1 (N$11) | K3-9 (P2-STB, pull-up RN2) | K3 | K3-1 (P2-/INT) |
| P2 IN | 2C00 (R) | IC10 | 0 | 1 (N$11) → průchozí | K3 | nezapojeno |

- Bity jsou rovně: datová sběrnice Dn ↔ bit portu n. Ověřeno párováním DIn/DOn v knihovních variantách
  3212_1/_2/_4: páry vývodů 22↔21, 20↔19, 3↔4, 5↔6, 9↔10, 18↔17, 16↔15, 7↔8.
- **P1-IN se zachytává signálem P1-STB** (K2-10): STB = 1 → latch průchozí, sestupná hrana → data drží a /INT
  (K2-1) jde do 0, čtení portu ho shodí. Bez připojeného STB (pull-up) je port průchozí. P0-IN a P2-IN jsou vždy
  průchozí.
- **Pull-upy 4k7 na všech vstupech:** RN4 = P0-IN0..7 a LOG1, RN3 = P1-IN0..7, P1-STB (K2-10) a N$11, RN2 =
  P2-IN0..7, P2-STB (K3-9), P1-STB (K3-19). Nezapojený vstup tedy čte 1. To odpovídá na nejasnost v
  `klavesnice.md` „má JPR-1 na P1-IN pull-upy“: JPR-1S ano.
- **LED D1:** VCC – R3 220R – D1 – T1 BC547 (C), báze přes R2 10k z **P1-OUT7** → **P1-OUT7 = 1 svítí** (rev. 2.2
  s opravenou polaritou; v rev. 2.1 jen při obráceně osazené LED). [WEB]: „diagnostická LED na výstupním pinu 80h na
  adrese 28xxh“. Na fotce rev. 2.2 je D1 červená LED na hraně desky mezi K2 a K3.
- /INT výstupy 3212 (K2-1, K2-12, K3-1, K3-22) vedou jen na konektory, ne do CPU.
- 3000–3FFF: na desce nic (žádný 3214). Zápis na 3000h (původní 3214) jde jen na sběrnici.

### K2 (TY 513 30 11, klávesnice)

| Kolík | Signál | Kolík | Signál | Kolík | Signál |
|---|---|---|---|---|---|
| 1 | P1-/INT (IC12) | 11 | GND | 21 | GND |
| 2 | P1-IN4 | 12 | P0-/INT (IC13) | 22 | **+5 V** |
| 3 | P1-IN2 | 13 | P0-OUT0 | 23 | P0-IN4 |
| 4 | P1-IN1 | 14 | P0-OUT1 | 24 | P0-IN6 |
| 5 | P1-IN3 | 15 | P0-OUT5 | 25 | P0-IN7 |
| 6 | P1-IN0 | 16 | P0-OUT6 | 26 | P0-IN5 |
| 7 | **P1-IN7** | 17 | P0-OUT4 | 27 | P0-IN2 |
| 8 | P1-IN5 | 18 | P0-OUT7 | 28 | P0-IN1 |
| 9 | P1-IN6 | 19 | P0-OUT3 | 29 | P0-IN3 |
| 10 | P1-STB | 20 | P0-OUT2 | 30 | P0-IN0 |

Shodné s X2 JPR-1ZI/JPR-1 i se zapojením ANK-1M2 v `klavesnice.md`: řádky P0-OUT D4–D0 = K2-17, 19, 20, 14, 13;
sloupce P0-IN D7–D0 = K2-25, 24, 26, 23, 29, 27, 28, 30; LED D5 = K2-15, D7 = K2-18; bzučák D6 = K2-16; INT = K2-7
(P1-IN7), T = K2-9 (P1-IN6); joystick P1-IN D5–D0. Reset z klávesnice (ANK-1 RES) na K2 není.

### K3 (TY 513 30 11, data/tiskárna/diagnostika)

| Kolík | Signál | Kolík | Signál | Kolík | Signál |
|---|---|---|---|---|---|
| 1 | P2-/INT (IC9) | 11 | GND | 21 | GND |
| 2 | P2-OUT5 | 12 | P2-IN2 | 22 | P1-/INT (IC11) |
| 3 | P2-OUT0 | 13 | P2-IN4 | 23 | P1-OUT4 |
| 4 | P2-OUT6 | 14 | P2-IN3 | 24 | P1-OUT0 |
| 5 | P2-OUT1 | 15 | P2-IN6 | 25 | P1-OUT7 |
| 6 | P2-OUT2 | 16 | P2-IN7 | 26 | P1-OUT1 |
| 7 | P2-OUT7 | 17 | **P2-IN1** (i zdroj INT) | 27 | P1-OUT2 |
| 8 | P2-OUT3 | 18 | P2-IN5 | 28 | P1-OUT5 |
| 9 | P2-STB | 19 | P1-STB (P1 OUT) | 29 | P1-OUT3 |
| 10 | P2-OUT4 | 20 | **P2-IN0** (i zdroj INT) | 30 | P1-OUT6 |

### Deska diagnostiky JPR1X_Diagnostika

Zasouvá se do K3 karet JPR-1x [DOC] („DIAGNOSTICKY DISPLEJ PRO DESKY JPR-1X“, DPLA0016DG2/2021, 1.00). Dva
sedmisegmentové displeje HD-H103 (společná katoda, nízkopříkonové) jsou přímo na výstupech 3212 přes rezistory
R1–R16, katody na GND (X1-21). Podle netlistu:

- **DIS3 = P2-OUT** (vlevo „P2/OUT“): a = X1-3 (bit 0), b = X1-5 (1), c = X1-6 (2), d = X1-8 (3), e = X1-10 (4),
  f = X1-2 (5), g = X1-4 (6), dp = X1-7 (7).
- **DIS4 = P1-OUT** (vpravo „P1/OUT“): a = X1-24 (bit 0), b = X1-26 (1), c = X1-27 (2), d = X1-29 (3), e = X1-23 (4),
  f = X1-28 (5), g = X1-30 (6), dp = X1-25 (7).
- U obou tedy **segmenty a–g, dp = bity 0–7, 1 = svítí**. Desetinná tečka DIS4 = P1-OUT7 = zároveň LED D1.
- **Tlačítko** S2 (B3F1000, jinak S1 Marquardt 6425) spíná X1-20 (**P2-IN0**) na GND: stisk = 0.
  Pozor: P2-IN0 je zároveň možný zdroj přerušení (INT_SRC 5-6).

## 4. Sběrnice K1, takt, reset, čekání, přerušení

K1 (TY 517 62 11): 1 /RTL, 3 RDY, 4 STSTB, 5 /HLDA, 6 /M1, 8 /RES, 9 /MR, 10 /IOR, 11 /MW, 12 /IOW, 13 /AEN,
15–18 +5 V, 19–20 GND, 23–30 A9 A15 A11 A14 A13 A12 A10 A8, 31–38 D4 D3 D6 D5 D2 D7 D0 D1, 39–46 A1 A0 A3 A2 A5
A4 A7 A6, 51–52 +12 V, 53–54 GND, 55–56 −5 V, 59 /INT1, 60 PHI2TTL, 61 /INTA, 62 /INT0.

- **Takt:** 18 MHz / 9 = 2 MHz, PHI2TTL na K1-60.
- **Reset:** RC 22 µF / 10 kΩ + D2 na /RESIN 8224 (po zapnutí). K1-1 **/RTL** jde přes R6 100 Ω na tentýž uzel
  (reset ze sběrnice, např. tlačítko na panelu). RESET 8224 → CPU, přes IC4 (p9→p8) **/RES** na K1-8 a /CLR všech
  3212. Tlačítko reset na desce není.
- **Čekání:** RDY z K1-3 (pull-up R7 1k) → RDYIN 8224. **Žádný vlastní čekací stav** (JPR-1 měl propojku 12-14).
- **STSTB** na K1-4 = negovaný /STSTB 8228 (IC4 p5→p6), tedy aktivní v 1 jako u JPR-1.
- **/M1** na K1-6 = negace DB5 na straně CPU (IC4 p11→p10). Platí jen ve stavovém slově (vzorkovat STSTB).
- **DMA:** HOLD (p13) na GND, /HLDA K1-5 jen pull-up R5 1k, /AEN K1-13 na GND. Bez DMA.
- **Přerušení:**
  - **INT_SRC** (JP4Q, 4 páry): 1-2 = /INT0 (K1-62), 3-4 = /INT1 (K1-59), 5-6 = P2-IN0 (K3-20), 7-8 = P2-IN1
    (K3-17). Sudé kolíky jsou spojené do sítě /INT (pull-up R10 10k) → IC4 (p13→p12) → INT CPU (R9 10k).
    Lze osadit více propojek najednou (drátové AND aktivních nul = OR požadavků). Přerušení je úrovňové, na desce
    není záchytný klopný obvod (zdroj musí držet do obsluhy). Fotka rev. 2.2: **osazeno INT1**.
  - **INT_ADR** (JP3): kolík 1 = /INTA sběrnice (K1-61, pull-up R1 10k), kolík 2 = /INTA 8228 (p23), kolík 3 = přes
    R4 1k na +12 V. **2-3 = „RST7“** (8228 při INTA sám vloží RST 7 = FFh → 0038h), **1-2 = „RSTx“** (8228 generuje
    /INTA na sběrnici, vektor dodá externí řadič). Potisk 2021 „RSTx / 38h“, rev 2.2 „RSTx / RST7“. Fotka
    rev. 2.2: **RST7** (kolík 1 je u čtverečku nahoře, propojka dole).
  - BIOS dělá DI a EI nikde nevolá. Na 0038h je `JMP INTadr` (2000h, RAM), výchozí obsah `JMP 4000h` (BIOS 1.02
    a 2.1). V BIOSu 1.0 je na 0038h přímo rutina RxD (vektor přerušení tam nemá).
  - BGP-1S umí generovat přerušení 50 Hz na INT0 nebo INT1 (nuluje se čtením operačního kódu z prostoru videoram).
- Vstupy P2-IN0/1 jsou zároveň čitelné na 2C00h bit 0/1.

## 5. BIOS (MikroBIOS + Falcon Monitor)

### Verze a obrazy

| Soubor | Verze | Rozsah | Velikost | Pro sestavu |
|---|---|---|---|---|
| `BIOS\bios_jpr1s.hex` (2021-09-07, = web ZIP) | MikroBIOS **1.0 (08/2021)**, FALCON MONITOR v6.1L | 0000–1267 | 4712 B | „JPR-1/BGP-1/ANK-1“ |
| `jpr1s_sw\jpr1s.hex` = `jpr1s.txt` = `schov\jpr1s.hex` (2022-04-19) | MikroBIOS **1.02 (04/2022)**, FALCON MONITOR V6.1L | 0000–1619 | 5658 B | „JPR-1S/BGP-1S/ANK-1“ |
| `jpr1s_sw\jpr1s.bin` (= `schov\jpr1s.bin`) | 1.02, stejný obsah, mezery 0015–0017, 001F, 0025–002F, 0037 vyplněné 00 (v HEX chybí) | 0000–1619 | 5658 B | |
| `jpr1s_mikrobios\jpr1s.hex` (2024-11-30, k němu `jpr1s.lst`) | MikroBIOS **2.1 (2024/11)**, Falcon Monitor V6.2L | 0000–1D54 | 7509 B | „JPR-1S/AND-1V/ANK-1“ |

- MD5 obrazů (od 0000, mezery FF): 1.0 `6bd8dce1…`, 1.02 `491b97c0…`, 2.1 `3a70f7db…`.
- **`jpr1s.txt` není novější BIOS**: je bajt po bajtu shodný s `jpr1s.hex` (jen kopie s příponou .txt z
  2022-06-06, nejspíš pro poslání přes terminál). Nejnovější je 2.1 v `jpr1s_mikrobios`, ten ale jako displej
  používá **AND-1V na 3800h**, ne BGP-1S. Pro BGP-1S (a Manic Miner) jsou určené 1.0 a 1.02.
- Všechny verze začínají na 0000 `DI; LXI SP,2400h; JMP Start`. SP = 2400h (zásobník v RAM 2000–23FF), proměnné
  od 2000h.
- RST 1–6 jsou ve všech třech verzích **bajtově stejné** (programy je používají): RST 1 (0008) = LDIR (HL→DE,
  C bajtů), RST 2 (0010) HL += A, RST 3 (0018) HL = word[HL + 2·A], RST 4 (0020) porovnání HL – DE (příznaky),
  RST 6 (0030) HL += 64. RST 0 = reset, RST 5 nedefinovaný.
- 1.02 má na 0038–0077 skokovou tabulku po 8 bajtech (JMP + 5znakové jméno): 0038 INTAD (JMP 2000h), 0040 PRINT,
  0048 PRNHL, 0050 PRNTX, 0058 INKEY, 0060 GTKEY, 0068 EDLIN, 0070 COLOR. Komentáře ve zdroji uvádějí chybně
  003B, 003E… Verze 2.1 tabulku nemá (Start je hned na 003B).

### Co BIOS inicializuje a používá

- Žádné I/O instrukce kromě příkazů IN/OUT monitoru a `BOOT DSM`. Hardware: P0 (2400h) pro klávesnici, LED a
  bzučák; P1-IN7 (2800h) jako sériový vstup; displej (BGP-1S nebo AND-1V); volitelně DSM-1 na 10h–13h (2.1).
  P2 ani LED P1-OUT7 BIOS nepoužívá. Na 3000h (3214) nesahá.
- `SystemInit` (2.1, `kbd_driver.asm` ř. 142–157): INTadr = `JMP 4000h`, PrnPos = 3800h, RptKad 15, RptDly 150,
  LastKey 0, **MaskLED = 20h** (P0-OUT5 = zelená LED SHIFT trvale svítí), editační bufer.
- `CountRAM` (2.1): zapíše P0 = FFh („kliknutí + LED“), testuje po 1 KB (zápis negace, porovnání, obnova)
  **jen 2000–23FF a 3800–FFFF** (0000–37FF kromě 2000 přeskočí, aby nezapisoval do portů), tiskne `#`/`.` a součet
  v kB, pak P0 = 00h. Verze 1.02 testuje celý prostor včetně portů (zapisuje i do 2400–2FFF!) a tiskne „TOTAL
  xxxxH BYTE RAM“.
- `DetectVideo` v 1.0/1.02 (`grafika.asm`): BGP-1S hledá zkušebním zápisem v 4000/8000/C000 (je-li RAM jen
  v jednom kvadrantu, je to videoram). Jinak volba klávesou 1/2/3 (řádek D4, sloupec b3 / D3 b3 / D2 b3), jinak
  C000h. Vypíše „VIDEO BASE: xxxxh“. Podrobnosti v `BGP-1S.md`.
- Displej 1.0/1.02: BGP-1S (kompatibilní s PMD-85, 288×256, řádek 64 B, viditelných 48 B, 6 bodů v b0–b5, b6–b7
  barevné atributy), text 48 sloupců, font 6×8 z PMD-85 (`font.asm`, znaky 32–95), příkaz COLOR 1–7.
- Displej 2.1: AND-1V (nebo AND-1Z) na **3800h**, 64×20. `Print` píše kód přímo do VRAM, CR = konec řádku,
  FS (1Bh) = CLS, scroll kopíruje 19 řádků. `Map1On/Map1Off` jsou prázdné (dřív zápis FFh/00h na P1 pro signál
  MAP1). Úvodní logo ze semigrafiky AND-1 (80–BFh).

### Ovladač klávesnice (`kbd_driver.asm`, ANK-1 / ANK-1M5 = ANK-1M2)

- Matice ANK-1: P0-OUT D4–D0 řádky (0 = aktivní), P0-IN sloupce (0 = stisk), D7..D5 = LED ERR / bzučák / LED SHIFT
  (z `MaskLED`).
- `Inkey` (2.1 ř. 54–89): nejdřív pevná prodleva (256× DCR/JNZ ≈ 3840 taktů ≈ 1,9 ms, kvůli rovnoměrnému
  autorepeatu; v 1.02 není). Pak pro kódy řádků **0Fh (D4), 17h (D3), 1Bh (D2), 1Dh (D1), 1Eh (D0)** zapíše
  `kód | MaskLED` na 2400h (`STAX B`, B = 24h, C libovolné), přečte 2400h a projde 8 bitů (RRC, b0 první).
  V D vrací kód poslední stisknuté klávesy (0 = nic). Klávesa s kódem 80h (SHIFT, řádek D4 b0) přepne zbytek
  skenu na tabulku se SHIFT.
- Tabulka (b0..b7): D4: SHIFT A Q 1 **ESC (= klávesa LF)** CR P 0; D3: Z S W 2 SP L O 9; D2: X D E 3 M K I 8;
  D1: C F R 4 N J U 7; D0: V G T 5 B H Y 6. Odpovídá matici ANK-1 v `klavesnice.md`.
- SHIFT: 1 `!`, LF → 0Ah, P → 7Fh (DEL), 0 `)`, 2 `?`, SP → TAB, L `=`, O `#`, 9 `(`, D `&`, 3 `"`, M `.`, K `*`,
  I `@`, 8 `>`, C `[`, F `\`, R `%`, 4 `$`, N `,`, J `-`, U `:`, 7 `<`, V `]`, G `_`, **T → 08h (BS)**, 5 `^`,
  B `;`, H `+`, Y `/`, 6 `'`; ostatní (A, Q, Z, S, W, X, E, CR) dávají 0. Rozložení je tedy jako **MIKOS**
  (bez Ctrl kódů Q/W/A/S/X), v emulátoru preset `software: "mikos"` (Esc na PC = LF, Backspace = Shift+T).
- `GetKey`: autorepeat čítáním volání (2.1: RPT_DLY 150, RPT_KAD 15; 1.02: 250/25).
- `GetESC_SHIFT` (falcon ř. 448–457): P0 = (MaskLED & E0h) | 0Fh (jen řádek D4), Z = 1 při **LF (ESC)**, CY = 0
  při SHIFT. SHIFT pozastavuje výpisy, ESC je přerušuje (i download).
- `Chirp` (D6 přepínán 256 smyček) v 2.1 nikdo nevolá. INT a T (P1-IN7/6) BIOS nečte, **P1-IN7 je sériový vstup**.
- Joystick (P1-IN D5–D0) BIOS nepoužívá.

### Příkazy

2.1 (Falcon Monitor V6.2L, česky; `?` vypíše nápovědu, falcon ř. 905–939): `A adr` (ASCII výpis), `M adr` (hex
výpis), `D adr` (disassembler 8080), `SCAN adr|port` (opakovaný výpis), `WAIT adr|port maska bajt` (čekání na
hodnotu), `> adr bajt|"TEXT"…` (zápis), `. adr instrukce` (assembler), `IN port`, `OUT port bajt` (přes
samomodifikující kód v RAM `PortMk`), `FILL od do bajt`, `CLS`, `JUMP adr` (výpis registrů po návratu),
`BOOT [DSM]`, `TEST [karta] [adresa]` (DGD/DGD-1V výchozí C000, BGP(-1S) C000, AND1V/AND1Z výchozí 3800;
AND1/AND1A/AND1B jen prázdný stub), `TEST RAM [od] [do]`, `?`. Výpisy jdou po stránkách (18 řádků), SHIFT pauza,
ESC konec.

1.02 (anglická nápověda): A, D, M, SCAN, >, ., IN, OUT, FILL, CLS, COLOR n, JUMP, BOOT (jen programový sériový),
TEST (test BGP-1S = motýlek z úseček), ?. 1.0: A D M ? > . BOOT CLS COLOR JUMP TEST (`?` jen vypíše řádek se
seznamem).

### BOOT – Intel HEX po sériové lince (`IntelHexBoot.asm`)

- **`BOOT`** (bez parametru, všechny verze) = **programový příjem na P1-IN7** (2800h, „MSB portu 1“):
  19200 Bd, start + 8 dat (LSB první) + 2 stop, bez parity, **klid = 1, start bit = 0** (pravá logika UART).
- `RxD` (2.1 ř. 278–300, adresa 189F; 1.0 na 0038h): `LDAX B / RAL / JC RxD` čeká na 0 (smyčka 21 taktů),
  pak `LDA 0` + `MVI D,FFh` (20 taktů; v 1.0 `MVI D` + 2× `MVI A`, 21 taktů). Potom smyčka vzorkuje 9× (start +
  8 dat) po **104 taktech** (LDAX 7, RAL 4, MOV 5, RAR 4, MOV 5, XTHL 18, XTHL 18, 4× MVI 28, MOV A,A 5, JC 10)
  = 52,0 µs = 19 230 Bd (−0,2 % proti 19 200). Konec smyčky pozná podle start bitu (0) vysunutého z D. Pak ještě
  jeden vzorek **stop bitu** (o 104 taktů dál, uprostřed 1. stop bitu): `RC` vrátí bajt jen při stop bitu = 1, jinak
  hlášení „STOP BIT“ a návrat do monitoru (v 1.0 skokem; v 1.02/2.1 je `jmp` zakomentovaný, kód ale do
  ReceiveError1 propadne).
- První vzorek padne asi 27–48 taktů po hraně start bitu (střed bitu = 52), všechny vzorky jsou tedy 4–25 taktů před
  středem bitu. Mezi vzorkem stop bitu a další hranou start bitu (2 stop bity) je asi 160–180 taktů, zpracování
  bajtu trvá asi 120 taktů → **s 1 stop bitem by to nestačilo** (komentář „START+8+1,5STOP“).
- Rezerva vzorkování je jen asi ±25 taktů (bitová perioda 2 000 000 / 19 200 = 104,17 taktu), úroveň vstupu se
  proto musí počítat z přesného času.
- Formát: ignoruje CR/LF, `:` RECLEN OFFSET(2) RECTYP; typ 00 data (zapisuje přímo na adresu), typ 01 konec;
  kontrolní součet. Jiný typ záznamu = chyba RECTYP. Program **nespouští** (záznam 01 nemá adresu), po „Program
  úspěšně načten“ / „DONE“ se vrací do monitoru, spouští se ručně `JUMP 4400`. Chyby: STOP BIT, RECORD MARK,
  RECTYP, CHKSUM, BYTE. ESC během čekání v režimu DSM přeruší.
- **`BOOT DSM`** (jen 2.1): přes DSM-1 na 10h–13h: `OUT 10h,1` (RTS zap), čeká na `IN 11h` b7 (DR), `IN 12h`
  data, `OUT 10h,0` (RTS vyp) po každém bajtu. UART neinicializuje (parametry podle propojek DSM-1). Registry
  sedí s emulovanou DSM-1/DMR-1 (base+0 b0 RTS, base+1 b7 DR, base+2 data).

### Redukce na K2 (Redukce34KB01, Redukce34REV)

- Podle popisu ANK-1M5 (= ANK-1M2): klávesnice má IDC34, na straně karty JPR je potřeba redukce IDC34 → FRB30 (K2).
  Popis uvádí prostou **Redukce34KB01** a **Redukce34REV** = „přechodová redukce s **opticky odděleným
  jednobitovým vstupem**“. Obě přesměrují +5 V z kolíku 22 na kolíky 33, 34 kabelu.
- Tři desky podle čísel na fotkách a ve schématu:
  - `..\SAPI-karty\ANK-1M5\fotky\redukce34KB01.jpg`: **DPLA0014A5R**, jen FRB30 a IDC34, bez sériového vstupu
    (první Redukce34KB01);
  - `redukce34REV.jpg`: **DPLA0026COM**, IDC34 „ANK-1M“, FRB30 „JPR-1x“ (X2), **zásuvka DSUB9 X3** (RS-232 z PC),
    optočlen v patici DIP8, D1, R1, R2 („Redukce34REV“);
  - `..\SAPI-karty\Redukce34\Redukce34KB01-rev.2.0.png` a `.jpg`: **Redukce34KB01 rev 2.0, DPLA0045R34**
    (17. 7. 2022), „RS232C RX ONLY“, optočlen 6N135 – nástupce obou, v emulátoru `k2_adapter: "redukce34kb01"`.
- Schéma `Redukce34KB01-rev.2.0.png` (rev 2.0) má optočlen také, zapojení je v kap. 7. Výstup optočlenu jde na
  **K2-7 = P1-IN7** (BIOS: „MSB portu 1“). Klid RS-232 (mark, záporné napětí) → LED optočlenu nesvítí → pull-up → 1;
  start bit (space, kladné napětí) → 0. Odpovídá logice RxD.
- Původní ANK-1M5 (2021) neměla tlačítka INT/T/RES, P1-IN7 byl tedy volný. ANK-1M2 má INT na P1-IN7 přes 74LS06
  (otevřený kolektor). S redukcí rev 2.0 ale INT ani T k CPU nedojdou (kap. 7).

## 6. Programy pro JPR-1S + BGP-1S (doplněno autorem 2026-10-06)

Všechny jsou v Intel HEX (24 B na záznam, CR LF, závěr `:00000001FF` bez startovací adresy). Načítají se příkazem
**BOOT** přes redukci s optočlenem (19200 8N2) a spouštějí **`JUMP 4400`**.

| Soubor (`jpr1s_sw\`) | V repu | Rozsah | Co to je | Potřebuje |
|---|---|---|---|---|
| `manic_sapi.txt` (55 078 B, 904 záznamů) | `media/jpr-1s/manic_miner.hex` | 4400–989E (21 663 B) | **Manic Miner** (port z PMD-85: „M A N I C   M I N E R … Matthew Smith“, české texty); `manic_main.jpg` = snímek obrazovky (barevná grafika PMD, „Hlavní sluj“, „Vzduch“, „Věcí: 5“) | RAM 4000–BFFF, BGP-1S na **C000** (+ RAM banka 3 pod ní pro čtení), BIOS jen kvůli **RST 4** |
| `auto.txt` | `media/jpr-1s/auto.hex` | 4400–4BAB (1964 B) | malá hra s autíčky („HAVAROVAL SI 003 KRAT“, „CIEL“) | BGP-1S na C000 (LXI H,C000…), RST 3 a RST 6 z BIOSu, **joystick** |
| `m.asm/.lst/.txt` | `media/jpr-1s/martinuv_prvni_pokus.hex` | 4400–4428 | „MARTINUV PRVNI POKUS (06/06/2022)“ (Martinův první test) | **BIOS 1.02** (volá přímo PrintText 0BC4h; s 1.0 nebo 2.1 spadne) |

- Přenos Manic Mineru trvá 55 078 znaků × 11 bitů / 19 200 ≈ 32 s.
- **Manic Miner** (vstup 4400: `JMP 9565` → 95E5 `LXI SP,95E5h`): sledovaný kód (1868 instrukcí) nemá IN/OUT,
  EI/DI ani HLT, mimo program skáče jen do RST 4 (0020h). Hardware ovládá jen přes **P0 (2400h)**:
  - klávesnice ANK-1 po řádcích: řádek D4 (P = bit 0 výsledku, SHIFT = bit 2) a D3 (O), zjevně O/P pohyb a
    SHIFT skok; D4 b5 (CR), D3 b1 (S = pauza, čeká na uvolnění), D4 SHIFT + LF současně = konec hry (skok na 9565);
  - **zvuk = přepínání D6 (a D5) pomocí `XRI 60h`** v časovacích smyčkách (bzučák ANK-1 + zelená LED), stav P0
    je v proměnné na 979Ah;
  - žádný joystick, žádné přerušení (INT BGP-1S nepoužívá), P1/P2 nečte;
  - videoram C000–FFFF (např. LXI H,C030h, C602h, E7AAh, F742h), program a data 4400–989E.
- **auto**: smyčka hry čte **P1-IN (2800h) bit 2 a bit 3** = joystick ANK-1M2 **vpravo / vlevo** (P1-IN D3 vlevo,
  D2 vpravo podle `klavesnice.md`), jinak klávesy: P0 = 20h (všechny řádky aktivní, zelená LED), sloupec b0
  (SHIFT/Z/X/C/V) a sloupec b4 (LF/SP/M/N/B). Zvuk motoru `XRI 40h` na P0 (D6). Zvuk havárie čte bajty ROM od
  0000h (`LDAX B`) & 40h jako šum, závisí tedy na obsahu ROM (libovolný BIOS). Po konci čeká na libovolnou klávesu
  a skočí na 4400.
- `jpr1s_sw\schov\` je záloha stejných zdrojů a binárek 1.02 (hex/bin shodné).
- Fotka rev. 2.2 (všechny banky RAM osazené, INT1, RST7) odpovídá sestavě JPR-1S + BGP-1S na C000.

## 7. Emulace (2026-10-06)

- Typ **`JPR-1S`** (třída `Jpr1`, `src/cards/jpr1.*`), test `test_bgp1s`. CPU 8080, takt 18 MHz / 9.
- Parametry:
  - `rom`: EEPROM 28C64, výchozí `roms/jpr-1s/MIKROBIOS_1.02.bin`. Dále `MIKROBIOS_2.1.bin` (pro AND-1V na 3800)
    a `MIKROBIOS_1.0.bin`. Obrazy mají 8 KB, jsou převedené z HEX, prázdná místa jsou FFh;
  - `ram_4000`, `ram_8000`, `ram_c000`: propojky RAM_EN, výchozí vše zapnuté jako na fotce rev 2.2;
  - `int_int0`, `int_int1`, `int_p2in0`, `int_p2in1`: propojky INT_SRC, výchozí jen INT1;
  - `int_ack`: `rst7` (výchozí, INT_ADR 2-3) nebo `bus` (1-2, vektor ze sběrnice);
  - `crystal_hz`, `keyboard`, `k2_adapter`, `rx_baud`, `rx_stop_bits`, `serial` (TCP/COM jako u DSM-1).
- Zápis do adresy na desce (RAM, porty i ROM) jde vždy také na sběrnici. Čtení z desky má přednost.
- **SRAM 628128 má po zapnutí pseudonáhodný obsah** (jako u RMD-1). MIKROBIOS v kroku 3 detekce videoram ponechá
  `VideoBase` z RAM, pokud má dolních 14 bitů nulových. Nulová RAM by dala 0000h a CLS by přepsal proměnné BIOSu.
- P2-IN = FFh (pull-upy, deska diagnostiky se neemuluje). LED na P1-OUT7 je na panelu.
- Nahrávání programů do RAM na procesorové kartě (Soubor → Nahrát program, `--load-hex`, MCP) jde přes nové
  `CpuCard::poke_local()`. Dřív zápis přes sběrnici RAM na kartě JPR-1/JPR-1S nedosáhl.
- **Redukce34KB01** (`k2_adapter: "redukce34kb01"`, jde i pro JPR-1):
  - schéma `..\SAPI-karty\Redukce34\Redukce34KB01-rev.2.0.png` (rev 2.0, 17.07.2022, „Redukce kabelu klávesnice
    ANK-1M5“): FRB30 (K2 karty JPR) ↔ IDC34 (klávesnice); joystick, matice, LED a bzučák 1:1;
  - K2-7 (P1-IN7) budí optočlen 6N135: vstup z DSUB9 kolík 3 přes 270 Ω, dioda 1N4148, výstup s pull-upem 4k7
    (klid = 1, start bit = 0);
  - K2-1, 9, 10, 12 nejsou zapojené, takže klávesy INT a T klávesnice s touto redukcí k CPU nedojdou. ANK-1M2 má
    v návodu jen obyčejnou redukci bez sériové linky;
  - v emulátoru je to sériový kanál „K2 RS-232 (Redukce34KB01)“. Znaky z PC se vysílají za sebou rychlostí `rx_baud`
    (výchozí 19200) s `rx_stop_bits` stop bity (výchozí 2): start bit, 8 bitů od LSB, stop bity. Úroveň se počítá
    z času sběrnice při každém čtení P1;
  - test: BOOT načte `auto.hex` po lince bez chyby (1964 bajtů).
- Sestavy:
  - `machines/jpr1s_bgp1s.sapi`: MIKROBIOS 1.02, BGP-1S, ANK-1M2, redukce, TCP 2325 raw;
  - `machines/zps/ZPS-1_JPR-1S_AND-1B.sapi`: MIKROBASIC 2.4 v EEPROM, AND-1B, DSM-1.
- Ověřeno v emulátoru: MIKROBIOS 1.02 naběhne (mapa RAM, VIDEO BASE C000, Falcon 6.1L, `?`), Manic Miner a auto
  běží, MIKROBASIC 2.4 na JPR-1S + AND-1B počítá.

## 8. Nejasnosti

1. Propojky na autorově desce: fotka rev. 2.2 ukazuje RAM_EN 1+2+3 osazené, INT_SRC = INT1, INT_ADR = RST7. Je to
   aktuální stav? Je sestava JPR-1S + BGP-1S na C000 (+ AND-1V)? Který BIOS je teď v PicoROM?
2. Redukce: ~~dvě různé desky, nebo dva názvy?~~ Vyřešeno z čísel desek: první Redukce34KB01 (DPLA0014A5R) byla
   prostá, Redukce34REV (DPLA0026COM) a Redukce34KB01 rev 2.0 (DPLA0045R34) mají sériový vstup. Zbývá: jaký
   převodník se používá na straně PC (COM, USB-RS232) a jaký program posílá soubor?
3. Deska diagnostiky: zapojení je vyřešené (segmenty a–g, dp = bity 0–7). Otázka je jen, zda ji emulovat (autor
   ji asi má, fotka `diagnostika.jpg`).
4. Manic Miner: ovládání je odvozené z kódu (O/P, SHIFT, S pauza, CR, SHIFT+LF konec). Jestli čte zpět videoram
   (tj. jestli je RAM pod BGP-1S povinná), se nezjistilo. S RAM pod kartou v emulátoru běží.
5. Kam vede K1 /RTL na skutečné sestavě (tlačítko reset)? Na desce tlačítko reset není, ANK-1 RES na K2 nevede.
6. IC4: 74HCT04 (2021) / 74LS04 (2.2) vs. 74HCT14 [WEB]. Jen nesoulad v dokumentaci, pro emulaci nepodstatné.
7. Starší revize 2021: podle netlistu z eaglecon logicky shodná (DIP SW1 místo propojek). Potisk „2000h PORT2“
   na malé fotce `mJPR-1S.jpg` je nejspíš jen nečitelné „2C00h“ (osazovák 2021 má 2C00h).
8. `TEST AND1/AND1A/AND1B` je v 2.1 prázdný (`ret`). Asi záměr.
