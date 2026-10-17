# SAPI-1 V: hry a dema

Sestava V s oběma grafikami, CGA-1V i DGD-1V, a zvukem YM3812. Všechno je na RAM disku A:.

## Jak spustit

1. Po zapnutí stiskněte `3` (CP/M z RAM disku), pak napište jméno programu a Enter. `DIR` ukáže všechny soubory.
2. Obraz her a dem je v okně CGA-1V nebo DGD-1V, zvuk hraje při rychlosti 100 %.
3. Joystick: Ctrl+F8 zapne joystick z klávesnice (šipky, mezerník = fire).

## Hry

- `BADAPPLE`: demo Bad Apple!! (3:39, video z RAM disku a hudba na YM3812).
- `FXS4`: Fuxoft Soundtrack IV, hudební demo; skladby se vybírají klávesami A až Z.
- `FLAPPY`: Flappy na CGA-1V (dB-SOFT 1984, port z MZ-800).
- `FLAPDGD`: Flappy na DGD-1V (port Libora Lasoty). Ovládá se joystickem: Ctrl+F8, mezerník = fire na úvodní
  obrazovce a ve výběru scény.
- `BOMBER`: Bomberman (Hudson Soft 1983, port z MZ-700), mezerník začne hru.
- `BKP`: textová hra Bratrstvo kočičí pracky (verze pro SAPI Libora Lasoty); na začátku se volí výstup, `3` je
  displej AND-1V.

## Dema a hudba Libora Lasoty

- `Y1STDEMO`: demo zvukové karty s obrázkem na DGD-1V; `SNDDEMON`: zvukové demo (jen zvuk).
- `CASTLE`, `REMBRAN1`, `REMBRAN2`: obrázky na CGA-1V; `CGATEST`, `MPHTEST`: testy karet.
- Hudba VGM a moduly RAD:

```
SPLAY HELLFIRE.VGM
RAT SMJOOP.RAD
```

  Další skladby ukáže `DIR *.VGM` a `DIR *.RAD`.
