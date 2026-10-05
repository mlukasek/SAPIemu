# RPD-1 – řadič pružných disků (RPD-1A + RPD-1B)

Zdroje:
- `BKP\sapi\rpd-1.php` (OCR návodu ze ZPS-3);
- obrázky registrů `BKP\sapi\obrazky\rpdobr2_*.png`, schémata `rpd-1a.png`, `rpd-1b.png`;
- ovladač EBIOS (`BKP\sapi\data\ebios_lst.php`, J. Hlaváč 1988, v EPROM MIKOS/D na 1300–17DA);
- systémová disketa MIKOS/D (`BKP\sapi\data2\m5mid_img.rar`, `m5mid_imd.rar`).

Nástupce: řadič FDC-1 (8272 + DMA, od roku 1989, `docs/desky/FDC-1.md`). Má stejné propojení desek (KB-17) a
skoro stejný konektor mechanik; rozdíly kolíků jsou v FDC-1.md.

## Zapojení

- **Dvě desky vedle sebe ve vaně:** RPD-1A (datové rozhraní, porty 20–25h) a RPD-1B (řízení mechaniky,
  separátor dat a hodin, porty 3C–3Fh). Obvod FDC není, značky, CRC a sektory obsluhuje software.
- **Mechaniky:** dvě 8" SS SD (Consul 7212/7213, Momflex) nebo 5,25" (Robotron K 5600, propojky TAB.4).
  Formát IBM 3740: 77 stop × 26 sektorů × 128 B, FM, 32 µs na bajt (5,25": 40 × 10 × 256 B, 64 µs).
- **Značky:** IDAM FE, DAM FB, DDAM F8 (hodiny C7), IAM FC (hodiny D7).

### Registry

| Port | Směr | Význam |
|---|---|---|
| 20–22h | W | čítače 0–2 obvodu 8253, **datové vodiče obráceně** (D0↔D7…) |
| 23h | W | řídicí slovo 8253 (EBIOS: 0CH, 5EH, 0DH = obráceně 30H, 7AH, B0H) |
| 24h | R/W | datový registr (bajt z diskety / na disketu) |
| 25h | R | stav: D7–D5 = D7–D5 řízení (tabulka skoků pro PCHL), D4 OVFL, D3 TOUT, D2 /DR (0 = bajt připraven / požadován), D1–D0 = 0 |
| 25h | W | řízení: D7–D5 tabulka skoků, D3 RAM (hledej adresovou značku), D2 RDM (datovou značku), D1 WE, D0 RE |
| 3Ch | W | řízení mechaniky: D5 INTE, D4 rezerva, D3 T43, D2 DIR (1 = ke středu), D1 FR, D0 SEL |
| 3Ch | R | stav mechaniky: D7 HLR, D6 STR, D5 T00, D4 INDX, D3 WP, D2 RDY, D1 WF, D0 INR (1 = platí) |
| 3Dh | W | přiklopení hlavy (bez argumentu), odklopí se po 8 otáčkách bez dalšího zápisu |
| 3Eh | W | jeden krok motoru |
| 3Fh | W | simulace přerušení |

- **Čítač 0** (mód 0, takt = 1 bajt) hlídá čas operace → TOUT.
- **Čítač 1** (mód 5, takt 0,5 µs pro 8") se spouští signálem SYC (nalezena značka).
  - Při zápisu odpočítá otevření zápisu za ID: EBIOS 1087 × 0,5 µs.
  - Při čtení je to okno pro datovou značku: 9023 × 0,5 µs.
- **Čítač 2** (mód 0) počítá přenosy CPU přes 24h. Po N+1 přenosech nastaví OVFL a vynuluje D3–D0 řízení,
  D7–D5 zůstávají. EBIOS čte 7 bajtů ID (FE trk 0 sec 0 CRC CRC) a 131 bajtů dat (FB, 128 B, CRC). Zapisuje
  `00 00 00 00 FB` + 128 B + CRC + `FF` a jeden bajt navíc, který už se nezapíše.
- **CRC** (CCITT, FFFF, x^16+x^12+x^5+1, včetně značky) počítá software.

## Emulace (`src/cards/rpd1.*`)

- Karta zabírá **dva sloty**.
- **Parametry:**
  - `drive_a` / `drive_b` s `image` (.img, 256 256 B v logickém pořadí sektorů, nebo ImageDisk `.imd` diskety
    8" SS SD s ID 1–26 v libovolném pořadí; data se drží v logickém pořadí a zápisy se ukládají zpět do `.imd`)
    a `write_protect`;
  - `working_copy` (obrazy z `media/` se kopírují do `work/`);
  - `step_ms` (perioda kroku, 8 ms Consul 7213, 12 ms Consul 7212);
  - `settle_ms` (ustálení přiklopené hlavy, předpoklad 35 ms).
- **Stopa** se generuje z obrazu podle IBM 3740 (mezery 40/26, 6 nul před značkami, gap 2 = 11, gap 3 = 27,
  5208 B na otáčku = 166,7 ms). Sektory jsou fyzicky 1..26 po sobě, prokládání dělá software (CP/M skew 6).
  ID pole mají správné CRC.
- **Disketa se točí podle emulovaného času** (taktů CPU). Bajty přicházejí každých 32 µs, kdo je nepřečte
  včas, přijde o ně. TURBO zrychlí i disketu.
- **Zápis:** po otevření hradla (čítač 1 po ID) se každých 32 µs posune bajt z datového registru. Prázdný
  registr = WF (pozdě dána data). Zapsaný proud (nuly, FB/F8, 128 B) nahradí datové pole sektoru, jehož ID se
  četlo naposledy. Špatné CRC se jen zaloguje.
- **WF** nastaví i zápis na chráněnou disketu, nuluje ho FR. RDY = vložená disketa (motory 8" běží stále).
- **HLR** dává časovač po zápisu na 3Dh, i bez diskety. EBIOS na HLR čeká bez časového limitu.
- **INTE/INR, 3Fh** a 5,25" mechaniky nejsou emulované (software je nepoužívá).
- **GUI:** okno Média, mechanika A/B: Vložit…, Nová disketa… (naformátovaná, celá E5h; s příponou `.imd` jako ImageDisk), Vysunout, ochrana
  proti zápisu, stopa. Změny se ukládají při vysunutí, vypnutí a ukončení.

## Ověřeno v emulátoru

`tests/test_rpd1.cpp` se sestavou `machines/zps/ZPS-2_MIKOS_D.sapi`:
- bez diskety `NELZE ZAVEST DISKOVY SYSTEM`;
- s disketou `ZAVEDEN DISKOVY SYSTEM`;
- DIR (13 souborů, OBSAZENO 0167);
- SAVE 4 KB do `TEST.BIN` a LOAD zpět;
- `BAS5C.SYS` z diskety se spustí (`.J`, `PRINT 12*34+5`).

## Nejasnosti

- Taktuje čítač 2 opravdu přístup CPU na 24h (N+1 přenosů)? Nechává OVFL bity D7–D5? (EBIOS to vyžaduje.)
- Doba ustálení hlavy (HLR) a zda opakovaný zápis na 3Dh při přiklopené hlavě drží HLR = 1.
- Bere RAM jen FE a RDM jen FB/F8? Kdy přesně se otevře hradlo zápisu a kterému bajtu se vynechají hodiny?
  Proč EBIOS posílá 4 nuly, když návod říká 5?
- Je TOUT výstup čítače 0, nebo i čítače 1 při čekání na datovou značku?
- **Rychlost:** `BAS5C.SYS` (10 KB) se v emulátoru čte asi 17 s. MIKOS/D po každém sektoru tráví tolik času, že
  další sektor (skew 6) mine a čeká otáčku. Jak dlouho to trvá na skutečném stroji?
