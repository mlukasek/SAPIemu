# CP/M J. Bíby (SAPIMAN) a EC1045 – přehled podkladů k disketám

Průzkum 2026-10-05 v `..\SAPI_DOKU` a `..\SAPI.CZ`, zaměřený na diskety pro FDC-1 a MFC-1A.

## Autoři

- **BI = Jiří (Jirka) Bíba**, přezdívka **SAPIMAN**. Udělal velký kus práce na CP/M pro SAPI-1: monitory
  JPR-1Z, BIOSy pro RPD-1, FDC-1, MFC-1A, RAM disky RMD-1, ZRMD-1 a IDE-1. Podpis „JB“ ve verzích
  (`V281211JB`), složka `SAPI_DOKU\SAPIMAN`. Dnes už se SAPI nevěnuje a zeptat se ho nedá.
- **EC = EC1045 = Petr Všijanský** (překreslování desek pro sapi.cz). Systém JB dál udržoval: verze „…EC“
  (`V100416EC`, `V280916EC`), podpora CD.COM a MOUNT, varianty pro 3,5" a 5,25". Dá se ho zeptat.
- Soubory `ECBI*.MAC` = BIOS (BI) ve verzi EC.

## Formáty disket v systémech JB/EC

| Mechanika | Formát | Kde |
|---|---|---|
| 8" SS SD | IBM 3740, 77 × 26 × 128 B, FM (obrazy `.img`/`.8sd`, 256 256 B) | FDC-1, RPD-1, MFC-1A, emulováno |
| 3,5" DS HD | **PC formát** 1,44 MB: 80 cylindrů × 2 strany × 18 × 512 B, MFM; DPB SPT 144, bloky 2 KB, DSM 710, DRM 255, OFF 1 | `ECBIF3*.MAC` (FDC-1A+1B), složky `…FDC 3 MFM 512X36 MOT PC…` |
| 5,25" DS HD | **PC formát** 1,2 MB: 80 × 2 × 15 × 512 B, MFM; DPB SPT 120, bloky 2 KB, DSM 591, DRM 255, OFF 1 | `ECBIF5*.MAC`, složky `…FDC 5 MFM 512X30 MOT PC…` |
| 5,25" DD | 9 × 512 B (360/720 KB), MFM | `SAPI_DOKU\MFC-1A\MFC1.ASM` (program „Práce s MFC-1A“, 2000) |

- Formáty 3,5" a 5,25" HD jsou PC-kompatibilní (stejná geometrie jako diskety PC). Prázdné nebo naplněné
  obrazy se dají vyrobit na PC: syrový obraz sektorů (1 474 560 nebo 1 228 800 B) se souborovým systémem CP/M
  podle DPB. Skutečné diskety ze SAPI k tomu nejsou potřeba.
- BIOSy F3/F5 ovládají i motory (`MOTOR`, časovače `TIM_Mx`), FDC-1 registr 0FH.

## Co v archivech je

- **Syrové obrazy 5,25" ani 3,5" disket ze SAPI se nenašly.** Obrazy `.img`/`.dsk`/`.imd` jsou jen 8" SS SD
  (MIKOS/D, CP/M ZPS-3/4, ELIS, TEXT-01) a cizí (Altair simulátor, Robotron 800 KB).
- Podle e-mailu EC1045 (`..\SAPI-karty\ZRMD-1\SRC\orig\EC1045_2016\info-z-emailu.txt`) jsou 5,25" diskety
  od J. Bíby vykopírované do obrazů PMD32-SD (složka `…/PETR/2MB/` na FTP SAPI_DOKU). V naší kopii SAPI_DOKU ta
  složka **není**. Zeptat se EC1045.
- **`SAPI_DOKU\SAPIMAN\PMD32-SD_IMAGE8\`**: 8" obrazy (`.8sd`) od SAPIMANa, roztříděné do složek EDITORY, FDC,
  IZOT, MFC, OPUS, PERINA, PREKLADACE, RPD, RUZNE, SUCHANEK, SYSPROG, TEXTY, ZDROJ_NOVE, ZDROJ_STARE. Mezi nimi:
  - `MFC\m1_281211_zrmd.8sd`, `m1_281211_rmd.8sd`: **bootovací 8" diskety CP/M pro MFC-1A** („BIOS-1K
    V281211JB“, A, B, D, E = MFC-1A, C = ZRMD-1 nebo RMD-1), k nim EPROM `mon281211` (2 KB BIOS v ROM);
  - `FDC\f1_*`, `f2_*`: bootovací diskety pro FDC-1 (`V130711JB`, různé RAM disky, PMD32-SD), `f_term62k/64k`;
  - `RPD\…`: totéž pro RPD-1.
- **`SAPI_DOKU\SAPIMAN\CPM_SYSTÉMY\ZMENA_CPM\FUNKCNI\`**: zdrojáky a výpisy těchto systémů po verzích
  (monitor `mon*.txt/.prn/.hex`, zavaděč, CCP, BDOS, BIOS, `IDENTIFIKACE.txt`), např. `JEN 8 PALCU\MFC\MFC281211_*`
  a `8 A 3,5 A 5,25\…` (RPD-1 + FDC-1 s 3,5"/5,25", 2012).
- **`SAPI_DOKU\SAPIMAN\CPM_PROGRAMY\CISLOVANI SEKTORU\CIS_FDC_MFC_DE\`**: programy pro FDC-1/MFC-1A na formátování
  a čtení/zápis 3,5", 5,25" a 8" disket ve FM i MFM s různými délkami sektorů (`fd5M_512_8`, `fd3M_512_9`,
  `RW…`). Dále `FORMATOVANI`, `Format_FDC_8`, `DUP_fyz_ kopie_8_disket`.

## Původ chyby ECBIM8

Monitor `mon281211` (MFC-1A, 8" boot, 2011) má v `INIT_MFC` za SPECIFY také `LD C,0 ;disk 0` bez `RET`. Tam je
to **záměr**: inicializace propadne do `.SELECT` a vybere disk 0 (40h, D6 = 1), ze kterého se pak bootuje.
V ECBIM8 (systém na RAM disku) za tímto místem následuje `READ_MFC` a disk je LOG_DISK − 1, odtud zápis FFh.
Chyba tedy vznikla přenesením bootovacího kódu do BIOSu pro RAM disk. Systémy JB pro MFC-1A shodně zapínají
mechaniky D6 = 1, tedy podle překreslení EC1045, ne podle originálního schématu Tesla (`docs/desky/MFC-1A.md`).

## Otázky na EC1045 (Petr Všijanský)

- Složka `…/PETR/2MB/` z FTP SAPI_DOKU (5,25" diskety J. Bíby vykopírované do obrazů PMD32-SD): v naší kopii
  SAPI_DOKU chybí. Má ji?
- Zdroják ROM `JPR-1ZI_ECMON_2k_IDE-1_Martin.bin` (ECMON pro IDE-1, 2021): zdroják nemáme.
- Případně: měl MFC-1A zapojený podle originálu Tesla, nebo jako jeho překreslení (D4/D5, polarita D6)?

## Plán (schválený autorem 2026-10-05)

1. Sestavy bootující CP/M z 8" diskety SAPIMANa: MFC-1A (`m1_281211_*`, EPROM `mon281211`), pak FDC-1
   (`f1_*`, `mon130711`) a RPD-1 (`r1_*`). EPROM přeložit ze zdrojáků (`cpmrun`) a ověřit proti `.hex`.
2. 3,5"/5,25" HD v PC formátu (MFM, 2 strany, 512 B) pro FDC-1 a MFC-1A a sestava EC1045 s původním ECMON_cd
   (menu FDC8/FDC3/FDC5/HDD) a BIOSy `ECBIF3/F5` ze zdrojáků.
