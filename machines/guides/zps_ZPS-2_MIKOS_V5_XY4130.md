# Zapisovač XY 4130

Grafická jednotka XY 4130 (Laboratorní přístroje Praha) kreslí perem na papír A4 po krocích 0,1 mm. Ovladač TESLA
DIZ v EPROM za MIKOSem V5 z ní udělá systémovou tiskárnu L:, kresba roste v okně zapisovače.

## Vyzkoušejte

1. Po startu ovladač ohlásí `L=XY4130`, pošle vozík na levý doraz a rozsvítí se READY. Asi po 8 sekundách přibude
   `MONITOR V5.0` a tečka.
2. V okně Zapisovač XY 4130 (Zobrazení → Zapisovač) stiskněte **PEN**, rozsvítí se žlutá kontrolka. Bez ní pero
   nekreslí, jako u skutečného zapisovače.
3. Soubor → Nahrát program do paměti → `media\mikos\XY4130_DEMO.hex`, v okně AND-1 (obrazovka s tečkou) za
   tečku `G8000` a Enter: nápis `SAPI-1 A XY 4130` a rámeček s úhlopříčkami (asi 20 s). Pak se MIKOS spustí znovu.
4. `G181E` a Enter, na výzvu k založení papíru libovolná klávesa: celá znaková sada (necelé 3 minuty, F8 zrychlí).

## Dál

- Nový papír zhasne READY, pero pak kreslí až po `G1803`. Kreslení z BASICu a `LLIST`:
  [návod XY 4130](docs/navod-xy4130.md).
