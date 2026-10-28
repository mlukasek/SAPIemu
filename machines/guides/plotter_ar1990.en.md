# Plotter from AR 1990

A DIY design published in Amatérské radio 1990 (V. Julius OK1IVJ): the JPR-1 card of SAPI-1 is not a computer here
but the controller of a Minigraf 0507 plotter. It executes Sharp MZ-1P16 plotter commands, sent in the emulator from
the Minigraf window. The machine has no display.

## Try it

1. In the Minigraf window (View -> Plotter) the plotter beeps after power on and RDY lights. Press **READY**: the
   carriage runs to the left stop and moves 10 mm away from it, that is the origin.
2. **Demo** draws a frame, axes, sine waves, a star and texts from `media/ar1990/ukazka.txt` (about two and a half
   minutes, F8 speeds it up).
3. When the demo has finished, **TEST** writes `MZ-821` between asterisks under it and underlines it four times.
4. Press **New sheet** and type into the line with the hint "commands, e.g. D400,0" (above New sheet) one by one
   `\x02` (graphics mode), `I` (the origin), `D400,0`, `D400,400`, `D0,400` and `D0,0`, each with Enter: a square of
   50 x 50 mm (steps of 0.125 mm).

## More

- All commands, the switches and Whole test (the character set): [AR 1990 plotter manual](docs/navod-ar1990.md),
  the wiring: [AR 1990](docs/desky/AR1990.md) (both Czech).
