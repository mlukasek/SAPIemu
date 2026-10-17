# MikroMon 1.3

Nejmenší sestava: procesorová karta JPR-1ZI (Z80), displej AND-1Z a klávesnice EKL-1. V paměti je monitor
MikroMon Libora Lasoty, malý program na prohlížení a zápis paměti, porty a spouštění strojového kódu.

## Vyzkoušejte

1. Klikněte do okna obrazovky. MikroMon se ohlásí `MikroMon 2kB V1.3`.
2. `?` a Enter vypíše povely: `M adr` paměť, `> adr bajt` zápis, `JUMP adr` spuštění, `IN`, `OUT`, `LOAD`…
3. Zkuste `M F000` (začátek MikroMonu v paměti).

## Dál

- Program se nahraje přes Soubor → Nahrát program do paměti a spustí `JUMP adresa`.
- Pro ladění je tu Ladění → Debugger (krokování F11, zarážky).
