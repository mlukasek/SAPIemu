# Měřicí jednotka APS-5 (pece)

Tesla APS-5 s původním firmwarem reguluje osm pecí podle termočlánků. V okně APS-5 je čelní panel s klávesnicí a
displejem a kotelna s pecemi a grafem teplot.

## Nejrychleji: DEMO

Tlačítko **DEMO** v okně APS-5 odťuká nastavení samo: kalibraci, typy termočlánků, program ohřevu a zapnutí
regulace tří pecí. Asi za minutu pece začnou žhnout.

## Ručně

- Klávesy panelu jde mačkat myší, nebo psát na PC: číslice, `M` (Esc), `R`, Backspace = ←, Enter = →.
- První zapnutí je studený start. Nejdřív kalibrace `6R` → `9990` →, pak pro každou pec `0` → číslo pece → a
  `8R` → typ termočlánku → (pec 1: 3 = ch-a).
- `11` → ukáže tabulku stavu pecí.
- **Reset nepoužívejte.** Skutečná jednotka ho nemá a firmware ho bere jako chybu, nastavení se smaže. Emulátor se
  proto před resetem (F12, Stroj → Reset, RESET na panelu ZDR) zeptá. Vypínejte vypínačem nebo Stroj → Vypnout
  a zapnout (Ctrl+F12), to proběhne jako výpadek sítě a nastavení zůstane.

## Dál

- Ukázka krok za krokem: [APS-5 ukázka](docs/aps5-ukazka.md), popis jednotky: [APS-5](docs/desky/APS-5.md).
