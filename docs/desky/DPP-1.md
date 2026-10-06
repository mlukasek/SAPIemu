# DPP-1 – deska paralelních portů

Stav: **emulovaná** (typ `DPP-1`, `src/cards/dpp1.*`), od 2026-10-05. Na kanál 2 (nebo 1) jde připojit emulátor disket
PMD32-SD (`docs/desky/PMD32-SD.md`), jak ho používají ECMON a CP/M J. Bíby. Od 2026-10-06 jde na druhý kanál
připojit tiskárnu Centronics (`docs/tiskarna.md`). Sestavy `cpm_zrmd_dpp1.sapi`, `cpm_pmd_rpd1.sapi`, testy
`test_sapi_sys`, `test_printer`.

## Podklady

- Návod Tesla (Tesla Elstroj, Tesla Liberec, Tesla Eltos-DIZ; 6XV 122 11, deska 6XK 199 19): sken
  `..\SAPI_DOKU\Manuály_desek\DPP-1\DPP1.pdf` (33 stran, tabulky 2 a 3 s adresami na str. 6 a 7), OCR na sapi.cz
  (`..\SAPI.CZ\sapi\dpp-1.php`, bez tabulek).
- Překreslené schéma EC1045: `..\SAPI_DOKU\sapi_eagle\REPLIKY\DPP-1\DPP-1.pdf` (Eagle `.sch`, `.brd`).
- Fotky součástek a spojů: `..\SAPI_DOKU\FOTO_poklady_petr\SAPI\DPP-1_*.jpg`. Programátor PROM s DPP-1:
  `..\SAPI_DOKU\SAPIMAN\Scany dokumentací\DPP1- PROGRPROM.pdf`.

## Zapojení (návod)

- Dva téměř shodné kanály, každý s **8255** (MHB 8255A, D1B a D2B), od sběrnice oddělené obvody 3216. Porty PA a PB
  jdou přes 3216 na konektory X2 (kanál 1) a X3 (kanál 2); směr 3216 nastavují přepínače S2–S4.
- **Port PC:** PC0 a PC3 výstupní, vždy invertované (7405, otevřený kolektor); PC1, PC5, PC7 výstupní, inverzi volí
  přepínač (4S3/4S4, 5S3/5S4, 7S3/7S4); PC2, PC4, PC6 vstupní přes 7404, inverzi volí přepínač (3S, 2S, 6S).
- **Adresa** (dekodér 3205 D5A, 7486 D4A, přepínače 1S1–3S1): osm prostorů po 8 portech v 00h–3Fh (A7 = A6 = 0).
  A2 vybírá kanál: základ+0 až +3 = kanál 1 (1PA, 1PB, 1PC, řídicí slovo), +4 až +7 = kanál 2.

| Porty | 1S1 | 2S1 | 3S1 |
|---|---|---|---|
| 00–07h | | X | X |
| 08–0Fh | | X | |
| 10–17h | | | X |
| 18–1Fh | | | |
| 20–27h | X | X | X |
| 28–2Fh | X | X | |
| 30–37h | X | | X |
| 38–3Fh | X | | |

  (X = sepnutý; 1S1 sepnutý = A5 = 1, 2S1 sepnutý = A4 = 0, 3S1 sepnutý = A3 = 0.)
- /RES (invertovaný) nuluje obě 8255 (mód 0, vše vstupy). INT0 ze sběrnice je vyvedený na kolíky 18 konektorů X2 a X3.

## Emulace

- Parametry: `base` (přepínače S1, výchozí 28h jako u ECMONu), `pmd_channel` (1 nebo 2, výchozí 2), sekce `pmd32sd`
  stejná jako u IDE-1 (SD karta, mechaniky A–D, ochrana, prodleva po zapnutí).
- PMD32-SD je na portu A zvoleného kanálu v módu 2 (handshake jako na IDE-1). **Přepínače S2–S4 se neemulují:**
  předpokládá se nastavení, které připojené zařízení potřebuje (směr 3216 a polarita PC pro mód 2). Druhý kanál
  nemá nic připojeno (vstupy čtou 1).
- ECMON (verze bez IDE-1) i CP/M J. Bíby používají PMD32-SD na kanálu 2 desky na 28h (2Ch–2Fh, řídicí slovo C4h na
  2Fh). **Kanál 1 (28h–2Bh) je u nich `PUN:` (děrovač)**, ne tiskárna: řídicí slovo AEh (PA výstup v módu 1),
  výstup čeká na /OBF a INTR. Tiskárna („CENTRONIX“, `LST:`) je na X3 procesorové karty (`docs/tiskarna.md`).
  Dřívější poznámka, že kanál 1 je tiskárna, byla omyl (opraveno 2026-10-06).
- **Tiskárna** (sekce `printer`, `printer_channel`, výchozí kanál 1): port A v módu 1, /OBF = /STROBE, /ACK
  tiskárny na PC6. ECMON na ni tiskne při `LST: = UL1:`. 8255 má od 2026-10-06 mód 1 i na portu A (vstup
  i výstup).
- Okno Média a nástroje MCP (`pmd32_images`, `pmd32_mount`) najdou PMD32-SD na IDE-1 i na DPP-1.

## Nejasnosti k ověření na HW

- Přepínače S2–S4 (směr 3216, polarita PC) při připojení PMD32-SD a tiskárny: jaké nastavení a jaký kabel se
  používaly? Zapojení konektoru PMD32-SD na X3?
- Kanál 1: děrovač (`PUN:`), nebo tiskárna přes `UL1:`? Zapojení signálů (STROBE, BUSY, ACK) na 1PA/1PC
  (emulace: /OBF → /STROBE, /ACK → PC6, BUSY nezapojený).
