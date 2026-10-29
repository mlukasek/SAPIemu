# SAPI-1 V: hry a dema

Sestava V s oběma grafikami, CGA-1V i DGD-1V, a zvukem YM3812. Všechno je na RAM disku A:.

## Jak spustit

1. Po zapnutí ECMON nabídne volby 0–3 (`Zadej 0-3`). Napište `3` (CP/M z RAM disku). Volby 0–2 čekají na řadič
   disket, který tahle sestava nemá, a stroj na nich zůstane stát. Reset sám nepomůže, ECMON si volbu pamatuje:
   stiskněte `3` hned po resetu (F12), nebo použijte Stroj → Vypnout a zapnout (Ctrl+F12).

```
3
```

2. Při startu se vypíše „PMD32-SD nenalezen“ a „INIT HDD YXYX…“: sestava nemá SD kartu ani IDE disk, je to
   v pořádku. Na výzvu `A>` se píše jméno programu a Enter, tlačítko Napsat do SAPI u bloků níže to udělá samo.
   `DIR` ukáže všechny soubory.
3. Obraz her a dem je v okně CGA-1V nebo DGD-1V, zvuk hraje při rychlosti 100 %. Joystick: Ctrl+F8 zapne joystick
   z klávesnice PC (šipky, mezerník = palba).
4. Program, který se sám neukončí, zastaví reset F12: ECMON si volbu 3 pamatuje a za pár sekund znovu spustí CP/M.

## Hry

- Flappy na CGA-1V (dB-SOFT 1984, port z MZ-800). Šipky a mezerník (start a akce), nebo joystick a palba: hra se
  ovládá tím, čím se spustila. Backspace vzdá úroveň, ESC vrátí do CP/M.

```
FLAPPY
```

- Flappy na DGD-1V (port Libora Lasoty). Hraje se joystickem: Ctrl+F8 ho zapne z klávesnice PC (šipky, mezerník
  = palba), palba na úvodní obrazovce a ve výběru scény. Hra se sama neukončí, zastaví ji F12 (CP/M
  naběhne znovu).

```
FLAPDGD
```

- Bomberman (Hudson Soft 1983, port z MZ-700). Šipky chodí, mezerník začne hru a pokládá bomby, ESC vrátí do CP/M.
  S joystickem jde chodit a pokládat bomby zároveň.

```
BOMBER
```

- Bratrstvo kočičí pracky, textová hra v češtině (verze pro SAPI Libora Lasoty). Nejdřív se volí výstup: `3` je
  displej AND-1V s diakritikou. Příkazy se píšou slovy (SEVER, SEBER TENISAK; zkratky S, J, V, Z, R, I), POMOC
  poradí, KONEC a `A` vrátí do CP/M.

```
BKP
```

## Dema a hudba

Upozornění: dema rychle mění obraz a Bad Apple!! místy rychle bliká. Pokud vám blikání vadí (citlivost na
světlo, fotosenzitivní epilepsie), dema nespouštějte.

- Bad Apple!!: video z RAM disku s hudbou na YM3812 (3:39). Obraz místy rychle bliká. Klávesa demo ukončí.

```
BADAPPLE
```

- Fuxoft Soundtrack IV, hudební demo: klávesy A až Z vybírají skladby, Enter hudbu zrychlí, mezerník změní
  animaci, ESC vrátí do CP/M.

```
FXS4
```

- Demo zvukové karty s obrázkem na DGD-1V (Libor Lasota); `SNDDEMON` je zvukové demo bez obrazu.
  Obě dema se sama neukončí, zastaví je F12 (CP/M naběhne znovu).

```
Y1STDEMO
```

- Obrázky Libora Lasoty na CGA-1V: hrad, další jsou `REMBRAN1` a `REMBRAN2`. Testy karet: `CGATEST`, `MPHTEST`.

```
CASTLE
```

- Hudba VGM a moduly RAD (přehrávače Libora Lasoty), ESC přehrávání ukončí. Další skladby ukáže `DIR *.VGM`
  a `DIR *.RAD`.

```
SPLAY HELLFIRE.VGM
```

```
RAT SMJOOP.RAD
```
