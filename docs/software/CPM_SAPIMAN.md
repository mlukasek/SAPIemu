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

## Sestavy s bootem z 8" diskety (2026-10-05)

`machines/cpm_boot_mfc1a.sapi`, `cpm_boot_fdc1.sapi`, `cpm_boot_rpd1.sapi`: JPR-1ZI s EPROM J. Bíby, DMR-1,
AND-1Z-89, ZRMD-1 (C:), řadič s bootovací disketou v A: a datovou disketou CP/M 2.2/P v B:.

- **EPROM:** `mon281211` (MFC-1A), `mon130711` (FDC-1), `MON271111` (RPD-1) z `.hex` ve `ZMENA_CPM`. Zdrojáky
  (`mon*.txt`) se přeloží přes `cpmrun` na stejné bajty, liší se jen proměnné (DS). Monitor se jako ECMON kopíruje
  na F000h a nese hlavní část BIOSu (2 KB), 1 KB CBIOSu („BIOS-1K“, EC00h) se načte z diskety.
- **Ověřeno:** všechny tři nabootují do `A>` („JPR-1Z 62K MON CZ …JB“, „Uzivatel: J. Biba“), `DIR A:` a `DIR B:`
  čtou diskety, `SAVE` na C: funguje.
- **RAM disk ZRMD-1:** tyto systémy mají DPB s OFF 0 a DSM 1023 (CP/M přes celé 4 MB, adresář od 0), ECMON
  od roku 2015 má OFF 128 (prvních 32 KB systém). Proto mají vlastní prázdný obraz `ZRMD-1_prazdny_OFF0.bin`.
  S obrazem pro ECMON by `DIR C:` vypsal nesmysly.
- **MFC-1A** v sestavě: `wiring = ec1045`, `reset_mode = level`. Systém zapíná mechaniky D6 = 1; se zapojením
  podle originálu Tesla se boot zastaví (ověřeno).
- Klávesnice Consul 259.11 v kódu KOI-8 cs: monitory mají „OVLADAC KLAVESNICE CONSUL 259.11 KOI8 CS2“ a hlásí
  „CON: AND1Z + C259.11“.

## Sestava EC1045 s disketami PC HD (2026-10-05)

`machines/cpm_ec1045.sapi`: jako `cpm_zrmd_fdc1`, ale EPROM `JPR-1ZI_ECMON_2k_EC1045.bin` = `ECMON_cd.MAC` od
EC1045 beze změny (menu 0 FDC8, 1 FDC3, 2 FDC5, 3 HDD; přeloženo `build.py --set ec1045`, originální EPROM nemáme)
a RAM disk `SAPI_CPM_Martin_2019-10-07_sysEC1045.bin` (soubory z Martinova obrazu, systémová oblast EC1045
s BIOSy `ECBIF8_cd`, `ECBIF3_cd`, `ECBIF5_cd`, `ECBIHD_cd`).

- B: a C: jsou mechaniky A a B řadiče FDC-1 se SA2 = 8" (8 MHz, MFM 500 kbit/s). V mechanice A je prázdná
  3,5" HD (`media/fdc-1/prazdna_3.5HD_CPM.img`, E5h). Pro volbu 2 vložit `prazdna_5.25HD_CPM.img`.
- **Ověřeno (test `test_fdc_ecmon`):** volba 1 i 2 nabootuje, `SAVE` na B: zapíše soubor, `STAT` hlásí 1412K
  (3,5") nebo 1174K (5,25") volných, adresář je v obrazu na cylindru 1, hlavě 0, sektoru 1 (OFF 1).
- Obrazy jsou v PC formátu (sektory za sebou: cylindr, hlava, sektor 1–18 nebo 1–15), dají se připravit i na PC.
  Prázdnou disketu kteréhokoli formátu vyrobí okno Média (Nová disketa, výběr formátu).
- Klávesnice Consul 262.3 s úpravou 7474: BIOS hlásí „CON: AND1Z + C262.3“, ROM má ovladač Consul 259.11 KOI8 CS2.

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

1. ~~Sestavy bootující CP/M z 8" diskety SAPIMANa~~ (hotové, `cpm_boot_*.sapi`).
2. ~~3,5"/5,25" HD v PC formátu pro FDC-1 a sestava EC1045~~ (hotové, `cpm_ec1045.sapi`). MFC-1A umí DD.
