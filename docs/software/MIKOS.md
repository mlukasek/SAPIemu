# MIKOS V4 a V5 (Monitor + kazetový operační systém)

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

## MIKOS V5.0 (Monitor V5.0, Consul)

Zdroje: OCR „Programové vybavení V5“ (ELTOS DIZ 1988) v `BKP\sapi\doku2\v5.php`, výpisy `BKP\sapi\data\m5e_*`,
`m5f_*` (monitor), `mi5_*` (MIKOS), `H:\SAPI-1\_ROM\MIKOS v5\` (binárky a `MONITOR5.TXT`).

| Soubor | Obsah | Zdroj |
|---|---|---|
| `roms/mikos/MONITOR_V5.0F_MIKOS_V5.0.bin` | Monitor V5.0F (0000–0BFF) + MIKOS V5.0 (0C00–13FF), 5 KB | prvních 5 KB `_ROM\MIKOS v5\MIKOS5.BIN` (MD5 celého 29d02716…). Za tím je v originálu ovladač souřadnicového zapisovače XY 4130 (1400–1FFF, autostart z 1800), ten v repu není. |
| `roms/mikos/MONITOR_V5.0E_MIKOS_V5.0.bin` | Monitor V5.0E + MIKOS V5.0, 5 KB | `_ROM\MIKOS v5\MONITOR5.BIN`, MD5 af17aef0… |

- Shoda s výpisy na sapi.cz: MIKOS V5.0 bajt po bajtu. Monitory se liší jen výplní za koncem kódu (0801 u E,
  0BFD–0BFF u F).
- Sestava `machines/zps/ZPS-2_MIKOS_V5.sapi`: stejná jako ZPS-2, jen jiná EPROM (5× 2708) a klávesnice Consul
  262.3 (s úpravou STROBE, 7474). Monitor V5 nepodporuje ANK-1.
- **Klávesnice** na JPR-1 X2: stav = bit 0 na 2400h (0 = znak), data = 2800h negovaná, potvrzení zápisem 3 a 2 na
  2400h. Stejný protokol jako u CP/M na JPR-1ZI (porty 1 a 2). Manuál popisuje zapojení C262.3 na FRB-30 a
  úpravu STROBE. Bez ní se znaky ztrácejí.
- **Přerušení:** monitor zapisuje úroveň (PRER 40EE, výchozí 01h) na 3000h a povoluje přerušení (EI). Zdroje
  přerušení jsou v sestavě vypnuté stejně jako u V4.
- Pod Monitorem V5 je na 1800h místo pro uživatelskou EPROM (obsah C3h = autostart z 1803h). Na 1400h je
  rezerva pro MIKOS/T nebo ovladač displeje.
- **Ovládání kazety** (dialogy česky):
  - `.K` `L`: `NAZEV:`. Motor se spustí sám, bez dotazu PLAY. `*` = první soubor s dotazem `--(A/N)?`
    (odpověď `A` nebo `Y`), Enter = první soubor bez dotazu, `JMÉNO` = hledá ten soubor.
  - `.K` `S`: `NAZEV:` `@JMÉNO` zapíše na místo, kde páska stojí (prázdná kazeta). `JMÉNO` hledá konec souboru
    a ptá se `(A/N)`. Pak `=začátek konec` + CR.
  - `.J` spustí načtený program, `.U` ukáže jeho adresy.
- **Ověřeno** (`test_mikos`): start (`MIKOS V5.0 (C) TESLA DIZ 1987`, `MONITOR V5.0`), `D` z klávesnice Consul,
  `.K_S @DATA` a `.K_L DATA` zpět, BASIC EX V4.0 ze skutečné nahrávky (MP3) se načte, `.J` → `READY`,
  `PRINT 12*34+5` = 413.

## BASIC EXP V5.0 a V5.0/G (pod Monitorem V5)

- Zdroj: Intel HEX ze sapi.cz (`BKP\sapi\data\bexp_hex.php`, `bexpg_hex.php`), shodné s
  `H:\SAPI-1\MIKOS_PGM\BASIC5.HEX`, `BASIC5G.HEX`. V repu `media/mikos/BASIC_EXP_V5.0.hex` (4400–6B00) a
  `BASIC_EXP_V5.0G.hex` (4400–7400). Nahrávky na kazetě nemáme.
- **Nahrání:** v GUI Soubor → „Nahrát program do paměti (Intel HEX)…“, pak `.B`. Přes MCP `load_hex`, v
  `sapiemu-cli --load-hex`.
- BASIC EXP je shora kompatibilní s BASIC EX V4.0, navíc má REPEAT/UNTIL, WHILE/DO/WEND, IF-THEN-ELSE s bloky
  BEG/BEND, PROC/ENDP s parametry, RENUM, SAVEB/LOADB…
- **V5.0/G** přidává grafiku na desce DGD-1 (`docs/desky/DGD-1.md`): SCALE, CLSG, COLOR, MOVE, PLOT, DRAW,
  LABEL, AXES, FILL, GETP… Souřadnice 0–319 × 0–239, y = 0 dole. Sestava `ZPS-2_MIKOS_V5.sapi` má DGD-1 na C000h
  a RAM-1 omezenou na 4000–BFFF. Grafika se ukazuje v okně „DGD-1“.
- **Ověřeno** (`test_mikos`): V5.0 naběhne, REPEAT/UNTIL a WHILE/WEND. V5.0/G: CLSG, PLOT na správné body
  v paměti DGD-1.

## MIKOS/D V5.0 (diskety, RPD-1)

- Zdroj: `BKP\sapi\doku2\v5.php` (výpisy `mi5d_*`, `ebios_*`, `bios_*`, `bdos_*`), EPROM `m5mid_hex` = 8 KB
  (Monitor V5.0F 0000–0BFF, MIKOS/D 0C00–12FF, EBIOS 1300–17DA), systémová disketa `m5mid_img.rar`
  (záložní kopie, IMD 4/2018). Dokumentace z diskety: `MIKOS_D.DOC`, `MONIT.DOC`, `BAS5.DOC`, `TEDIT.DOC`…
- V repu: `roms/mikos/MONITOR_V5.0F_MIKOS_D_V5.0.bin`, `media/rpd-1/MIKOS_D_V5.0_system.img`, sestava
  `machines/zps/ZPS-2_MIKOS_D.sapi` (ZPS-2 + RPD-1 + 8" mechanika, RAM-1/48K, Consul).
- Po resetu: `MIKOS/D V5.0 (C) TESLA DIZ 1988`, `ZAVEDEN DISKOVY SYSTEM`, `VOLNA RAM OD ADRESY 4400H DO EFFFH`.
  Bez diskety `NELZE ZAVEST DISKOVY SYSTEM`. Systém (BDOS a BIOS v RAM F000–FFFF) se zavádí ze stop 0 a 1.
- **Disketa:** formát CP/M 2.2 (8" SS SD, 2 systémové stopy, bloky 1 KB, 64 položek adresáře, 241 KB, skew 6).
- **Ovládání:** `K` otevře MENU, příkaz se volí prvním písmenem. Pak následuje `NAZEV:` (jméno 8.3, `A:`/`B:`,
  hvězdičky):
  - DIR (Enter = vše), IDIR, ERA, TYPE, RENAME;
  - LOAD a `.J` spustí;
  - SAVE (jméno s příponou, pak `začátek konec`);
  - FIND, OPEN, CLOSE.
- **Pozorování v emulátoru:**
  - LOAD bez přípony hlásí `CP/M.. ERROR` (soubor nenalezen), s příponou (`BAS5C.SYS`) funguje. Návod
    přitom slibuje výchozí `.SYS`.
  - Načítání je pomalé, 10 KB asi 17 s (viz `docs/desky/RPD-1.md`).
  - Stisk klávesy během LOAD čtení přeruší.
- **Programy na disketě:**
  - `BAS5C.SYS` (BASIC EXP V5.0), `TEDIT.SYS`, `TOOL.SYS`;
  - `BASIMS4.SYS` (BASIC s IMS-2), `KONVERT.SYS` (převod kazeta ↔ disketa);
  - `COPYSYS.SYS`, `X2.SYS`.

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
