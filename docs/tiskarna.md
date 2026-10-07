# Tiskárna

Stav: **emulovaná** od 2026-10-06 (`src/chips/printer.*`, test `test_printer`). Tiskárna s rozhraním Centronics
(EPSON LQ-1070, Robotron K6313…) jde připojit na konektor X3 procesorových karet a na desku DPP-1.

## Kde ji systémy mají

- **ECMON i monitory J. Bíby** (např. MON200412) tisknou `LST:` (IOBYTE LST = LPT, po startu) přes **X3
  procesorové karty**. Ovladač „EPSON LQ-1070 CENTRONIX“ (`lst1` v `ECMONEC.MAC`):
  1. `OUT P2` data;
  2. `IN P2`: při bitu 7 = 1 znak zahodí a skončí, při bitu 6 = 1 (BUSY) čeká;
  3. `OUT P1,1` a `OUT P1,3`: pulz /STROBE na P1-OUT1.
- **DPP-1 kanál 1** (28h–2Bh) je v ECMONu `PUN:`, tedy děrovač. Řídicí slovo je AEh (port A je výstup v módu 1),
  `0Dh` nastaví INTE (PC6). Výstup čeká na /OBF = 1 (PC7) a INTR (PC3). Tiskne se tam při `LST: = UL1:`
  (IOBYTE LST = 11), protože `UL1:` vede na `PUN:`.
- `^P` v CP/M posílá výstup konzole i na `LST:`. Nejjednodušší zkouška je `^P` a pak `DIR`.

## Zapojení v emulátoru

**X3 karet JPR-1ZI, JPR-1Z a JPR-1V** (kabel z návodu JPR-1Z pro Robotron K6313, Centronics):

| Signál | Port | X3 |
|---|---|---|
| data D0–D7 | P2-OUT (03h) | 3, 5, 6, 8, 10, 2, 4, 7 |
| /STROBE | P1-OUT1 (02h) | 26 |
| BUSY | P2-IN6 | 15 |
| kolík 16 | P2-IN7 | 16, parametr `printer.pin16` (výchozí 0, při 1 ECMON znaky zahodí) |

- Bajt se převezme při **náběžné hraně /STROBE** (konec pulzu). Po resetu je P1-OUT vynulovaný, /STROBE tedy
  aktivní. ECMON ho při startu v `CINIT` (`OUT P1,3`, „init LST“) uvolní a tiskárna dostane bajt 00h z P2.
  Emulace ho zapíše do souboru, v textu se neukáže.
- Volné vstupy P2-IN0–5 čtou 1.
- JPR-1 a JPR-1A (8080) tiskárnu nemají: P2-IN je u nich vstup požadavků přerušení (MH3214).

**DPP-1** (parametr `printer_channel`, výchozí kanál 1): port A v **módu 1 výstup**. /OBF (PC7) je /STROBE
tiskárny a její /ACK vede na PC6 (/ACK 8255). Po zápisu na PA tiskárna bajt převezme. Po době BUSY
pošle /ACK, /OBF se vrátí do 1 a nastaví se INTR. V módu 0 se netiskne nic, signál BUSY na PC zapojený není.
Kanál nesmí být stejný jako kanál PMD32-SD.

## Parametry (sekce `printer`)

- `output`: soubor, do kterého jdou bajty tak, jak přišly, včetně řídicích sekvencí ESC/P. Výchozí je
  `work/printer/tiskarna.prn` a soubor se jen doplňuje. Testy do něj nepíšou (`battery_files_enable`).
- `charset`: znaková sada textového zobrazení pro 80h–FFh. Výchozí je `koi8cs` (jako obrazovka SAPI, C0–FFh
  česká písmena), dál `kamenicky`, `cp852` a `ascii`.
- `busy_us`: jak dlouho po každém bajtu drží BUSY (výchozí 100 µs, jako tiskárna s vyrovnávací pamětí).
  Hodnota 0 je okamžitý tisk, tedy odchylka.

## Zobrazení

- **Okno Tiskárna** (Zobrazení → Tiskárna) ukazuje vytištěný papír po tiskárnách. Tlačítko Nový papír vymaže
  zobrazení (soubor pokračuje), Kopírovat text zkopíruje text do schránky. `--ui-printer` okno otevře ve
  snímku `--ui-snapshot`.
- **Text:**
  - sekvence ESC/P se vynechají, včetně grafiky `ESC *`, `ESC K` a dalších;
  - CR vrátí vozík a další znaky přetisknou řádek (podtržítko a mezera písmeno nechají);
  - LF ukončí řádek, FF přidá oddělovač stránky, HT skáče na násobky 8, BS vrací o znak;
  - drží se posledních 5000 řádků.
- **MCP:** `printer_output` vrátí poslední řádky textu, cestu k souboru a počet bajtů. S `clear` pak začne nový
  papír.

## Sestavy

- Všechny sestavy `cpm_*` s ECMONem a monitory J. Bíby mají tiskárnu na X3 (`work/printer/tiskarna.prn`), kromě
  `cpm_term64k_fdc1` (jen terminál). Monitory MON130711, 271111 a 281211 tisknou stejně jako ECMON (ověřeno
  `^P` a `DIR`).
- `cpm_zrmd_dpp1.sapi` má navíc druhou tiskárnu na DPP-1 kanálu 1 (`work/printer/dpp1.prn`, `LST: = UL1:`).

## Nejasnosti k ověření na HW

- Co je v kabelu k tiskárně na kolíku 16 X3 (P2-IN7)? ECMON při 1 netiskne, emulace ho drží v 0. (EC1045 neví,
  na X3 měl jen plotter MINIGRAF přes rozhraní z AR 1988 s přepínači polarity STB a ACK.)
- Bere tiskárna (LQ-1070, K6313) data při sestupné, nebo náběžné hraně /STROBE? Emulace je bere při náběžné.
- Tiskárna na DPP-1: jak byl zapojený kabel (/OBF → /STROBE, /ACK → PC6, BUSY?) a přepínače S2–S4? Tiskl
  někdo přes `UL1:`, nebo tam byl jen děrovač?
