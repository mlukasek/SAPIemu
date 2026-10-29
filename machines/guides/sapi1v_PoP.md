# SAPI-1 V: Prince of Persia

Hra Prince of Persia (Jordan Mechner, 1989) na sestavě V s barevnou grafikou CGA-1V a zvukem YM3812. Logika hry je
převedená z Apple II, obrázky a hudba jsou z DOS verze. Celá hra i data jsou na RAM disku A:.

## Jak spustit

1. Po zapnutí ECMON nabídne volby 0–3 (`Zadej 0-3`). Napište `3` (CP/M z RAM disku). Volby 0–2 čekají na řadič
   disket, který tahle sestava nemá, a stroj na nich zůstane stát. Reset sám nepomůže, ECMON si volbu pamatuje:
   stiskněte `3` hned po resetu (F12), nebo použijte Stroj → Vypnout a zapnout (Ctrl+F12).

```
3
```

2. Na výzvu `A>` spusťte hru:

```
POP
```

3. Hra je v okně CGA-1V, hudba hraje při rychlosti 100 %. Úvod přeskočí klávesa nebo palba, tím začne hra.
4. Ctrl-Q vrátí do CP/M.

## Ovládání

- Šipky nebo J, L, I, K: běh, skok nahoru, přikrčení; U a O skok šikmo.
- Mezerník: opatrný krok, chycení okraje, rána mečem, napití. Shift se směrem dělá totéž co směr s tlačítkem.
- ESC pauza (v pauze ESC krok o snímek, Enter dál), T zbývající čas.
- Joystick: Ctrl+F8 zapne joystick z klávesnice (šipky, mezerník = palba).

## Dál

- Port vznikl v repu [SAPI-Prince-of-Persia](https://github.com/mlukasek/SAPI-Prince-of-Persia), na skutečném SAPI-1
  běží stejně rychle jako originál.

## Autorská práva

Ukázka historické technologie: neoficiální nekomerční port hry na 40 let starý československý počítač. Prince of
Persia © Jordan Mechner, grafika a hudba DOS verze © Brøderbund Software, práva vydávat hry Prince of Persia má
Ubisoft. Licence SAPIemu (GPLv3) se na data hry nevztahuje. Na žádost nositelů autorských práv cokoli upravím nebo
odstraním. Kontakt: Martin Lukášek, [8bity.cz](https://www.8bity.cz).
