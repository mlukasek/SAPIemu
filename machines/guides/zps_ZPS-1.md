# ZPS-1 s MIKROBASICem 2.4

První sestava SAPI-1 z roku 1983: procesor 8080, displej AND-1 (40 × 24 znaků), membránová klávesnice ANK-1 a
magnetofon. MIKROBASIC je v paměti EPROM, po zapnutí se hned píše program.

## Vyzkoušejte

1. Klikněte do okna obrazovky, ať se píše na klávesnici SAPI. Na obrazovce je `MIKRO BASIC`, `READY` a výzva `>`.
2. Napište krátký program (nebo použijte tlačítko Napsat do SAPI):

```
10 FOR I=1 TO 5
20 PRINT I,I*I
30 NEXT I
RUN
```

3. Vypíše čísla 1 až 5 a jejich druhé mocniny. `LIST` ukáže program, `NEW` ho smaže.

## Dál

- Program jde uložit na kazetu do magnetofonu (okno Média, Nová kazeta…, NAHRÁT), postup je v
  [MIKROBASIC](../../docs/software/MIKROBASIC.md).
- Klávesnice ANK-1 nemá všechny znaky PC, přehled kláves je v [klávesnice](../../docs/desky/klavesnice.md).
