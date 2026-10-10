# Obrazy médií

Tyto soubory jsou **nedotčené originály**. Emulátor si je při prvním použití zkopíruje do `work/`, který je
v `.gitignore`, a pracuje jen s kopiemi. Originál se změní pouze tehdy, když to uživatel v konfiguraci výslovně nastaví.

## `pmd32sd/` – obsah SD karty PMD32-SD

Zdroj: `..\SAPI-karty\PMD32-SD\SW`. Geometrii obrazů určuje `images.cfg`.

| Soubor | Formát | Obsah |
|---|---|---|
| `VZOR_2M.2MB` | 255×64×128 B | Výchozí obraz (drive A, v CP/M jako D:). PIP, STAT, M80/L80, ZSID, IDE.COM, INIHDBC, OFFSHDD, RTC nástroje, XMODEM. |
| `POWER_2M.2MB` | 2MB | POWER, PW, PZ a dokumentace. |
| `XMODEM_2M.2MB` | 2MB | Zdrojové kódy DOCPM/DOPC (XMODEM). |
| `SYS_2M.2MB` | 2MB | Prázdný obraz (celý E5h). |
| `a_sluzebni CPM.8sd` | 77×26×128 B (8" SS SD) | Systémové utility: ED, M80, MAC, DDUMP, SYSGEN… |
| `b_sluzebniCPM.8sd` | 8SD | DDT, LINK, RMAC, MOVCPM… |
| `b_rtc_zdp_zrmd.8sd` | 8SD | Zdrojové kódy pro RTC, ZDP, ZRMD a IDE.COM. |
| `ZZRMD.8SD` | 8SD | ZZRMD.MAC/COM, přenos systému na RAM disk ZRMD-1. |
| `uzivat_prog.8sd` | 8SD | Uživatelské programy. |

5,25" diskety J. Bíby vykopírované do obrazů PMD32-SD (`..\SAPI_DOKU_FTP\ZALOHY_8SD_EC1045.01\nove_a_vse_co_mám\PETR\2MB`,
2026-10-05):

| Soubor | Formát | Obsah |
|---|---|---|
| `1613.2MB` | 2MB | Programy v BASICu (pl1–pl6, play, exp…), DIRF, FIALA. |
| `1614.2MB` | 2MB | BASIC, BASCOM, BASLIB, MBAS, KBAS2, XREF a programy (PARABOLA, TELE…). |
| `1615.2MB` | 2MB | dBASE II s instalací a databázemi (ADRESY, SKLAD, ZOZNAM). |
| `1616.2MB` | 2MB | FAND (databáze v Pascalu) s instalací. |
| `1617.2MB` | 2MB | WordStar (i české verze WS1715CS, WS80CS), MAILMERGE, SPELSTAR, VEDIT, TED. |
| `1618.2MB` | 2MB | (obsah neověřený) |
| `1619.2MB` | 2MB | Turbo Pascal 3 s příklady a instalací. |
| `1618_1.8SD` | 8SD | Zdroje FORMFDC, INIRMD, MONITOR, TRAM, SAPDI, SINFLOP, SYGEN… |
| `1618_2.8SD` | 8SD | (obsah neověřený) |
| `system.sap`, `emu48.sap` | 8SD | Obecný CP/M 2.2 („BOOT v1.0“), původ nejasný. |

## `zrmd-1/` – obsah RAM disku ZRMD-1

Zdroj: `..\SAPI-karty\ZRMD-1\SW`.

- `SAPI_CPM_Martin_2019-10-07_sysEC1045.bin`: soubory z Martinova obrazu, systémová oblast EC1045 (menu FDC8,
  FDC3, FDC5, HDD) přeložená `build.py --set ec1045`. Sestava `cpm_ec1045.sapi`.
- `ZRMD-1_prazdny_OFF0.bin`: prázdný RAM disk (E5h) pro systémy J. Bíby z roku 2011, které mají CP/M přes celé
  4 MB (DPB OFF 0, DSM 1023). S obrazy pro ECMON (OFF 128, systém v prvních 32 KB) se nedá sdílet.
- `SAPI_CPM_Martin_2019-10-07_fixMFC.bin` (2026-10-05): totéž s novou systémovou oblastí (0–7FFF) přeloženou
  ze zdrojáků `..\SAPI-karty\ZRMD-1\SRC` (`build.py --image`). Proti originálu se liší jediný bajt kódu, oprava
  `RET` v BIOSu ECBIM8 (583Dh: 0Eh → C9h). Neinicializované buffery v systémových blocích jsou vynulované, data od
  8000h jsou beze změny. Používá ho sestava `cpm_zrmd_mfc1a.sapi`.
- `SAPI_CPM_Martin_2019-10-07.bin`: celý obsah 4 MB RAM disku.
  - Offset v souboru = `(port63 & 3F) << 16 | port62 << 8 | port61`.
  - Na RAM disku je CP/M 2.2 (DPB: SPT 2, BSH 5, DSM 1015, DRM 1023, OFF 128; adresář od 0x8000).
  - Duplicitní `.hex` verze (10 MB) se do repa nedává.
- `puvodni_SYS0_3_s_CD.HEX`: systémová oblast (bajty 0–7FFF RAM disku), zaváděná na 4000–BFFF.
- `ECSYS_zaloha_0-3.hex`: jiná varianta systémové oblasti.
- Hry a dema pro sestavu V (CP/M jako `SAPI_CPM_Martin_2019-10-07.bin`, autorovy obrazy z 2026-10-08 a 09):
  - `ba-ramdisk.bin`: BADAPPLE.COM a BADAPPLE.DAT (3,1 MB), demo Bad Apple!! z repa SAPI-Bad-Apple. Sestava
    `sapi1v_BadApple.sapi`;
  - `pop-ramdisk.bin`: POP.COM, POP.DAT (data hry, 3,3 MB), POP.SAV, ZRDBENCH.COM, port Prince of Persia z repa
    SAPI-Prince-of-Persia. Sestava `sapi1v_PoP.sapi`;
  - `ramdisk_pop-fxs4-flappy-bomberman-bkp.bin`: PoP, FXS4, FLAPPY, BOMBER, BKP, DOCPM, DOCPM10, ZRDBENCH (záloha
    se všemi hrami), volba v konfigurátoru u ZRD-1V.

  **Obrazy s Prince of Persia jsou jen v repu**, do balíčku alfy je `tools/package.py` nedává (data hry Jordana
  Mechnera / Broderbund se dál nešíří; `REPO_ONLY`, stejně tak sestava `sapi1v_PoP.sapi`).

## `mikos/` – programy pro MIKOS (Intel HEX)

Zdroj: `H:\SAPI-1\MIKOS_PGM\` (shodné s výpisy na sapi.cz). Nahrávají se v GUI přes Soubor → Nahrát program do
paměti (Intel HEX), přes MCP `load_hex` nebo `sapiemu-cli --load-hex`, a pak se spouští `.B` v monitoru.

| Soubor | Program | Adresy |
|---|---|---|
| `BASIC_EXP_V5.0.hex` | BASIC EXP V5.0 (TESLA DIZ 1986), `BASIC5.HEX` | 4400–6B00 |
| `BASIC_EXP_V5.0G.hex` | BASIC EXP V5.0/G s grafikou pro DGD-1, `BASIC5G.HEX` | 4400–7400 |
| `BASIC_EXP_V5.0G_SZ.hex` | BASIC EXP V5.0/G+SZ s příkazem `KRES` pro zapisovač (Minigraf 0507, XY 4130), zdroj sapi.cz `data/bexpgsz_hex.php` (kopie „JiM“, konfigurace 0Ch = Minigraf na DPP-1). **Upraveno:** porty F3h/F1h vráceny na 33h/31h podle tabulky v návodu `doku2/bexpgsz.php` (6 bajtů: 7951, 7AB5, 7B03, 7C23, 7CC2, 7CC6). `docs/desky/Minigraf.md` | 4400–8BC7 |
| `KRES_DEMO.bas` | Ukázka příkazu `KRES` (rámeček, domek se šrafovanou střechou, slunce, otočený nápis, čárkovaná vlna), napsaná pro SAPIemu. Na začátku pero dojede na levý doraz a odtud 90 mm doprava, takže nezáleží, kde předtím stálo. Vkládá se Stroj → Klávesnice → Vložit text po `.B`. Návod: `docs/navod-minigraf.md`. | (text) |
| `MINIGRAF_DEMO.bas` | Ukázka pro zapisovač Minigraf 0507 v BASICu EXP V5.0 (spirála šestiúhelníků, nápisy, logo ARITMA), převzatá ze sapi.cz `doku2/0507.php` s opraveným řádkem 450 (`TO 17302`). Text, vkládá se Stroj → Klávesnice → Vložit text po `.G=1800` a `.B`. `docs/desky/Minigraf.md` | (text) |
| `CKM.hex` | Kopírovací program CKM magnetofon ↔ KZD-1 (TESLA ELTOS DIZ, 24. 3. 1988) pro MIKOS/K, spouští se `.G=4400`. Zdroj sapi.cz `data/ckm_hex.php`; překlad `CKM.MAC` ze SAPI_DOKU se s ním shoduje bajt po bajtu (4400–4D92, dál jen buffery). Originál má chybu: po čtení z KZD-1 nastaví délku bloku MIKOSu/K na 0. Návod v `docs/navod-kzd1.md`, podrobnosti v `docs/desky/KZD-1.md` | 4400–4FAA |
| `CKM_opraveny.hex` | **Opravená kopie** CKM (není originál): 47D4h `XRA A` → `SBB A`, takže po čtení z KZD-1 zůstane délka bloku FFh; první řádek menu `OPRAVENY KOPIR. PROGRAM MGF-KZD-1`. Vyrábí ji `python tools/ckm_oprava.py` z `CKM.hex`. `docs/desky/KZD-1.md` | 4400–4FAA |

## `rpd-1/` – diskety 8" pro RPD-1

| Soubor | Obsah |
|---|---|
| `CPM22P_48A_Perina.img` | CP/M 2.2/P 48K (R. Peřina 1988) pro ZPS-3 s AND-1A a RPD-1, bootovatelná. Zdroj sapi.cz `data2/cpm48a_img.rar` (`CPM48A.img`). Obsahuje BIOS.MAC, BOOT.MAC, MOR24.MAC, SYSGEN, CPM.DOC a další. |
| `CPM22P_62A_Perina.img` | CP/M 2.2/P 62K (R. Peřina 1988) pro ZPS-3 s AND-1A na F800 a RPD-1, k ROM MOR62A. Zdroj autor (`..\SAPI-sestavy\ZPS-3\CPM62A.img`). |
| `CPM48K_IMA_004.img` | CP/M 2.2 46K Tesla IMA pro ZPS-3 s ROM MON V1.0-C1 („JPR-1A 46K CPM V2.2“, BIOS V2.2-C1). Soubory originální (`!!!!_TRIDENO_!!!!\ZPS3\083B.8SD` v `..\SAPI_DOKU_FTP\ZALOHY_8SD_EC1045.01\nove_a_vse_co_mám`: MOVCPM48, SYSGEN, M80, F80, zdroje), **systémové stopy vyrobené** originálním `MOVCPM48 46 *` a `SYSGEN` (originál se nedochoval, `docs/software/CPM_ZPS-3.md`). Sestava `ZPS-3_CPM48K_IMA`. |
| `CPM48K_IMA_003.img` | Disketa IMA ZPS-3 „!CPMZPS3.003“ (Pascal MT+), bez systému (stopy 40h). `CPMZPS3_003.IMG` z téže složky (= sapi.cz `cpmzps3_img.rar`). |
| `CPM64K_IMA_01.img`, `_02`, `_03` | Originální CP/M 2.2 64K Tesla Eltos IMA Pardubice (CCP V2.2-A1, 15/11/1984) pro ZPS-3 s ROM MON64C3: 01 systém, ED, PIP, M80, F80, ASM; 02 STAT, FORMAT, MBASIC, COBOL; 03 Pascal MT+. Z `..\SAPI_DOKU_FTP\ZALOHY_8SD_EC1045.01\nove_a_vse_co_mám\DEX\SAPI_ZPS3\01–03.8SD`. Sestava `ZPS-3_CPM64K_IMA`. |
| `CPM22P_62Z_Perina.img` | CP/M 2.2/P 62K (R. Peřina 1988) pro ZPS-4 (JPR-1Z, AND-1Z, RPD-1, RAM disk RMD-1 jako E:). Zdroj sapi.cz `data2/cpm62z_img.rar`. |
| `ELIS_61K.img` | ELIS 61k CP/M V2.2 (A, B: RPD-1, C: RMD-1, AND-1Z, Consul). Zdroj sapi.cz `data2/elis_img.rar` (= `MELSOFT\001.8SD` na SD kartě EC1045). ED, PIP, STAT, MBASIC, SYSGEN, INIRMD5 (JB 2011). |
| `MIKOS_D_V5.0_system.img` | Systémová disketa MIKOS/D V5.0 („ZALOZNI“), 77 × 26 × 128 B v logickém pořadí. Zdroj `H:\SAPI-1\Klasika\mikos5\m5mid_img.rar` = sapi.cz `data2/m5mid_img.rar`, shodná s daty v `m5mid.IMD`. Obsahuje BAS5C, TEDIT, TOOL, BASIMS4, KONVERT, COPYSYS, X2 a dokumentaci (.DOC). |

## `sapiman/` – bootovací 8" diskety CP/M J. Bíby (SAPIMAN, 2011)

Zdroj `..\SAPI_DOKU\SAPIMAN\PMD32-SD_IMAGE8\` (obrazy `.8sd` = 77 × 26 × 128 B, přejmenované na `.img`). Bootují
z EPROM `roms/jpr-1zi/JPR-1ZI_MON*_JB_*.bin`, C: je RAM disk ZRMD-1 se souborovým systémem přes celé 4 MB
(`zrmd-1/ZRMD-1_prazdny_OFF0.bin`). Na disketách jsou i zdrojáky systému (`docs/software/CPM_SAPIMAN.md`).

| Soubor | Obsah |
|---|---|
| `m1_281211_zrmd.img` | CP/M pro MFC-1A, „BIOS-1K V281211JB“, A, B, D, E = MFC-1A. Zdrojáky BI2812*, MO2812, BO1307D6, CCPD6, BDOSDE. (`MFC\m1_281211_zrmd.8sd`) |
| `f1_zrmd4_130711.img` | CP/M pro FDC-1, „BIOS-1K V130711JB“. (`FDC\f1_zrmd4_130711.8sd`) |
| `r1_zrmd4m_271111.img` | CP/M pro RPD-1, „BIOS-1K V271111JB“. (`RPD\r1_zrmd4m_271111.8sd`) |
| `m1_281211_rmd.img`, `f1_rmd1m_130711.img`, `r1_rmd1m_271111.img` | Totéž s RAM diskem RMD-1 1 MB na 40h jako C: (`MFC\`, `FDC\`, `RPD\` `*.8sd`). Sestavy `cpm_boot_fdc1_rmd`, `cpm_boot_rpd1_rmd` (RMD-1 inicializuje `B:IRMD40` z diskety Z6). |
| `knihovna/*.img` | Výběr 16 datových disket J. Bíby (překladače, editory, služební programy, Kermit) ze `SAPIMAN\PMD32-SD_IMAGE8`, soupis a obsah v `docs/software/SAPIMAN_diskety.md`. |
| `r2_mfc256x14_261211.img`, `r3_mfc512x8_261211.img`, `r2_rmd1m_fdc_261211.img` | CP/M J. Bíby bootované z RPD-1 s RMD-1 jako C: a druhým řadičem pro D:, E: (MFC-1A 8" FM 14 × 256 nebo 8 × 512, FDC-1 8"), `RPD\*.8sd`. Sestavy `cpm_boot_rpd1_mfc256`, `_mfc512`, `cpm_boot_rpd1_fdc1_rmd`. |
| `f2_irmd1m_130711.img` | CP/M pro FDC-1 s BIOSem BIRMD, který RAM disk RMD-1 sám najde, změří (256 KB / 1 MB) a připraví (`FDC\f2_irmd1m_130711.8sd`). Sestava `cpm_boot_fdc1_irmd`. |
| `prazdna_8_FM_14x256.imd`, `prazdna_8_FM_8x512.imd` | Prázdné 8" diskety FM 77 × 14 × 256 a 77 × 8 × 512 (ImageDisk, E5h) pro ty sestavy. |
| `f2_pmd2d2m_140212.img`, `f2_pmd1d2m_140212.img` | CP/M pro FDC-1 s PMD32-SD přes DPP-1 (C:, D: nebo jen C:), bez RAM disku (`FDC\f2_pmd*.8sd`). Sestava `cpm_pmd_fdc1`. |

Systémové diskety J. Bíby z `..\SAPI_DOKU_FTP\ZALOHY_8SD_EC1045.01\nove_a_vse_co_mám\PETR\SAPI_SYS\` (Z1–Z10,
soupis `Zaloha_sys_disket_sapi.txt`; disky 1–10 jsou totožné):

| Soubor | Obsah |
|---|---|
| `Z1_RPD271111_programy.img`, `Z2_RPD271111_zdroje.img` | RPD-1, MON 271111, C: ZRMD-1 SS256 (rozložení ECMON); programy a zdroje systému. |
| `Z4_FDC130711_programy.img`, `Z10_FDC130711_zdroje.img` | FDC-1, MON 130711, C: ZRMD-1 SS256. |
| `Z6_MFC281211_RMD_programy.img`, `Z5_MFC281211_RMD_zdroje.img` | MFC-1A, MON 281211, C: RMD-1 1 MB na 40h. |
| `Z9_RPD050412_dlouhe_CPM_boot.img` | Disketa Z9 (programy „dlouhého“ CP/M) **se systémem doplněným ze zdrojáků** (2026-10-05): zavaděč `bo2608CE` na stopě 0 sektoru 1, CCP, BDOS a BIOS `bi0504E4_5_512_30_PC` od CE00h na stopách 0 (sektory 2–26) a 1 (1–26), konec BIOSu na 6 sektorech stopy 76 (12h, 18h, 04h, 0Ah, 10h, 16h). BIOS a zavaděč souhlasí s původními výpisy `.PRN`. |
| `Z9_RPD050412_dlouhe_CPM_boot_3.5HD.img` | Totéž s BIOSem `bi0504E4_3_512_36_PC` (D:, E: = 3,5" HD 512 × 36), také souhlasí s původním `.PRN`. |
| `Z3_RPD050412_dlouhe_CPM_zdroje.img` | Disketa Z3: zdroje „dlouhého“ CP/M (verze s 1,44 MB a RAM diskem, `142FZ2P*`, `BI142ZD2`, `BOOTD2`). |
| `r2_pmd2d2m_170212.img` | Bootovací CP/M 170212 (BIOS-2K) s PMD32-SD (C:, D:) přes DPP-1, `SAPIMAN\PMD32-SD_IMAGE8\RPD\r2_pmd2d2m_170212.8sd`. |
| `Z7_RPD170212_PMD_zdroje.img` | Disketa Z7: zdroje systému RPD 170212 + PMD32-SD. |
| `Z8_FDC64K_TERM_zdroje.img` | CP/M 64K jen s terminálem (MON 151111 64K, CCP DE00), FDC-1, RMD-1; zdroje včetně `MON64.MAC`. |

## `fdc-1/` – diskety 8" pro FDC-1

| Soubor | Obsah |
|---|---|
| `CPM22P_48AF_Perina.img` | CP/M 2.2/P 48K (R. Peřina 1988) pro ZPS-3 s AND-1A a řadičem FDC-1, k ROM MOR48AF. Zdroj autor (`..\SAPI-sestavy\ZPS-3\CPM48AF.img`). Obsahuje FDC.DOC, FDC.INC, MOR24F.MAC, FORM6.COM (formátovač), MOR*F.HEX. |
| `CPM22P_62AF_Perina.img` | Totéž pro 62K (AND-1A na F800), k ROM MOR62AF (`CPM62AF.img`). |
| `CPM22P_62ZF_Perina.img` | CP/M 2.2/P 62K pro ZPS-4 s FDC-1 (AND-1Z), k ROM MOR62ZF; sapi.cz `data2/cpm62zf_img.rar` (`CPM62ZF.img`, od 62AF se liší 2 bajty). |
| `prazdna_3.5HD_CPM.img` | Prázdná disketa PC 3,5" HD 1,44 MB (80 × 2 × 18 × 512 B, E5h) pro BIOS EC1045 `ECBIF3`. |
| `prazdna_5.25HD_CPM.img` | Prázdná disketa PC 5,25" HD 1,2 MB (80 × 2 × 15 × 512 B, E5h) pro BIOS `ECBIF5`. |
| `prazdna_8SSSD.img` | Prázdná 8" SS SD (77 × 26 × 128 B, E5h). |

## `sapi15/` – diskety CP/M 2.2 62K pro SAPI-1.5 (Melsoft, 1990)

Z `..\SAPI_DOKU_FTP\SAPI15\MELSOFT_DISKETY\SAPI15_*.sp2` (jen kopie z FTP, 2026-10-07). Formát `.sp2`: 40 stop × 2 strany
× 16 sektorů × 256 B (327 680 B, C/H/R, sekce `[SP2]` programu CpmImgMngr); jednostranná varianta `.sp1` má 163 840 B.
Sestava `machines/zps/ZPS-6.sapi`, popis `docs/software/SAPI-1.5.md`.

| Soubor | Obsah |
|---|---|
| `SAPI15_1.sp2` | systém (boot, CCP, BDOS, CBIOS V050690), CPM62-5.TXT, FORMAT, PIP, SDT40, SETDBS1, STAT, SYSGEN |
| `SAPI15_2.sp2` | ASM, ED, M80, L80, LIB, CREF80, F80, WM, SORT, DDUMP, SUBMIT, XSUB |
| `SAPI15_3.sp2` | MBASIC, BASCOM, COBOL; originál s poškozeným sektorem BDOS (C0 H0 R14) |
| `SAPI15_3_opr.sp2` | totéž se systémovou stopou z diskety 1 (bootuje) |
| `SAPI15_4.sp2` | Pascal MT+, DIS8080 |
| `SAPI15_7.sp2` | zdrojáky ROMBIOS.MAC, SYSTEM.MAC, programy pro RSW-1 (CONNECT, INTER, SAPI), SIMPROG, TAPEREC, DOCTOR |
| `SAPI15_8.sp2` | ROMBIOS.MAC/PRN/REL, TAPEREC, ASM, LOAD, DUMP, FORMAT |

## `jpr-1x/` – ukázky pro desku diagnostiky na K3 (2026-10-06, Intel HEX od 4400h, zdroje `.asm`)

| Soubor | Co dělá |
|---|---|
| `diag_int.asm` / `.hex` | čítač 00–99 na dvou sedmisegmentovkách, tlačítka + a − přes přerušení 3214 (JPR-1X, RSTN/7 rozpojená: RST 7 → 2000h) |
| `diag_poll.asm` / `.hex` | totéž bez přerušení, čte P2-IN (JPR-1S i JPR-1X) |

Spuštění z Falconu `JUMP 4400`, konec ESC (klávesa LF). Překlad `python tools/asm8080.py SOUBOR.asm SOUBOR.hex`.

## `jpr-1s/` – programy pro JPR-1S s BGP-1S (Intel HEX od 4400h)

Zdroj: `..\SAPI-karty\JPR-1S\jpr1s_sw\` (doplnil autor 2026-10-06). Sestava `jpr1s_bgp1s`: Soubor → Nahrát
program (Intel HEX), nebo jako na stroji `BOOT` a soubor poslat po sériové lince redukce Redukce34KB01 na TCP
127.0.0.1:2325 (raw, např. Tera Term → Poslat soubor; postup v `docs/desky/JPR-1S.md`), pak `JUMP 4400`.

| Soubor | Obsah |
|---|---|
| `manic_miner.hex` | Manic Miner (Vít Libovický & Daniel Jenne, ViLiSoft & DaJeSoft 1985, port z PMD-85), 4400–989E. Potřebuje BGP-1S na C000h v režimu Colorace a RAM pod ní. Ovládání O, P, SHIFT (skok), M (melodie), CR (start). Snímek ze skutečného stroje `..\SAPI-karty\JPR-1S\jpr1s_sw\manic_main.jpg`. Původně `manic_sapi.txt`. |
| `auto.hex` | Hra s autíčkem („HAVAROVAL SI … KRAT“), 4400–4BAB: joystick ANK-1M2 vlevo/vpravo (P1-IN D3/D2), čte videoram kvůli srážkám. Původně `auto.txt`. |
| `martinuv_prvni_pokus.hex` | „MARTINUV PRVNI POKUS (06/06/2022)“ (autor emulátoru), volá tisk BIOSu 1.02 na 0BC4h, jen s ním. Původně `m.txt`. |

## `cga-1v/` – programy pro CGA-1V (Intel HEX, CP/M `.COM` od 0100h)

Zdroj: `..\SAPI-karty\CGA-1V\sw\` (Libor Lasota). V CP/M sestavy V:
1. nahrát HEX (Soubor → Nahrát program do paměti, MCP `load_hex`);
2. uložit `SAVE n JMENO.COM`;
3. spustit.

| Soubor | Program | `SAVE` |
|---|---|---|
| `castle.hex` | obrázek hradu, EGA 32 barev | 64 |
| `rembran1.hex` | Rembrandt (stařec), EGA | 64 |
| `rembran2.hex` | Rembrandt (Syndikové), CGA se střídáním stránek po snímcích | 127 |
| `cga_test.hex` | CGATEST rev. 0.99, testy karty | 18 |

## `and-1v/` – měření AND-1V (Intel HEX, CP/M `.COM` od 0100h)

Vlastní program projektu (2026-10-09), udělaný podle ZRDBENCH (`zrd-1v/`). Zdroj `VRAMBENCH.ASM` pro pasmo,
překlad `pasmo --hex VRAMBENCH.ASM vrambench.hex`.

| Soubor | Program | `SAVE` |
|---|---|---|
| `vrambench.hex` | VRAMBENCH: takty na bajt `LDIR` paměť ↔ videopaměť AND-1V (F800h, MAP1 = L), takt CPU podle 82C54 na MPH-1V; obrazovka zůstane, jak byla (sestava `sapi1v.sapi`) | 6 |

Výsledky emulace a otázka na skutečnou desku: `docs/desky/AND-1V.md`.

## `rkd-1/` – kazety kazetového disku RKD-1 (CM 5403, CM 5400)

| Soubor | Obsah |
|---|---|
| `tesla_cpm_system.rkd` | Systémová kazeta CP/M 2.2 Tesla Elstroj & JaMeSoft 1987 (24 × 256 B, A: a B:): systém na stopách 0 a 1 média 0 (CCP, BDOS, CBIOS 87-02-13), soubory diskety `..\SAPI_DOKU\SAPIMAN\PMD32-SD_IMAGE8\ZDROJ_STARE\a_mercl.8sd` (zdrojáky RKD.MAC, CBES.MAC, ROMES.MAC…, shell SH se STARTSYS.SUB, FORMTRKD, ES, MOVCPM, WM) a PIP.COM. |

Kazeta je vyrobená v emulátoru originálními programy (`FORMTRKD`, `ES 59`), jen zápis systému na stopy je náš
(originální SYSGEN pro RKD-1 se nenašel). Postup a opakování: `docs/desky/RKD-1.md` (`RKD_MAKE=… test_rkd1`).
Formát obrazu `.rkd` je vlastní (bajty zapsané do každého sektoru), popsaný tamtéž.

| Soubor | Obsah |
|---|---|
| `jb_cm5400_boot.img` | Bootovací 8" disketa (RPD-1) CP/M J. Bíby s BIOSem-3K pro RKD-1 a bulharskou mechaniku CM 5400 (D: až G:, 12 × 512 B). **BIOS není originál**, vznikl v emulátoru (soubory diskety jsou z `sapiman/r1_zrmd4m_271111.img`, systémové stopy z `sapiman/r3_mfc512x8_261211.img`). Vyrábí ho `tools/rkd5400_bios.py`. |
| `cm5400/BIRKD54.MAC`, `.PRN` | Zdroják a výpis toho BIOSu (z 3K BIOSu J. Bíby, ovladače ZRMD-1 z jeho BIOSu-1K a ovladače `RKD.MAC` Tesly ve tvaru z FORMRKD; vyrábí ho `tools/rkd5400_bios_src.py`). |

## `rmp-1/` – magnetická páska RMP-1 (IZOT SM 5300)

| Soubor | Obsah |
|---|---|
| `a_lad_izo.img` | 8" disketa J. Bíby s programy pro pásku: MTDAT, DIRMT, MTCOPY, MTZAP, MTCTE, MTKAN, ovladač `RMP.REL`, zdroje a návody `MTDAT.MAN`, `MTKAN.MAN`. Kopie `..\SAPI_DOKU\SAPIMAN\PMD32-SD_IMAGE8\ZDROJ_STARE\a_lad_izo.8sd` (stejná je i v kopii z FTP). |
| `sapi01_ukazka.tap` | Ukázková páska SAPI01 (SIMH `.tap`): VOL1, soubor 1 MTZAP (`MTZAP.MAC`) a soubor 2 MTDATMAN (`MTDAT.MAN`), blok 800, záznam 80, návěští JSEP, EBCDIC. Zapsal ji program MTDAT v emulátoru (`RMP_MAKE=… test_rmp1`). |

## `ar1990/` – povely pro plotter z AR 1990

| Soubor | Obsah |
|---|---|
| `ukazka.txt` | Ukázka povelů Sharp MZ-1P16 (sestava `plotter_ar1990.sapi`, tlačítko Ukázka v okně Minigraf): grafický režim, rámeček 170 × 120 mm, osy, sinusovka, tlumená sinusovka přerušovanou čarou, hvězda, texty ve dvou velikostech a svisle, pak textový režim ve třech velikostech. Souřadnice v krocích 0,125 mm, parametry končí CR. |

## `mph-1v/` – programy pro zvukovou kartu MPH-1V (Intel HEX, CP/M `.COM` od 0100h)

Zdroj: `..\SAPI-karty\MPH-1V\` (Libor Lasota), do repa se souhlasem autora. Hudba (VGM v `SPLAY\*.hex`, RAD
v `RATplayer\*.rad`) v repu není, testy ji berou z `..\SAPI-karty\MPH-1V`, pokud tam je. Postup v CP/M sestavy V
(`sapi1v.sapi`, karta na 50h) je stejný jako u CGA-1V: nahrát HEX, `SAVE n JMENO.COM`, spustit.

| Soubor | Program | `SAVE` |
|---|---|---|
| `mph_test_0v99.hex` | MPH-test 0.99, test karty (porty joysticku, čítač a přerušení, YM3812) | 31 |
| `splay.hex` | SPLAY, přehrávač skladeb VGM převedených pro SAPI (`SPLAY BUBBLE.VGM`) | 4 |
| `rat_0_99_93.txt` | RAT PLAYER 0.99.93, přehrávač RAD v1 (`RAT SMJOOP.RAD`) | 18 |
| `y1stdemo_2025_10_26.hex` | y1stdemo (2025-10-26), hudební demo | 154 |

Skladby pro SPLAY (`..\SAPI-karty\MPH-1V\SPLAY\*.hex`, také od 0100h): `SAVE 17 BUBBLE.VGM`, `SAVE 33 BIRDIE.VGM`,
`SAVE 47 DEMON.VGM`, `SAVE 46 DINOISLE.VGM`, `SAVE 109 FURY.VGM`, `SAVE 28 HELLFIRE.VGM`,
`SAVE 152 MONKISLA.VGM`, `SAVE 75 OLMANG.VGM`, `SAVE 54 SUPAPLEX.VGM`, `SAVE 180 TYRIAN.VGM`,
`SAVE 22 VIMANA.VGM`. Soubor RAD se nahraje binárně od 0100h (GUI Soubor → Nahrát binární soubor do paměti…,
`sapiemu-cli --load-bin soubor 100`, MCP `load_binary`) a uloží `SAVE n JMENO.RAD`, kde n = velikost / 256
zaokrouhleno nahoru (`smjoop.rad` 38, `Void - Terrania.rad` 40, `Void - Raster.rad` 72). Okno pro nahrání
binárního souboru n ukáže.

## `dsp-1/` – přenos souborů XMODEM přes DSP-1 (J. Bíba, 2013)

Kopie z `..\SAPI_DOKU\SAPIMAN\CPM_PROGRAMY\XMODEM\DSP-1\` (CP/M `.COM` od 0100h a zdrojáky `.MAC`), test `test_dsp1`.

| Soubor | Obsah |
|---|---|
| `dopcp.com`, `DOPCP.MAC` | `DOPCP SOUBOR` pošle soubor do PC (XMODEM, součet), DSP-1 konektor X2, porty 48h/49h |
| `docpmp.com`, `DOCPMP.MAC` | `DOCPMP SOUBOR` přijme soubor z PC (starý smaže), X2 |
| `dopcpx3.com`, `docpmpx3.com`, `…X3.MAC` | totéž pro konektor X3 (4Ah/4Bh) |
| `IDENTIFIKACE.txt`, `DSM1_DSPX2.txt` | popis (9600 Bd, úplný kabel s řízením) a výpis s porty DSM-1, DSP-1, DPP-1 |

Na RAM disku `SAPI_CPM_Martin_2019-10-07.bin` jsou `DOPCP.COM` a `DOCPMP.COM` také, test je ale ukládá z této složky.

## `zrd-1v/` – test a měření RAM disku ZRD-1V

- **ZRDTEST** je vlastní program projektu (zdroj `ZRDTEST.ASM`, překlad
  `python tools/asm8080.py ZRDTEST.ASM zrdtest.hex`).
- **ZRDBENCH** je převzatý z portu Prince of Persia (`..\SAPI-Prince-of-Persia\sapi\zrdbench.asm`, vlastní kód
  bez dat hry, převzetí nabízí jeho `docs/sapiemu-podnety.md`). Zdroj `ZRDBENCH.ASM` pro pasmo,
  překlad `pasmo --hex ZRDBENCH.ASM zrdbench.hex`. Binárka je shodná s tou, kterou autor měřil na skutečném
  SAPI-1 V (2026-10-08).

| Soubor | Program | `SAVE` |
|---|---|---|
| `zrdtest.hex` | ZRDTEST 1.0: baterie (MAX187), čtení 61h, okno zápisu a zámek (rev 2.1), `ZRDTEST [MHz]` | 6 |
| `zrdbench.hex` | ZRDBENCH: takty na bajt pro paměť, RAM disk ZRD-1V (INIR, OTIR, IN) a CGA-1V (LDIR oběma směry), takt CPU podle 82C54 na MPH-1V; potřebuje MPH-1V a CGA-1V na C000 (sestava `sapi1v.sapi`) | 7 |

Výsledky ze skutečného SAPI-1 V a z emulátoru jsou v `docs/desky/ZRMD-1.md` (ZRDBENCH) a `docs/desky/CGA-1V.md`.
