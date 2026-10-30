# Plotter z AR 1990

Stavební návod z Amatérského radia 1990 (V. Julius OK1IVJ): deska JPR-1 ze SAPI-1 tu není počítač, ale řadič
zapisovače Minigraf 0507. Provádí povely plotteru Sharp MZ-1P16, které jí v emulátoru posílá okno Minigraf. Sestava
nemá displej.

## Vyzkoušejte

1. V okně Minigraf (Zobrazení → Zapisovač) plotter po zapnutí pípá a svítí RDY. Stiskněte **READY**: vozík najede
   na levý doraz a popojede o 10 mm, tam je počátek.
2. **Ukázka** nakreslí rámeček, osy, sinusovky, hvězdu a texty ze souboru `media/ar1990/ukazka.txt` (asi dvě a půl
   minuty, F8 zrychlí).
3. Až ukázka dokreslí, **TEST** pod ni napíše `MZ-821` mezi hvězdičkami a čtyřikrát ho podtrhne.
4. Stiskněte **Nový papír** a do řádku s nápovědou „povely, např. D400,0“ (nad tlačítkem Nový papír) pište postupně
   `\x02` (grafický režim), `I` (počátek), `D400,0`, `D400,400`, `D0,400` a `D0,0`, každý s Enterem: vznikne
   čtverec 50 × 50 mm (krok 0,125 mm).

## Dál

- Všechny povely, přepínače a Celý test (znaková sada): [návod k plotteru z AR 1990](../../docs/navod-ar1990.md), zapojení:
  [AR 1990](../../docs/desky/AR1990.md).
