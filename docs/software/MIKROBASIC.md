# MIKROBASIC

Zdroje: manuál „Programové vybavení SAPI-1“ (TESLA ELTOS 1983) v `BKP\doku2\mikrobasic.php`, výpisy MACRO-80
(09-Dec-81) `BKP\data\mb_*_{lst,asm,hex}.php`, skeny `D11\Mikrobasic\original_dokumentace\`.

## Verze v emulátoru

| Soubor | Verze | Kde | RAM | Sestava |
|---|---|---|---|---|
| `roms/mikrobasic/MIKROBASIC_2.4_ZPS-1.bin` | 2.4 s Mikromonitorem (ELTOS 1983) | REM-1 EPROM 0000–0FFF | REM-1 4000–47FF | `machines/zps/ZPS-1.sapi`, `ZPS-2.sapi` |
| `roms/mikrobasic/MIKROBASIC_AR1983_1k.bin` | starší z AR/B 1983 (bez REWIND) | JPR-1 EPROM 0000–0FFF | JPR-1 2000–23FF | `machines/zps/MIKROBASIC_AR1983.sapi` |

Na sapi.cz je ještě verze 2.4 pro RAM na JPR-1 a verze 3.1 pro klávesnice Consul (jen HEX a výpisy, ne dumpy).

## MIKROBASIC 2.4

- Vstupní body: studený start 0019, teplý start 004D (READY), výzva 0070, tabulka klíčových slov 00DC–0267.
- Vstup a výstup: LOAD 0B23, SAVE 0BB3 (DSM-1 na portech 10h–12h), výstup znaku na displej 0C67 (OUTDIS),
  klávesnice 0D6A (CI).
- RAM: proměnné od 4000 (`PORT24` = stín P0 na 4000h), TXTUNF 4024, zásobník 40B1, program 40EF–47AB, vstupní
  buffer 47AD, vektory RST na 47EE.
- **Displej:** píše na 3800 + řádek·64 + sloupec, kód znaku = ASCII AND 3Fh plus režim (proměnná MODE), kurzor
  = bit 7. Maže 2048 bajtů od 3800, roluje po 40 bajtech.
- **Klávesnice ANK-1:** skenuje řádky D0–D4 (zápis na 2400h, bity D7–D5 ze stínu), čte sloupce z 2400h. Kód =
  (řádek)·8 + 7 − bit, SHIFT = kód 7. Tabulky na 0DFA (bez SHIFT) a 0E22 (SHIFT). Klávesa vedle CR je BS (08h),
  SHIFT+P = DEL. Po přijetí klávesy pípá asi 65 ms a další klávesu bere až po kole skenování bez stisku.
- Přerušení nepovoluje (nemá EI). Klávesu INT čte přímo z P1-IN7.

## Kazeta (DSM-1 na 10h)

- **SAVE:** `NAME =` jméno, pak motor (`OUT 10h,30h`: relé + KAZ, záznam), UART 8 bitů, 2 stop bity, úvodní tón,
  `'B'`, jméno, CR, konečná adresa programu (2 bajty), program, kontrolní součet, motor stop (`OUT 10h,10h`).
- **LOAD:** motor (`OUT 10h,38h`: relé + KAZ + čtení), čeká na ticho, vypíše `*`, hledá `'B'`, vypíše jméno, načte
  program a ověří součet (jinak ERROR).
- **REWIND:** pustí motor a čeká na klávesu (převíjí se na magnetofonu).
- V emulátoru: okno Média → „Nová kazeta…“ (nebo vložit WAV/MP3), stisknout NAHRÁT a napsat SAVE. Pro čtení
  Převinout, PŘEHRÁT a napsat LOAD.

## Ověřeno v emulátoru

`tests/test_cassette.cpp`: SAVE na novou kazetu, NEW, LOAD (`*PROG`), RUN.
`tests/test_mikrobasic.cpp`: naběhne READY, `PRINT 12*34+5` = 413, program s FOR/NEXT, LIST, znaky se SHIFT
z ANK-1. Verze AR/B 1983 naběhne z EPROM na JPR-1.
