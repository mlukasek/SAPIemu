# DMT-1 – deska měřicího zesilovače

Stav: **emulovaná** (typ `DMT-1`, `src/cards/dmt1.*`) od 2026-10-08, v sestavě APS-5 (`APS-5.md`), test `test_aps5`.
Tesla Liberec, návod 6XV 122 85.

## Podklady

- Návod: `..\SAPI.CZ\sapi\dmt-1.php`, schéma `..\SAPI.CZ\sapi\pdf\dmt-1_cb.pdf`, replika
  `..\SAPI_DOKU\sapi_eagle\REPLIKY\DMT-1\` (s WSH 222).

## Zapojení

- Osm vstupů K1–K8 (XC2: HIGH, LOW, GND) přes relé RP 210 na přesný zesilovač WSH 222. Zesílení 100 / 200 / 500 /
  1000 se přepíná zpětnovazebními odpory multiplexerem NL1. Galvanické oddělení zajišťují modulátor,
  transformátor a optočleny.
- Výstup U1 na XC3/15: ±8 V (±12 V) nebo ±10 V (±15 V z XC3). Ustálení po přepnutí 0,5 s.
- Teploměr srovnávacích konců: tranzistor KF 524 na XC3, výstup U2 0 až −5 V pro 0–50 °C.
- **Adresa:** jen A7–A4 (SA1–SA4, báze je negace přepínačů), 16 zrcadel, jen zápis. Registr (MHB 8282):
  - bit 7 = relé vypnuta;
  - bit 5 = náběžná hrana spustí DD5: po 600 ms pulz 20 ms na INT1 (propojka 7-8);
  - bity 4–3 = zesílení;
  - bity 2–0 = kanál.
- Propojka 5-6: rozpojený vstup (přerušený termočlánek) dá asi −15 mV.

## Emulace

- Parametry: `base` (80h), `break_detect` (5-6, true), `int1` (7-8), `supply_15v`.
- Vstupy dává technologie sestavy (`Bus::plant`, termočlánky APS-5). Bez ní jsou vstupy rozpojené.
- U1 = zesílení × vstup, omezené ±8/±10 V, po změně kanálu nebo zesílení se ustaluje exponenciálně (τ 0,1 s).
- Zesílení **100, 199, 498, 1000** jsou hodnoty z tabulky firmwaru APS-5. Skutečná deska se trimrem RP3 nastaví
  přesně pro jedno zesílení, ostatní mají chybu do 0,5 %.
- U2 = −T svorkovnice / 10 V. ADP-1 čte U1 a U2 kabelem (parametr `wiring` = `aps5`).
- /RES registr nemaže (8282 nemá nulování).
