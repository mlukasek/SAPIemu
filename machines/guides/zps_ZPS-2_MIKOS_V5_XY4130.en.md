# XY 4130 plotter

The XY 4130 graphics unit (Laboratorní přístroje Praha) draws with a pen on A4 paper in steps of 0.1 mm. The TESLA
DIZ driver in EPROM behind MIKOS V5 makes it the system printer L:; in the emulator the drawing grows in the plotter
window.

## Try it

1. At start the driver reports `L=XY4130`, runs the carriage to the left stop and READY lights. In about 8 seconds
   `MONITOR V5.0` and the dot follow.
2. Press **PEN** in the Plotter XY 4130 window (View -> Plotter), the yellow lamp lights. Without it the pen does
   not draw, as on the real plotter.
3. File -> Load program into memory -> `media\mikos\XY4130_DEMO.hex`, then `G8000` and Enter after the dot in the
   AND-1 window (the screen with the dot): the text `SAPI-1 A XY 4130` and a frame with its diagonals (about
   20 s). Then MIKOS starts again.
4. `G181E` and Enter, any key when it asks for paper: the whole character set (almost 3 minutes, F8 speeds it up).

## More

- New sheet turns READY off, the pen draws again after `G1803`. Drawing from BASIC and `LLIST`:
  [XY 4130 manual](docs/navod-xy4130.md) (Czech).
