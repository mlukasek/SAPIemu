# Magnetická páska RMP-1 a IZOT SM 5300

Magnetopásková jednotka IZOT SM 5300 s řadičem RMP-1 a CP/M J. Bíby. V okně Média se točí cívky, svítí kontrolky
a vpravo je mapa pásky.

## Vyzkoušejte

1. Zobrazení → Média: mechanika s ukázkovou páskou SAPI01.
2. Vypište pásku:

```
B:DIRMT 0
```

3. `B:MTDAT`, funkce 1 zkopíruje soubor z pásky na disketu (páska `SAPI01`, soubor `2`, blok `800`, záznam `80`,
   jméno `MTDATMAN`, cíl `A:MTDAT.MAN`).

## Dál

- Nová páska, zápis a kopie z pásky na pásku krok za krokem: [návod RMP-1](../../docs/navod-rmp1.md).
