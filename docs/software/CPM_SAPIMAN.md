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

## Dodělávky (2026-10-05, autor: „jsou tam dobré věci“)

1. **Ověřit diskety originálním softwarem:** programy SAPIMANa v `SAPI_DOKU\SAPIMAN\CPM_PROGRAMY\`:
   `CISLOVANI SEKTORU\CIS_FDC_MFC_DE` (formátování a čtení/zápis 3,5", 5,25" a 8" ve FM i MFM, sektory 128–1024 B,
   pro FDC-1 i MFC-1A), `FORMATOVANI`, `Format_FDC_8`, `DUP_fyz_ kopie_8_disket`; program `SAPI_DOKU\MFC-1A\MFC1.ASM`
   (5,25" DD na MFC-1A). Prověří MFM, DD na MFC-1A (zatím neověřené žádným softwarem), FORMAT a READ ID.
   **Většinou hotové 2026-10-05** (sekce „Formátovací programy SAPIMANa“): FORMAT a READ TRACK na FDC-1 (HD, 8")
   a MFC-1A (DD, FM, 8"), číslování sektorů 8". READ ID programy nepoužívají. Místo `MFC1.ASM` (monitor) jeho
   CP/M verze `SAPI15\MFC_TEST_CPM\mfc1test.asm` s výpisy z reálných desek: emulace se shoduje
   (`docs/desky/MFC-1A.md`). ~~`DUP_fyz_ kopie`~~ hotové 2026-10-06 (níže). Sestavy SAPIMANa s MFC-1A mají
   od 2026-10-06 `rdy_ground = true`. **Bod 1 je tím celý hotový.**
2. ~~**Motory mechanik**~~ hotové 2026-10-06: parametr `motor_line` a `spinup_ms` u FDC-1 a MFC-1A, zapnutý
   v sestavách s mechanikami PC (EC1045, dlouhé CP/M). `docs/desky/FDC-1.md`, `docs/desky/MFC-1A.md`.
3. **Další sestavy a diskety SAPIMANa:** ~~bootovací diskety s RAM diskem RMD-1 a varianty s PMD32-SD~~ hotové
   2026-10-06 (`cpm_boot_fdc1_rmd`, `cpm_boot_rpd1_rmd`, `cpm_pmd_fdc1`; `r2_pmd*` už je `cpm_pmd_rpd1`).
   ~~Knihovna datových disket~~ hotová 2026-10-06: soupis všech asi 180 disket a výběr 16 do
   `media/sapiman/knihovna/` (`docs/software/SAPIMAN_diskety.md`; MBASIC i Turbo Pascal 3.00A pro AND-1Z běží).
   ~~Bootovací diskety RPD `r2_*`, `r3_*`~~ vyzkoušené 2026-10-06: všechny naběhnou s MON271111, sestavy
   `cpm_boot_rpd1_mfc256`, `cpm_boot_rpd1_mfc512` a `cpm_boot_rpd1_fdc1_rmd` (níže). ~~Zbývající bootovací
   diskety~~ vyzkoušené 2026-10-06, **všechny bootovací diskety ze `SAPIMAN\PMD32-SD_IMAGE8` jsou tím prošlé**
   (výsledky v sekci „Zbylé bootovací diskety“ níže).
4. ~~**`cpmrun`:** malý test v ctestu, L80 a `/P:0` z příkazové řádky~~ hotové 2026-10-06: `test_cpmrun`;
   `/P:0` rozbíjel Git Bash (převod cest), L80 se pak zacyklil po odpovědi Y na „move anyway“ (správně N), stav
   konzole teď hlásí stdin (`docs/build.md`).
5. ~~**Okno Média:** ověřit vzhled výběru formátu nové diskety~~ hotové 2026-10-05: výběr je jednou nad
   mechanikami řadiče, snímek `sapiemu --ui-snapshot X.png --ui-media`.
6. **Otázky na EC1045** (sekce výše): `PETR/2MB`, zdroják ECMON IDE-1, zapojení jeho MFC-1A.
7. **Pracovní kopie autora** `work/zrmd-1/SAPI_CPM_Martin_2019-10-07.bin`: oprava `RET` v ECBIM8 (bajt 583Dh
   0Eh → C9h) jen se souhlasem autora; sestava `cpm_zrmd_mfc1a` má zatím vlastní opravený obraz `_fixMFC`.

## Původ chyby ECBIM8

Monitor `mon281211` (MFC-1A, 8" boot, 2011) má v `INIT_MFC` za SPECIFY také `LD C,0 ;disk 0` bez `RET`. Tam je
to **záměr**: inicializace propadne do `.SELECT` a vybere disk 0 (40h, D6 = 1), ze kterého se pak bootuje.
V ECBIM8 (systém na RAM disku) za tímto místem následuje `READ_MFC` a disk je LOG_DISK − 1, odtud zápis FFh.
Chyba tedy vznikla přenesením bootovacího kódu do BIOSu pro RAM disk. Systémy JB pro MFC-1A shodně zapínají
mechaniky D6 = 1, tedy podle překreslení EC1045, ne podle originálního schématu Tesla (`docs/desky/MFC-1A.md`).

## SAPI_DOKU z FTP (`..\SAPI_DOKU_FTP`, stažené 2026-10-05)

Autor stáhl SAPI_DOKU i z FTP; liší se od kopie z Dropboxu (`..\SAPI_DOKU`). Obsahem (MD5) je v ní navíc 4995
souborů, z toho 4793 v **`ZALOHY_8SD_EC1045.01\nove_a_vse_co_mám\`** (archiv EC1045 z jeho e-mailu):

- **`PETR\SAPI_SYS\`**: systémové 8" diskety J. Bíby Z1–Z10 a 1–10 se seznamem `Zaloha_sys_disket_sapi.txt`:
  RPD 271111 (Z1 programy, Z2 zdroje), **RPD 3K 050412 + FDC-1 5,25" DS/HD 512 × 30 PC formát** (Z3, Z9), FDC 130711
  (Z4, Z10), MFC 281211 s RMD-1 (Z5 zdroje, Z6 programy), RPD 170212 + PMD32-SD (Z7), **FDC 64K PC-MONITOR**
  (RAM 64K, RMD-1, Z8). Zdroje systémů včetně `.MAC`, `.PRN`, `.HEX`.
- **`PETR\2MB\`**: 5,25" diskety J. Bíby vykopírované do obrazů PMD32-SD (`1613`–`1619.2MB`), použitelné
  rovnou jako disk D: PMD32-SD.
- **`PETR\CPM\`** (93 obrazů `.8SD`), `TEXT01`, `TNS-2_32`, `C2713`, `C271X`.
- **`!!!!_TRIDENO_!!!!\`**: roztříděné originály CPM64K, ELIS, PERINA (CP/M 2.2/P a jeho úpravy), ZPS3, OPV663,
  V040386ZF (obrazy `.8SD`/`.IMG` a vybalené soubory).
- **`OBRAZY_IMD\`**: 890 obrazů ImageDisk (`.imd`, záznam po stopách včetně FM/MFM a délky sektorů):
  - `JILEK\04`: **5,25" diskety R. Peřiny a J. Jílka (1990–91)**, DD 40 × 2 × 9 × 512 (360 KB) i HD 80 × 2 × 15 × 512
    (1,2 MB PC), např. „CPM MONITOR V5.F/Z zdroj“, „Perina 4/91 zsid, l80, m80“; `JILEK\01–03` 8" MIKOS/D, ISIS, CP/M;
  - `BILY\40A/40B`: 8" DS DD 77 × 2 × 15 × 512 („archyv zdrojaku“); `STRIZEK`, `DEX`, `8DD`: 8" MFM 26 × 256;
  - `MELSOFT`: 8" FM 9 × 512 (CP/X); `PC`: diskety PC; `C2713`, `C2715`, `IPDS`, `HP42` …: jiné počítače;
  - kořen: `POKUS5A1.IMD` aj. „5.25" ala 8"“ (FM stopa 0, MFM 80 × 2 × 15 × 512).
- Dále: `SAPI15\MELSOFT_DISKETY\SAPI15_*.sp2` (327 680 B), `MS-80\A3` (skeny řadiče FDD SM 80-341),
  `Jarda\STD-BUS`, `PS-Mikro`, `skeny_dokumentace_bez_retusi_Petr` (BT100, SP210T, KP311), `_SAPIDOKU_Archive`
  (celé SAPI_DOKU z 2020-01-01, zip 1,6 GB).

**Plán (autor schválil 2026-10-05):** 1) ~~čtení **ImageDisk** v mechanikách~~ (hotové: RPD-1, FDC-1, MFC-1A
čtou i zapisují `.imd`; MIKOS/D z `JILEK\01\01.IMD` nabootuje na RPD-1; `JILEK\04` jsou ve skutečnosti diskety
MS-DOS 1,2 MB (FAT12, složky `CPM`, `MONV5_FZ`), na kterých Peřina a Jílek přenášeli soubory SAPI přes PC),
2) ~~sestavy podle `SAPI_SYS`~~ (hotové, níže),
3) `PETR\2MB` pro PMD32-SD. Slučování obou kopií SAPI_DOKU dělá autor, zatím neřešit.

**Pro emulátor z toho plyne:** čtení obrazů **ImageDisk** (`.imd`) přímo v mechanice (libovolná geometrie, smíšené
FM/MFM, 8" DD, DS, 9 × 512) by zpřístupnilo stovky disket; 5,25" diskety z `JILEK\04` a sestavy podle `SAPI_SYS`
(RPD + FDC 5,25" HD, FDC 64K) jsou materiál pro další sestavy; `PETR\2MB` pro PMD32-SD.

## Sestavy ze systémových disket SAPI_SYS (2026-10-05)

Diskety Z1–Z10 jsou v `media/sapiman/Z*.img` (disky 1–10 jsou totožné). Bootovací: Z1/Z2 (RPD 271111), Z4/Z10
(FDC 130711), Z5/Z6 (MFC 281211 s RMD-1), Z8 (CP/M 64K s terminálem); Z3, Z7, Z9 jsou jen zdroje (RPD 3K 050412 +
FDC-1 5,25" HD, RPD 170212 + PMD32-SD).

**„Dlouhé“ CP/M** (verze 26, CPM050412, 3K BIOS; `cpm_boot_dlouhe_rpd1_fdc1.sapi`): bootuje z 8" diskety na RPD-1
s EPROM 260112, C: = ZRMD-1 SS256, D: a E: = FDC-1 5,25" DS HD 512 × 30 v PC formátu (varianta pro 3,5" 512 × 36 je
ve vedlejší složce `ZMENA_CPM`). Bootovací disketu jsme neměli, a tak je **poskládaná ze zdrojáků**
(`ZMENA_CPM\FUNKCNI\8 A 3,5 A 5,25\ZRMD256\3K RPD FDC 5 MFM 512X30 MOT PC ZRMD256_050412`, M80/L80 pod `cpmrun`):
- zavaděč `bo2608CE` (stopa 0, sektor 1, EPROM ho načte na 0080h) čte stopu 0 sektory 2–26 a stopu 1 sektory 1–26
  souvisle od CE00h (CCP `ccpCE`, BDOS `bdosD6` na D600h, BIOS `bi0504E4_5_512_30_PC_mo_s` na E400h) a konec
  BIOSu ze 6 sektorů stopy 76 (12h, 18h, 04h, 0Ah, 10h, 16h = posledních 6 logických sektorů se skew 6, které DPB
  s DSM 242 nikdy nepřidělí) na E780h; 3K BIOS se tak vejde i na standardní disketu (proto „dlouhý“);
- základ je disketa Z9 (programy tohoto systému bez systémových stop), B: = Z3 (zdroje);
- BIOS a zavaděč z překladu souhlasí s původními výpisy `.PRN` bajt po bajtu; EPROM je z `MON260112MO.HEX`
  (zdroják `.txt` je o 3 bajty novější).
- Ověřeno: boot, `DIR B:`, `SAVE` a `STAT` na D: (1174K); test `test_sapi_sys`.
- Varianta pro 3,5" HD 512 × 36: `cpm_boot_dlouhe_rpd1_fdc1_35.sapi`, disketa `Z9_…_boot_3.5HD.img` (jiný jen BIOS
  `bi0504E4_3_512_36_PC_mo_s`, také shodný s výpisem), `STAT D:` 1412K.

- `cpm_boot_rpd1_ss256.sapi`, `cpm_boot_fdc1_ss256.sapi`: A: programy (bootovací), B: zdroje. **C: = RAM disk ZRMD-1
  v rozložení SS256** (DPB OFF 128 jako ECMON), tedy stejný obraz jako `cpm_zrmd` (`DIR C:` vypíše tvoje soubory).
  Výzva CCP je `U0A>` (číslo uživatele).
- `cpm_boot_mfc1a_rmd.sapi`: MFC-1A s RAM diskem RMD-1 1 MB na 40h (náhodný obsah po zapnutí jako DRAM;
  `IRMD40` ho inicializuje, ptá se „Pokracovat? (A/N)“).
- `cpm_boot_fdc1_rmd.sapi`, `cpm_boot_rpd1_rmd.sapi` (2026-10-06): bootovací diskety `FDC\f1_rmd1m_130711.8sd`
  a `RPD\r1_rmd1m_271111.8sd` ze `SAPIMAN\PMD32-SD_IMAGE8` („C: RMD1/1MB BASE40H“). Inicializační program na nich
  není, proto je v B: disketa Z6 a C: se připraví `B:IRMD40` (odpověď A). Bootovací disketa MFC
  `m1_281211_rmd.8sd` je v `media/sapiman` jako volba (stejný systém jako Z6).
- **Systémy RPD-1 s druhým řadičem** (2026-10-06, `RPD\r2_*`, `r3_*`, všechny bootují s EPROM MON271111, A:, B: =
  RPD-1, C: = RAM disk, D:, E: = druhý řadič):
  - `r2_mfc256x14_261211` („BIOS-2K V261211JB“, „D,E: MFC1; 8" 14x256“) a `r3_mfc512x8_261211` („BIOS-3K“,
    „8" 8x512“): MFC-1A s 8" disketami **ve FM s většími sektory** (IBM 256 × 14 a 512 × 8 na stopu, ne DD;
    MFC-1A je čte 250 kbit/s jako 8" SD). Sestavy `cpm_boot_rpd1_mfc256.sapi` a `cpm_boot_rpd1_mfc512.sapi`,
    prázdné diskety `media/sapiman/prazdna_8_FM_14x256.imd` a `prazdna_8_FM_8x512.imd` (ImageDisk);
  - `r2_rmd1m_fdc_261211` („BIOS-2K V260811JB“, D:, E: = FDC-1 8" SS SD; na disketě IRMD40, FORMFDC, M80, L80):
    sestava `cpm_boot_rpd1_fdc1_rmd.sapi`, prázdná 8" `media/fdc-1/prazdna_8SSSD.img`;
  - další varianty bootují také (`r2_rmd1m_mfc_261211`: MFC-1A 8" SS SD; `r2_rmd1_fdc_260811`,
    `r3_rmd1_fdc_260811`: FDC-1 8"; `r2_/r3_zrmd4mss_fdc_260811`: s ZRMD-1 SS256), sestavy z nich nejsou;
  - ve všech je v B: disketa Z6 kvůli `IRMD40` (C: je po zapnutí náhodná). Test `test_sapi_sys` (`rpd_second`).
- **Zbylé bootovací diskety** (2026-10-06, sestava jen z té, která se liší něčím zajímavým):
  - `FDC\f2_irmd1m_130711` („BIOS-2K V130711JB“, BIOS `BIRMD`): **RAM disk si připraví BIOS sám**. Při startu
    hledá v první položce adresáře skrytou značku „RAMDISK.RMD“, když chybí, zjistí velikost RMD-1 (256 KB nebo
    1 MB) a zapíše prázdný adresář. C: je pak hned použitelný bez IRMD40 (STAT: 253K, nebo 1018K). Sestava
    `cpm_boot_fdc1_irmd.sapi` (RMD-1 256 KB, B: = Z9 se STAT), test `irmd` v `test_sapi_sys`;
  - `FDC\f2_rmd1m_130711`, `f3_rmd1m_130711` (BIOS 2K a 3K, CCP D200h a CE00h): naběhnou s MON130711, jinak
    jako `cpm_boot_fdc1_rmd`, sestavy ne;
  - `FDC\f1_rmd1m_and1a_130711`: naběhne s AND-1Z jako `f1_rmd1m`, na disketě jsou jen zdrojáky monitoru
    a BIOSu pro AND-1A (MOAND, BIAND), sestava ne;
  - `FDC\f_term62k`: systémové stopy jsou totožné se Z8 (`cpm_term64k_fdc1`), naběhne se 64K ROM a je to stejný
    systém. MON62 („62K MONTERM“) je na disketě jen jako zdroják, 62K systém nainstalovaný není;
  - `RPD\r_test2732_101211`: zkušební systém se **4KB monitorem v EPROM 2732** („ROM 4KB JPR-1Z S AND-1Z
    CESTINA 101211JB“, MO1012), monitor se kopíruje do RAM na E800h. Naběhne s ROM z MO1012.HEX na disketě
    a dekodérem D3C `8k` (`DIR`, `SAVE` na RMD-1 fungují). Zkušební sestavení stejného CP/M, sestava ne.
- `cpm_pmd_fdc1.sapi` (2026-10-06): FDC-1 s PMD32-SD přes DPP-1, bootovací disketa `FDC\f2_pmd2d2m_140212.8sd`
  (C:, D: = PMD32-SD, Turbo Pascal a WordStar); `f2_pmd1d2m_140212` má jen C:. Bez RAM disku.
- **Tiskárna:** všechny sestavy s ECMONem a monitory J. Bíby (kromě `cpm_term64k_fdc1` bez klávesnice) mají od
  2026-10-06 tiskárnu na X3 (`LST: CENTRONIX`, `docs/tiskarna.md`), ověřeno `^P` a `DIR` s MON130711, 271111,
  281211 i ECMONem.
- `cpm_term64k_fdc1.sapi`: **CP/M 64K** (CCP DE00, BDOS E600, BIOS F400) s EPROM „64K MONTERM CZ 151111JB“, která
  se po startu odpojí; DMR-1 s celými 64 KB, konzole jen přes DSM-1/1 (DMR-1 na 10h, DCD aktivní), bez klávesnice
  a AND-1Z. A: Z8 (zdroje, `MON64.MAC`), B: Z4 (programy). Hlásí „CON: TERMINAL PRES DSM1.1“.
- Ověřeno testem `test_sapi_sys` (boot, `DIR B:`, `DIR C:`; 64K: přes sériovou linku `DIR`, `IRMD40`, `SAVE` na C:).

## PMD32-SD přes DPP-1 a diskety PETR\2MB (2026-10-05)

- Systémy J. Bíby a ECMON (bez IDE) používají PMD32-SD přes desku **DPP-1** na 28h (kanál 2, 2Ch–2Fh), od
  2026-10-05 emulovanou (`docs/desky/DPP-1.md`).
- `cpm_pmd_rpd1.sapi`: „dlouhé“ CP/M **CPM170212** (BIOS-2K, CCP D200, BDOS DA00, BIOS E800), A:, B: RPD-1 8",
  **C:, D: = PMD32-SD 2 MB**; EPROM MON 271111 (podle IDENTIFIKACE jde i s 130711). A: = bootovací disketa
  `r2_pmd2d2m_170212` (`PMD32-SD_IMAGE8\RPD`), B: = Z7 (zdroje). Výzva `U0A>`.
- `cpm_zrmd_dpp1.sapi`: autorova sestava `cpm_zrmd` + DPP-1: ECMON hlásí „PMD32-SD OK“, D: = 2MB obraz, E: = obraz 8"
  diskety („PMD32-SD/IMAGE 8"SS,SD“).
- **5,25" diskety J. Bíby** (`PETR\2MB`) jsou na SD kartě `media/pmd32sd`: `1613.2MB` (BASIC programy), `1614.2MB`
  (BASIC, BASCOM, MBAS), `1615.2MB` (dBASE II), `1616.2MB` (FAND), `1617.2MB` (WordStar, VEDIT, TED), `1618.2MB`,
  `1619.2MB` (Turbo Pascal 3), `1618_1.8SD` (zdroje FORMFDC, INIRMD, MONITOR, TRAM…), `1618_2.8SD`.

## Otázky na EC1045 (Petr Všijanský)

- ~~Složka `…/PETR/2MB/`~~: je v kopii z FTP (`..\SAPI_DOKU_FTP`).
- Zdroják ROM `JPR-1ZI_ECMON_2k_IDE-1_Martin.bin` (ECMON pro IDE-1, 2021): zdroják nemáme, EC1045 ho také nemá
  (HDD nikdy připojený neměl, odpověď 2026-10-06).
- Případně: měl MFC-1A zapojený podle originálu Tesla, nebo jako jeho překreslení (D4/D5, polarita D6)?
- Diskety TNS ze Slušovic v `DEX\SAPI_ZPS3` (04A, 04B, 05): odkud jsou a proč jsou u ZPS-3? Má někdo ROM zavaděče
  TNS (ZAV) nebo popis řadiče KORD4? (`docs/software/TNS_Slusovice.md`, 2026-10-06, zatím neodesláno)
- **Odesláno autorem 2026-10-06** (odpověď níže): zdroják ECMON IDE-1; jestli ECBIM8 na skutečné MFC-1A
  fungoval (chybějící `RET`); podle čeho je překreslení MFC-1A (D4/D5, D6) a podle kterého zapojení se staví
  repliky; mechaniky a RDY u MFC-1A; kdo dělal testy `SAPI15\MFC_TEST_CPM` a s jakým `DRVON`; připojení mechanik
  PC HD k FDC-1 (SA2, /RDY, HD/DD) a motory /M0–/M3 z X3; přepínače S2–S4 a kabel PMD32-SD na DPP-1; kanál 1
  DPP-1 (děrovač, nebo tisk přes `UL1:`); kolík 16 kabelu tiskárny na X3.
- **Odpověď EC1045 (2026-10-06 večer, „z hlavy“)**, uložená mimo repo v `..\Dotazy\EC1045_2026-10-06\` (e-mail
  `Re_Dotazy_k_ECMON_a_MFC-1.txt` s celou historií, zdrojáky, fotky `DPP-1_pro_PMD-32SD.jpg` a `PROM_prog.jpg`):
  - **Zdrojáky** (`ECMON*.MAC`, `ECBI*.MAC` včetně `_cd` a `ECBIF8_nefunguje.MAC`) jsou bajt po bajtu shodné
    s `..\SAPI-karty\ZRMD-1\SRC\orig\EC1045_2016\`. Nic nového, zdroják ECMONu pro IDE-1 nemá.
  - **MFC-1A:** J. Bíba rozchodil diskety na Petrově replice MFC-1A (měl ji zapůjčenou). Systémy JB se zapínáním
    mechanik D6 = 1 tedy odpovídají replice, což podporuje `wiring = ec1045` v jejich sestavách. Petrovi samotnému
    mechaniky na MFC-1A nechodily (připojil je kabelem pro RPD-1, MFC-1A má jiné zapojení). Na `RET` v ECBIM8
    neodpověděl; ECBIM8 na MFC-1A nejspíš nikdo nezkoušel (JB pracoval s bootovacím monitorem). Možná má
    i originální MFC-1A se sestavou SAPI-1.5 8" (zdroj pro ověření zapojení 0FH na originálu).
  - **FDC-1 a 5,25" HD „jako 8"“:** kabel přímý jako pro 8", přiklopení hlavy (/HL) vede na MOTOR ON mechaniky,
    první mechanika má výběr nastavený na „0“ (v PC je na „1“). Přepínače SA2 stejně jako pro 8" (`11000011`,
    krajní dva na obou stranách sepnuté). Nezapojené vodiče 2, 4, 6 a 34 (u mechaniky PC: výběr hustoty, nic, DS3,
    READY / DISK CHANGE), /RDY FDC-1 tedy zůstal uzemněný. S FDC-1 to chodí, s RPD-1 ne (příčinu nezkoumal),
    MFC-1A nezkoušel. /M0–/M3 z FDC-1B tedy nepoužíval (`docs/desky/FDC-1.md`).
  - **DPP-1 a PMD-32SD:** PMD-32SD je na horním konektoru (podle ECMONu kanál 2, X3), zapojení podle dokumentace
    PMD-32SD, signál DIR není potřeba. Nastavení přepínačů je na fotce, na spolehlivé přečtení nestačí rozlišení
    (`docs/desky/DPP-1.md`).
  - **Programátor PROM** na dolním konektoru DPP-1 (fotka `PROM_prog.jpg`, návod `SAPIMAN\Scany dokumentací\DPP1-
    PROGRPROM.pdf`): úpravy +5 V (kolík do spodní řady připájený na spoj u konektoru) a +12 V přes dvě diody
    v sérii (např. KY130/80). Přepínače pro druhý konektor „bez záruky“, platí pro obě desky.
  - **Tiskárna:** na X3 měl jen plotter MINIGRAF přes rozhraní z AR (Mikroelektronika 1988) s přepínači polarity
    STB a ACK. Kolík 16 neví (musel by kabel propískat).

## Plán (schválený autorem 2026-10-05)

1. ~~Sestavy bootující CP/M z 8" diskety SAPIMANa~~ (hotové, `cpm_boot_*.sapi`).
2. ~~3,5"/5,25" HD v PC formátu pro FDC-1 a sestava EC1045~~ (hotové, `cpm_ec1045.sapi`). MFC-1A umí DD.

## Formátovací programy SAPIMANa v emulátoru (2026-10-05)

Zdroje jsou v `..\SAPI_DOKU\SAPIMAN\CPM_PROGRAMY\FORMATOVANI` (`FORM_FDC_DE`, `FORM_MFC_DE`, `FORM_MFC_AB`)
jako `.txt` (M80, J. Bíba 2011–2012). Každý program se ptá na disk (D:, E:, C: = formát RAM disku), formátuje
stopy 0–80 po stranách a pak každou stranu zkontroluje povelem READ TRACK (42h) s DMA na délku ID pole.

- **Překlad:** `cpmrun M80 =FD5P` a `cpmrun L80 FD5P,FD5P/N/E` (`.MAC` = `.txt`, M80 a L80 jsou ve
  `..\SAPI-karty\ZRMD-1\SRC`). M80 hlásí u `SLA` bez operandu varování Q a přeloží ho jako `SLA B` (CB 20), takže
  to tak měl i autor.
- **Spuštění:** v běžící sestavě `--load-bin X.COM 100`, pak `SAVE 6 X.COM` a `X`. Pro pokus kopie sestavy
  s disketami v dočasné složce (`working_copy: false`, ZRMD-1 `readonly`).
- **Výsledky:**
  - FDC-1, `fd5p_512_30` (5,25" HD PC, MFM 2 × 15 × 512) v `cpm_boot_dlouhe_rpd1_fdc1`: „Disketa je OK“. PIPU pak na
    disketu D: zkopíroval celý B: s ověřením (`[V]`), STAT ukazuje 1038K volných.
  - MFC-1A, `fd3m_512_18` (3,5" DD, MFM 2 × 9 × 512, strana 1 sektory 10–18) a `fd5_256_16` (5,25" FM 2 × 8 × 256)
    v `cpm_boot_mfc1a` na jednotce 1 (E:): „Disketa je OK“.
  - 8" `fa128_26` (`FORM_MFC_AB`, FM 26 × 128, A: B:) na B: v `cpm_boot_mfc1a` (MFC-1A) i `cpm_boot_fdc1` (FDC-1):
    „Disketa je OK“, `SAVE` a `DIR B:` na nové disketě fungují.
  - Číslování sektorů `cis_8_128_26` (`CISLOVANI SEKTORU`, zápis značky stopy a čísla sektoru do každého sektoru)
    na D: (FDC-1 jednotka 0, 8" disketa) v dlouhém CP/M: všech 2002 sektorů obrazu má správné značky.
  - READ ID žádný z programů SAPIMANa nepoužívá.
  - **DUP-80** (`CPM_PROGRAMY\DUP_fyz_ kopie_8_disket`, varianty pro 8", 5,25" a 3,5" SS SD, A: = ZRMD-1, B: a C:
    diskety, E: = obraz 8" na PMD32-SD). Fyzická kopie, test a inicializace přes BIOS (SELDSK, SETTRK, SETSEC,
    READ, WRITE). Ověřeno 2026-10-06 s ECMONem: kopie B: → C: na FDC-1 i MFC-1A je bajt po bajtu shodná (2002
    sektorů), test disku na FDC-1 i obrazu E: na PMD32-SD bez chyb. Inicializace zapíše E5h jen do adresáře
    (16 sektorů stopy 2), data nechá. DUP8.COM je i na RAM disku sestavy `cpm_zrmd`, test `test_fdc_ecmon`
    (`dup8`) s ním kopíruje na FDC-1 a MFC-1A. Dočasná sestava pro pokusy potřebuje ZRMD-1 v režimu `nosave`
    (`readonly` neuloží ani `SAVE`).
- **Poznatky:**
  - 81 stop (0–80): emulovaná hlava teď dojede na stopu 82. Dřív se 81. stopa zapsala přes stopu 79.
  - Formáty MFC-1A s číslováním 10–18 na straně 1 a FM na 5,25" uloží jen ImageDisk. Novou disketu `.imd` založí
    okno Média (Nová disketa…, jméno s příponou `.imd`).
  - Programy DE počítají s D: a E: na jednotkách 0 a 1 řadiče a C: jako RAM diskem. V sestavách SAPIMANa s MFC-1A
    jsou A: a B: na jednotkách 0 a 1, takže E: tam znamená jednotku 1 (B:).