# Minigraf 0507 plotter

The small ARITMA Minigraf 0507 plotter on the DPP-1 card. Its driver in EPROM draws from the monitor and from
BASIC. The drawing grows in the plotter window and is saved as PNG or SVG.

## Try it

1. Click the screen window, type `G1800` after the dot and Enter: the pen goes to its origin.
2. `G1809` and Enter draws the whole character set of the plotter (almost 2 minutes, F8 speeds it up).
3. The example of the IQ 151: File -> Load program into memory -> `media\mikos\BASIC_EXP_V5.0.hex`, `B` after the
   dot, New sheet in the plotter window, paste the text of `media\mikos\MINIGRAF_DEMO.bas` with Machine ->
   Keyboard -> Paste text (Ctrl+Shift+V), then `RUN`.
