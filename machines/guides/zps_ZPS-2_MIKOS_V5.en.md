# ZPS-2 with MIKOS V5 and graphics DGD-1

MIKOS V5.0 (Tesla DIZ 1987) with the Consul keyboard and the DGD-1 graphics card (320 x 240 points). The graphics
is drawn by BASIC EXP V5.0/G, loaded from the folder `media\mikos`.

## Try it

1. File -> Load program into memory (Intel HEX)... -> `media\mikos\BASIC_EXP_V5.0G.hex`.
2. Click the screen window and type `B` after the dot. BASIC starts with the prompt `:`.
3. Type the line and Enter:

```
CLSG:FOR I=0 TO 319 STEP 8:MOVE I,0:PLOT 319-I,239:NEXT I
```

4. A fan of lines appears in the DGD-1 graphics window.

## More

- `BASIC_EXP_V5.0.hex` is BASIC without graphics, `BASIC_EXP_V5.0G_SZ.hex` also draws on a plotter (`KRES`).
