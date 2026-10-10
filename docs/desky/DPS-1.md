# DPS-1 – deska přístrojové sběrnice IMS-2

Stav: **emulovaná** (2026-10-10), karta `DPS-1` (`src/cards/dps1.*`), sběrnice IMS-2 (`src/core/ims2.*`) a měřicí
pracoviště jako technologie mimo počítač (`plant` typu `IMS-2`, `src/plant/ims2_bench.*`): číslicový voltmetr Metra
M1T 330 (`src/plant/m1t330.*`) a laboratorní zdroj Tesla BS 525 (`src/plant/bs525.*`). Sestava
`machines/zps/ZPS-2_DPS-1_IMS-2.sapi` (ZPS-2 s MIKOSem V4.0F a kazetou BASIMS), okno Měřicí pracoviště IMS-2,
nástroj MCP `ims2`, test `tests/test_dps1.cpp`. Ověřeno originálním softwarem: BASIMS V4.0 (BASIC EX V4.0
s příkazy `IMS#`) běží beze změn a příklad z jeho návodu změří napětí. **Návod k obsluze:** `docs/navod-dps1.md`.

Zdroje:
- [D1] návod Tesla (6XV 122 21, 6XK 199 52, TPTE 82-197/84): sken `..\SAPI_DOKU\SAPIMAN\Scany dokumentací\DPS1.pdf`
  (schéma na stranách 22–32, korespondenční cyklus str. 20), OCR `..\SAPI.CZ\sapi\dps-1.php` (popis, porty,
  konektory X1 a X2, rozpiska, obsah PROM), stručná verze tamtéž;
- [D2] schéma překreslené EC1045 (rev. 1.0, 2010) `..\SAPI.CZ\sapi\pdf\dps-1.pdf`, replika v EAGLE
  `..\SAPI_DOKU\sapi_eagle\REPLIKY\DPS-1\` (netlist vytažený `eaglecon`, `work/dps1/n.txt`, mimo git);
- [D3] BASIMS V4.0: návod a výpis ovladače `..\SAPI.CZ\sapi\doku2\basims.php`, HEX `..\SAPI.CZ\sapi\data\basims_hex.php`
  (v repu `media/dps-1/BASIMS.hex`; `BASIMS4.SYS` na disketě MIKOS/D V5 je tentýž program s hlavičkou souboru);
- [V1] P. Schauer, VUT Brno: „GPIB komunikace s přístroji M1T330, M1T380 a BM595“,
  http://fyzika.fce.vutbr.cz/file/schauer/automatizace/m1t330_m1t380_bm595.pdf (příkazy M1T 330);
- [V2] Anubis' blog, „Voltmetr M1T330 a jeho oprava“ (2013), http://anubisuv.blogspot.com/2013/11/voltmetr-m1t330-jeho-oprava.html
  (převodník, fotografie panelu a desek);
- [Z1] návod BS 525 na ManualsLib (str. 10–11: M1, M2, PŘ.1/PŘ.2 „Měření“, R24 omezení proudu, E40–E42).

## Deska (D1, D2)

- **Porty pevně 30h–37h** (dekodér 3205, v replice 74LS138; A7 = A6 = 0, A5 = 1). Dva MHB8255A:
  - **D2C 30h–33h, řídicí:** PA vstup IFC-I, REN-I, EOI-I, ATN-I, SRQ-I, ERR, PON, ZS (bity 0–7); PB vstup V0–V3
    (číslo vícevodičové zprávy z PROM), EOIS, RDY, DAC, DAV; PC výstup IFC-O, REN-O, EOI-O, ATN-O, SRQ-O, CEN, RER,
    LIST/TALK (v originále READ). Řídicí slovo 92h.
  - **D1C 34h–37h, datový:** PA = DIO vstup, PB = DIO výstup. Posluchač B0h (PA strobovaný vstup, mód 1), mluvčí
    94h (PB strobovaný výstup, mód 1). PC6 IESRQ, PC7 IEZS (povolení přerušení), stav na 36h.
- **Logika vodiče = 1:** všechny vstupní bity jsou 1, když je vodič aktivní (na kabelu v L). Budiče a přijímače
  MH8641 invertují, data na sběrnici jsou tedy logická (DIO1 = bit 0).
- **Budiče** pracují, jen když je klopný obvod **PON** = 0. /RES ho nastaví (plovoucí PC6 drží RER v H přes R9),
  RER = 0 ho shodí. ATN, EOI, IFC a REN navíc jen s **CEN** = 1 (deska je řidič).
- **Mluvčí:** zápis do PB nastaví /OBF B. Posuvný registr 74164 na F2 (0,5 µs):
  - po 0,5 µs vezme klopný obvod **ERR** stav „NDAC i NRFD uvolněné“ (nikdo neposlouchá);
  - po 2,5 µs (T1) nastaví latch D2A **DAV**, je-li NRFD uvolněný a není ERR;
  - DAV drží, dokud trvá /OBF B a NDAC;
  - uvolněný NDAC (všichni převzali) je **/ACK B**: /OBF B jde do H, DAV skončí.
- **Posluchač:** DAV svým začátkem strobuje data do PA (STB A). IBF A budí NRFD, NDAC = not (IBF A and DAV).
  S LIST a EOI je STB A držený (paralelní hlášení).
- **ZS (změna stavu):** hrana ATN, EOI, REN, IFC, náběh SRQ a ERR (pulzy RC 150 Ω/220 pF přes XOR). RER = 1 nuluje
  ZS i ERR.
- **PROM D3D** 74S571 (512 × 4): adresa = DIO + ATN (A8), výstup V0–V3 přebírá 74174 (D4D) náběhem DAV i s EOI
  (EOIS). Obsah z návodu (`roms/dps-1/DPS-1_D3D_74S571.bin`, kontrolní součty HEXu sedí): bez ATN 00h–08h → 0–8,
  s ATN 00h–06h → 9–15, jinak 0. Uživatel si může naprogramovat vlastní.
- **/INT0** = ZS a IEZS, nebo SRQ a IESRQ, nebo INTR A, nebo INTR B.
- **Propojky:** JP1 = IFC od tlačítka RESET (/RTL přes V1), JP2 = odpověď na paralelní hlášení na DIO1 (ATN a EOI,
  když deska není řidič, přes V2).
- **Konektor X2** (FRB 30): signály IMS-2, převod na CANNON 25 v návodu. Deska podle návodu nestíhá odezvu na ATN
  (200 ns), může být jen řidičem.

## Emulace

- **Sběrnice IMS-2** (`core/ims2.h`): vodiče jsou bity (1 = aktivní), sběrnice je OR všech uzlů. Uzel reaguje na
  změnu vodičů hned (hradla) a má vlastní časované události (T1 desky, zpoždění přístroje). Čas je čas sběrnice
  stroje. Monitor si pamatuje posledních 256 přenosů (bajty s ATN a EOI, změny IFC a REN), ukládá se do stavu.
- **Karta** počítá hradla a klopné obvody z netlistu při každém přístupu CPU a při každé změně vodičů. K pracovišti
  se připojí při prvním přístupu na porty (kabel X2). Bez pracoviště jsou vodiče uvolněné (terminace).
- **Jediný předpoklad proti schématu:** LIST/TALK (PC7 D2C) blokuje vlastní NDAC desky, když deska mluví.
  - Schéma (originál Tesly str. 24 i EC1045) vede READ jen do hradla D2B (EOI → STB A) a NDAC = NAND(IBF A, DAV)
    nemá jak vypnout. Deska by tak při vysílání sama držela NDAC a žádný bajt by nebyl potvrzen.
  - Ověřeno v emulátoru: čistě podle schématu se BASIMS zasekne na `IMS# O` ve smyčce OU1 (PC 6270h, čeká na
    /OBF B).
  - Návod i BASIMS přitom říkají, že PC7 vlastní NDAC blokuje („0EH … zablokování vlastního signálu NDAC“,
    „0FH … uvolnění“). Emulace se drží návodu.
- **Nikdo neposlouchá:** NDAC je uvolněný, /ACK B (přijatý NDAC) je trvale v L. 8255A podle datasheetu nuluje /OBF
  úrovní /ACK, takže zápis do PB se „potvrdí“ hned. Posuvný registr nedoběhne ke kontrole ERR, DAV nepřijde a bajt
  se ztratí. Klopný obvod ERR se tak s 8255 nenahodí nikdy. Proto asi BASIMS kontroluje posluchače programem (viz
  Software).
- **Zajímavost:** BASIMS přepíná desku na posluchače zápisy B0h a 03h do 37h. Mezi nimi je PC1 (= /OBF B v módu 0)
  v L a `OUT` trvá déle než 2,5 µs, takže logika vyšle DAV s daty 00h, ještě s ATN. V monitoru je to bajt 00h po
  `TAD 3`. Přístroje bajt 00h s ATN ignorují. Podle schématu to tak skutečná deska dělat musí.

## Měřicí pracoviště (`plant` „IMS-2“)

```jsonc
"plant": { "type": "IMS-2", "m1t330": { "address": 3 }, "bs525": { "u1": 12.564, "i1": 0.5, "u2": 5.0, "i2": 0.5 },
           "input": "I" }   // input: výstup BS 525 na svorkách H, L voltmetru (I, II, none)
```

### Voltmetr Metra M1T 330

- Stejnosměrný voltmetr (asi 1982), převodník s dvojí integrací přes půlperiodu sítě (10 ms), rozsahy 300 mV, 3 V,
  30 V, 300 V s 30000 dílky, autokalibrace, číslicový filtr [V2]. IMS-2 s adresou z přepínačů vzadu (v sestavě 3, jako
  v příkladu BASIMS).
- **Příkazy** [V1]: R0–R3 rozsah, R4 automatika, D1/D0 filtr, E start měření, W0 / Wx zpoždění x × 10 ms,
  C kalibrace, K0/K1 autokalibrace vyp/zap, Q chyba měřené hodnoty. Příkaz končí dalším písmenem, CR, LF, mezerou,
  čárkou, `!` nebo EOI.
- **Odpověď** podle návodu BASIMS: `V+1.2564E+1` (12,564 V). Pět platných číslic, exponent bez nul.
- **Rozhraní:** AH1, SH1, L a T, RL (REN a adresováno = dálkové ovládání, kontrolka EXT; GTL, uvolnění REN), DCL/SDC,
  GET, IFC.
- **Panel** podle fotografií [V2]:
  - červený displej (znaménko a pět číslic), kontrolky mV, V, %, štítek „VOLTMETR M1T 330“ s logem Metry, TEST, EXT;
  - tlačítka SÍŤ, AUTO a FILTR (s kontrolkami), START REP a 1x (kontrolka mezi nimi), CHYBA (displej ukáže chybu
    v %);
  - svorky G, L, H.
- **Měření v emulaci:**
  - doba: zpoždění W, integrace 10 ms (s filtrem 4 × 10 ms) a 8 ms režie; každé osmé měření autokalibrace 20 ms;
  - automatika mění rozsah po jednom kroku (relé 30 ms a nový převod), nahoru nad 30000 dílků, dolů pod 2900;
  - místně START REP měří znovu po 200 ms. Dálkově se měří jen na E (nebo GET), výsledek čeká na mluvčího;
  - šum asi 1 dílek (opakovatelný), s filtrem menší.

### Zdroj Tesla BS 525

- Dva nezávislé výstupy 0–30 V s omezením proudu 50 mA–1,1 A [Z1]: měřidlo s horní stupnicí pro napětí a dolní
  pro proud, přepínač „Měření“ V/A, knoflík napětí, knoflík omezení proudu, kontrolka ochrany (E41/E42),
  svorky + a −; síťový vypínač s kontrolkou E40.
- Bez sběrnice, nastavuje se rukou v okně. Na výstup lze dát zátěž pracoviště (1 kΩ, 220, 100, 47, 22, 10 Ω).
  Když chce víc než omezení, výstup drží proud a napětí klesne, rozsvítí se ochrana.
- Voltmetr (10 MΩ) je na výstupu I, v okně jde přepojit na II nebo odpojit.

## Software

- **BASIMS V4.0** [D3]: BASIC EX V4.0 (varianta B) s klíčovým slovem `IMS#` místo `EDIT` (editace jen přes `@`).
  Běží jen pod Monitorem V4.x a **MIKOSem V4.0F** (volá jeho podprogramy TIME, START, NULBF, STOP, WOB). Načte se
  z kazety `.K_L BASIMS`, `.J` (4400h–635Fh). Příkazy:
  - `IMS# N` nulování sběrnice (IFC);
  - `IMS# O adr;seznam` adresuje posluchače a pošle seznam jako PRINT (na konci LF s EOI, pak UNL);
  - `IMS# T adr` adresuje mluvčího, deska se přepne na posluchače;
  - `IMS# I A$` čte řetězce až do čárky nebo EOI (ZS), pak REN pryč a IFC.
- **Příklad z návodu** (v emulátoru ověřený, test `test_dps1`): `10 IMS# N`, `20 IMS# O3;"R4D0E"`, `30 IMS# T3`,
  `40 IMS# I A$`, `50 PRINT A$` → `V+1.2564E+1`.
- **Chyba BASIMS, když nikdo neposlouchá:** OUTDAT před zápisem testuje RDY a DAC. Při „NDAC i NRFD uvolněné“ volá
  ERRLI, ta KONVY, ta posílá UNL přes OUTDAT, který znovu zavolá ERRLI. Rekurze nekončí, zásobník přeteče
  a MIKOS se spustí znovu (v emulátoru ověřeno, test). Návod BASIMS mluví jen o neexistující adrese (jiný přístroj
  na sběrnici je), tehdy se „nic nedozvíme“. Bez jakéhokoli přístroje (vypnutý voltmetr, odpojený kabel) BASIMS
  spadne.
- **Čtení musí odpovídat** počtu údajů (návod BASIMS): víc čtení zablokuje program (voltmetr po odpovědi nic dalšího
  neposílá, dokud nedostane E).

## Nejasnosti

- **Předpoklad LIST/TALK → vlastní NDAC** (viz Emulace): jak je to na skutečné desce zapojené? Schéma Tesly
  i EC1045 vede READ (PC7 D2C) jen do D2B. Funguje replika EC1045 s BASIMS jako mluvčí (`IMS# O`)?
- **/OBF B a /ACK B:** nuluje MHB8255A /OBF úrovní /ACK (datasheet Intel), nebo hranou? Při úrovni se ERR nenahodí
  nikdy a zápis bez posluchače zmizí. Jak se deska chová bez přístroje na sběrnici?
- **Bajt 00h s ATN** mezi zápisy B0h a 03h do 37h (viz Emulace): je ho na skutečné sběrnici vidět (logický
  analyzátor na DAV)?
- **M1T 330:**
  - odpověď končí CR LF s EOI na LF (odhad podle BM 595 [V1]; BASIC to snese);
  - tvar odpovědi na Q (emulace: `%+d.ddddE±e`, chyba v %);
  - přetečení (emulace: `V+9.9999E+9`, displej OL);
  - doby měření, autokalibrace a rozsahů;
  - chování REP v dálkovém režimu;
  - popisky tlačítek podle fotografie (SÍŤ, AUTO, FILTR, START REP/1x, CHYBA);
  - zda `R4` opravdu jede od nejmenšího rozsahu („přepnutí z minimálního na maximální rozsah“ [V1]), emulace mění
    rozsah po krocích od posledního.
- **BS 525:** rozmístění ovládacích prvků na panelu (nakreslené podle popisu v návodu, ne podle fotografie).
- **PROM D3D:** obsah z návodu je zjevně jen ukázka. Měl někdo skutečné kódy zpráv (UNL, DCL…)?
