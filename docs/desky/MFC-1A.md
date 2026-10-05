# MFC-1A – řadič minidisket (Mini Floppy Controller)

Stav: **emulovaná** (typ `MFC-1A`, `src/cards/mfc1a.*`, společná část s FDC-1 v `src/cards/fdc8272.*`, obvody
`src/chips/i8272.*`, `i8257.*`), ověřeno s CP/M ze ZRMD-1 (ECMON, varianta 2 „MFC8“, BIOS ECBIM8),
sestava `machines/cpm_zrmd_mfc1a.sapi`, test `test_fdc_ecmon`.

> **POZOR – odchylka emulace od schématu (důležité pro stavbu repliky):** bit **D5** registru 0FH („reset
> 8272“) podle schématu drží 8272 v resetu, dokud je 1 (DD11 Q5 → DD4B OR s RES → RES72 → RESET 8272). BIOS
> ECBIM8 ale při inicializaci zapíše do 0FH hodnotu FFh (rozbor níže) a s tím by čekal navěky. Emulace proto ve
> výchozím nastavení (`reset_d5 = "pulse"`) D5 bere jen jako **pulz na náběžné hraně** a mechaniku nechá vybranou,
> aby varianta 2 nabootovala. Chování podle schématu je `reset_d5 = "level"` (test ověřuje, že se pak BIOS
> zasekne). Autor neví, jestli ECBIM8 s MFC-1A na skutečném stroji fungoval. Pro repliku doporučení: opravit
> BIOS (chybějící RET, viz níže), zapojení desky neměnit.

## Podklady

- `..\SAPI-karty\MFC-1A\` (autor):
  - `MFC-1A_MP.pdf`, `MFC-1A_MP_cb.pdf`, `MFC-1A_MP.png`: schéma rev. 1.0 překreslené EC1045.01 (6. 6. 2010),
    Eagle `.sch`, `.brd` (binární Eagle 4/5), `MFC-1A_8_4.sch` (2015), destičky `redukce*` (redukce konektorů,
    `doc\FRB_MFC_PC*.xls`: zapojení FRB na konektor mechanik PC);
  - `soucastky.txt` (rozpiska Eagle), `doc\MFC_1A.xls`;
  - `doc\ADRESOVY_DEKODER.BIN` (PROM DD9) a `doc\EPROM_FAZOVY_ZAVES.BIN` (PROM DD25, číslicový fázový závěs
    datového separátoru);
  - `doc\Snímek_004`–`016.jpg`: návod Tesla Liberec (obsah, základní údaje, blokové schéma, technické a provozní
    údaje, popis funkce, instalace, programování), fotky desky a schémat `Schema001–003.jpg`.
- Ovladač: BIOS EC1045 `ECBIM8.MAC` („OVLADAC MFC-1 V281211JB“, `..\SAPI-karty\FDC-1\sw\`), v systému na RAM disku
  ZRMD-1 jako varianta 2 „B,C: MFC8“ menu ECMON.

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
- **Signály DS0, DS1 z 8272 nejsou využité**, mechaniku vybírá registr.
- Z kanálů 8257 se používá jen kanál 2, ostatní DRQ jsou uzemněné. HLDA je zpožděná o jednu periodu hodin
  (DD3A, B). Dekodér je při DMA blokovaný.
- **Obsazení adres (standardní PROM):**

| Adresa | R/W | Význam |
|---|---|---|
| 04H | R/W | registr adresy kanálu 2 obvodu 8257 |
| 05H | R/W | registr TC kanálu 2 (TC15 TC14: 01 zápis do paměti, 10 čtení z paměti) |
| 08H | W | registr módu 8257 (D7, D4, D3, D1, D0 = 0; D6 TC stop, D5 prodloužený zápis, D2 kanál 2) |
| 08H | R | stavový registr 8257 (využívá se D2 = konec přenosu) |
| 0CH | R | hlavní stavový registr 8272 |
| 0DH | R/W | datový registr 8272 |
| 0FH | W | registr volby mechaniky |

- Instalace: deska do libovolné pozice (kromě pozic RTI-1 a JPR-1x), kabel k mechanikám na **XP2**. Mechaniky
  zapínat před mikropočítačem, pokud nejsou napájené ze systémového zdroje.

## Registr volby mechaniky 0FH (podle schématu `MFC-1A_MP.pdf`)

Ve schématu je vpravo nahoře od 8272 (DD10): DD11 (74LS174), pod ním DD23A a DD23B (74LS74), nad ním DD4B
(74LS32), DD12A (74LS08) je pod 8272 vlevo, DD4D (74LS32) pod ním, dekodéry DD18 a DD19 (74LS145) vpravo.

- **Zápis:** PROM DD9 výstup O4 (vývod 9, REGN) je aktivní pro port 0FH. DD4D (vývody 12 IOWN, 13 REGN, výstup 11
  WREG) dává WREG = /IOW OR /REG. WREG je hodiny DD11 (vývod 9) a DD23A (vývod 3), data se zapíšou náběžnou
  hranou na konci zápisu. DD11 nuluje /RES (RESN, vývod 1).
- **Vstupy DD11:** D0 → vývod 3 (D1), D1 → 4 (D2), D2 → 6 (D3), D3 → 11 (D4), **D5 → 13 (D5), D4 → 14 (D6)**
  (bity 4 a 5 jsou na vstupech prohozené).

| Bit | Obvod | Význam |
|---|---|---|
| D0, D1 | DD11 Q1, Q2 (vývody 2, 5) → DD19 | vybraná mechanika /SEL0–/SEL3; její READY, INDEX, TRACK 0, WP a data vidí 8272 |
| D2, D3 | DD11 Q3, Q4 (vývody 7, 10) → DD18 | mechanika, které běží motor /MO0–/MO3 |
| D4 | DD11 Q6 (vývod 15) → DD12A vývod 1 | povolení přerušení: INT (DD12A vývod 3 → TV1 → /INT0) = INT 8272 (vývod 18) AND Q6 |
| D5 | DD11 Q5 (vývod 12) → DD4B vývod 4 | **reset 8272:** RES72 (DD4B vývod 6) = Q5 OR RES → RESET 8272 (vývod 1). Drží, dokud je 1 |
| D6 | DD23A (D vývod 2, Q, /Q) → DD19 | povolení mechanik; bez něj DD19 nevybere žádnou |
| D7 | – | nezapojený |

- **DD23B:** D (vývod 12) = RES72, hodiny RCLK (vývod 11), /Q (vývod 8) → CLR DD23A (vývod 1). Když je 8272
  v resetu, s nejbližším RCLK se vynuluje DD23A, tedy zruší se výběr mechaniky (D6).
- Komentáře v BIOSu ECBIM8 mají bity 4 a 5 prohozené („bit 5 povolení přerušení, bit 4 reset 8272“). Emulace se
  řídí schématem.

## Co dělá BIOS ECBIM8 při inicializaci (proč se zasekne)

BIOS CP/M (varianta 2 menu ECMON, zdroj `..\SAPI-karty\FDC-1\sw\ECBIM8.MAC`):

1. `BOOT`: `LOG_DISK` = 0 (disk A: = RAM disk ZRMD-1), vypíše hlavičku a zavolá `INIT_MFC`.
2. `INIT_MFC`: počká na RQM 8272 (`READY_BUSY`), pošle SPECIFY 03 7F 30. Pak je `LD C,0 ;disk 0` a **chybí
   `RET`**, takže pokračuje rovnou do `READ_MFC` (u FDC-1 v ECBIF8 je na tom místě `RET`).
3. `READ_MFC` → `SEL_DIS_MFC`: číslo disku = `LOG_DISK − 1` = 0 − 1 = **FFh** (počítá s B: = 1), `SET 6,A`, a
   `OUT (0FH),A` zapíše **FFh**: vybraná mechanika 3, motor 3, **D4 = 1 (přerušení), D5 = 1 (reset 8272)**, D6 = 1.
   Další smyčka `DJNZ` s B = 0 proběhne 256× a čte bajt za tabulkou `STD_A` (druhá chyba).
4. Podle schématu je teď 8272 v resetu (a DD23B zruší výběr mechaniky). `RECALIBR`/`READY_BUSY` čte MSR (0CH) a
   čeká na RQM. Řadič v resetu RQM nedá (v emulátoru čte 00h), BIOS stojí navěky (E6CD–E6D2).
5. Při normální práci BIOS zapisuje 40h + číslo mechaniky (D4 = D5 = 0), to je v pořádku.

**Oprava BIOSu:** doplnit `RET` za SPECIFY v `INIT_MFC` (jako u ECBIF8). Pak FFh nikdy nezapíše a deska podle
schématu funguje. Systém je na RAM disku ZRMD-1 (varianta 2), oprava znamená přeložit BIOS a zapsat ho na RAM disk.

**Emulace (`reset_d5 = "pulse"`):** náběžná hrana D5 jen resetuje 8272 a mechanika zůstane vybraná. Inicializace
pak zkusí číst z jednotky 3, bez diskety v ní (READY z mechaniky) to po třech pokusech vzdá a CP/M naběhne.

## PROM DD9 (dekodér portů)

Dump `ADRESOVY_DEKODER.BIN` (v repu `roms/mfc-1a/MFC-1A_DD9_74S287.bin`), vstupy A0–A7, 4bitové výstupy:
04H, 05H, 08H = 0Ah, 0CH, 0DH = 09h, 0FH = 03h, jinde 0Fh. D0 = /CS 8257, D1 = /CS 8272, D2 = /SEL (deska
odpovídá, oddělovače), D3 = zápis registru 0FH. Na rozdíl od FDC-1 tedy 8257 jen na 04H, 05H a 08H.

PROM DD25 (fázový závěs, `MFC-1A_DD25_74S287_PLL.bin`) emulace nepotřebuje, je v repu jen pro úplnost.

## Emulace

- Stejný 8272 a 8257 jako u FDC-1 (`docs/desky/FDC-1.md`: povely, časování, přerušení, DMA a HOLD).
- 8272 vidí mechaniku vybranou registrem 0FH (D6 a D1 D0), bez ohledu na US v povelu.
- **READY:** parametr `rdy_ground`. Výchozí je ne: READY dává mechanika, když je v ní disketa (jako 5,25"
  mechaniky). Ano = X2-19 uzemněný (8" Consul 7113 READY nemají); pak 8272 bez diskety čeká na index věčně, a
  protože ECBIM8 při inicializaci čte z jednotky 3, sestava by bez diskety v D nenabootovala.
- **Mechaniky:** 4 × 8" SS SD (IBM 3740, obrazy `.img` jako u RPD-1). Motory (D3 D2) a 5,25" mechaniky ani dvojitá
  hustota (MFM, hodiny 4 MHz) se zatím neemulují.
- Parametry: `drive_a`–`drive_d`, `working_copy`, `decoder` (PROM DD9), `rdy_ground`, `reset_d5` (`pulse` / `level`).
- Okno Média ukazuje mechaniky A–D.

## Nejasnosti k ověření na HW

- **D5 registru 0FH:** fungoval BIOS ECBIM8 s MFC-1A na skutečném stroji? Podle schématu by se zasekl (chybí
  `RET` v `INIT_MFC`). Při stavbě repliky ověřit, jaké MSR 8272 vrací v resetu.
- Které mechaniky se k MFC-1A připojovaly (5,25" s READY, nebo 8" Consul s uzemněným RDY X2-19)?
- Strany návodu za „Programováním“ (obsah PROM, vstupy a výstupy, připojení mechanik, rozpiska) v podkladech
  chybí.
