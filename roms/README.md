# ROM a PROM dumpy

Dumpy pocházejí z autorových repozitářů desek (`..\SAPI-karty\<deska>\ROM_PROM`, `..\SAPI-karty\DMR-1\PROM`, `..\MikroMon`).
Byly rozbaleny ze ZIP archivů. Emulátor používá PROM dumpy přímo jako dekodéry adres, jak je to na skutečné desce.

| Soubor | Deska / pozice | Popis |
|---|---|---|
| `jpr-1zi/JPR-1ZI_D3C_2k.bin` | JPR-1ZI D3C (74S287) | Paměťový dekodér. Při BOOT=1 je ROM na 0000–07FF. Odpovídá původnímu D14 z JPR-1Z. |
| `jpr-1zi/JPR-1ZI_D3C_8k.bin` | JPR-1ZI D3C | Varianta s ROM na 0000–1FFF při BOOT=1. |
| `jpr-1zi/JPR-1ZI_D6B.bin` | JPR-1ZI D6B (74S287) | I/O dekodér portů 00–03h. |
| `jpr-1zi/JPR-1ZI_ECMON_2k_Martin.bin/.HEX` | EPROM JPR-1ZI | Monitor ECMON 2k „JPR-1Z 62K MON CZ“. |
| `jpr-1zi/JPR-1ZI_ECMON_2k_IDE-1_Martin.bin` | EPROM JPR-1ZI | ECMON s podporou IDE-1 (porty 5C–5Fh). |
| `jpr-1zi/JPR-1ZI_ECMON_2k_EC1045.bin` | EPROM JPR-1ZI | ECMON EC1045 (`ECMON_cd.MAC`, menu FDC8, FDC3, FDC5, HDD, „Uzivatel: EC1045“), **přeloženo ze zdrojáku** (`..\SAPI-karty\ZRMD-1\SRC`, `build.py --set ec1045`), originální EPROM nemáme. Zbytek FFh. |
| `jpr-1zi/JPR-1ZI_ZZRMD_epr2k.bin`, `_epr8k.bin` | EPROM JPR-1ZI | Zavaděč CP/M z RAM disku ZRMD-1. |
| `jpr-1zi/JPR-1ZI_MON281211_JB_MFC.bin` | EPROM JPR-1ZI | Monitor J. Bíby „JPR-1Z 62K MON CZ 281211JB“ (2 KB BIOS v ROM), boot CP/M z 8" diskety přes MFC-1A. Z `mon281211.hex` (`..\SAPI_DOKU\SAPIMAN\CPM_SYSTÉMY\ZMENA_CPM\FUNKCNI\JEN 8 PALCU\MFC\MFC281211_ZRMD0`), zbytek FFh. Zdroják `mon281211.txt` se přeloží (`cpmrun`) na stejné bajty kromě proměnných (DS). |
| `jpr-1zi/JPR-1ZI_MON130711_JB_FDC.bin` | EPROM JPR-1ZI | Totéž pro FDC-1 (`mon130711`, složka `FDC\FDC130711_ZRMD0`). |
| `jpr-1zi/JPR-1ZI_MON271111_JB_RPD.bin` | EPROM JPR-1ZI | Totéž pro RPD-1 (`MON271111`, složka `RPD\RPD271111_ZRMD0`). |
| `jpr-1zi/JPR-1ZI_MON260112_JB_RPD_FDC.bin` | EPROM JPR-1ZI | „Dlouhé“ CP/M J. Bíby (3K BIOS): RPD-1 8" + FDC-1 3,5"/5,25" HD. Z `MON260112MO.HEX` (`ZMENA_CPM\FUNKCNI\8 A 3,5 A 5,25\…`, ve všech 8 složkách stejný). Zdroják `MON260112MO.txt` je o 3 bajty novější než `.hex` a původní `.PRN`. |
| `jpr-1zi/JPR-1ZI_MON151111_JB_64K_TERM.bin` | EPROM JPR-1ZI | „JPR-1Z 64K MONTERM CZ 151111JB“: CP/M 64K jen s terminálem přes DSM-1/1, FDC-1, RMD-1. Z `mon151111_64K.HEX` (`FDC\TERM64K_15112011_RMD`), shodný s `MON64.HEX` na disketě Z8. Po startu se odpojí `OUT 0`. |
| `jpr-1zi/JPR-1ZI_PROM_Info.txt` | | Poznámka o označení: D3C = D14, D6B = D10 (původně). |
| `and-1z-89/AND-1Z_CharGen.bin/.hex` | AND-1Z-89 (2732) | Znakový generátor, 4 KB, 16 B na znak. |
| `and-1z-89/AND-1Z_CharGen_upr10-1F.bin/.hex` | AND-1Z-89 | Varianta s upravenými znaky 10–1Fh. |
| `and-1z-89/PROM_D1.hex.bin`, `PROM_DA1-D1.hex` | AND-1Z-89 D1/DA1 (74S287) | Adresový dekodér VRAM (F800–FFFF). |
| `and-1z-89/PROM_DD4-D24.hex` | AND-1Z-89 DD4/D24 | Horizontální časování. |
| `and-1z-89/PROM_DE6_D35.hex` | AND-1Z-89 DE6/D35 (74S571) | Vertikální časování. |
| `dmr-1/DMR-1_D2C.bin/.hex` | DMR-1 D2C (74S571) | Dekodér RAM, 8 konfigurací (JP3). |
| `mikromon/MikroMon1_3_f000.txt` | EPROM JPR-1ZI | MikroMon 1.3 (Intel HEX, zavádí se od 0000, běží od F000). Autor: Libor Lasota, použito s jeho souhlasem. |
| `mikromon/navod-k-MikroMon.pdf` | | Návod k MikroMonu. |
| `mikrobasic/MIKROBASIC_2.4_ZPS-1.bin` | REM-1 EPROM 0000–0FFF (4× 2708) | MIKROBASIC 2.4 s Mikromonitorem (TESLA ELTOS 1983), verze ZPS-1, RAM na REM-1 4000–47FF. Zdroj `H:\SAPI-1\_ROM\vypis_programu_MBASIC.BIN`, MD5 a9dde20b…, shodný s dumpy čtyř skutečných 2708 (`SAPI_DOKU_2011-09-10\EPROM_od_PEPAX\SAPI\MBASIC_v24_0..3.BIN`) a s HEX na sapi.cz. |
| `mikrobasic/MIKROBASIC_AR1983_1k.bin` | EPROM 0000–0FFF | Starší MIKROBASIC z AR/B 1983, 1 KB RAM na JPR-1 (2000–23FF). Zdroj `Foto\Jiri_Slansky\SW\JPR-1 MicroBASIC 1k`, MD5 6d42ad34…. |
| `mikos/MIKOS_V4.0F_ANK-1.bin` | REM-1 EPROM 0000–0FFF | Monitor V4.0 + MIKOS-1 V4.0F pro ANK-1 (ZPS-2). Zdroj `H:\SAPI-1\_ROM\MIKOS v4\SAPI_V4_0_MIKOS-1_V4_0F.BIN`, MD5 0da74a65…, shodný s výpisy `m40f`/`mi40f` na sapi.cz. První 1 KB je shodný s dumpem 2708 `SAPI_DOKU_2011-09-10\EPROM_od_PEPAX\SAPI\SAPI_V4_0.BIN`. |
| `mikos/MIKOS_V4.0D_ANK-1.bin` | REM-1 EPROM 0000–0FFF | Monitor V4.0 (C) + MIKOS-1 V4.0D. Zdroj `_ROM\MIKOS v4\SAPI_V4_0_MIKOS-1_V4_0D.BIN`, MD5 e7f3f09a…. |
| `mikos/MIKOS_V4.1F_Consul.bin` | EPROM 0000–0FFF | Monitor V4.1 + MIKOS-1 V4.0F pro klávesnici Consul. Zdroj `_ROM\MIKOS v4\SAPI_V4_1_MIKOS-1_V4_0F.BIN`, MD5 52936e84…. Další varianty (V4.1H, dumpy 2716 `Klasika\MIKOS4\36.BIN`, `37.BIN`) nejsou v repu. |
| `mikos/MONITOR_V5.0F_MIKOS_V5.0.bin` | REM-1 EPROM 0000–13FF (5× 2708) | Monitor V5.0F + MIKOS V5.0 (ELTOS DIZ 1987/88), klávesnice Consul. Prvních 5 KB `H:\SAPI-1\_ROM\MIKOS v5\MIKOS5.BIN` (8 KB, za tím ovladač XY 4130). Shodné s výpisy `m5f`/`mi5` na sapi.cz kromě výplně 0BFD–0BFF. |
| `mikos/MONITOR_V5.0E_MIKOS_V5.0.bin` | REM-1 EPROM 0000–13FF | Monitor V5.0E + MIKOS V5.0. Zdroj `_ROM\MIKOS v5\MONITOR5.BIN`, MD5 af17aef0…, shodný s výpisy `m5e`/`mi5` kromě výplně na 0801. |
| `mikos/MONITOR_V5.0F_MIKOS_V5.0_MINIGRAF.bin` | REM-1 EPROM 0000–1FFF (8× 2708) | Monitor V5.0F + MIKOS V5.0 (`MONITOR_V5.0F_MIKOS_V5.0.bin`, 0000–13FF), FFh, a ovladač zapisovače ARITMA Minigraf 0507 V.1-MGF (15. 2. 1989) na 1800–1FFF: sapi.cz `data/eplotr_hex.php` („původní verze“ pro EPROM, shodná s výpisem `eplotr_lst`; součet 9Ah). `docs/desky/Minigraf.md`. |
| `mikos/MONITOR_V5.0E_MIKOS_K_V5.0.bin` | REM-1 EPROM 0000–17FF (6× 2708 nebo 3× 2716) | Monitor V5.0E + MIKOS/K V5.0 (6. 4. 88) pro kazetopáskový záznamník dat KZD-1 na DPP-1 (30h). Zdroj `..\SAPI_DOKU\MIKOS5_K\Binárky\MIKOS5K.BIN` (= M5K-EPR1+2+3), MIKOS/K 0C00–17FF shodný s překladem `KZD.MAC`, monitor shodný s `M5E.HEX`. `docs/desky/KZD-1.md`. |
| `mikos/MONITOR_V5.0F_MIKOS_D_V5.0.bin` | REM-1 EPROM 0000–1FFF (8× 2708) | Monitor V5.0F + MIKOS/D V5.0 + EBIOS (ovladač RPD-1) pro diskety. Z `H:\SAPI-1\Klasika\mikos5\m5mid.hex` (= sapi.cz `m5mid_hex`), MD5 428eb954…. EBIOS 1300–17DA shodný s výpisem `ebios_lst`. |
| `aps-5/APS-5_REM-1.bin` | REM-1 2716: EPROM 0–3 (0000–1FFF), 4–6 (6000–77FF) | Původní firmware měřicí jednotky APS-5 (Tesla Liberec, regulace osmi pecí), 7 × 2 KB za sebou, 8. patice prázdná. Z výpisů Intel HEX na `sapi.cz/sapi/doku/aps-5.php` (`..\SAPI.CZ\sapi\doku\aps-5.php`), všechny kontrolní součty sedí, MD5 a1c23bc3…. `docs/desky/APS-5.md`. |
| `jpr-1a/JPR-1A_MON_V1.0-C1.bin` | JPR-1A EPROM D3B (0000–07FF) | JPR-1A 48K MON V1.0-C1 (Tesla Eltos IMA 2/86), boot CP/M ZPS-3 z RPD-1. Zdroj `H:\SAPI-1\SAPI_DOKU_2011-09-10\SAPI_EPROM\BOOT_pro_CPM\B_V10_JPR-1A_RPD-1_ZPS-3.BIN` (shodné s `Foto\Jiri_Slansky\SW\JPR-1A 48K MON V1.0-C1`), MD5 2eb1690d…. Zdrojový kód MON48C1.MAC je v `cpmzps3_soubory.rar` na sapi.cz. |
| `jpr-1a/MOR48A.bin` | JPR-1A EPROM 0000–080B | CP/M 2.2/P 48K boot (R. Peřina 1988), z sapi.cz `data/mor48a_hex.php` (= `rpd_soubory\MOR48A.HEX`), zdroj MOR24.MAC. |
| `jpr-1a/MOR62A.bin` | JPR-1A EPROM 0000–080B | CP/M 2.2/P 62K boot pro RPD-1 (Peřina 1988, „SAPI-1 ZPS3-62K“), kopíruje se na F000. Zdroj autor `..\SAPI-sestavy\ZPS-3\MOR62A.hex`. |
| `jpr-1a/MOR48AF.bin`, `MOR62AF.bin` | JPR-1A EPROM 0000–06C7 | Totéž pro řadič FDC-1 („SAPI-1 ZPS3-48K/FDC-1“, „…62K/FDC-1“), z `MOR48AF.hex`, `MOR62AF.hex`. Zdroj MOR24F.MAC je na disketě. |
| `fdc-1/FDC-1A_D5B_74S287.bin` | FDC-1A PROM D5B (74S287) | Dekodér portů FDC-1: 04–0BH 8257, 0CH–0DH 8272, 0EH–0FH FDC-1B. **Dump** `..\SAPI-karty\FDC-1\PROM_D5B.hex`. |
| `mfc-1a/MFC-1A_DD9_74S287.bin` | MFC-1A PROM DD9 (74S287) | Dekodér portů MFC-1A: 04H, 05H, 08H 8257, 0CH–0DH 8272, 0FH registr mechanik. **Dump** `..\SAPI-karty\MFC-1A\doc\ADRESOVY_DEKODER.BIN`. |
| `mfc-1a/MFC-1A_DD25_74S287_PLL.bin` | MFC-1A PROM DD25 (74S287) | Číslicový fázový závěs datového separátoru, **dump** `EPROM_FAZOVY_ZAVES.BIN`. Emulace ho nepoužívá. |
| `jpr-1s/MIKROBIOS_1.02.bin` | JPR-1S EEPROM 28C64 (0000–1FFF) | SAPI MIKROBIOS 1.02 (04/2022) + Falcon Monitor 6.1L pro JPR-1S, BGP-1S a ANK-1 (Libor Lasota). Z `..\SAPI-karty\JPR-1S\jpr1s_sw\jpr1s.hex` (= `jpr1s.bin`, = `BGP-1S\jpr1s_sw`), prázdná místa FFh. Výchozí BIOS JPR-1S. |
| `jpr-1s/MIKROBIOS_2.1.bin` | JPR-1S EEPROM 28C64 | MikroBIOS 2.1 (11/2024) s displejem AND-1V na 3800h, `BOOT DSM` přes DSM-1. Z `jpr1s_mikrobios\jpr1s.hex`. |
| `jpr-1x/JPR-1X_MIKROBIOS_MIKROBASIC.bin` | JPR-1X ROM IC11 (16 KB, dvě banky 0000–1FFF) | Složený obraz (autor, 2026-10-06): dolní banka = `jpr-1s/MIKROBIOS_1.02.bin`, horní banka = `mikrobasic/MIKROBASIC_2.4_ZPS-1.bin` + FFh. Dump skutečné ROM JPR-1X zatím není. |
| `pkt-1/PKT-1_MON-V100690_EPROM.bin` | PKT-1 EPROM 2716 (SAPI-1.5) | „PKT-1 62K MON-V100690*LCH“, **dump EPROM** (`..\SAPI-karty\PKT-1\PKT1_ZPS-7.BIN`, = `SAPI_DOKU\SAPI_EPROM\BOOT_pro_CPM`), zdroják `System\pkt1.asm` se přeloží bajt po bajtu. ROM BIOS ZPS-6/7 (DBS-1, MFC-1A, Consul 262.3). `docs/software/SAPI-1.5.md` |
| `pkt-1/PKT-1_ROMBIOS_MELSOFT.bin` | PKT-1 EPROM 2716 | Opravená verze (`System\pkt1-rombios.bin`, = Melsoft `ROMBIOS.MAC` z disket 7 a 8): stav tiskárny, časový limit tisku. |
| `pkt-1/PKT-1_DD8_74S287.bin`, `PKT-1_DD12_74S287.bin` | PKT-1 PROM DD8 (paměť), DD12 (I/O) | **Dumpy z desky** (INTELHEX na `SAPI.CZ\sapi\pkt-1.php`, Tesla 6XN 902 54 a 56): EPROM 0000–07FF při BOOT; porty 00–03 a 0E. |
| `pkt-1/PKT-1_DD8_8k_replika.bin` | PKT-1 PROM DD8 | Upravený dekodér z doby replik (`DEK PKT-1 DD8.pdf`): EPROM 0000–1FFF. Vyrobený skriptem, ne dump. |
| `rsw-1/RSW-1_DD1_74S571.bin`, `RSW-1_DD2_74S287.bin` | RSW-1 PROM DD1 (RAM, 512 × 4), DD2 (I/O) | **Dumpy z desky** (`SAPI.CZ\sapi\rsw-1.php`, 6XN 902 51 a 52). |
| `dbs-1a/DBS-1A_CharGen_59.bin` | DBS-1A EPROM M2732A „59“ (znakový generátor) | **Dump z desky** (EC1045, e-mail 2026-10-07, Intel HEX `eprom59.HEX`), MD5 ab4caccaaa204d3f467f02ddeecc36f6. Výchozí. Rámečky, ASCII, semigrafika 2 × 3, KOI-8 čs2 s řeckými písmeny. `docs/desky/DBS-1A.md` |
| `dbs-1a/DBS-1A_CharGen_nahrada.bin` | DBS-1A EPROM 2732 (znakový generátor) | **NÁHRADA** z doby před nalezením dumpu, volba. Vyrábí `tools/make_dbs1a_font.py` z fontu AND-1Z ve stejném rozložení kódů (ASCII, semigrafika 2 × 3, KOI-8 čs2), adresování podle desky (A3 = bit 7 kódu, A11 = RA3). `docs/desky/DBS-1A.md` |
| `jpr-1s/MIKROBIOS_1.0.bin` | JPR-1S EEPROM 28C64 | MIKROBIOS 1.0 (08/2021), první verze pro BGP-1S. Z `JPR-1S\BIOS\bios_jpr1s.hex`. |
| `and-1b/FontAND1B11.bin` | AND-1B EPROM 27C256 (znakový generátor) | Font Libora Lasoty pro AND-1B (sada MHB 2501, 16 bank po 1 KB s atributy blikání, podtržení, dvojitá šířka; obě poloviny 16 KB shodné, deska čte horní). = `..\SAPI-karty\AND-1B\FontAND1B11\FontAND1B11.bmp.bin` (vyrobený `main.c` z `FontAND1B11.bmp`). |
| `and-1a/AND-1A_D3A_CPM62K_74S287.bin` | AND-1A PROM D3A | Domácí varianta pro CP/M 62K: F800 (S1 sepnutá 3800). Podle tabulky `SAPIMAN\CPM_SYSTÉMY\ZMENA_CPM\FUNKCNI\JEN 8 PALCU\FDC\FDC_AND1A130711_RMD\dekoder AND1ACPM.txt`. |
| `jpr-1a/MON64C3.bin` | JPR-1A EPROM 0000–07FF | „JPR-1A 64K V C3“, boot originálního CP/M 64K Tesla IMA z RPD-1. Zkopíruje se na F800 a vypne EPROM, video F000. Z `..\SAPI_DOKU\SAPI_EPROM\BOOT_pro_CPM\JPR1A_BOOT_64k.BIN` (= `JPR1A_BOOT_64k_C3.BIN` na FTP), zdroj `MON64C3.MAC`. |
| `and-1a/AND-1A_D3A_CPM64K_74S287.bin` | AND-1A PROM D3A | Varianta pro CP/M 64K: F000 (položka EFh místo DFh), S1 sepnutá 3800. **Předpoklad**. |
| `ram-1a/RAM-1A_D1A_64K_74S287.bin` | RAM-1A PROM D1A | Standardní D1A, jen S1 S2 = 10 dává C000–EFFF a F800–FFFF (položky BCh, BDh = F0h, díra F000–F7FF pro AND-1A). Druhá RAM-1A pro CP/M 64K. **Předpoklad**. |
| `ram-1a/RAM-1A_D1A_62K_74S287.bin` | RAM-1A PROM D1A | Standardní D1A, jen S1 S2 = 10 dává C000–F7FF místo C000–FFFF (druhá RAM-1A pro CP/M 62K). **Předpoklad**, ne doložená PROM. |
| `jpr-1a/JPR-1A_D2C_74S571.bin` | JPR-1A PROM D2C (74S571, 512 × 4) | Dekodér EPROM a portů. **Přepis tabulky** z návodu (sapi.cz `jpr-1a.php`), ne dump. |
| `and-1a/AND-1A_D3A_74S287.bin` | AND-1A PROM D3A (74S287) | Dekodér video RAM (E800, s S1 3800). Přepis z návodu (`and-1a.php`). D3C AND-1A je shodná s `and-1/AND-1_D3C_74188.bin`. |
| `ram-1/RAM-1_D1A_74S287.bin` | RAM-1 PROM D1A | Standardní dekodér RAM-1 (4000–…, tab. 1). Přepis z návodu (`ram-1.php`, 1. tabulka). |
| `ram-1/RAM-1_16K_D1A_74S287.bin` | RAM-1/16K PROM D1A | Druhá tabulka z `ram-1.php` (RAM-1/16K: C000–E7FF + F000–FFFF, C000–FFFF, C000–E7FF, 0000–BFFF). |
| `ram-1a/RAM-1A_D1A_74S287.bin` | RAM-1A PROM D1A | Dekodér RAM-1A (ZPS-3: 0000–BFFF). Přepis z návodu (`ram-1a.php`), shodný se strojopisnou tabulkou `ram-1a_pri.png`. |
| `jpr-1z/MOR62Z.bin` | JPR-1Z EPROM 2716 | CP/M 2.2/P 62K boot pro ZPS-4 (R. Peřina 1988, „SAPI-1 ZPS4-62K“), z sapi.cz `data/mor62z_hex.php`. |
| `jpr-1z/MOR62ZF.bin` | JPR-1Z EPROM 2716 | Totéž pro řadič FDC-1 („SAPI-1 ZPS4-62K/FDC-1“, 1700 B), z sapi.cz `data/mor62zf_hex.php`. |
| `jpr-1z/ELIS_V7.01.bin` | JPR-1Z EPROM 2716 | Monitor ELIS 62k MON-č R62Z v7.01 *JME/JPE (Z80, C259.11/C262.3 + K6313). Zdroj `H:\SAPI-1\Petr_repliky_k_vyrobe\JPR-1ZI\JPR-1Z_BIOS_ELIS.BIN`. |
| `jpr-1z/ELIS_V3.00.bin` | JPR-1Z EPROM 2716 | Monitor ELIS V3.00 *JME (8080 kód, C262.3/CS + D100/P), z sapi.cz `data/elis_hex.php`. |
| `jpr-1z/JPR-1Z_D14_74S287.bin` | JPR-1Z PROM D14 (= D3C ZI) | Dekodér paměti, dump `Petr_repliky_k_vyrobe\JPR-1ZI\JPR-1Z_PROM_D14.hex`. |
| `jpr-1z/JPR-1Z_D10_74S287.bin` | JPR-1Z PROM D10 (= D6B ZI) | Dekodér I/O, dump `JPR-1Z_PROM_D10.hex` (shodný s `JPR-1ZI_D6B.bin`). |
| `ram-1z/RAM-1Z_D3B_74S571.bin` | RAM-1Z PROM D3B | Dekodér RAM, dump `Petr_repliky_k_vyrobe\DMR-1\RAM-1Z_PROM_B9.hex` (shodný s HEX v návodu). |
| `rmd-1/RMD-1_74S287.bin` | RMD-1 PROM | Dekodér portů (40–43, 45), z návodu `rmd-1.php`. Přiřazení O3/O4 je k ověření. |
| `jpr-1v/JPR-1V_28C64_MikroBIOS_ECMON.bin` | JPR-1V EEPROM 28C64 | Banka 0 (0000–07FF) MikroBIOS 3.0 AllRAM (Libor Lasota 2024, „JPR-1V/RAM-1V/AND-1V, Consul 262.3“), banka 1 ECMON IDE-1 (= `jpr-1zi/JPR-1ZI_ECMON_2k_IDE-1_Martin.bin`), zbytek FF. Rozdělení bank je rozhodnutí autora. |
| `jpr-1v/JPR-1V_MikroBIOS_3.0.hex` | | MikroBIOS 3.0 v Intel HEX, z `H:\SAPI-1\LiborLasota\JPR-1V\@orig\JPR-1V_monitor.zip` (`JPR-1V/jpr1v.txt`). Binárka se souhlasem autora, zdroje ne. |
| `and-1v/font_and1v22_1024.bin`, `newfont_and1v22_1024.bin` | AND-1V rev 2.x 27C256 | Fonty 10×24 typ 1 a typ 2, z `..\SAPI-karty\AND-1V\rev2.3\font_and1v22\`. |
| `and-1v/font_and1v22_1012.bin` | AND-1V rev 2.1 28C64 | Font 10×12. |
| `and-1v/FontAND1V_rev1.bin` | AND-1V rev 1.x 28C64 | Dvě sady 6×12, z `..\SAPI-karty\AND-1V\FontROM\FontAND1V.bmp.bin`. |
| `and-1/MHB2501.bin` | AND-1 znakový generátor MHB 2501 (latinka) | **Překreslený**, ne dump (p-MOS ROM, dump neexistuje). Podle tabulky v katalogu MHB250x (`Foto\Jiri_Slansky\Dokumentace\AND-1 a 1A\MHB250x.png`), automaticky převedeno, několik vad skenu opraveno ručně. 64 znaků × 8 řádků, bity 4–0 = sloupce zleva, řádek 8 nevyužit. |
| `and-1/MHB2502.bin` | AND-1 MHB 2502 (azbuka) | Překreslený stejně. Kódy 20–3Fh převzaté z MHB 2501. |
| `and-1/AND-1_D3C_74188.bin` | AND-1 D3C (74188) | Kurzor, blikání a dvojitá šířka, 32 B z výpisu na sapi.cz (`and-1.php`). |

Zdrojové kódy MikroMonu se do tohoto repozitáře nedávají.
