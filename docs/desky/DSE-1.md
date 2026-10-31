# DSE-1 – deska simulátoru a programátoru EPROM

Stav: **emulovaná** od 2026-10-30:
- karta `DSE-1` (`src/cards/dse1.*`) a čip EPROM 2716/2732 vyhodnocovaný z vývodů (`src/chips/eprom27.*`);
- sestava `machines/zps/ZPS-2_DSE-1.sapi` (ZPS-2 s MIKOSem V4.0F a programem DSE Tesla Elstroj od 1000h);
- okno **Programátor EPROM DSE-1** (`src/frontend/dse1.cpp`, od 2026-10-31): krabička kabelu X2 se zelenou paticí,
  EPROM v ní, LED PROG a MAP, UV lampa na smazání, pod tím médium a stav desky; nástroj MCP `dse1`;
- testy `test_dse1` (primitivy na portech), `test_dse1_tesla` (program Tesla v ZPS-2), `test_dse1_simprog`
  (SIMPROG Melsoft z diskety 7 v kopii ZPS-6) a `test_dse1_eprom` (EPROM.COM J. Bíby v kopii `cpm_boot_rpd1`);
- **návod k obsluze: `docs/navod-dse1.md`**.

Tesla, deska 3WK 055 04 / 3WK 055 07, provedení DSE-1A a DSE-1B. Vyráběla TESLA Blatná, dodávala TESLA Eltos DIZ,
program napsal Ing. Petr Navrátil (Tesla Elstroj). Deska má dvě funkce:
- **simulátor EPROM:** 4 KB statické RAM (8× MHB 2114). Kabel X3 se sondou v patici cílového zařízení ji ukáže jako
  EPROM 2716 nebo 2732;
- **programátor EPROM 2716 a 2732:** kabel X2 s paticí („packový konektor“) a s LED1 a LED2 v krabičce.

Počítač na desku vidí jen přes porty: 8255 na F0–F3h a registr 74LS174 na F4h. Okno v paměti, přerušení ani RDY
deska nemá.

## Podklady

- **Návod Tesla** (OCR, schéma, výkres sestavy, rozpiska, tabulky konektorů): https://sapi.cz/sapi/dse-1.php,
  schéma https://sapi.cz/sapi/pdf/dse-1.pdf (černobílé `dse-1_cb.pdf`). Kopie zálohy webu `..\SAPI.CZ\sapi\`.
- **Verze z AR B 6/1985** (Ing. Smutný): schéma https://sapi.cz/sapi/pdf/dse-1_ar.pdf, výkres osazení na téže
  stránce. Logika, porty i bity jsou stejné (netlisty porovnané), liší se jen budiče k cíli (74125 místo 74LS257)
  a zdroj Vpp.
- **Překreslení EC1045 v EAGLE** (deska i verze AR): `..\SAPI_DOKU\sapi_eagle\REPLIKY\DSE-1\`. Netlist z `eaglecon`
  je v pracovní složce rozboru mimo git.
- **Fotky desek:**
  - deska s výrobními štítky 6750 a 44328: https://sapi.cz/sapi/obrazky/dse-1.jpg, totéž
    `..\SAPI_DOKU\FOTO_poklady_petr\SAPI\DSE-1_soucastky.jpg` a `DSE-1_spoje.jpg`;
  - druhá deska se štítkem 0917 a s měničem: `..\SAPI_DOKU\SAPI15\SimProg_opr\Fotky_měnič\` (`osazení_555.JPG`,
    `osazení_tlumivka.JPG`, `DSE_1_celek.JPG`; `měnič.jpeg` je článek Ing. Pavčíka o měniči s 555 z +5 V).
- **DSE.DOC a DSECPM.DOC** (TESLA Eltos DIZ, ke kabelům X2 a X3, k Vpp a k programu pro CP/M) a **DSE.COM**
  (J. Hlaváč 1988, CP/M ZPS-3): disketa `..\SAPI_DOKU\SAPIMAN\PMD32-SD_IMAGE8\PERINA\dse.8sd`.
- **Článek Ing. P. Navrátila** „Deska simulátoru EPROM – software“ (soubor `CLANEK.DSE`): disketa
  `..\SAPI_DOKU\SAPIMAN\PMD32-SD_IMAGE8\ZDROJ_STARE\a_teleclub.8sd`.
- **Program Tesla** (MONITOR V4, „NAP051186“): `..\SAPI_DOKU\MIKOS4\DSE-1_1000H.BIN` (= `SAPI_EPROM\ZPS-2\V4\
  DSE-1_1000H_programator.BIN`), výpisy Intel HEX https://sapi.cz/sapi/data/dse_1000_hex.php (1000h) a
  https://sapi.cz/sapi/data/dse_7000_hex.php (7000h, také `..\SAPI_DOKU\MIKOS_PGM\DSE-RAM.HEX`), stránka
  https://sapi.cz/sapi/doku2/dse.php.
- **Repliky adaptéru X2 Petra Všijanského** (EAGLE): `..\SAPI_DOKU\SW_SAPI_Petr\sapi\DSE-1\deska_pro_EPROM*.sch`.
  Dvě patice 2716 + 2732 (`deska_pro_EPROM`, `_2`), nebo jedna patice s propojkami JP1–JP3 (`_B`, `_B2`).
- **Další programy:** SIMPROG Melsoft `..\SAPI_DOKU\SAPI15\MELSOFT_DISKETY\DISK7\SIMPROG.*` a upravená kopie
  `..\SAPI_DOKU\SAPI15\SimProg_opr\` (s `poznámka.txt`), EPROM.COM J. Bíby, DSE4 a MON1 Emila (OK2KOJ)
  `..\SAPI_DOKU\SW_SAPI_Petr\EMIL\Eprom_2\` a `OK2KOJ\Eprom_2\` (oddíl Software).

## Zapojení

### Sběrnice a porty

- **X1 (TY 517 6211, 27 kontaktů):** 8 /RES, 10 IOR, 12 IOW, 15–18 +5 V, 19–20 0 V, **21 MAP1** (výstup),
  31–38 D0–D7, 39–46 A0–A7, **50 +30 V** nestabilizovaných (jen DSE-1B).
- **Dekodér:** 74LS85 (D4A) porovná A7–A4 s přepínači S1. 74LS138 (D5A) dekóduje A2 a A3, vstup C je přes kaskádu
  v H:
  - Y4 (A3 = 0, A2 = 0) vybere 8255, adresy **základ+0…3**;
  - Y5 (A3 = 0, A2 = 1) zapíše registr 74LS174, adresy **základ+4…7** (A1 a A0 registr nebere, +5…+7 jsou zrcadla);
  - **základ+8…F deska nedekóduje.**
- **S1 (S1/1–S1/4):** S1/1 = A6, S1/2 = A7, S1/3 = A5, S1/4 = A4. Sepnutý spínač = L = bit 0. Bez propojek (všechny
  rozepnuté) je základ **F0h**, tak se deska dodávala. Emilův komentář „S1/3 OFF (0), ostatní ON (1)“ pro 20h tomu
  odpovídá (potvrzuje polaritu a S1/3).
- **Oddělovač 8286** (D3A): /OE = Y4 nebo Y5, směr T = IOR. Při čtení budí sběrnici na všech osmi adresách,
  i na +3 a +4…+7, kde nic nečte.

| Port | Zápis | Čtení |
|---|---|---|
| základ+0 (F0h) | 8255 PA: data PD0–PD7 vnitřní sběrnice | PA, když je vstup: PD |
| základ+1 (F1h) | 8255 PB: adresa PA0–PA7 | PB |
| základ+2 (F2h) | 8255 PC: PC0–PC3 = adresa PA8–PA11, PC4 = W, PC5 = R, PC6 = PROG.PULS, PC7 = R/W | PC |
| základ+3 (F3h) | řídicí slovo 8255 | nic (NMOS 8255A ho číst neumí) |
| základ+4…7 (F4–F7h) | registr 74LS174 | nic |

**Rozpory v návodu Tesla:** „Pro adresaci využívá deska DSE-1 5 adres“, dekóduje ale osm (zrcadla +5…+7). U řídicího
slova uvádí směr „IOW, IOR“, NMOS 8255A ho ale číst neumí (katalog: nedovolený stav, sběrnice ve 3. stavu).

### Vnitřní sběrnice

- **Data PD0–PD7** propojují 8255 PA, RAM simulátoru (přes 74LS257) a EPROM v patici X2.
- **Adresa PA0–PA11:** 8255 PB a PC0–PC3, nebo v režimu ON LINE adresa SA0–SA11 od sondy X3 (multiplexery 74LS257
  D3C–D5C, /G = /ONLA). Adresu RAM i EPROM dává tatáž sběrnice.
- **RAM simulátoru:** /CS = NOR(W, R) přes 74LS138 (D1C), /WE = NOT W. Zápis je **úrovní**, ne hranou: 2114 nemá
  takt, dokud je W = 1, zapisuje se. Při R = 1 a W = 0 budí RAM data na PD.
- **Data k sondě X3** (74LS257 D9D, D10D): /G = NAND(ONLD, OR(/CS1·/OE1, /CS2·/OE2)). Pull-upy 1k na /CS a /OE,
  R27 1k na SA11.
- **Úrovně vývodů 8255 v režimu vstupu:** v H jsou jen linky se vstupem LS-TTL: PA10, PA11 (74LS138), PC4–PC7 a PD0–7.
  PA0–PA9 (PB0–7, PC0–1) vedou jen na vstupy MOS (2114, EPROM) a na třístavové výstupy 74LS257, jejich úroveň
  je neurčitá.

### Registr 74LS174 (F4h, jen zápis)

| Bit | Signál | Význam |
|---|---|---|
| D0 | U0V | 0: Vpp 0 V |
| D1 | U25V | s U0V = 1: 0 = Vpp 5 V (čtení), 1 = Vpp 25 V (programování) |
| D2 | ONLA | adresa od sondy X3 na PA0–PA11 |
| D3 | ONLD | data RAM k sondě X3 |
| D4 | LED1 | „indikace progr.“ (LED v kabelu X2) |
| D5 | LED2, MAP | „indikace map.“ a přes **JP1** výstup MAP1 (X1/21) |

- /CLR = /RES: reset vynuluje registr (Vpp 0 V, OFF LINE, LED zhasnuté, MAP1 = L).
- **LED svítí při 1.** Deska linku invertuje (7402), LED v kabelu má anodu na +5 V.
- **ON LINE** = ONLA a ONLD. Návod Tesla: ve stavu OFF LINE má k RAM přístup procesor, v ON LINE „objímka“ EPROM.
  8255 se při přechodu do ON LINE přepne na vstupy.
- **MAP:** návod uvádí jako výhodu desky ovládání mapování RAM-1 („1. / 2. 64 KB“). Tabulka X1 má kolík 21 MAP1
  osazený. Programy Tesla, SIMPROG a DSE.COM bit 5 nikdy nenastavují, Emil a J. Bíba ano (LED).

### Programovací napětí Vpp

- **MAA 723** (V6) stabilizuje Vpp na 24 V (trimr P2) a pro čtení na 5 V (trimr P1). V1 (KFY 46) regulátor zavře při
  U0V = 0 (i při U25V = 1), V2 přepíná dělič P2 / R8 podle U25V. Výstup jde na X2.26 a X2.25.
- **Napětí:** návod a DSECPM.DOC 24 V, jinde návod a AR 25 V. Navrátil: pro 2732A 21 V nastavených trimrem P2.
- **Zdroj:**
  - **DSE-1A:** vnitřní měnič z +12 V (+12 V nejvýš 100 mA);
  - **DSE-1B:** +30 V nestabilizovaných z kolíku 50 sběrnice, „s výhodou“ ze svorky N zdroje ZDR-1A. Rozpiska:
    součástky měniče (R16–R18, C2, C3, C5, C7–C11, C20, C21) jsou v provedení B neosazené.
- **DSE.DOC a DSECPM.DOC:** bez +30 V na kolíku 50 „nebude pracovat ani čtení obsahu EPROM z programovacího
  konektoru“.
- **Fotky:** deska 6750/44328 je provedení B. Deska 0917 má osazený měnič s 555, toroidní tlumivku, diody BY 298
  a kondenzátory 47 µ/40 V a 47 µ/50 V, a u X1 3pinový kolíček s propojkou na místě JP2 z překreslení EC1045. Je to
  tedy A, nebo upravená B. Návod Tesla JP2 nezmiňuje.
- **Přepnutí Vpp není okamžité:** výstup MAA723 má C18 100n a R28 2k2 k zemi. Po zavření regulátoru napětí klesá
  s τ ≈ 0,2 ms.

### Programovací kabel X2

Konektor FRB-30 (TY 513 3011). Kabel se dodával **zapojený pro 2716** (DSE.DOC, DSECPM.DOC), vývody 11, 20 a 25 jsou
„pro 2716 nepřip.“:

| Vývod patice | 2716 (z výroby) | 2732 (upravený kabel) |
|---|---|---|
| 18 | X2.18 = PC6: /CE/PGM | X2.20 = NOR(PC6, PC7): /CE |
| 20 | X2.12 = NOT PC7: /OE | X2.25 = Vpp: OE/Vpp |
| 21 | X2.26 = Vpp | X2.11 = PA11: A11 |
| 19 | PA10 | PA10 |

- **Úprava pro 2732** (DSE.DOC, DSECPM.DOC): v konektoru FRB-30 kabelu se přepojí tři vodiče 26 → 11, 18 → 20
  a 12 → 25.
- **Repliky Petra Všijanského:** dvě patice (2716 a 2732), nebo jedna patice s propojkami.
- **Krabička kabelu:** podle návodu má patici („packový konektor“, tedy patici s pákou) a LED1 a LED2. Jak vypadala,
  nevíme (fotka chybí).
- **EPROM, které Tesla vyráběla:** MHB2708C a MHB2716C, 2732 ne (autor 2026-10-31). Kabel X2 a program Tesla počítají
  s 2716 a 2732. Jestli DSE-1 programovala i 2708, nevíme (Nejasnost 13).
- **2716:** čte při /CE = 0 a /OE = 0 (PC6 = 0, PC7 = 1) a Vpp 5 V nebo 25 V (při 25 V je to Program Verify).
  Programuje pulz PGM = PC6 do H při Vpp 25 V a /OE = 1 (PC7 = 0).
- **2732:** čte při /CE = 0 (PC6 nebo PC7 = 1) a OE/Vpp = 0 V. Programuje /CE dole při OE/Vpp 25 V, stav PC7 nehraje
  roli.
- **2732 v kabelu pro 2716** čte při registru 11h horní 2 KB: na A11 (vývod 21) je Vpp 5 V. Naprogramovat nejde
  (OE/Vpp dostává jen logiku) a při programování by na A11 bylo 25 V.

### Simulační kabel X3 a sonda

- Dodává se také zapojený pro 2716: vývod 11 (SA11) je „pro 2716 nepřip.“ a pull-up R27 dá SA11 = 1. **2716
  v simulátoru proto leží na 800–FFFh RAM**, tak s ní počítají všechny programy (Navrátil tomu říká „hardwarová
  chyba“, simulovat jde jen jedna 2716).
- **Pro 2732** se SA11 přivede stíněným vodičem na vývod 21 sondy (DSECPM.DOC). „Takto upravený kabel lze použít
  i pro 2716“: v její patici je na vývodu 21 Vpp = 5 V, takže SA11 = 1 i tak.
- Dvě 2716 (CS1/OE1, CS2/OE2) jen otevírají data, adresu A11 dává jen společný SA11.
- DSECPM.DOC: kabel asi 1 m je už kritický, lepší je do 60 cm.

## Software

| Program | Systém | Kde je | V emulátoru |
|---|---|---|---|
| DSE (Tesla Elstroj, Ing. P. Navrátil, „NAP051186“) | MONITOR V4.0F/V4.1F, od 1000h v EPROM (2× 2708) | SAPI_DOKU `MIKOS4\DSE-1_1000H.BIN`, sapi.cz | `roms/dse-1/MIKOS_V4.0F_DSE_1000.bin`, sestava `zps/ZPS-2_DSE-1.sapi`, test `test_dse1_tesla` |
| DSE od 7000h | MONITOR V4 v RAM | sapi.cz `dse_7000_hex.php`, SAPI_DOKU `MIKOS_PGM\DSE-RAM.HEX` | ne (jen přemístěná kopie) |
| DSE pro MIKOS V5 (1800h v EPROM, 4400h z kazety) | MONITOR V5 | popis v DSE.DOC, nenašel se | ne |
| SIMPROG (Melsoft, převod programu Tesla) | CP/M 2.2, 8080 | disketa 7 SAPI-1.5 (`media/sapi15/SAPI15_7.sp2`), upravená kopie `SimProg_opr` | test `test_dse1_simprog` v kopii ZPS-6 |
| DSE.COM (J. Hlaváč 1988, Intel HEX, asi 20 příkazů, pulzy se přizpůsobují) | CP/M ZPS-3, 8080 | SAPIMAN `PERINA\dse.8sd` | ne (později, s autorem) |
| EPROM.COM (J. Bíba, „V130786“, 2708–2764, základ portů nastaví příkaz `B`) | CP/M, Z80 | `media/sapiman/knihovna/sysprog_a_sluzebni_CPM.img` | test `test_dse1_eprom` v kopii `cpm_boot_rpd1` (JP1 rozpojená) |
| DSE4 a MON1 (Emil, OK2KOJ 2007, základ 20h) | vlastní monitor | SAPI_DOKU `SW_SAPI_Petr` | ne, jen pro poučení |

- **Program Tesla** volá MONITOR V4.0F nebo V4.1F (HILO 0624h, DISP, FILL, SUBS…), s V4.0D ani V5 nejde.
  Nabídka: T (EPROM → RAM), P (RAM → EPROM), D (výpis RAM), S (RAM → simulátor a ON LINE), R (simulátor → RAM),
  C (porovnání), A (změna RAM), M (nabídka), F (vyplnění RAM), E (návrat do monitoru), Z (zkouška desky: ROM
  0000–0FFF do RAM simulátoru), K (kontrola smazání). Verze 7000h se liší ve 165 bajtech, všechny jsou horní bajty
  adres 1x → 7x. Popisek „univerzální“ na sapi.cz je proto zavádějící.
- **Programování v programu Tesla:** F4 = 13h (Vpp 25 V, LED1), řídicí slovo 80h, pro každý bajt adresa, data a pulz
  PC6 = 1 (bitové nastavení 0Dh ve smyčce 03E7h, **50 ms na 2 MHz**), pak kontrola celého rozsahu. 2716 má registr
  11h (čtení 5 V), 2732 10h (Vpp 0 V, OE/Vpp = L).
- **Délka pulzu** časuje program, deska nehlídá nic. Na 2 MHz:
  - program Tesla 50 ms, Emil 49,9 ms;
  - SIMPROG z diskety 7 (`LXI H,07CEh` na 0824h, build pro 4 MHz) 98 ms, upravená kopie `SimProg_opr` (03E7h,
    „nemám rychlé SAPI (4MHz) ale normální JPR-1Z na 2MHz“) 50 ms;
  - DSE.COM 20 + 10 ms, při shodě 5 ms (schválně);
  - EPROM.COM J. Bíby 98 ms.
- **Celá 2716** trvá podle návodu „cca 2,5 min“. Program (2048 × 50 ms) a Navrátilův článek dávají asi 100 s.
- **SIMPROG, chyba porovnání:** smyčka `coma1` skončí na horní adrese a poslední bajt neporovná. Do FFFFh (přepíná
  přitom PC4/PC6) uteče jen při rozsahu jednoho bajtu.
- **Bity LED podle programů:** Tesla LED1 = programování, LED2 = mapování. Emil a Petr: bit 4 (10h) zelená
  „simulátor“, bit 5 (20h) červená „programování“ (`SW_SAPI_Petr\RUTINY.ASM`).
- **Kde má 2716 v RAM simulátoru:** program Tesla, SIMPROG, DSE.COM i J. Bíba 800–FFFh (kabel X3 z výroby). Emil
  000–7FFh, nejspíš s jiným kabelem.

## Emulace

- **Parametry** (`src/cards/catalog.cpp`):
  - `base`: S1, 00h…F0h po 10h, text volby ukazuje polohu S1/1–4; výchozí F0h;
  - `jp1_map`: JP1, bit D5 budí MAP1; výchozí **osazená** (tabulka X1 Tesla). Sestava pro EPROM.COM J. Bíby ji musí
    mít rozpojenou, s komentářem, a říká to i text parametru. EPROM.COM při programování svítí bitem 5 LED (33h,
    u 2764 71h). S osazenou JP1 by MAP1 pod CP/M přepnula paměť karet, které na MAP1 reagují (RAM-1Z, RAM-1V,
    DMR-1, AND-1Z, AND-1V… podle jejich propojek). Na železe s osazenou JP1 je to stejné;
  - `vpp`: `pin50` DSE-1B s +30 V z kolíku 50 (výchozí), `converter` DSE-1A s měničem, `none` DSE-1B bez +30 V
    (Vpp vždy 0 V);
  - `x2_cable`: `2716` z výroby (výchozí), `2732` upravený podle DSE.DOC, `dual` dvě patice jako repliky (typ
    EPROM podle velikosti souboru vybere patici);
  - `eprom`: EPROM v patici X2 jako médium (platí hned), výchozí `media/dse-1/prazdna_2716.bin`; `working_copy`.
- **Model na úrovni signálů:** po každém zápisu do 8255 nebo registru a po resetu se z úrovní vývodů spočítá
  vnitřní sběrnice. Tak zvládne všechny programy i s jejich zvláštnostmi bez zvláštních případů.
  - Vývod 8255 v režimu vstupu plave. PA10, PA11, PC4–PC7 a PD jsou v H (vstupy LS-TTL), **PA0–PA9 také v H
    (předpoklad**, ve skutečnosti neurčité).
  - **PD** budí 8255 PA (výstup), RAM (R = 1, W = 0) a EPROM podle svých vývodů. Když nebudí nikdo, je FFh. Dva
    budiče naráz: wired AND a jedno varování v logu za každý druh srážky.
  - **RAM** se zapisuje úrovní: dokud je PC4 = 1, platí RAM[adresa] = PD. Funguje tak zápis Tesla (PC5 = 0)
    i Emilova kopie EPROM rovnou do RAM (PC4 + PC7).
  - **Čtení PA** v režimu vstupu vrátí PD. **Čtení +3 a +4…+7** vrátí FFh (předpoklad).
  - `decode_io` hlásí čtení i zápis na základ+0…7 (8286 budí sběrnici na všech osmi), mapa portů tak ukáže kolize.
- **EPROM** (`Eprom27`) je čip podle katalogu, vyhodnocený z vývodů 18, 20, 21 a adresy: čtení, standby,
  programování, Program Verify, zákaz programování. Úrovně 0 V = L, 5 V = H, 25 V = H a „programovací napětí“.
  Patici zapojí kabel podle `x2_cable`. Špatná kombinace se chová podle vývodů: 2732 v kabelu pro 2716 čte horní
  2 KB a nenaprogramuje se. 25 V na vývodu, který není Vpp (A11 2732 v kabelu pro 2716, /OE 2716 v kabelu pro 2732),
  zapíše jednou varování „na skutečném čipu by ho to mohlo poškodit“. Okno programátoru upozorní, když typ EPROM
  nesedí se zapojením patice.
- **2716 při Vpp 0 V** (bez +30 V nebo F4 = 00h) PD nebudí a čte se FFh (předpoklad podle DSE.DOC, Nejasnost 3).
- **EPROM v patici při S a R:** DSE.DOC chce mít při přenosu mezi RAM počítače a RAM simulátoru (příkazy S a R)
  patici programátoru prázdnou. Když se RAM simulátoru zapisuje nebo čte (W nebo R) a v patici je 2716 s Vpp 0 V
  a /CE dole (PC6 = 0), zapíše se do logu jednou varování. RAM se v emulátoru přečte správně, skutečná deska možná
  ne (Nejasnost 3). 2732 v kabelu pro 2732 má při S a R /CE nahoře, varování nedostane.
- **Programovací pulz:**
  - měří se v čase sběrnice (`(now − začátek) / cpu_hz`, jako DMT-1 a ADP-1), takže závisí na taktu CPU jako na
    železe;
  - PD se vzorkuje po celou dobu pulzu (AND), zápis, který pulz ukončí, se nepočítá. Řídicí slovo, které vynuluje PA
    i PC naráz (Tesla CMPTAIL, SIMPROG), tak do buňky nezapíše 00h;
  - **pulz od 1 ms programuje** (buňka = buňka AND data, bity jen 1 → 0), kratší ne. Práh skutečné 2716/2732 neznáme
    (předpoklad, Nejasnost 4). Počítají se jen hrany: smyčka Tesla zapisuje 0Dh 999×, to není nový pulz;
  - změna adresy během pulzu pulz ukončí a začne nový (jednou varování);
  - **programování** jsou pulzy od prvního do chvíle, kdy je Vpp **0,5 s času sběrnice** mimo 25 V, nebo do resetu
    či výměny EPROM. Krátký pokles Vpp programování neukončí: DSE.COM J. Hlaváče kolem každého pulzu přepne na
    5 V (F4 = 11h) a bajt přečte (2732 s F4 = 10h, tedy 0 V). Do logu jde jedno shrnutí: bajty, pulzy na bajt,
    nejkratší a nejdelší pulz v ms, počet krátkých pulzů. Když součet pulzů jednoho bajtu vyjde mimo 45–55 ms
    (katalogových 50 ms ± 5), jde do logu varování „naprogramováno stejně“. Přísný režim (krátký pulz =
    nenaprogramováno) není;
  - příklady: program Tesla na 2 MHz `DSE-1: programming: 256 bytes, 1.00 pulses per byte, pulses 49.97-49.97 ms`,
    SIMPROG z diskety 7 98,15 ms a EPROM.COM J. Bíby 98,3 ms, oba s varováním. DSE.COM dá bajtu 20 + 10 ms
    a bajtu, který už sedí, 5 ms (algoritmus se přizpůsobuje schválně), varování 5–30 ms dostane také.
- **Očekávaná poznámka u 2732:** program Tesla zapíše F4 = 13h dřív než řídicí slovo 80h a PC7 je přitom ještě 1 (po
  resetu plave v H, po K zůstane z čtení; C a T PC na konci každého bajtu vynulují řídicím slovem v CMPTAIL). U 2732
  je to /CE dole při 25 V, tedy krátký pulz s daty FFh, který nic nezmění. Shrnutí v logu ho uvede jako „1 pulse
  shorter than 1 ms did not program“. DSE.COM čte bajt s PC7 = 1 před každým pulzem, krátký pulz má tedy před
  každým a shrnutí je sečte.
- **Vpp** je 0, 5 nebo 25 V jmenovitě a přepíná okamžitě (zjednodušení: skutečný pokles s τ ≈ 0,2 ms, Nejasnost 8).
  Trimry P1 a P2 se nemodelují.
- **Reset** vynuluje registr a dá porty 8255 na vstupy. RAM simulátoru zůstane. Skutečná deska po resetu nejspíš
  zapíše FFh někam do C00–FFFh (W plave v H do prvního řídicího slova, adresa neurčitá). To se nemodeluje, zápis do
  RAM se nedělá, dokud PA0–PA9 plavou (Nejasnost 6).
- **RAM simulátoru** po zapnutí plní `power_on_fill` (nuly, nebo náhodně podle `power_on_memory` sestavy). Zálohu
  nemá.
- **Sonda X3 není nikam zapojená.** ON LINE je jen stav, který ukáže okno programátoru a MCP. ONLA při PB nebo PC0–3
  v režimu výstupu zapíše jednou varování o srážce. Sonda v patici jiné karty téže sestavy (REM-1, JPR-1) je
  další krok, pak bude potřeba i parametr simulačního kabelu (2716 / 2732).
- **MAP1** (`map_lines()`) dává MAP_1 při D5 = 1 a osazené JP1. Sběrnice linky MAP sčítá (OR), takže nevidí, když
  by DSE-1 držela L proti jiné kartě v H.
- **EPROM jako médium** (rozhodnutí 2026-10-10 o médiích):
  - `.bin` s 2048 B je 2716, s 4096 B 2732, jiná velikost se odmítne (2708, 2764 a 2732A zatím ne);
  - soubor z `media/` se používá přes pracovní kopii ve `work/dse-1/`, dump z `roms/` se zkopíruje do
    `work/dse-1/roms/<cesta v roms>` (se složkami, dumpy stejného jména se nepletou). Kopie vznikne, jen když ještě
    není (kopie z jiného běhu zůstane). Originály se nemění, programování píše do kopie a ukládá se průběžně jako
    ostatní média (dočasný soubor a přejmenování);
  - vložení téhož souboru znovu (Vložit…, MCP `insert`) ponechá, co se do EPROM naprogramovalo: obsah se nejdřív
    uloží a v patici zůstane;
  - prázdné originály `media/dse-1/prazdna_2716.bin` a `prazdna_2732.bin` (celé FFh). V okně programátoru Vložit…
    (nebo soubor `.bin` přetažený na okno), Vysunout, Nová 2716, Nová 2732 (`work/dse-1/eprom_N.bin`), „…“ → Obnovit
    z originálu a smazání UV lampou. Ochrana proti zápisu není potřeba, bez 25 V EPROM nic nezmění;
  - výměna EPROM při Vpp ≠ 0 V zapíše do logu varování (skutečnou EPROM by to mohlo zničit).
- **Stav** (`state()`): 8255, registr, 4 KB RAM, rozběhnutý pulz (začátek, adresa, data), statistika programování
  (i kdy Vpp kleslo z 25 V), zalogovaná varování, obsah EPROM a její obraz. Po načtení stavu se obsah EPROM zapíše
  i do pracovní kopie. Když soubor EPROM ze stavu nejde přečíst, dostane patice obsah ze stavu bez souboru (nic se
  neukládá, v logu je varování).
- **Okno Programátor EPROM DSE-1** (`###dse1`, `src/frontend/dse1.cpp`; přání autora 2026-10-31, rozhodnutí
  téhož dne). Otevře se samo se sestavou, která má DSE-1 (jako přístroje APS-5, IMS-2 a zapisovač), sestava
  `zps/ZPS-2_DSE-1.sapi` ho má v `windows`. Zobrazení → Programátor EPROM DSE-1. Kreslí krabičku na konci kabelu X2
  shora:
  - **vzhled krabičky Tesla neznáme** (fotka chybí), kresba je **záměrná odchylka** jako rack v konfigurátoru: šedá
    plastová krabička s plochým kabelem, zelená patice s nulovou zasouvací silou v duchu 3M TEXTOOL (24 vývodů, páka
    s kulatou koncovkou), LED a nápis „DSE-1 X2“;
  - **páka** je nakreslená zvednutá s prázdnou paticí a sklopená s EPROM v patici. Neovládá se (autor: uživatele
    nenutit páku mačkat);
  - **EPROM:** keramické pouzdro DIP 24 s křemenným okénkem a čipem pod ním, potisk „TESLA MHB2716C“ u 2716 a „intel
    D2732“ u 2732 (Tesla 2732 nevyráběla), kód data. Jak skutečná MHB2716C vypadá, nemáme ověřené, potisk je
    ilustrační;
  - **štítek:** naprogramovaná EPROM (aspoň jeden bajt jiný než FFh) má přes okénko bílý papírový štítek se jménem
    souboru bez `.bin` (dlouhé jméno na víc řádků a zkrácené), jak se lepil na vypálené EPROM. Prázdná ukazuje okénko;
  - **LED1 „PROG“ červená, LED2 „MAP“ zelená** (barvy vybral autor 2026-10-31, aby nebyly stejné; skutečné nejsou
    známé, Nejasnost 12). Kulaté, se září, zhasnuté tmavé;
  - **UV lampa** („horské sluníčko“) vedle krabičky smaže EPROM v patici, viz níže;
  - pod kresbou: EPROM v patici (soubor a typ), varování, když typ nesedí se zapojením kabelu, Vložit…, Nová 2716,
    Nová 2732, Vysunout, „…“ → Obnovit z originálu a **Uložit RAM simulátoru…** (4 KB do `.bin`, pomoc emulátoru,
    skutečná deska to neumí); řádek stavu: Vpp 0 / 5 / 25 V, ON LINE / OFF LINE, poslední programovací pulz v ms,
    „pulz běží“ a počet pulzů během programování. Jsou to **údaje emulátoru** (bublina to říká), krabička má jen LED;
  - soubor `.bin` přetažený myší na okno jde do patice (jako Vložit…), jinam přetažený se nahraje do paměti jako dřív;
  - UI snímek `--ui-dse1`, `--ui-crop dse1`; náhled sestavy ukazuje kresbu okna.
- **Smazání UV lampou** (pohodlí emulátoru, skutečná EPROM jde z patice pod UV lampu na 15–20 minut): klik na lampu,
  u naprogramované EPROM nejdřív otázka (obsah se ztratí, Enter nic nedělá, Esc zruší). Štítek zmizí, okénko asi
  1,5 s skutečného času svítí fialově a lampa na něj svítí, pak je EPROM celá FFh. Zapíše se do pracovní kopie jako
  programování (`dirty`, médium se uloží s ostatními), do logu jde „EPROM … erased by UV light (emulator comfort)“.
  **Při Vpp jiném než 0 V** se smazání odmítne s hláškou, jako se varuje před výměnou média pod napětím (vyndat EPROM
  pod napětím by ji mohlo zničit). Rozběhnuté programování (čekání 0,5 s na jeho konec) smazání ukončí. MCP `dse1`
  `erase` dělá totéž bez animace.
- **MCP `dse1`:** stav (rozepsaný registr, Vpp, ON LINE, LED, patice, programování, varování), `insert`, `new`,
  `eject`, `erase`, `flush`, `sim_read`, `sim_save` (`docs/mcp.md`).
- **Panel v konfigurátoru:** dva konektory FRB-30 jako DSM-1 a jiné karty s FRB, nahoře K3 = X2 programovací kabel
  (poznámka „PRG“), dole K2 = X3 sonda simulátoru („SIM“). Pořadí podle výkresu desky (X2 nad X3), fotka desky ukazuje
  oba konektory TY 513 30 11 na předním okraji.
- **Kolize portů:** F0–F7h se ZDP-1 (neemulujeme) a s RTC-1 na F0h v sestavě `aps5`. Emilův základ 20h koliduje
  s RPD-1A 20–25h. V ZPS-2, ZPS-3, ZPS-6 a v sestavách CP/M s JPR-1ZI je F0h volné (PKT-1, RSW-1 a DBS-1A
  dekódují celých 8 bitů).

### Ověřeno v emulátoru

- **Program Tesla v ZPS-2** (`test_dse1_tesla`, `docs/navod-dse1.md`): K hlásí `CISTA  0000 - 07FF`, P z 5000–50FF
  do EPROM od 0000 naprogramuje 256 bajtů pulzy 49,97 ms a ohlásí `SHODNY OBSAH  0000 - 00FF`, S a R tam a zpět
  `SHODNY OBSAH  0800 - 08FF`. S 2732 v kabelu pro 2732 od 900h `SHODNY OBSAH  0900 - 09FF`. Jediné varování je
  podle DSE.DOC u S a R (prázdná 2716 zůstala v patici).
- **SIMPROG z diskety 7 v kopii ZPS-6** s mechanikou B a DSE-1 (`test_dse1_simprog`): `B:SIMPROG A:X.BIN`
  naprogramuje 256 bajtů ze souboru pulzy 98,15 ms, v logu je varování mimo 45–55 ms.
- **EPROM.COM J. Bíby v kopii `cpm_boot_rpd1`** (CP/M J. Bíby na JPR-1ZI s DMR-1, služební disketa
  `sysprog_a_sluzebni_CPM.img` v B:, DSE-1 na F0h s rozpojenou JP1; `test_dse1_eprom`): `B:EPROM`, `P`, typ `1`,
  RAM 2000–20FF do EPROM od 0 naprogramuje 256 bajtů pulzy 98,3 ms a ohlásí `SHODNÝ OBSAH`. Při programování svítí
  LED1 i LED2, MAP1 zůstane v L. V logu je varování mimo 45–55 ms.
- **Primitivy** (`test_dse1`): zápis a čtení RAM, řídicí slovo nuluje PC, pulz 50 ms programuje jen 1 → 0, pulz
  0,5 ms ne, PD vzorkované během pulzu, 2732 v obou kabelech, bez +30 V nic, reset, základ 20h a zrcadla, MAP1,
  Emilova kopie EPROM → RAM, statistika (i pulzy po způsobu DSE.COM s 2716 a 2732), varování DSE.DOC u RAM
  s 2716 v patici, vložení téhož souboru, stav uprostřed pulzu, s jinou EPROM, s prázdnou paticí a bez souboru.

## Nejasnosti k ověření na HW

1. **Pořadí a polarita S1:** S1/1 = A6, S1/2 = A7, S1/3 = A5, S1/4 = A4, sepnuto = bit 0 (podle schématu). Emilův
   komentář pro 20h potvrzuje jen polaritu a S1/3.
2. **Co čte F0h, když vnitřní sběrnici nic nebudí** (prázdná patice, PC5 = PC7 = 0), a co čte F3h a F4–F7h
   (8286 budí sběrnici i tam). Emulace FFh.
3. **Jak se chová 2716 s Vpp 0 V a /CE dole.** Nastane bez +30 V a při S a R, kdy programy zapisují F4 = 00h a PC6 = 0.
   DSE.DOC i DSECPM.DOC: bez 30 V „nebude pracovat ani čtení“, DSE.DOC: při S a R nesmí být v programovacím
   konektoru EPROM. Nejspíš 2716 při Vpp 0 V zatěžuje PD. Emulace: 2716 při Vpp 0 V nebudí PD ani ho nezatěžuje,
   při práci s RAM s 2716 v patici (Vpp 0 V, /CE dole) zapíše jednou varování do logu.
4. **Nejkratší programovací pulz,** který skutečnou 2716 a 2732 naprogramuje. Emulace programuje od 1 ms
   (předpoklad). Programy používají 5–98 ms.
5. **Je JP1 (D5 → MAP1) z výroby osazená?** Tabulka X1 Tesla má kolík 21 MAP1 osazený, na fotce desky 6750/44328
   kolíček s propojkou v místě JP1 vidět není (drátová propojka v tom rozlišení vyloučit nejde). Emulace: osazená.
6. **RAM simulátoru po /RES:** porty 8255 jsou vstupy a W (PC4) plave v H do prvního řídicího slova. Zapíše se FFh
   někam do C00–FFFh? Emulace RAM po resetu nemění.
7. **Měnič DSE-1A:** jaké má zapojení a osazení (na překreslení EC1045 „????“) a co přepíná JP2 na desce 0917
   (fotky v `SAPI15\SimProg_opr\Fotky_měnič`)?
8. **Čte program Tesla po programování 2732 (P) první bajt správně?** Kontrola po programování zapíše F4 = 10h
   a první bajt čte asi za 50 µs. Vpp přitom klesá z 25 V s τ ≈ 0,2 ms, takže 2732 je ještě v režimu programování
   s vypnutým výstupem a první bajt by mohl vyjít „ROZDILNY“. 2716 je v té chvíli v Program Verify a čte správně.
   Emulace přepíná Vpp okamžitě.
9. **Vpp 24, nebo 25 V** (návod a DSECPM.DOC 24 V, AR 25 V), jak nastavit P2 pro 2732A (21 V).
10. **Doba programování 2 KB:** návod „cca 2,5 min“, program a Navrátilův článek asi 100 s.
11. **Co je SP1 „spínač TS 501 4141“** z rozpisky a kde je na desce?
12. **Barva LED1 a LED2 a vzhled krabičky** kabelu X2 Tesla. Okno programátoru kreslí LED1 (PROG) červeně a LED2
    (MAP) zeleně (zvolil autor 2026-10-31). Petrovy repliky a Emilův program mají naopak bit 4 zelený („simulátor“)
    a bit 5 červený („programování“). Krabička je nakreslená volně.
13. **Programovala DSE-1 i EPROM 2708?** Tesla vyráběla MHB2708C a MHB2716C, 2732 ne (autor). Kabel X2 je zapojený
    pro 2716 nebo 2732 a program Tesla zná jen tyto dvě. 2708 potřebuje navíc +12 V a −5 V a jiný způsob
    programování (katalog Intel: mnoho průchodů celou pamětí s krátkými pulzy). EPROM.COM J. Bíby typ 2708 nabízí,
    s jakým programátorem, nevíme. Emulace 2708 neumí (soubor 1 KB se odmítne).
