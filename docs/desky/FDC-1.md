# FDC-1 – řadič pružných disků (FDC-1A + FDC-1B)

Stav: **emulovaná** (typ `FDC-1`, `src/cards/fdc1.*`, obvody `src/chips/i8272.*`, `i8257.*`, mechanika
`src/chips/floppy.*`), ověřeno se CP/M 2.2/P 48K a 62K pro FDC-1 (Peřina) v ZPS-3.

## Podklady

- `..\SAPI-karty\FDC-1\` (autor):
  - `fdc-1a.pdf`, `fdc-1b.pdf`, `FDC-1A.png`, `FDC-1B.png`, Eagle `.sch`/`.brd`: schémata překreslená EC1045.01
    (rev. 1.0, 13. 2. 2011);
  - `navod-scan\1.jpg`–`13.jpg`: návod (strany 2–27): instalace, programování, popis 8272, konfigurace mechanik,
    rozdíly proti RPD-1, zapojení Consul 7113, rozpisky, rozložení součástek;
  - `PROM_D5B.hex`: dump PROM D5B (dekodér portů);
  - `FDC-1AB.xlsx`, `.csv` (rozpisky), `Kabel_pro_FDD.xls`, fotky desek `Foto\`;
  - `sw\`: zdroje BIOSů EC1045 (`ECBIF*.MAC`, ovladač „FDC-1 V050290JB“) a `info-z-emailu.txt`.
- Systémové diskety CP/M 2.2/P pro FDC-1 (`..\SAPI-sestavy\ZPS-3\CPM48AF.img`, `CPM62AF.img`, v repu
  `media/fdc-1/`). Na nich je `FDC.DOC` (popis změn proti RPD-1), `FDC.INC`, zdroj monitoru `MOR24F.MAC` a
  formátovač `FORM6.COM` (AGRODAT Znojmo 1988).

## Zapojení

- **Dvě desky** propojené plochým kabelem stejně jako RPD-1 (konektor X2): FDC-1A (8272, DMA, dekodér portů,
  rozhraní mechanik X3) a FDC-1B (separátor dat, krystal 8 MHz, registry 0EH/0FH, přepínače SA1 a SA2, vlastní
  výstupy SEL, HL a MOT pro mechaniky na X3).
- **FDC-1A:** 8272 (NEC 765A, uPD765), DMA 8257 (KR580IK57, na schématu „8257 (8237-B)“), oddělovače 8282/8286,
  PROM D5B (74S287) na A0–A7, monostabil 555.
- Řadič umí až 4 mechaniky, 8" i 5,25", SS i DS, SD i DD (návod). **Emulace má 4 mechaniky 8" SS SD**
  (IBM 3740, 77 × 26 × 128 B, obrazy `.img` jako u RPD-1).
- Od roku 1989 dodávaný místo RPD-1 (FDC.DOC). Tiskárnu DPP-1 je kvůli kolizi portů nutné přeadresovat z 08H na 18H.

## Porty (PROM D5B, výchozí)

| Port | Obvod | Význam |
|---|---|---|
| 04–0BH | 8257 | 04 adresa kanálu 2, 05 počet kanálu 2, 06/07 kanál 3, 08 režim (W) / stav (R) |
| 0CH | 8272 | hlavní stav MSR (R) |
| 0DH | 8272 | data: povely, výsledky, data v režimu bez DMA |
| 0EH | FDC-1B | W: D1 D0 = výběr S0–S3, D2 = 1 reset 8272, D3 povolení INT0; R: přepínače SA2 vybrané mechaniky |
| 0FH | FDC-1B | W: D3–D0 motory M3–M0, D7–D4 přiklopení hlav H3–H0 (signály na X3 FDC-1B) |

- Kanály 0 a 1 osmice 8257 (00–03H) jsou zakryté porty JPR (PROM pro ně nic nevybírá). Návod píše „8257 na 04 až 08“.
- **PROM D5B:** výstupy D0 = /CS 8257 (Y0), D1 = /CS 8272 (Y1), D2 = /MR a /MW obou (Y2), D3 = FDC-1B (Y3).
  Dump: 04–0BH FAh, 0CH–0DH F9h, 0EH–0FH F7h, jinde FFh. Vstupy jsou přímo A0–A7.
- **8272** je připojený na DMA kanál 2 (DREQ2, DACK2, TC z 8257). INT přes hradlo s povolením D3 (0EH) na /INT0.
- **Čtení 0EH (SA2):** FORM6 zapíše na 0EH číslo mechaniky a v D1 čeká 1 pro 8" formáty. D1 = 8" (S1–S4
  „funkční a identifikační“), D0 = DD (S5–S8 jen identifikační, **polarita je odhad**). Návod: „IN 0EH lze číst
  stav DIL přepínače na bitech D0 a D1“.
- **READY:** na dodávaných deskách je /RDY (X3-20) spojený se zemí pro Consul 7113 (návod, FDC.DOC). 8272 pak
  považuje všechny 4 jednotky za připravené.

## Software

- **MOR48AF / MOR62AF** (Peřina 1988, „SAPI-1 ZPS3-48K/FDC-1“, zdroj `MOR24F.MAC`):
  - init: `OUT 0EH,04` a `OUT 0EH,00` (reset 8272), `OUT 08H,00`, SPECIFY 03 0F FE (SRT 16 ms, HUT 240 ms,
    HLT 254 ms, DMA);
  - čte a zapisuje po jednom sektoru: 8257 kanál 2 (adresa, počet 7Fh + 40h čtení / 80h zápis, režim 44h =
    kanál 2 a stop po TC), READ/WRITE DATA s EOT 26, GPL 7, DTL FFh;
  - po SEEK/RECALIBRATE volá SENSE INTERRUPT STATUS, dokud nedostane ST0 se SE pro svou mechaniku (80h = ještě
    ne, jiná mechanika = znovu, chybové bity = chyba).
- **ECBIF8.MAC** (EC1045, ovladač „FDC-1 V050290JB“): stejné porty, SPECIFY 03 7F 30.
- **FORM6.COM** (formátovač): typy A–I (8" i 5,25", SD i DD), SPECIFY 03 7F 80, FORMAT A TRACK s DMA
  (26 × 4 bajty ID), ověří přepínače SA2 na 0EH.

## Emulace

- **8272:** povely READ DATA, READ DELETED DATA, WRITE DATA, WRITE DELETED DATA, READ TRACK, READ ID, FORMAT,
  RECALIBRATE, SEEK, SENSE INTERRUPT STATUS, SENSE DRIVE STATUS, SPECIFY. SCAN se neemuluje (odpověď jako na
  neplatný povel, varování v logu). Režim bez DMA (ND = 1) je emulovaný, software ho nepoužívá.
- **Časování:** bajt 32 µs (FM, 8"), 360 ot/min, stopa 5208 bajtů se stejnými mezerami jako u RPD-1; krok
  16 − SRT ms, přiklopení hlavy HLT × 2 ms, odklopení HUT × 16 ms po povelu; RQM se vrací 12 µs po bajtu povelu
  nebo výsledku (odhad). Sektor se hledá podle polohy disku, chyba (ND, MA) po druhém indexu.
- **Přerušení:** jeden stav na jednotku (jako MAME): konec seeku přepíše starší stav téže jednotky, SENSE
  INTERRUPT STATUS bere jednotky 0–3 popořadě. Po resetu 8272 hlásí pro každou připravenou jednotku „ready
  changed“ (ST0 C0h + jednotka); nový SEEK/RECALIBRATE stav své jednotky smaže. Bez toho MOR po RESETu s hlavou
  na stopě 0 hlásil BOOT ERROR.
- **Bez diskety** a s uzemněným /RDY 8272 čeká na index věčně (jako skutečný čip).
- **DMA:** bajt se přenese v okamžiku, kdy ho 8272 přečte. Každý přenos ubere CPU 4 takty (HOLD, `Bus::hold`).
  Nevyřízený DRQ (kanál vypnutý) je přetečení (OR).
- **Parametry:** `drive_a`–`drive_d` (obraz, ochrana), `working_copy`, `decoder` (PROM D5B), `rdy_ground`,
  `sa2_a`–`sa2_d` (přepínače SA2: 8sd, 8dd, 5sd, 5dd).
- Okno Média ukazuje mechaniky A–D (vložit, nová, vysunout, ochrana), stejně jako u RPD-1.

## Nejasnosti k ověření na HW

- Polarita D0 přepínačů SA2 (DD/SD) při čtení 0EH a k čemu je dvoupólový přepínač SA1 na FDC-1B (návod mluví
  o jednom „DIL přepínači“ čteném na D0 a D1).
- Hlásí 8272 na FDC-1 po resetu „ready changed“ pro všechny 4 jednotky? (MOR to snese, bylo by dobré to ověřit
  sekvencí SENSE INTERRUPT STATUS po resetu.)
- Kolik taktů CPU zabere jeden přenos DMA (emulace 4).
- Registr 0FH (motory, hlavy) a výběr 0EH D1 D0 vedou jen na X3 FDC-1B. Používá je něco kromě FORM6
  (`OUT 0FH,0FH` = všechny motory)?
- Doba RQM po bajtu povelu (emulace 12 µs).
