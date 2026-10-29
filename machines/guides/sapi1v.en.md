# SAPI-1 V

The V set of Libor Lasota's boards: Z80 at 4 MHz (JPR-1V), 64 KB of memory, a VGA display (AND-1V), the CGA-1V
colour graphics, the MPH-1V sound card with the YM3812 chip, a 4 MB RAM disk and an IDE disk. ECMON boots CP/M.

## Try it

1. After power on ECMON asks `Zadej 0-3` ("enter 0-3"). Type `3` (CP/M from the RAM disk). Choices 0 to 2 wait
   for a floppy controller the set does not have (as the real board does), and the machine stays stuck there. A
   reset alone does not help, ECMON remembers the choice: press `3` right after the reset (F12), or use Machine ->
   Switch off and on (Ctrl+F12).

```
3
```

2. `DIR` lists the RAM disk A:. B: and C: are the IDE disk, empty in a new installation (`No File`).
3. CGA-1V graphics: File -> Load program into memory -> `media\cga-1v\castle.hex`, then in CP/M:

```
SAVE 64 CASTLE.COM
CASTLE
```

4. A castle appears in the CGA-1V window. TURBO (4 MHz) is switched by a button on the ZDR panel in the
   configurator.

## More

- Games and demos for this set are ready in the machine "SAPI-1 V: games and demos".
