# ZPS-2 s MIKOSem V5 a grafikou DGD-1

MIKOS V5.0 (Tesla DIZ 1987) s klávesnicí Consul a grafická karta DGD-1 (320 × 240 bodů). Grafiku kreslí BASIC
EXP V5.0/G, který se nahraje z adresáře `media\mikos`.

## Vyzkoušejte

1. Soubor → Nahrát program do paměti (Intel HEX)… → `media\mikos\BASIC_EXP_V5.0G.hex`.
2. Klikněte do okna obrazovky a za tečku napište `B`. Naběhne BASIC s výzvou `:`.
3. Napište řádek a Enter:

```
CLSG:FOR I=0 TO 319 STEP 8:MOVE I,0:PLOT 319-I,239:NEXT I
```

4. V okně grafiky DGD-1 se nakreslí vějíř čar.

## Dál

- `BASIC_EXP_V5.0.hex` je BASIC bez grafiky, `BASIC_EXP_V5.0G_SZ.hex` umí navíc kreslit na zapisovač (`KRES`).
- MIKOS V5 a jeho povely: [MIKOS](../../docs/software/MIKOS.md).
