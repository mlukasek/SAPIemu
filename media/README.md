# Obrazy médií

Tyto soubory jsou **nedotčené originály**. Emulátor si je při prvním použití zkopíruje do `work/`, který je
v `.gitignore`, a pracuje jen s kopiemi. Originál se změní pouze tehdy, když to uživatel v konfiguraci výslovně nastaví.

## `pmd32sd/` – obsah SD karty PMD32-SD

Zdroj: `..\PMD32-SD\SW`. Geometrii obrazů určuje `images.cfg`.

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

Zdroj: `..\ZRMD-1\SW`.

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
| `MIKOS_D_V5.0_system.img` | Systémová disketa MIKOS/D V5.0 („ZALOZNI“), 77 × 26 × 128 B v logickém pořadí. Zdroj `H:\SAPI-1\Klasika\mikos5\m5mid_img.rar` = sapi.cz `data2/m5mid_img.rar`, shodná s daty v `m5mid.IMD`. Obsahuje BAS5C, TEDIT, TOOL, BASIMS4, KONVERT, COPYSYS, X2 a dokumentaci (.DOC). |
