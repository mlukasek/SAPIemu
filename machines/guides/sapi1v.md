# SAPI-1 V

Sestava V z desek Libora Lasoty: Z80 4 MHz (JPR-1V), 64 KB paměti, displej VGA (AND-1V), barevná grafika
CGA-1V, zvuková karta MPH-1V s čipem YM3812, RAM disk 4 MB a IDE disk. Systém ECMON zavede CP/M.

## Vyzkoušejte

1. Po zapnutí se ECMON ptá na volbu `Zadej 0-3`. Napište `3` (CP/M z RAM disku). Volby 0 až 2 čekají na řadič
   disket, který sestava nemá (jako skutečná deska), a stroj na nich zůstane stát. Reset sám nepomůže, ECMON si
   volbu pamatuje: stiskněte `3` hned po resetu (F12), nebo použijte Stroj → Vypnout a zapnout (Ctrl+F12).

```
3
```

2. `DIR` vypíše RAM disk A:. B: a C: jsou IDE disk, v nové instalaci prázdný (`No File`).
3. Grafika CGA-1V: Soubor → Nahrát program do paměti → `media\cga-1v\castle.hex`, pak v CP/M:

```
SAVE 64 CASTLE.COM
CASTLE
```

4. Obrázek hradu se ukáže v okně CGA-1V. TURBO (4 MHz) se přepíná tlačítkem na panelu ZDR v konfigurátoru.

## Dál

- Hry a dema pro tuto sestavu jsou připravené v sestavě „SAPI-1 V: hry a dema“.
- Desky V: [JPR-1V](docs/desky/JPR-1V.md), [CGA-1V](docs/desky/CGA-1V.md), [MPH-1V](docs/desky/MPH-1V.md).
