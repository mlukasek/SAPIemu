# ZPS-4 s CP/M 62K

ZPS-4 má procesor Z80 (karta JPR-1Z), displej AND-1Z 64 × 20 znaků, disketu 8" a RAM disk RMD-1. Systém je
CP/M 2.2/P 62K R. Peřiny, podle dobové dokumentace základní verze pro ZPS-4.

## Vyzkoušejte

1. Klikněte do okna obrazovky. Naběhne `CPM V2.2 V/P1` a výzva `A>`.
2. `DIR` vypíše systémovou disketu (zdrojáky BIOSu, `CPM.DOC`, programy W40, W64, X64…).
3. `TYPE CPM.DOC` ukáže dobový popis systému.

## Dál

- RAM disk RMD-1 je jednotka E:, po zapnutí je v něm náhodný obsah jako ve skutečné paměti DRAM.
  Připraví ho `RTEST`.
- Podrobnosti: [CP/M ZPS-4](../../docs/software/CPM_ZPS-4.md).
