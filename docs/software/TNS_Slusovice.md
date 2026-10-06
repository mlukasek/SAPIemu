# Diskety ze Slušovic (TNS CP/M, MIC CP/M): nejsou pro SAPI-1

Rozbor 2026-10-06, jen čtením obrazů, originály beze změny. Obrazy:
`..\SAPI_DOKU_FTP\ZALOHY_8SD_EC1045.01\nove_a_vse_co_mám\DEX\SAPI_ZPS3\04A.8SD`, `04B.8SD`, `05.8SD`
(256 256 B, 8" SS SD IBM 3740, fyzické pořadí sektorů), výpis adresářů `obash_SAPI_ZPS3.txt` (cp852).

## Závěr (jistota vysoká)

Diskety jsou pro **počítače TNS z JZD AK Slušovice**: Z80 s přerušením IM 2, displej 64×16 na FC00h, CRT na portu B0h,
klávesnice na B8h, řadič 8" disket na portech 60h–78h. S žádnou kartou ani monitorem SAPI-1 nesouhlasí procesor,
porty, řadič, displej, přerušení ani zavaděč. Ve složce `SAPI_ZPS3` jsou nejspíš jen proto, že se archivovaly spolu
s jinými 8" disketami (u 05 je ve výpisu poznámka „CPM SYSYTEM distribucni disketa TNS ??“).

- **04A, 04B:** **TNS CP/M V2.27-K** („JZD AK Slusovice 24.7.1986 FFM – dan“), úvodní hlášení BIOSu upravené na
  „UB Usti nad Orlici“ (VÚB Ústí nad Orlicí). CCP a BDOS jsou **bajt po bajtu shodné** s `CCPTNS.PRN`
  a `BDOSTNS.PRN` J. Bíby, BIOS je stejný kód jako jeho `BIOSTNS.MAC` („JAN 86 – TNS“), jen jinak sestavený.
- **05 (a `SYS.COM` na 04B):** **MIC CP/M 2.30 / 2.32** („MIC – Software Slušovice 1987“), modulární systém pro
  stejný hardware TNS. Zavaděč **OSBOOT-F V1.06** na stopě 0 nahraje systém ze souboru `SYS.SYS`.

J. Bíba měl zdrojáky TNS ve složce `..\SAPI_DOKU_FTP\SAPIMAN\CPM_SYSTÉMY\ZMENA_CPM\POROVNANI TNS\` a porovnával je
se svým CCP83/BDOS83 pro SAPI. TNS mu byl inspirací, ne sestavou SAPI. Text sektoru T0 S1 disket 04A/04B je
i v `...\SAPIMAN\SAPI1_ARCHIV\ze SAPI\original\kobootns.txt`.

## Formát

- Standardní CP/M na IBM 3740: proklad 6, adresář na stopě 2, bloky 1 KB, 64 položek adresáře.
- TNS 2.27 má **DSM 241** (o blok méně), protože konec stopy 76 drží část BIOSu („BIOS I“). MIC 2.3x má DSM 242.
- `KONVERZE.CO_` na 04B má poškozenou položku adresáře, dobrá kopie `KONVERZE.COM` je na 05.
- **Soubory:**
  - **04A:** CPMKH.SYS, CPMWH.SYS, DES, DOCTOR, DUP, DUPLIK, FORMAT, GENER, MEZE.PAS, PIP, PPTEST, PW, SLAP, STAT, T,
    TED, TISK, TURBO, WD, ZSID.
  - **04B:** AUT.AUT, CBOOT, COMMAND.SUB, RB, RBASD, S, SIRKY, SYS.COM, SYSTEM.
  - **05:** ASK.PAS, AUT.AUT, D + DBASEOVR (dBASE II), DISKETY, FBOOT, KONVERZE, LP, PIP, PW, S, SYS.SYS, SYSTEM, TED,
    TURBO, WD.

## TNS CP/M V2.27-K

| Část | Adresa | Na disketě |
|---|---|---|
| T0 S1 | – | jen text „TNS CPM V 2.27-K …“, žádný kód |
| CCP | D600 | stopa 0 od S2 |
| BDOS | DE00 (vstup DE06) | stopy 0–1 |
| BIOS II (skoková tabulka) | EC00 | T1 S20–26 |
| BIOS I | EF80–F8FF | stopa 76 |
| Tabulka vektorů IM 2 | FB00 (I = FBh) | – |
| Videopaměť | FC00–FFFF, 64 × 16 | – |

- **Zavaděč** je ROM TNS („ZAV“). Nahraje systém a skočí na EF80 s D = 'S' (53h) a C = číslo mechaniky. Dump ROM
  nemáme. Žádné volání monitoru jako u SAPI (`monitor+21h`).
- `CPMKH.SYS` a `CPMWH.SYS` jsou celé obrazy systému pro `GENER.COM` (stopy 0–1 a BIOS I). Přípona K = řadič
  **KORD4**, W = řadič ve stylu **WD/FD17xx** (příkazy 88h, A8h, D8h, data přes INIR/OTIR na 6Ah).
- **CPU:** jen Z80 (IM 2, `LD I,A`, `OUT (C)`, JR, DJNZ, EXX, INIR, OTIR). Zdroje mají `.Z80`.

### Porty (konvence TNS: data na sudém portu, stav a maska přerušení na lichém, vektor IM 2 = adresa portu)

| Port | Zařízení |
|---|---|
| B0h / B1h | CRT: potvrzení přerušení časovače 50 Hz / maska |
| B8h / B9h | klávesnice: znak / stav a maska přerušení |
| 50h / 51h | paralelní tiskárna |
| 3Ch / 3Eh | USART ve stylu 8251 (tiskárna DARO) |
| 40h / 41h | děrovač pásky |
| 18h / 19h | snímač pásky |
| 3Ah | stránkování paměti (RAM disk) |
| 60h–78h | KORD4: 60 výběr mechaniky, 61 stav, 62 zápis sektoru, 64/66/68 data a adresa v paměti řadiče, 70 krok, 78 čtení sektoru |

## MIC CP/M 2.30 / 2.32 a OSBOOT-F

- Stopa 0 je zapsaná v logickém pořadí s prokladem 6. Sektor 1 (0100h) vypadá jako hlavička pro zavaděč v ROM
  (seznam sektorů a vstupní bod 0180h, přesný význam polí je odhad).
- OSBOOT-F (Z80) smaže obrazovku na FC00, hledá `SYS.SYS`, při chybě „Nelze najít soubor:“. Řadič je typu
  **FD1771/WD179x**: 60h příkaz/stav, 62h stopa, 64h sektor, 66h data, řízení 68h–6Eh (SEEK 19h, READ 88h).
- `FBOOT.COM` zapíše OSBOOT na stopu 0, `CBOOT.COM` ho kopíruje mezi disketami.
- `SYS.SYS` / `SYS.COM` jsou moduly: řadič disket, KORD, RAM disk, Winchester, kazetový disk, tiskárny VT-21200
  a D-100, grafika, hodiny reálného času.

## Programy

- **Volají hardware TNS** (na SAPI nepoběží):
  - `FORMAT` („FORMATOVANIE A KOPIROVANIE FLOPPY A RPP V1.2“) a `DUP`, `DUPLIK`: porty řadiče;
  - `PPTEST` („PREJIMACI TEST TNS“), `TED` („AGROSYSTEM – TNS – JZD SLUSOVICE“), `RBASD`: přímo B0/B8 a další porty;
  - `SIRKY`: BIOS přímo na EC06–EC0C.
- **Jen přes BDOS:** `SYSTEM`, `S`, `DISKETY`, `GENER`, `KONVERZE`, `TISK`, `LP` (Turbo Pascal 2.02 z VÚB; LP možná
  píše i na port tiskárny, neověřeno).
- **Obecné programy:** Turbo Pascal, PIP, STAT, WordStar (WD), PW, dBASE II, ZSID. Tyto a programy jen přes BDOS by
  šly zkopírovat na disketu CP/M pro SAPI se Z80 (JPR-1Z/ZI). Tato cesta zatím není vyzkoušená.

## Srovnání se SAPI-1

| | TNS | SAPI-1 |
|---|---|---|
| CPU | Z80, IM 2, vektor = port | JPR-1A 8080; JPR-1ZI Z80 s 8259 (vektor CALL) |
| Disk | KORD4 / FD17xx na 60h–78h | RPD-1 20–25h / 3C–3Fh, FDC-1 a MFC-1A (8272) 04–0Fh |
| Displej | FC00, 64 × 16, CRT na B0h | AND-1Z F800 64 × 20, AND-1A E800/F800, bez portů |
| Klávesnice | port B8h s přerušením | porty JPR (01h/02h) |
| Zavaděč | ROM ZAV | monitor SAPI (B800/F000/F800, +21h/+24h) |

Porty TNS navíc kolidují s kartami SAPI: 60–63h a 70h se ZRMD-1, 3C–3Fh s RPD-1B, 50–57h s MPH-1V.

**Spustit beze změn na SAPI nejde.** Emulace TNS by byla samostatný stroj. Chybí k ní ROM ZAV a dokumentace KORD4
a desky CRT, porty ale podrobně popisuje `BIOSTNS.MAC`. V archivu EC1045 je dalších disket TNS mnoho:
`DEX\TNS\A–D` (na `TNS\B\03` zdroje BIOS), `hry\TNS`, `PETR\TNS-2_32`.

## Otázky na EC1045 (Petr Všijanský)

- Odkud jsou diskety 04A, 04B a 05 (VÚB Ústí nad Orlicí?) a proč jsou ve složce `SAPI_ZPS3`?
- Má někdo dump ROM zavaděče TNS (ZAV) nebo popis řadiče KORD4?
