# CRM-1 – deska zálohovaných pamětí

Stav: **emulovaná** (typ `CRM-1`, `src/cards/crm1.*`) od 2026-10-07, test `test_crm1`. Tesla Liberec,
návod 6XV 122 83. Autor desku fyzicky nemá. Software, který by ji vyžadoval, zatím neznáme.

## Podklady

- **Návod Tesla** (OCR včetně tabulek propojek a konektoru): `..\SAPI.CZ\sapi\crm-1.php`. Skeny návodu:
  `..\SAPI_DOKU\skeny_dokumentace_bez_retusi_Petr\APS-5\CRM-1\TMP62..TIF`.
- **Replika** (EAGLE, PDF, obrázek): `..\SAPI_DOKU\sapi_eagle\REPLIKY\CRM-1\`.
- **Poznámka EC1045 a J. Bíby** o chybě blokování zápisu: `..\SAPI_DOKU\io_SAPI.txt`, oddíl CRM-1.

## Zapojení

- **Paměť 4 KB:** 32 × MHB 1902 (CMOS, 1K × 1), DS1–DS32. Osazuje se po kilobajtech (CS1–CS4) podle potřeby.
- **Napájení:** zálohu drží články na desce (2 × burel 5105, 2 × SR 45 nebo 3 × NiCd DK 60, 2,9–5,25 V). Propojka
  22–23 připojí vnější zálohu na špičku 48 konektoru XC1. Při poklesu napájení VT2 a DD12 zablokují CS pamětí.
- **Dekodér bloku DD5** (3205), propojky 1–2 až 1–9 vyberou blok 8 KB:

| Propojka | Blok |
|---|---|
| 1–2 | 0000–1FFF |
| 1–3 | 2000–3FFF |
| 1–4 | 4000–5FFF |
| 1–5 | 6000–7FFF |
| 1–6 | 8000–9FFF |
| 1–7 | A000–BFFF |
| 1–8 | C000–DFFF |
| **1–9** | **E000–FFFF** (z výroby) |

- **Dekodér kilobajtů DD4** (3205) dělí blok po 1 KB. Každý CS (propojky 10, 11, 12, 13 pro CS1–CS4) si vezme
  jeden výstup 14–21 (posun 0000h, 0400h … 1C00h). Z výroby: 10–14, 11–15, 12–16, 13–17, tedy **E000–EFFF**.
- **Přepínač SA1** a čtvrtina DD11 blokují zápis do kilobajtu CS1. Podle poznámky EC1045 to **na deskách z výroby
  nefunguje** (chyba DPS i schématu). Deska J. Bíby už chybu nemá.
- **RDY:** MHB 1902 potřebuje cyklus 1000 ns, SAPI-1 počítá se 450 ns. Monostabilní obvod **DD6** (74121, R10 5k6,
  C1 podle rozpisky „A150“, nejspíš 150 pF, tedy asi 0,7 · R · C ≈ 590 ns) spuštěný každým čtením i zápisem drží
  RDY přes invertor s otevřeným kolektorem v L. Návod zmiňuje i propojku 12–14 na JPR-1, která přidá jeden WAIT za každé čtení a zápis.
- Při aktivním RESET je paměť blokovaná. Konektor XC1 (FRB, klíčování C6): RDY výstup; MR, MW, IOR, IOW, RES,
  STSTB, INTA, A0–A15 vstupy; D0–D7 obousměrné; 48 = záložní napájení.

## Emulace

- **Propojky** jsou v konfigurátoru: `base` (blok 8 KB), `kb1`–`kb4` (posun kilobajtu CS1–CS4 v bloku, nebo
  `none` = neosazeno), `write_protect` (SA1, zapnutý blokuje zápis do CS1 jako opravená deska; na deskách z výroby
  by zůstal bez účinku). Kilobajt, který nikdo nevybere, čte FFh (otevřená sběrnice).
- **Baterie** (`battery`): obsah se při vypnutí emulátoru uloží do souboru `file` (výchozí `work/crm-1/crm.bin`) a při
  zapnutí načte, stejně jako SRAM na DMR-1. Bez baterií začíná paměť nulami.
- **Čekání** (`rdy_ns`, výchozí 590): počet WAIT = vzorky RDY (uprostřed T2 a každého WAIT) před koncem impulzu, tedy
  **1 WAIT při 2 MHz, 2 při 4 MHz**. Hodnota 0 čekání vypne (odchylka od HW, jen pro pohodlí).
- Stav emulace (`state`) ukládá celý obsah 4 KB.

## Nejasnosti k ověření na HW

Autor desku nemá, ověření je možné jen na cizím kusu.

1. Skutečná délka impulzu DD6 (R10 5k6, C1 150 pF, tolerance 74121): 1, nebo 2 WAIT při 2 MHz?
2. Co přesně dělá SA1 na neopravené desce z výroby (blokuje jiný kilobajt, nic, nebo celé čtení)?
