# MFC-1A – řadič minidisket (Mini Floppy Controller)

Stav: **emulovaná** (typ `MFC-1A`, `src/cards/mfc1a.*`, společná část s FDC-1 v `src/cards/fdc8272.*`, obvody
`src/chips/i8272.*`, `i8257.*`), ověřeno s CP/M ze ZRMD-1 (ECMON, varianta 2 „MFC8“, BIOS ECBIM8),
sestava `machines/cpm_zrmd_mfc1a.sapi`, test `test_fdc_ecmon`.

> **POZOR – důležité pro stavbu repliky:**
> 1. **Překreslené schéma EC1045 (`MFC-1A_MP.pdf`, 2010) se v registru 0FH liší od naskenovaného schématu
>    a návodu Tesla** ve dvou místech: bity D4 a D5 jsou na vstupech DD11 prohozené (reset 8272 / povolení
>    přerušení) a do dekodérů DD18, DD19 vede /Q místo Q obvodu DD23A (D6 = 1 mechaniky zapne místo vypne).
>    **Skutečná deska Tesla odpovídá překreslení:** EC1045 2026-10-07 ověřil na originální MFC-1A (v. č. 0124)
>    DD11 a bity D4, D5. S D6 = 1 počítá i veškerý software SAPI-1.5 a BIOS ECBIM8. Chybné je tedy schéma Tesla
>    (a `MFC1.ASM`, který podle něj vznikl).
> 2. **BIOS ECBIM8 má chybu:** v `INIT_MFC` chybí `RET`, takže při inicializaci zapíše do 0FH hodnotu FFh. Podle
>    obou schémat tím drží 8272 v resetu a čeká navěky. Oprava pro EC1045 (zdrojáky a obraz RAM disku s přepsanými
>    dvěma bajty na 583Dh a 3B832h, ověřená v emulátoru) je v `..\Dotazy\EC1045_2026-10-07\pro_Petra\`.
> 3. Emulace umí obě zapojení (`wiring = ec1045`, výchozí od 2026-10-07, nebo `tesla`) a reset přesně podle
>    schématu (`reset_mode = level`, výchozí). Sestava `cpm_zrmd_mfc1a.sapi` má `ec1045`, reset podle schématu a **RAM disk s opraveným BIOSem**
>    (`media/zrmd-1/SAPI_CPM_Martin_2019-10-07_fixMFC.bin`, zdroják `..\SAPI-karty\ZRMD-1\SRC`). Pro původní
>    obraz zůstává odchylka `reset_mode = pulse` (reset jen jako pulz), se kterou neopravený BIOS nabootuje.

## Podklady (ověřovat z více zdrojů)

- **`..\SAPI_DOKU\MFC-1A\`** (aktuální kopie 2026-10-05, nejúplnější):
  - `Snímek_004`–`027.jpg`: **celý návod Tesla Liberec** (TPTE 82-251/89). Strana návodu = číslo snímku − 3:
    programování str. 12–14 (**registr 0FH str. 14** = `Snímek_017`), obsah PROM str. 15, konektor XP1 str. 16,
    XP2 str. 17, připojení mechanik str. 18 (`Snímek_021`), dál rozložení součástek a rozpiska;
  - `Schema001`–`003.jpg`: **sken originálního schématu Tesla** (DD11, DD23, DD18, DD19 na `Schema002/003`);
  - `MFC1.ASM`: program „Práce s MFC-1A“ (19. 9. 2000, ORG 7000H, 5,25" DD 9 × 512 B, volá rutiny monitoru);
  - `ADRESOVY_DEKODER.BIN`, `EPROM_FAZOVY_ZAVES.BIN`, `MFC-1A.ZIP`, fotky desky a spojů, `FRB_MFC_PC*.xls`
    (redukce na konektor mechanik PC).
- **`..\SAPI-karty\MFC-1A\`** (autor): překreslení EC1045.01 rev. 1.0 (6. 6. 2010): `MFC-1A_MP.pdf`, `_cb.pdf`,
  `.png`, Eagle `.sch`/`.brd` (binární Eagle 4/5), `MFC-1A_8_4.sch` (2015), destičky `redukce*`, `soucastky.txt`,
  `doc\MFC_1A.xls`, návod jen do str. 13.
- Ovladač CP/M: BIOS EC1045 `ECBIM8.MAC` („OVLADAC MFC-1 V281211JB“, `..\SAPI-karty\FDC-1\sw\`), v systému na RAM
  disku ZRMD-1 jako varianta 2 „B,C: MFC8“ menu ECMON (ne v ROM).

## Návod (Tesla Liberec, TPTE 82-251/89)

- Deska mikropočítačového souboru **SAPI-1.5** (i SAPI-1), výroba Tesla Liberec, prodej Tesla Liberec a Tesla
  Eltos DIZ Praha, servis Tesla Eltos DIZ Písek.
- Až **4 mechaniky** 5,25" nebo 8" v programově volitelné kombinaci: 5,25" DS/DD 500 kB, SS/DD 250 kB, DS/SD
  250 kB, SS/SD 125 kB, 8" SS/SD 400 kB, DS/SD 800 kB (neformátovaná kapacita).
- Přenos DMA nebo programově řízený (přerušení), rychlost 250 kbit/s nebo 125 kbit/s.
- Obvody: 8272 (SM 609, DD10), 8257 (KR580VT57, DD5; ve schématu „8257 (8237-B)“), záchytný registr horní
  adresy DMA DD6 (74LS373, ADSTB), oddělovače DD7/DD8 (74LS245), dekodér adresy PROM DD9 (74S287), generátor
  hodin (krystal 8 MHz, DD14 74LS192: CLK 8 MHz pro SD a 4 MHz pro DD, přepíná se jen při HL = H; WCLK 500 kHz),
  datový separátor (stavový automat s PROM DD25, registrem DD24 a DD22), fázová prekompenzace (DD20 74LS175,
  multiplexer DD21 podle PS0, PS1), registr volby mechanik DD11 + DD23, dekodéry DD18, DD19 (74LS145) pro MO0–3
  a SEL0–3, budiče KC635 (VT2–VT8), vstupy tvarované 74LS14 (DD17).
- **Signály DS0, DS1 z 8272 nejsou využité**, mechaniku vybírá registr 0FH.
- Z kanálů 8257 se používá jen kanál 2, ostatní DRQ jsou uzemněné. HLDA je zpožděná o jednu periodu hodin
  (DD3A, B). Dekodér je při DMA blokovaný.
- **Obsazení adres (standardní PROM DD9):**

| Adresa | R/W | Význam |
|---|---|---|
| 04H | R/W | registr adresy kanálu 2 obvodu 8257 |
| 05H | R/W | registr TC kanálu 2 (TC15 TC14: 01 zápis do paměti, 10 čtení z paměti) |
| 08H | W | registr módu 8257 (D7, D4, D3, D1, D0 = 0; D6 TC stop, D5 prodloužený zápis, D2 kanál 2) |
| 08H | R | stavový registr 8257 (využívá se D2 = konec přenosu) |
| 0CH | R | hlavní stavový registr 8272 |
| 0DH | R/W | datový registr 8272 |
| 0FH | W | registr volby mechaniky |

- **Registr 0FH podle návodu (str. 14):** D7 nevyužit, D6 „povolení činnosti mechaniky“ (rukou dopsáno „40H“),
  D5 povolení přerušení od 8272, D4 reset 8272, D3 D2 číslo mechaniky pro signál MOTOR ON, D1 D0 číslo mechaniky
  pro signál SELECT. Polaritu D6 návod neuvádí (podle originálního schématu a `MFC1.ASM` je aktivní v nule).
- **Obsah PROM (str. 15):** tabulky DD9 a DD25 se shodují s dumpy `ADRESOVY_DEKODER.BIN` a `EPROM_FAZOVY_ZAVES.BIN`.
- **XP1** (sběrnice, klíč C6, vidlice TY 517 4311 08, str. 16): standardní sběrnice SAPI, tabulka je částečně
  doplněná rukou.
- **XP2** (mechaniky, klíč A5, vidlice TY 513 3011 30, str. 17): sudé kolíky 2 HLn, 4 STEPn, 6 SEL0n, 8 RDn,
  10 WEn, 12 WDn, 14 T00n, 16 WPn, 18 IDXn, 20 MO0n, 22 T43n, 24 SEL1n, 26 MO2n, 28 DIR, 30 MO3n; liché 1–17 a 29
  zem, 19 RDYn (vstup „jednotka připravena“), 21 MO1n, 23 HD1n, 25 SEL2n, 27 SEL3n. Zapojení sudé řady je shodné
  s výstupním konektorem X3 desky RPD-1B.
- **Připojení mechanik (str. 18):** 5,25" DS/DD jako systémová mechanika A (SEL0n, MO0n, HD1n) a 8" Consul 7115
  SS/SD jako přídavná mechanika D (SEL3n, T43n). Při využití HD1 (propojit 23-14) lze C 7115 použít pro DS/SD.
  **V žádném příkladu není zapojený RDYn (XP2-19).**
- Instalace: deska do libovolné pozice (kromě pozic RTI-1 a JPR-1x), kabel k mechanikám na **XP2**. Mechaniky
  zapínat před mikropočítačem, pokud nejsou napájené ze systémového zdroje.

## Registr volby mechaniky 0FH ve schématech

Zápis: PROM DD9 výstup O4 (vývod 9, REGN) je aktivní pro port 0FH; DD4D (74LS32, vývody 12 IOWN, 13 REGN,
výstup 11) dává WREG = /IOW OR /REG. WREG je hodiny DD11 (74LS174, vývod 9) a DD23A (74LS74, vývod 3), data se
zapíšou náběžnou hranou na konci zápisu. /RES (RESN) nuluje DD11. V překreslení je DD11 vpravo nahoře od 8272
(DD10), pod ním DD23A a DD23B, nad ním DD4B, DD12A pod 8272 vlevo, dekodéry DD18 a DD19 vpravo.

| Bit | Originál Tesla (`Schema002/003.jpg`, návod) | Překreslení EC1045 (`MFC-1A_MP.pdf`) |
|---|---|---|
| D0, D1 | DD11 vývody 3, 4 → Q1, Q2 → DD19 → SEL0n–SEL3n | stejně |
| D2, D3 | vývody 6, 11 → Q3, Q4 → DD18 → MO0n–MO3n | stejně |
| **D4** | vývod 13 → Q5 (12) → DD4B (OR s RES) → RES72 → **RESET 8272** (vývod 1) | vývod 14 → Q6 → DD12A → **povolení INT** |
| **D5** | vývod 14 → Q6 (15) → DD12A (AND s INT 8272) → TV1 → INT0n: **povolení INT** | vývod 13 → Q5 → DD4B → RES72 → **RESET 8272** |
| **D6** | DD23A **Q** → vstup C (vývod 4) DD18 i DD19: **D6 = 1 žádná mechanika ani motor** | DD23A **/Q** → dekodéry: **D6 = 1 mechaniky zapnuté** |
| D7 | nezapojený | nezapojený |

- **DD23B:** D (vývod 12) = RES72, hodiny RCLK (vývod 11), /Q (vývod 8) → CLR DD23A (vývod 1). Když je 8272
  v resetu, s nejbližším RCLK vynuluje DD23A. V originálu to znamená mechaniky zapnuté (Q = 0), v překreslení
  vypnuté (/Q = 1 → C = 1).
- 74145 se vstupem C = 1 vybere výstupy 4–7, které nejsou zapojené: žádná mechanika (DD19) ani motor (DD18).
- **Komu které zapojení odpovídá:**
  - Originál Tesla souhlasí s návodem (D4 reset, D5 INT) a s programem `MFC1.ASM`: ten pro práci zapisuje
    motor | mechanika s D6 = 0 a na konci `DRVOF .EQU 40H ;konec prace s radicem` („vypni jednotku“).
  - BIOS ECBIM8 má v komentářích D4/D5 podle návodu, ale pro práci nastavuje D6 = 1 (`SET 6,A`), tedy podle
    překreslení. Na originální desce by ECBIM8 nevybral žádnou mechaniku ani po opravě `RET`.

## Co dělá BIOS ECBIM8 při inicializaci (proč se zasekne)

BIOS CP/M (varianta 2 menu ECMON, zdroj `..\SAPI-karty\FDC-1\sw\ECBIM8.MAC`, na RAM disku ZRMD-1):

1. `BOOT`: `LOG_DISK` = 0 (disk A: = RAM disk ZRMD-1), vypíše hlavičku a zavolá `INIT_MFC`.
2. `INIT_MFC`: počká na RQM 8272 (`READY_BUSY`), pošle SPECIFY 03 7F 30. Pak je `LD C,0 ;disk 0` a **chybí
   `RET`**, takže pokračuje rovnou do `READ_MFC` (u FDC-1 v ECBIF8 je na tom místě `RET`).
3. `READ_MFC` → `SEL_DIS_MFC`: číslo disku = `LOG_DISK − 1` = 0 − 1 = **FFh** (počítá s B: = 1), `SET 6,A`, a
   `OUT (0FH),A` zapíše **FFh**: mechanika 3, motor 3, D4 = D5 = D6 = D7 = 1. Další smyčka `DJNZ` s B = 0
   proběhne 256× a čte bajty za tabulkou `STD_A` (druhá chyba).
4. V obou zapojeních je teď 8272 v resetu (D4 v originálu, D5 v překreslení). `RECALIBR`/`READY_BUSY` čte MSR
   (0CH) a čeká na RQM. Řadič v resetu RQM nedá (v emulátoru čte 00h), BIOS stojí navěky (E6CD–E6D2).
5. Při normální práci BIOS zapisuje 40h + číslo mechaniky (D4 = D5 = 0, D6 = 1).

**Oprava BIOSu:** doplnit `RET` za SPECIFY v `INIT_MFC` (jako u ECBIF8). Za SPECIFY stačí přepsat `0E 00`
(`LD C,0`) na `C9 00` (`RET`, `NOP`):
- v systémové oblasti obrazu RAM disku ZRMD-1 je to bajt na offsetu 583Dh (22589, blok varianty 2);
- druhá kopie (verze bez `_cd`) je v souboru `ECBIM8.BIN` na RAM disku (offset obrazu 243762).

Registr C po návratu nevadí: `INIT_MFC` volá jen `BOOT`, ten pak volá `MCHECK`, který C nebere (ECBIF8 vrací C =
30h stejně). Na originální desce Tesla by BIOS musel navíc pracovat s D6 = 0.

**Oprava je ve zdrojáku** `..\SAPI-karty\ZRMD-1\SRC\ECBIM8C.MAC` (`docs/software/ECMON.md`, překlad `build.py`).
**Ověřeno 2026-10-05:** systém přeložený s opravou nabootuje variantu 2 i s `wiring = ec1045`
a `reset_mode = level`, tedy s resetem 8272 přesně podle schématu, a `DIR B:` čte disketu. Odchylka `pulse` je
potřeba jen pro neopravený obraz.

## PROM DD9 (dekodér portů)

Dump `ADRESOVY_DEKODER.BIN` (v repu `roms/mfc-1a/MFC-1A_DD9_74S287.bin`), vstupy A0–A7, 4bitové výstupy:
04H, 05H, 08H = 0Ah, 0CH, 0DH = 09h, 0FH = 03h, jinde 0Fh. D0 = /CS 8257, D1 = /CS 8272, D2 = /SEL (deska
odpovídá, oddělovače), D3 = zápis registru 0FH. Na rozdíl od FDC-1 tedy 8257 jen na 04H, 05H a 08H.

PROM DD25 (fázový závěs, `MFC-1A_DD25_74S287_PLL.bin`) emulace nepotřebuje, je v repu jen pro úplnost.

## Emulace

- Stejný 8272 a 8257 jako u FDC-1 (`docs/desky/FDC-1.md`: povely, časování, přerušení, DMA a HOLD).
- 8272 vidí mechaniku vybranou registrem 0FH (D1 D0, zapnutou podle D6 a zapojení), bez ohledu na US v povelu.
- **Parametry:**
  - `wiring`: `ec1045` (výchozí od 2026-10-07: deska Tesla, jak se vyráběla, = překreslení EC1045) / `tesla`
    (naskenované schéma a návod, od desky se liší);
  - `reset_mode`: `level` (výchozí, schéma: bit resetu drží 8272 v resetu a DD23B vynuluje DD23A) / `pulse`
    (**odchylka**: náběžná hrana bitu jen resetuje 8272 a D6 zůstane, jak byl zapsán);
  - `rdy_ground`: výchozí ne = READY dává mechanika, když je v ní disketa (jako 5,25" mechaniky); ano = RDY
    trvale aktivní, i při vypnutých mechanikách (D6) (pak 8272 bez diskety čeká na index věčně a ECBIM8 by bez diskety v D nenabootoval);
  - `drive_a`–`drive_d`, `working_copy`, `decoder` (PROM DD9).
- **Sestava `cpm_zrmd_mfc1a.sapi`:** `wiring = ec1045`, `reset_mode = level`, RAM disk s opraveným ECBIM8
  (`..._fixMFC.bin`). Test `test_fdc_ecmon` ověřuje i původní obraz: s `level` se BIOS v obou zapojeních zasekne,
  s `pulse` nabootuje. Neopravená inicializace pak zkusí číst z jednotky 3, bez diskety v ní to po třech pokusech
  vzdá a CP/M naběhne.
- **Mechaniky:** 4, obrazy ImageDisk (`.imd`) nebo formát podle velikosti (`src/chips/floppy.h`): 8" SS SD (IBM 3740) a DD 720/360 KB
  (MFM 250 kbit/s, 512 B, dvě strany). Hodiny podle návodu: 8 MHz pro SD (FM), 4 MHz pro DD (MFM), takže
  rychlost je vždy 250 kbit/s a diskety HD (500 kbit/s) MFC-1A nepřečte. DD (MFM) i FM ověřily formátovací
  programy SAPIMANa `fd3m_512_18` (3,5" DD 2 × 9 × 512, strana 1 se sektory 10–18) a `fd5_256_16` (5,25" FM
  2 × 8 × 256), oba s kontrolou READ TRACK (2026-10-05). Jejich formáty uloží jen ImageDisk (`.imd`). CP/M pro
  MFC-1A s DD nemáme. **Motory:** s parametrem `motor_line` se točí jen mechanika, jejíž číslo je v D3 D2
  (MO0n–MO3n přes DD18, povolené jako výběr mechaniky bitem D6), rozběh `spinup_ms`. Výchozí je bez něj
  (8" Consul se točí stále), sestavy SAPIMANa ho nemají: BIOS 281211 pro 8" mechaniky nechává D3 D2 = 0 a
  s `motor_line` by nepřečetl B:. `mfc1test` (SAPI15) motor vybrané mechaniky zapíná.
- Okno Média ukazuje mechaniky 0–3 s písmenem v CP/M (parametr `cpm`, jinak zjištěné za chodu).

## Testy z reálného HW: `SAPI15\MFC_TEST_CPM` (2010)

V `..\SAPI_DOKU\SAPI15\MFC_TEST_CPM` je `mfc1test.asm` (CP/M verze `MFC1.ASM`, slovenské komentáře, assembler
AS, 15. 9. 2010) a výpisy `TEST_VYPIS.TXT`, `TEST_SHORT.TXT` (9. 9. 2010) z desek „Original MFC1A“, „repl1“,
„repl_n1“, „repl_n2“. Program seekne na zadanou stopu (bez SENSE), čte sektor 512 B (46h), pak vypíše MSR
a výsledky po SENSE INTERRUPT STATUS (ST0, PCN a zbytek výsledku READ), po READ ID (4Ah, jednotka 0) a po
RECALIBRATE. Mechaniky zapíná `DRVON EQU 40H` (D6 = 1), vypíná `DRVOF EQU 00H`.

- **Originál, mechanika A, stopa 20h, sektor 5:** `80 20 20 00 20 00 06 02` (SENSE: ST0 = 20h, PCN = 20h; zbytek READ:
  ST2 00, C 20, H 00, R 06, N 02 = sektor 512 B nenalezen), READ ID `80 00 00 00 20 00 02 01` (disketa má sektory
  256 B), po RECALIBRATE MSR 80.
- **Emulátor** (2026-10-05, `cpm_boot_mfc1a`, jednotka 1 s disketou DD 16 × 256 B, M80 s `ASEG` před `ORG 0100h`):
  s `rdy_ground = true` `80 21 20 00 20 00 06 02`, READ ID `80 00 00 00 20 00 01 01`, MSR 80. **Shoduje se**
  (R z READ ID záleží na poloze disku, na HW 02, 09, 01). Výchozí `rdy_ground = false` dá místo toho `C8`
  (READY changed, not ready, jednotka 0), protože READY sleduje D6 a program mechaniky vypíná a zapíná.
- **Co z toho plyne:**
  - 8272 na těch deskách nehlásil změnu READY při zapínání a vypínání mechanik. RDY byl tedy stálý (RDYn
    nezapojený?), což odpovídá `rdy_ground = true`.
  - Deska „Original MFC1A“ s tímto programem četla, tedy mechaniky zapnula s D6 = 1 jako překreslení EC1045.
    Pozor: zdroják je o 6 dní mladší než výpisy, `DRVON` mohl být při testu jiný.
  - Stav dokončeného SEEKu přežije READ a vyzvedne ho až SENSE (emulace stejně).

## SAPI-1.5 (ZPS-6)

Veškerý software SAPI-1.5 z roku 1990 (ROM BIOS PKT-1 MON-V100690, FORMAT.COM, SYSGEN) zapisuje do 0FH při práci
`40h | mechanika<<2 | mechanika` a po nečinnosti 00h, bity D4 a D5 nikdy. Počítá tedy s **D6 = 1 = mechaniky
zapnuté**, tedy se zapojením podle překreslení EC1045. Sestava `machines/zps/ZPS-6.sapi` má `wiring = ec1045`,
`rdy_ground`, `motor_line` a 5,25" DS DD disketu `.sp2` (40 × 2 × 16 × 256 B, `docs/software/SAPI-1.5.md`).
EC1045 to 2026-10-07 potvrdil na originální desce MFC-1A (v. č. 0124): zapojení odpovídá jeho překreslení.

## Nejasnosti k ověření na HW

- ~~Které zapojení mají vyrobené desky Tesla~~ **Vyřešeno 2026-10-07:** originální MFC-1A (v. č. 0124) odpovídá
  překreslení EC1045 (DD11, bity D4 a D5; D6 = 1 zapnuté podle veškerého softwaru). Sken schématu a návod se
  s deskou neshodují. Replika podle překreslení je správně.
  **EC1045 (2026-10-06):** J. Bíba rozchodil diskety na Petrově replice (měl ji zapůjčenou), systémy JB s D6 = 1
  tedy sedí na repliku. Petr sám mechaniky na MFC-1A nerozchodil (kabel pro RPD-1). Možná má i originální MFC-1A
  (sestava SAPI-1.5 8"), na které by šlo zapojení 0FH ověřit.
- Fungoval BIOS ECBIM8 s MFC-1A na skutečném stroji? (EC1045 na to neodpověděl, MFC-1A s mechanikami sám
  nerozchodil; nejspíš ho nikdo nezkoušel.) Podle obou schémat by se zasekl (chybí `RET` v `INIT_MFC`)
  a na originálu by navíc D6 = 1 mechaniky vypínal. Jaké MSR vrací 8272 v resetu?
- RDYn (XP2-19) není v příkladech připojení mechanik zapojený. Jakou úroveň pak vidí 8272 na RDY? Výpisy
  `SAPI15\MFC_TEST_CPM` (sekce výše) ukazují stálé RDY (`rdy_ground = true`). Sestavy SAPIMANa
  (`cpm_boot_mfc1a*`) ho mají od 2026-10-06 (rozhodnutí autora), `cpm_zrmd_mfc1a` ne (ECBIM8 čte z jednotky 3).
- Kdo dělal testy `SAPI15\MFC_TEST_CPM` (2010) a s jakým `DRVON` (40h, nebo 00h) běžel test na „Original MFC1A“?
- Které mechaniky se k MFC-1A u SAPI skutečně připojovaly (5,25" s READY, nebo 8" Consul)?
