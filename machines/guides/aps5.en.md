# APS-5 measuring unit (furnaces)

The Tesla APS-5 with its original firmware controls eight furnaces by their thermocouples. The APS-5 window shows
the front panel with its keys and display, and the plant with the furnaces and a plot of the temperatures.

## Fastest: DEMO

The **DEMO** button in the APS-5 window types the setup itself: the calibration, the thermocouple types, a heating
program and the control of three furnaces. In about a minute the furnaces start to glow.

## By hand

- Press the keys of the panel with the mouse or type on the PC: digits, `M` (Esc), `R`, Backspace = ←, Enter = →.
- The first power on is a cold start. First the calibration `6R` → `9990` →, then for each furnace `0` → its
  number → and `8R` → the thermocouple type → (furnace 1: 3 = ch-a).
- `11` → shows the table of the furnaces.
- **Do not use Reset.** The real unit has none and the firmware takes it as an error, the setup is lost. The
  emulator therefore asks before a reset (F12, F9, RESET on the ZDR panel). Switch it off with the power switch or
  Machine → Switch off and on (Ctrl+F12), that is a power failure and the setup stays.
