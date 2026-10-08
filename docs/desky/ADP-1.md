# ADP-1 – deska A/D převodníku

Stav: **emulovaná** (typ `ADP-1`, `src/cards/adp1.*`) od 2026-10-08, v sestavě APS-5 (`APS-5.md`), test `test_aps5`.
Tesla Liberec, návod 6XV 122 81.

## Podklady

- Návod (OCR s tabulkami): `..\SAPI.CZ\sapi\adp-1.php`, překreslené schéma `..\SAPI.CZ\sapi\pdf\adp-1_cb.pdf`,
  replika EAGLE `..\SAPI_DOKU\sapi_eagle\REPLIKY\ADP-1\`.

## Zapojení

- 16 vstupů S1–S16 (X3/15–30), multiplexer MAB 16, zesilovač (zesílení 1 nebo 10 multiplexerem MAB 24),
  absolutní hodnota (NA4), polarita (NA1), integrační převodník **C 520 D**: tři číslice BCD.
- Rozsahy: ±5 V (±9,99 V při ±15 V) po 10 mV, ±0,999 V po 1 mV. Přetečení = BBB, záporné napětí na C 520 D
  (jen při rozladění) = A v MSD.
- Rychlost převodu 48–168/s (podle C 520 D). Reference na X3: +9,99 V (pin 2), +0,999 V (pin 1).
- **Adresa:** SA1–SA4 = A7–A4 (báze je negace přepínačů, 1111 = 00h), DD5A dělí dolní čtyři bity:
  - zápis +1: řízení (bity 0–3 kanál, bit 4 zesílení 10). Nuluje READY a spustí jeden převod;
  - čtení +1: bit 7 READY, bit 4 polarita (1 = minus, sleduje okamžitý stav), bity 0–3 MSD;
  - čtení +0: bity 4–7 NSD, 0–3 LSD.
  Hodnota zůstává do dalšího zápisu.

## Emulace

- Parametry:
  - `base` (výchozí 30h: tovární nastavení není známé, APS-5 má 00h);
  - `wiring` (`none` = vstupy nezapojené, 0 V; `aps5` = kabely APS-5: S1 = U1 DMT-1, S2 = U2 DMT-1, S5 = baterie,
    S6 = 0 V, S7 = KAL +9,99 V);
  - `ref_mv` (RP1, 9990);
  - `conversion_ms` (20).
- Převod bere napětí v okamžiku dokončení. Polarita se čte okamžitě. Bez šumu.

## Nejasnosti

- Doba převodu C 520 D na desce, hodnota bitů 5 a 6 stavového registru (emulace 0), co udělá /RES s registry
  74173 a s READY (emulace vše 0).
