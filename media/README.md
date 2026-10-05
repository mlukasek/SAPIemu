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
| `system.sap`, `emu48.sap` | 8SD | Obecný CP/M 2.2 („BOOT v1.0“), původ nejasný. |

## `zrmd-1/` – obsah RAM disku ZRMD-1

Zdroj: `..\SAPI-karty\ZRMD-1\SW`.

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

## `mikos/` – programy pro MIKOS (Intel HEX)

Zdroj: `H:\SAPI-1\MIKOS_PGM\` (shodné s výpisy na sapi.cz). Nahrávají se v GUI přes Soubor → Nahrát program do
paměti (Intel HEX), přes MCP `load_hex` nebo `sapiemu-cli --load-hex`, a pak se spouští `.B` v monitoru.

| Soubor | Program | Adresy |
|---|---|---|
| `BASIC_EXP_V5.0.hex` | BASIC EXP V5.0 (TESLA DIZ 1986), `BASIC5.HEX` | 4400–6B00 |
| `BASIC_EXP_V5.0G.hex` | BASIC EXP V5.0/G s grafikou pro DGD-1, `BASIC5G.HEX` | 4400–7400 |

## `rpd-1/` – diskety 8" pro RPD-1

| Soubor | Obsah |
|---|---|
| `CPM22P_48A_Perina.img` | CP/M 2.2/P 48K (R. Peřina 1988) pro ZPS-3 s AND-1A a RPD-1, bootovatelná. Zdroj sapi.cz `data2/cpm48a_img.rar` (`CPM48A.img`). Obsahuje BIOS.MAC, BOOT.MAC, MOR24.MAC, SYSGEN, CPM.DOC a další. |
| `CPM22P_62A_Perina.img` | CP/M 2.2/P 62K (R. Peřina 1988) pro ZPS-3 s AND-1A na F800 a RPD-1, k ROM MOR62A. Zdroj autor (`..\SAPI-sestavy\ZPS-3\CPM62A.img`). |
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

## `fdc-1/` – diskety 8" pro FDC-1

| Soubor | Obsah |
|---|---|
| `CPM22P_48AF_Perina.img` | CP/M 2.2/P 48K (R. Peřina 1988) pro ZPS-3 s AND-1A a řadičem FDC-1, k ROM MOR48AF. Zdroj autor (`..\SAPI-sestavy\ZPS-3\CPM48AF.img`). Obsahuje FDC.DOC, FDC.INC, MOR24F.MAC, FORM6.COM (formátovač), MOR*F.HEX. |
| `CPM22P_62AF_Perina.img` | Totéž pro 62K (AND-1A na F800), k ROM MOR62AF (`CPM62AF.img`). |

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

## `zrd-1v/` – test karty RAM disku ZRD-1V

Vlastní program projektu (zdroj `ZRDTEST.ASM`, překlad `python tools/asm8080.py ZRDTEST.ASM zrdtest.hex`).

| Soubor | Program | `SAVE` |
|---|---|---|
| `zrdtest.hex` | ZRDTEST 1.0: baterie (MAX187), čtení 61h, okno zápisu a zámek (rev 2.1), `ZRDTEST [MHz]` | 6 |
