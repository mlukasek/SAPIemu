# DRV-1A – deska reléových výstupů

Stav: **emulovaná** (typ `DRV-1A`, `src/cards/drv1a.*`) od 2026-10-08, v sestavě APS-5 (`APS-5.md`).
Tesla Liberec, návod 6XV 122 82.

## Podklady

- Návod: `..\SAPI.CZ\sapi\drv-1a.php`, schéma `..\SAPI.CZ\sapi\pdf\drv-1a_cb.pdf`, replika
  `..\SAPI_DOKU\sapi_eagle\REPLIKY\DRV-1A\`.

## Zapojení

- Osm jazýčkových relé KH1–KH8, kontakty S1–S4 na XC2, S5–S8 na XC3 (0,5 A, 12 W).
- Registr MH 3212 (DD1), bit n = 1 sepne S(n+1). /RES ho nuluje (vše rozepnuto).
- Adresa: dva 74S138 (IC1: A6–A4, G2A = A3, G2B = A7, propojka S1 vybere 40/50/60/70h; IC2: A2–A0, propojka S2),
  úplné dekódování, jediný port 40–47h, 50–57h, 60–67h nebo 70–77h. Z výroby 77h. Jen zápis.

## Emulace

- Parametry:
  - `port` (77h);
  - `load`: `none`, nebo `plant` (kontakty spínají topení pecí technologie sestavy, APS-5).
- Stav kontaktů je v konfigurátoru („S1-S8: …“) a v okně APS-5. Skutečná deska LED nemá.
