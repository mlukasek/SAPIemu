# MIKOS V4 a V5 (Monitor + kazetový operační systém)

Zdroje: manuál „Kazetopáskové programové vybavení SAPI-1 V4.0“ (1985) na [sapi.cz/sapi/doku/zps2-v4.php](https://sapi.cz/sapi/doku/zps2-v4.php) a
[sapi.cz/sapi/pdf/zps2.pdf](https://sapi.cz/sapi/pdf/zps2.pdf), výpisy `sapi/data/m40f_*.php` (monitor) a `mi40f_*.php` (MIKOS) na sapi.cz,
programy v archivu SAPI_DOKU (`MIKOS_PGM`, HEX, shodné s sapi.cz), nahrávky z archivu SAPI_DOKU (složka `Kazetopáskové vybavení AUDIO WAV`:
BASIC EX V4.0, TOOL 48, TOOL 80).

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
    **BASIMS** (`docs/desky/DPS-1.md`) má na `.B` studený start a program smaže, u něj `.G4403` (teplý start).
  - **Kazeta s předchozím souborem:** `SAVE`, `LAST NAME` = jméno posledního souboru, PŘEHRÁT a `Y` (přečte
    ho), `RECORD  DONE?` → NAHRÁT a `Y`, `NEW  NAME` = jméno nového souboru.
  - **Načtení:** PŘEHRÁT, `LOAD`, jméno, `PLAY  DONE?` → `Y`.
  - Jméno v rámci bloku je uložené v kódech obrazovky AND-1 (AND 3Fh), v datech hlavičky v ASCII. Skutečná
    nahrávka TOOL 80 ho má stejně.
- MIKOS bere klávesu až chvíli po vypsání otázky. Klávesa stisknutá dřív se ztratí (ověřeno v emulátoru).

## MIKOS V5.0 (Monitor V5.0, Consul)

Zdroje: OCR „Programové vybavení V5“ (ELTOS DIZ 1988) na [sapi.cz/sapi/doku2/v5.php](https://sapi.cz/sapi/doku2/v5.php), výpisy `sapi/data/m5e_*`,
`m5f_*` (monitor), `mi5_*` (MIKOS) na sapi.cz, binárky a `MONITOR5.TXT` v archivu SAPI_DOKU (`MIKOS5`).

| Soubor | Obsah | Zdroj |
|---|---|---|
| `roms/mikos/MONITOR_V5.0F_MIKOS_V5.0.bin` | Monitor V5.0F (0000–0BFF) + MIKOS V5.0 (0C00–13FF), 5 KB | prvních 5 KB `_ROM\MIKOS v5\MIKOS5.BIN` (MD5 celého 29d02716…). Za tím je v originálu na 1400–17FF program přenosu dat (BLOCK DATA TRANSFER) a na 1800–1FFF ovladač zapisovače
XY 4130 (autostart z 1800h, shodný s `xy4130_hex` na sapi.cz); celé je jako `MONITOR_V5.0F_MIKOS_V5.0_XY4130.bin` (sestava `zps/ZPS-2_MIKOS_V5_XY4130`, `docs/desky/XY4130.md`). |
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

- Zdroj: Intel HEX ze sapi.cz ([sapi.cz/sapi/data/bexp_hex.php](https://sapi.cz/sapi/data/bexp_hex.php), [sapi.cz/sapi/data/bexpg_hex.php](https://sapi.cz/sapi/data/bexpg_hex.php)), shodné
  s `BASIC5.HEX` a `BASIC5G.HEX` v archivu SAPI_DOKU (`MIKOS_PGM`). V repu `media/mikos/BASIC_EXP_V5.0.hex` (4400–6B00) a
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

- Zdroj: [sapi.cz/sapi/doku2/v5.php](https://sapi.cz/sapi/doku2/v5.php) (výpisy `mi5d_*`, `ebios_*`, `bios_*`, `bdos_*`), EPROM `m5mid_hex` = 8 KB
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
  - `BASIMS4.SYS` (BASIC s IMS-2), `KONVERT.SYS` (převod kazeta ↔ disketa). BASIMS4.SYS je bajt po bajtu
    BASIMS V4.0 pro MIKOS V4.0F (s hlavičkou souboru), pod MIKOSem/D V5 neběží (volá podprogramy V4.0F). Poběží
    v sestavě `zps/ZPS-2_DPS-1_IMS-2.sapi` (`docs/desky/DPS-1.md`);
  - `COPYSYS.SYS`, `X2.SYS`.

## MIKOS/K V5.0 (kazetopáskový záznamník dat KZD-1)

- Zdroj: `..\SAPI_DOKU\MIKOS5_K\` (návod `MIKOS5K.DOC`, zdroják `KZD.MAC` z 6. 4. 88, překlad, binárky
  3× 2716). V repu `roms/mikos/MONITOR_V5.0E_MIKOS_K_V5.0.bin` = `MIKOS5K.BIN` (Monitor V5.0E 0000–0BFF, MIKOS/K
  0C00–17FF). Sestava `machines/zps/ZPS-2_MIKOS_K.sapi`: jako ZPS-2 s MIKOSem V5, navíc DPP-1 na 30h s KZD-1 na X2.
  Magnetofon na DSM-1 zůstává pro kopírovací program CKM.
- Po resetu `MIKOS/K V5.0 (C) TESLA DIZ 1987` a `MONITOR V5.0`. Příkazy jsou jako u MIKOSu V5 (`.K` a písmeno,
  `NAZEV:`), služby přes 157h zůstaly kompatibilní.
- **Novou kazetu je nutné připravit `.K_I`** (název pásky), jinak se na ni nedá zapsat.
- Pro načtení od začátku pásky použít `.K_Z` nebo jméno s tečkou (`.K_L` `.JMÉNO`), ne `.K_R`. Po K_R a K_D najde
  K_L soubor na špatném místě a skončí `CHYBA - NENI KONEC SOUBORU`. Je to chování originálu, rozbor
  v `docs/desky/KZD-1.md`.
- Chybová hlášení: 30 ZAZNAM NENI POVOLEN (chybí zoubek), 32 CHYBA CTENI DAT, 34 CHYBA OZNACENI BLOKU,
  36 KONEC PASKY, 38 NENI KONEC SOUBORU, 3A NENI BINARNI FORMAT, 3C SOUBOR NEEXISTUJE.
- Kopírovací program CKM (magnetofon ↔ KZD-1) je v `media/mikos/CKM.hex`, opravená kopie (délka bloku po čtení
  z KZD-1) v `CKM_opraveny.hex`, postup v `docs/desky/KZD-1.md` a `docs/navod-kzd1.md`.
- **Ověřeno** (`test_kzd1`): K_I, K_S, K_D, K_Z, K_L (data souhlasí), zápis bez zoubku, kopie přes CKM (originál
  i opravená kopie).

## Zapisovač Minigraf 0507 pod MIKOSem V5

- `roms/mikos/MONITOR_V5.0F_MIKOS_V5.0_MINIGRAF.bin`: Monitor V5.0F + MIKOS V5.0 a ovladač V.1-MGF na 1800h, sestava
  `zps/ZPS-2_MIKOS_V5_Minigraf.sapi`. `.G=1800` inicializace, `.G=1809` test, `.G=180F` kopie obrazovky, `.G=1815`
  tiskárna; z BASICu EXP `CALL(6171…)`. Ukázka `media/mikos/MINIGRAF_DEMO.bas`, podrobně `docs/desky/Minigraf.md`.

## Formát pásky

Podrobně (ověřeno podle zdrojáků ROM a skutečných nahrávek, 2026-10-06): `docs/software/kazeta.md`.

- Bloky: 3× 10h, typ, 255 bajtů dat, jméno 11 znaků (kódy AND-1), číslo bloku, číslo souboru (binárně, ne BCD),
  kontrolní součet (součet 269 bajtů od typu po číslo souboru, mod 256), 10h, 00h. Celkem 275 bajtů, UART
  8 bitů, 2 stop bity, 2400 Bd.
- Typy bloků: 01 hlavička (data = jméno v ASCII), `:` svazek, `A` ASCII, `B` binární, 04 poslední (nese zbytek
  dat). Hlavička je blok 1, data od 2.
- ROM nastavuje mezery 800 ms a 100 ms před binárními bloky; na skutečných nahrávkách je mezi bloky `B` asi 210 ms
  a před posledním blokem asi 1 s. Mezery jsou tón, ne ticho.
- Binární soubor: `'<' konec_lo konec_hi začátek_lo začátek_hi '>' KS1 data KS2 1Ah`; KS1 = součet 4 bajtů adres
  a 3Eh, KS2 = součet dat. `.J` znovu sečte paměť a skočí na začátek (nejnižší adresu).
- `.K_L jméno` porovnává zadané jméno s ASCII jménem v hlavičce jen v délce zadaného jména (shoda začátku).
- TOOL 80 zabírá 4400–5FF3, BASIC EX 4400–5FFF (oba 30 bloků, status `1E` = číslo posledního bloku), TOOL 48
  má 58 bloků.
- **Kazeta z programu:** okno Média → Vložit… → soubor `.hex` nebo `.bin` → formát MIKOS. Emulátor kazetu hned
  vyrobí (`docs/software/kazeta.md`).

## Ověřeno v emulátoru

`tests/test_mikos.cpp`:
- start a výzva;
- `.K_A` + `.K_S` na novou kazetu, vymazání paměti, `.K_L` zpět (data souhlasí);
- skutečné nahrávky (když jsou na disku): TOOL 80 z WAV (`.J` → `* PEDIT V2.5 *`) a BASIC EX z MP3 (`READY`,
  `PRINT 12*34+5` = 413).
