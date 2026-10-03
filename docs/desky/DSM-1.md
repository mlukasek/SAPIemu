# DSM-1 – sériové rozhraní s kazetou

Zdroje:
- `H:\SAPI-1\sapi.cz_BKP_2023-02-22\sapi\dsm-1.php`, `obrazky\dsmobr3.png`
- `H:\SAPI-1\DSM-1\`

## Základní parametry

- **UART:** MHB1012.
- **Krystal:** 12,288 MHz.
- **Bázová adresa:** jumpery 16-12, 16-13, 16-14 a 16-15 volí 10h, 14h, 18h nebo 1Ch. Z výroby je nastaveno 10h.
- **Rychlost terminálu:** 50–9600 Bd podle jumperu. Z výroby 600 Bd (jumper 1-6).
- **Registry:** stejné jako u [DMR-1](DMR-1.md), včetně významu bitů modemu.

## Kazeta

- **Přepínání:** stejný UART se přepíná bitem KAZ (base+0, bit 4). Bit RD (bit 3) volí čtení nebo zápis, bit 5 ovládá relé motoru.
- **Formát:** pevně 2400 Bd, 1 start bit, 8 datových bitů, 2 stop bity.
- **Kódování:** vysílaný bit XOR hodiny, tedy jedna perioda nosné na bit a změna fáze při změně dat. Prakticky jde o Manchester 2400 bit/s.
- **Čtení:** rozhoduje doba mezi průchody nulou. 208 µs znamená „beze změny“, 416 µs „změna“. Práh čítače je 12.
- **Ověřeno na nahrávce** `H:\SAPI-1\SAPI_DOKU_2024-01-03\Kazetopáskové vybavení AUDIO WAV\tool_48.wav`: mezery jsou přesně 208 a 416 µs. Tyto WAV nahrávky jsou vhodné jako testovací vstup pro emulátor.

## Nejasnosti

- Pořadí bitů délky slova (b3–2) v registru řízení UART. OCR uvádí „10“ = 6 bitů.
- Stav po resetu: dokumentace uvádí „DR=1“, což je pravděpodobně chyba OCR.
