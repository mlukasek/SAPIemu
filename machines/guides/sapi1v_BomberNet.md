# SAPI-1 V: BomberNet

Síťový Bomberman BomberNet 0.3.0 na sestavě V s barevnou grafikou CGA-1V, zvukem YM3812 a síťovou kartou DPI-1L.
Hraje se proti počítači, ve více hráčích na jednom SAPI, nebo po síti s dalšími SAPI, Sharpy MZ-800, ZX Spectry,
CPC a PC.

## Jak spustit

1. Po zapnutí ECMON nabídne volby 0–3 (`Zadej 0-3`). Napište `3` (CP/M z RAM disku). Volby 0–2 čekají na řadič
   disket, který tahle sestava nemá, a stroj na nich zůstane stát. Reset sám nepomůže, ECMON si volbu pamatuje:
   stiskněte `3` hned po resetu (F12), nebo použijte Stroj → Vypnout a zapnout (Ctrl+F12).

```
3
```

2. Při startu se vypíše „PMD32-SD nenalezen“ a „INIT HDD YXYX…“: sestava nemá SD kartu ani IDE disk, je to v
   pořádku. Pak napište:

```
BOMBNET
```

3. Hra je v okně CGA-1V. V menu šipky nahoru a dolů vybírají řádek, šipky vlevo a vpravo mění hodnotu, mezera
   začne hru.

## Ovládání

- Šipky a mezera (sada A), nebo W A S D a E (sada B). Jeden stisk šipky posune hráče o celé políčko.
- Joystick je plynulejší: Ctrl+F8 zapne joystick z klávesnice PC (šipky, mezerník = palba).
- ESC opustí zápas, Ctrl+C vrátí do CP/M.

## Hra po síti

1. V menu nastavte NETWORK na HOST: hra založí místnost a ukáže její kód. Druhý hráč nastaví JOIN a kód zadá.
2. Oba potvrdí READY mezerou nebo palbou. Hostitel může dát READY kdykoli, hra počká na ostatní.
3. Karta DPI-1L se připojí k relay `ws://api.mzpico.com/net` až po volbě HOST nebo JOIN. Spojení není šifrované,
   relay vidí IP adresu a kód místnosti. Bez internetu hra hlásí NO CONNECTION, místní hra funguje dál.
4. V jedné místnosti hraje skutečné SAPI-1 s DPI-1L, další SAPIemu, Sharp MZ-800 (i web
   [mzpico.com](https://mzpico.com)), ZX Spectrum, CPC a PC.

Když se síťový zápas přeruší během hlášky na konci kola, hra zamrzne a pomůže jen Ctrl+C. Je to chyba jádra
BomberNetu.

## Dál

- Port vznikl v repu [SAPI-BomberNet](https://github.com/mlukasek/SAPI-BomberNet). Hra je
  [BomberNet](https://github.com/MZPico/BomberNet) Martina Matyáše podle Bomber Man © 1983 Hudson Soft.
