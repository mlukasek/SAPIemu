# DPP-1 – deska paralelních portů

Stav: **emulovaná** (typ `DPP-1`, `src/cards/dpp1.*`), od 2026-10-05. Na kanál 2 (nebo 1) jde připojit emulátor disket
PMD32-SD (`docs/desky/PMD32-SD.md`), jak ho používají ECMON a CP/M J. Bíby. Od 2026-10-06 jde na druhý kanál
připojit tiskárnu Centronics (`docs/tiskarna.md`). Od 2026-10-09 jde na kanál 1 (X2) připojit kazetopáskový záznamník
dat KZD-1 pro MIKOS/K (`docs/desky/KZD-1.md`, sestava `zps/ZPS-2_MIKOS_K.sapi`, test `test_kzd1`) a na port B
kanálu zapisovač ARITMA Minigraf 0507 (`docs/desky/Minigraf.md`, sestava `zps/ZPS-2_MIKOS_V5_Minigraf.sapi`, test
`test_minigraf`). Sestavy `cpm_zrmd_dpp1.sapi`, `cpm_pmd_rpd1.sapi`, `cpm_pmd_fdc1.sapi`, testy
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
  Oba konektory jsou FRB 30 na čele desky, X2 nahoře, X3 dole (fotka `..\SAPI_DOKU\FOTO_poklady_petr\SAPI\DPP-1_soucastky.jpg`,
  X1 vlevo); na panelu konfigurátoru K3 = X2, K2 = X3.
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
- **Směr portů (S2):** vstup DIEN obvodu 3216 každého portu dává hradlo 7408 (D4C) se dvěma vstupy s pull-upy.
  1S2 / 4S2 jeden vstup uzemní (port je výstup), 2S2 / 3S2 druhý připojí na /ACK_A (port budí konektor jen při
  /ACK = 0, tedy obousměrná sběrnice módu 2). Obě rozepnuté = vstup. **Tabulky 7 a 9 návodu (str. 11 a 19)** mají
  pro obousměrný port sepnuté oba přepínače, podle schématu i repliky by tím byl port trvale výstup. Správně je
  sepnutý jen 2S2 (kanál 1) nebo 3S2 (kanál 2). Řádky „výstup“ a „vstup“ tabulek se schématem souhlasí.
- **Polarita (S3 kanál 1, S4 kanál 2):** sepnutý přepínač signál na konektoru invertuje proti 8255 (tab. 6, str. 10).
  Tabulka konektoru X3 (příloha III, str. 30) platí pro rozepnuté 2S4–7S4. 1S3 / 1S4 = směr portu B (sepnutý
  výstup), 8S3 / 8S4 nevyužité.

### Nastavení desky EC1045 (PMD32-SD na X3, programátor PROM na X2)

Fotka `..\Dotazy\EC1045_2026-10-07\DPP-1_pro_PMD-32SD_2.jpg` (2026-10-07). Číslování podle výkresu osazení (str. 32),
na fotce otočeného o 90° (X1 vpravo, **X3 vlevo nahoře**, X2 vlevo dole). Sepnuto = strana „I“.

| Přepínač | Sepnuté | Význam |
|---|---|---|
| S1 | 1, 2 (3, 4 rozepnuté) | **28h–2Fh** |
| S2 | 1, 3 (2, 4 rozepnuté) | kanál 1: 1PA trvale výstup; kanál 2: **2PA obousměrný řízený /ACK (PC6)** |
| S4 (kanál 2, X3) | žádný | 2PB vstup, PC bez inverze (nativní polarita 8255) |
| S3 (kanál 1, X2) | všechny kromě 7 | 1PB výstup, 1PC1, 1PC5 a vstupy 1PC2, 1PC4, 1PC6 invertované, 1PC7 ne |

- Kanál 2 tedy odpovídá módu 2 na portu A, jak ho používá PMD32-SD (řídicí slovo C4h na 2Fh): /OBF_A = 2PC7
  (X3/9), IBF_A = 2PC5 (X3/10), /STB_A = 2PC4 (X3/15), /ACK_A = 2PC6 (X3/14). Emulace to předpokládá, změna
  není potřeba.
- Drobnost: C4h dává portu B mód 1 výstup, ale 1S4 je rozepnutý, takže 3216 portu 2PB budí proti 8255. PMD32-SD
  port B nepoužívá.
- U S3 je jistá jen poloha rozepnutého přepínače (7S3, při opačném čtení 2S3); oba krajní jsou sepnuté.

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
- **KZD-1** (sekce `kzd1` s `cassette`, `kzd1_channel`, výchozí kanál 1 = X2): port A řízení (výstup), port B stav
  (vstup), řídicí slovo 8Bh, jak to chce MIKOS/K na 30h (1S1 = 1, 2S1 = 0, 3S1 = 1, 1S2 = 1, 2S2 = 0, 1S3 = 0).
  Kanál nesmí sdílet s PMD32-SD ani tiskárnou. Podrobnosti v `docs/desky/KZD-1.md`.
- **Minigraf 0507** (sekce `minigraf`, `minigraf_channel`, výchozí kanál 1): port B výstup (řídicí slovo 99h), X =
  PB0–2, Y = PB3–5, pero PB7. Kanál nesmí sdílet s PMD32-SD ani KZD-1. Okno Minigraf, MCP `plotter`
  (`docs/desky/Minigraf.md`).
- Okno Média a nástroje MCP (`pmd32_images`, `pmd32_mount`) najdou PMD32-SD na IDE-1 i na DPP-1. Okno Média ukazuje
  i kazetu KZD-1, MCP má nástroj `kzd1`.

## Nejasnosti k ověření na HW

- ~~Přepínače S2–S4 při připojení PMD32-SD~~ **vyřešeno 2026-10-07** z ostřejší fotky EC1045 (tabulka výše).
  **EC1045 (2026-10-06):** PMD-32SD na horním konektoru (X3, kanál 2, potvrzeno i výkresem osazení), kabel podle
  dokumentace PMD-32SD, DIR se nezapojuje. Na dolním konektoru (X2) měl **programátor PROM** (`PROM_prog.jpg`, návod
  `SAPIMAN\Scany dokumentací\DPP1- PROGRPROM.pdf`, úpravy +5 V a +12 V přes dvě diody).
- Přepínače S2–S4 při připojení tiskárny: jaké nastavení a jaký kabel se používaly?
- Kanál 1: děrovač (`PUN:`), nebo tiskárna přes `UL1:`? Zapojení signálů (STROBE, BUSY, ACK) na 1PA/1PC
  (emulace: /OBF → /STROBE, /ACK → PC6, BUSY nezapojený).
