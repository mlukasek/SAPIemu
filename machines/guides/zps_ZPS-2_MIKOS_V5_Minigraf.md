# Zapisovač Minigraf 0507

Malý souřadnicový zapisovač ARITMA Minigraf 0507 na desce DPP-1. Ovladač v EPROM kreslí z monitoru i z BASICu.
Kresba roste v okně zapisovače a uloží se jako PNG nebo SVG.

## Vyzkoušejte

1. Klikněte do okna obrazovky a za tečku napište `G1800` a Enter: pero dojede na počátek.
2. `G1809` a Enter nakreslí celou znakovou sadu zapisovače (necelé 2 minuty, F8 zrychlí).
3. Ukázka z IQ 151: Soubor → Nahrát program do paměti → `media\mikos\BASIC_EXP_V5.0.hex`, za tečku `B`, v okně
   zapisovače Nový papír, obsah `media\mikos\MINIGRAF_DEMO.bas` vložit přes Stroj → Klávesnice → Vložit text
   (Ctrl+Shift+V), pak `RUN`.

## Dál

- Kreslení v milimetrech příkazem `KRES` a vlastní kresby: [návod Minigraf](../../docs/navod-minigraf.md).
