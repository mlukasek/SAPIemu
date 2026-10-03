# MIKOS V4 (Monitor V4.0 + kazetový operační systém)

Zdroje: manuál „Kazetopáskové programové vybavení SAPI-1 V4.0“ (1985) v `BKP\sapi\doku\zps2-v4.php` a
`BKP\pdf\zps2.pdf`, výpisy `BKP\sapi\data\m40f_*.php` (monitor) a `mi40f_*.php` (MIKOS), programy
`H:\SAPI-1\MIKOS_PGM\` (HEX, shodné s sapi.cz), nahrávky
`H:\SAPI-1\SAPI_DOKU_2024-01-03\Kazetopáskové vybavení AUDIO WAV\` (BASIC EX V4.0, TOOL 48, TOOL 80).

## Verze v emulátoru

| Soubor | Obsah | Klávesnice |
|---|---|---|
| `roms/mikos/MIKOS_V4.0F_ANK-1.bin` | Monitor V4.0 + MIKOS-1 V4.0F | ANK-1 |
| `roms/mikos/MIKOS_V4.0D_ANK-1.bin` | Monitor V4.0 (C) + MIKOS-1 V4.0D | ANK-1 |
| `roms/mikos/MIKOS_V4.1F_Consul.bin` | Monitor V4.1 + MIKOS-1 V4.0F | Consul (zatím bez sestavy) |

Sestava `machines/zps/ZPS-2.sapi`: JPR-1, REM-1 (EPROM 0000–0FFF, RAM odpojená přerušením R-2), RAM-1/48K
(4000–FFFF), AND-1, DSM-1 na 10h s magnetofonem, BDK-1, ANK-1 (profil `mikos`).

## Paměť a vstupy

- ROM 4 KB: monitor 0000–07FF, MIKOS 0800–0FFF.
- Systémová RAM 4000–42FF: vektory RST od 4000, IOBYTE 40ED, MEMTOP 40EE, buffery kazety 40F1 a 41F1.
- Programy od 4400: BASIC (start 4400, teplý start 4403), TOOL (4C00/4C03).
- Monitor nepovoluje přerušení. Při každé výzvě zapisuje 02h na 3000h (3214). BASIC EX ale provede EI (4CF7).
  Manuál proto doporučuje vypnout přerušení desek: AND-1 bez propojky 49–50 (INT1), JPR-1 bez 10–11. V sestavě
  jsou `int1`, `int0` a `int_key` vypnuté.
- DSM-1: `OUT 13h` reset, `OUT 11h,0Dh` terminál; kazeta `OUT 11h,FFh` a `OUT 10h,38h` čtení / `30h` zápis.

## Ovládání

- Studený start vypíše `MIKOS-1 V4.0F +`, `SAPI-1 V4.0` a výzvu `.`.
- Příkazy monitoru: A, C, D, F, G, M, Q, S, X, K (kazeta), J (kontrola a start programu zavedeného `.K_L`).
- Oddělovač parametrů se píše mezerou a na obrazovce je `=` (`.D=4400=440F`).
- MIKOS (`.K` a písmeno):
  - `I` inicializace pásky (jméno svazku), `L` čtení, `S` zápis (`=začátek=konec`), `A` zápis bez vyhledávání,
    `D` adresář, `M` motor, `O`/`C`/`F` otevření a uzavření souborů, `B` čtení z MIKROBASICu.
  - **Nahrání programu:** `.K` `L`, jméno nebo `*` (první soubor), `PLAY  DONE?` → `Y`, po nalezení hlavičky
    `TOOL 80 --(Y/N)?` → `Y`. Na prvním řádku se ukazuje jméno a číslo bloku. Potom `.J` program spustí.
  - **Uložení bloku paměti:** `.K` `A` (`RECORD  DONE?` → `Y`, jméno), pak `.K` `S` a `5000 52FF` + CR.
- **BASIC EX a kazeta** (ověřeno v emulátoru přes MCP):
  - `SAVE` uloží program jako text (ASCII, bloky typu `A`, na konci ^Z), `LOAD` ho načte. LOAD nemaže starý
    program (přepisuje řádky se stejnými čísly), čistí se příkazem `NEW`.
  - IOBYTE po startu je A9h (`.Q` ukáže `C=C R=1 P=1 L=L`), čtečka i děrovač jsou už na magnetofonu.
  - **Prázdná kazeta:** `SAVE` se ptá `LAST NAME` a hledal by konec předchozího souboru. Výstupní soubor se proto
    otevře v monitoru: `BYE`, `.K` `A`, `RECORD  DONE?` → `Y`, jméno, `.B` (program zůstane), `SAVE`.
  - **Kazeta s předchozím souborem:** `SAVE`, `LAST NAME` = jméno posledního souboru, PŘEHRÁT a `Y` (přečte
    ho), `RECORD  DONE?` → NAHRÁT a `Y`, `NEW  NAME` = jméno nového souboru.
  - **Načtení:** PŘEHRÁT, `LOAD`, jméno, `PLAY  DONE?` → `Y`.
  - Jméno v rámci bloku je uložené v kódech obrazovky AND-1 (AND 3Fh), v datech hlavičky v ASCII. Skutečná
    nahrávka TOOL 80 ho má stejně.
- MIKOS bere klávesu až chvíli po vypsání otázky. Klávesa stisknutá dřív se ztratí (ověřeno v emulátoru).

## Formát pásky

- Bloky: 3× 10h, typ, 255 bajtů dat, jméno 11 znaků, číslo bloku, číslo souboru (BCD), kontrolní součet
  (součet 269 bajtů mod 256), 10h, 00h. Celkem 275 bajtů, UART 8 bitů, 2 stop bity, 2400 Bd.
- Typy bloků: 01 hlavička, `:` svazek, `A` ASCII, `B` binární, 04 poslední.
- Mezery mezi bloky 800 ms (dlouhá, magnetofon jde zastavit), 100 ms před binárními bloky.
- Binární soubor: `'<' konec_lo konec_hi začátek_lo začátek_hi '>' KS1 data KS2 1Ah`.
- TOOL 80 a BASIC EX zabírají 4400–5FFF (30 bloků, status `1E`), TOOL 48 má 58 bloků.

## Ověřeno v emulátoru

`tests/test_mikos.cpp`:
- start a výzva;
- `.K_A` + `.K_S` na novou kazetu, vymazání paměti, `.K_L` zpět (data souhlasí);
- skutečné nahrávky (když jsou na disku): TOOL 80 z WAV (`.J` → `* PEDIT V2.5 *`) a BASIC EX z MP3 (`READY`,
  `PRINT 12*34+5` = 413).
