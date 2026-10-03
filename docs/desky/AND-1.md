# AND-1 – původní alfanumerický displej

Zdroje: `BKP\and-1.php` (OCR manuálu, výpis PROM 74188), `BKP\pdf\and-1.pdf`, `BKP\obrazky\andpriloha-d1.png`,
`andpriloha-d2.png`, `and-1_font*.jpg`, skeny `H:\SAPI-1\Foto\Jiri_Slansky\Dokumentace\AND-1\*.tif`, katalogový
list znakových generátorů `...\Dokumentace\AND-1 a 1A\MHB250x.png`, Eagle `D11\sapi_eagle\REPLIKY\AND-1\`.

## Zapojení

- **Obraz:** 40 × 24 znaků (propojkou 20 řádků), matice 5 × 7, 1 bod mezera mezi znaky, 3 řádky mezi řádky
  textu, 10 TV řádků na řádek textu.
- **Časování:** oscilátor 6 MHz / 6 = znakový takt 1 µs, řádek 64 µs, 320 řádků, snímek 20,48 ms.
- **VRAM:** 1 KB (2× 2114) v okně 3800–3FFF (dekodér D3A, A15–A11 = 00111, mimo /RES, bez propojek).
  Adresa = 3800 + řádek·40h + sloupec, sloupce 0–39. Adresový multiplexer s D4C (A5) skládá 64 adres řádku
  do 1 KB.
- **Přístup CPU:** bez čekání (RDY není zapojená), přepnutí multiplexeru způsobí krátké černé pruhy v obraze.
- **Bajt ve VRAM:** D7–D6 režim: 00 normální, 01 blikání, 10 blikající podtržení (kurzor), 11 dvojitá šířka.
  D5–D0 je 6bitový kód: 00–1F = `@A–Z[\]^_`, 20–3F = mezera až `?` (na 24h je `¤`).
- **Znakový generátor:** MHB 2501 (latinka) nebo MHB 2502 (azbuka), p-MOS ROM 2560 bitů s napájením −12 V.
  Dump neexistuje, emulátor používá znaky překreslené z tabulky v katalogu (`roms/and-1/`).
- **PROM D3C (74188):** kurzor, blikání a dvojitá šířka (`roms/and-1/AND-1_D3C_74188.bin`). Emulátor zatím
  režimy kreslí podle popisu, PROM nečte.
- **Přerušení:** INT1 na konci snímku (perioda 20 ms, pulz 0,64 ms), propojka 49-50 (výchozí zapojená).

| Propojka | Výchozí | Význam |
|---|---|---|
| 47-48 | ano | 24 řádků |
| 46-47 | — | 20 řádků |
| 42-43 | ano | MHB 2501 |
| 41-42 | — | MHB 2502 |
| 44-45 | ano | −12 V ze sběrnice |
| 4-13, 11-17 | ano | zpoždění H-sync |
| 21-30 | ano | zpoždění V-sync 2,5 ms |
| 35-34 | ne | rozpojeno TV, spojeno AZJ 462 |
| 49-50 | ano | INT1 na konci snímku |

## Emulace (`src/cards/and1.*`)

- Parametry: `charset` (MHB2501/MHB2502), `rows` (24/20), `int1`.
- Skládání VRAM (předpoklad): sloupce 0–31 = řádek·32 + sloupec, sloupce 32–63 = 300h + řádek·8 + (sloupec
  AND 7). Viditelné buňky se nepřekrývají. Sloupce 40–63 zrcadlí 32–39. MIKROBASIC píše jen sloupce 0–39 a při
  mazání vyplní celé okno, takže na přesném skládání nezáleží.
- Blikání: 16 snímků svítí, 16 nesvítí (předpoklad). Podtržení kurzoru na 9. TV řádku buňky.
- Poměr stran obrazu „Poměr TV“ počítá s bodovým taktem 6 MHz.

## Nejasnosti

- Skutečné skládání sloupců 32–63 do 1 KB (D4C) a frekvence blikání.
- Ověřit překreslený font na fotce obrazovky skutečné AND-1.
