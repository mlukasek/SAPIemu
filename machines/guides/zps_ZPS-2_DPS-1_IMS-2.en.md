# Measuring bench on the IMS-2 bus

The DPS-1 card connects SAPI to the IMS-2 instrument bus (the Czechoslovak IEEE 488). On the bus are a Metra M1T 330
voltmeter and a Tesla BS 525 power supply, whose knobs turn with the mouse. BASIMS, a BASIC with the command `IMS#`,
measures.

## Try it

1. The IMS-2 measuring bench window is on top. The voltmeter measures the supply by itself (about `12.564 V`);
   turn a knob of the supply and the value changes.
2. Load BASIMS: File -> Load program into memory (Intel HEX)... -> `media\dps-1\BASIMS.hex`, click the screen
   window, type `G4400` after the dot and Enter. `* BASIC - EX  V4.0 *` starts.
3. The programs are on the cassette in the tape recorder: `LOAD`, the name `PRIKLAD`, PLAY in the Media window,
   `Y`, then `RUN` prints the measured voltage.
