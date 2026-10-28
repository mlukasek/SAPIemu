# SAPI-1 V: Prince of Persia

Hra Prince of Persia (Jordan Mechner, 1989) na sestavě V s barevnou grafikou CGA-1V a zvukem YM3812. Logika hry je
převedená z Apple II, obrázky a hudba jsou z DOS verze. Celá hra i data jsou na RAM disku A:.

## Jak spustit

1. Po zapnutí stiskněte `3` (CP/M z RAM disku), pak napište:

```
POP
```

2. Hra je v okně CGA-1V, hudba hraje při rychlosti 100 %. Úvod přeskočí klávesa nebo fire, tím začne hra.
3. Ctrl-Q vrátí do CP/M.

## Ovládání

- Šipky nebo J, L, I, K: běh, skok nahoru, přikrčení; U a O skok šikmo.
- Mezerník: opatrný krok, chycení okraje, rána mečem, napití. Shift se směrem dělá totéž co směr s tlačítkem.
- ESC pauza (v pauze ESC krok o snímek, Enter dál), T zbývající čas.
- Joystick: Ctrl+F8 zapne joystick z klávesnice (šipky, mezerník = fire).

## Dál

- Port vznikl v repu [SAPI-Prince-of-Persia](https://github.com/mlukasek/SAPI-Prince-of-Persia), na skutečném SAPI-1
  běží stejně rychle jako originál.
