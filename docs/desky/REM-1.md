# REM-1 – deska pamětí RAM a EPROM

Zdroje: `BKP\rem-1.php` (OCR manuálu), `BKP\pdf\rem-1.pdf`, `BKP\obrazky\rempri1.png`, `rempri3a.png`,
`rempri3b.png`, `rempri4.png`, skeny `H:\SAPI-1\Foto\Jiri_Slansky\Dokumentace\REM-1\*.tif`, Eagle
`D11\sapi_eagle\REPLIKY\REM-1\` (zkratky viz JPR-1.md).

## Zapojení

- **Kapacita:** 8 patic EPROM (2708 = celkem 8 KB, 2716 = 16 KB) a 16 patic 2114 (až 8 KB RAM).
- **Dekodér D3B (3205):** A15–A13 dělí paměť na bloky po 8 KB, během /RES je vypnutý. Propojky R (RAM), E a F
  (EPROM) připojují výstupy 0–7.
- **EPROM:** D3C vybírá patici. U 2708 adresami A10–A12 (propojky A-M, B-N, C-P = 1 KB stránky, všech 8 patic
  v bloku E), u 2716 adresami A11–A12 (4 patice v bloku E, 4 v bloku F). Napájení patic propojkami H/G/I/J na
  U/V/T/W.
- **RAM:** D7A vybírá 1 KB stránky od začátku bloku R.
- **Výrobní nastavení:** R-2 (RAM 4000–5FFF), E-0 (EPROM 0000–1FFF), A-M, B-N, C-P, H-U, J-U, I-V, G-V.
- **Osazení v ZPS-1:** 4× MHB 8708C s MIKROBASICem a Mikromonitorem (0000–0FFF), 4× 2114 = RAM 2 KB
  (4000–47FF).
- **Osazení v ZPS-2 (MIKOS):** EPROM s Monitorem V4.0 a MIKOSem (0000–0FFF), RAM na REM-1 odpojená
  přerušením R-2, paměť je na RAM-1 (4000–FFFF).
- Volitelný generátor čekání pro pomalé EPROM (74121) se neosazoval.

## Emulace (`src/cards/rem1.*`)

- Parametry: `rom` (obsah EPROM od 1. patice), `eprom_type` (2708/2716), `eprom_block` (E), `eprom_block2` (F,
  jen 2716), `ram_block` (R), `ram_kb` (osazená RAM).
- Karta odpovídá v celém dekódovaném bloku. Prázdné patice čtou FFh (předpoklad).

## Nejasnosti

- Co CPU čte z prázdných patic EPROM a RAM (FFh, nebo plovoucí sběrnice).
