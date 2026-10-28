# SAPI-1 V: BomberNet

The network Bomberman BomberNet 0.3.0 on the V set with the CGA-1V colour graphics, YM3812 sound and the DPI-1L
network card. Play against the computer, with several players on one SAPI, or over the network with other SAPIs,
Sharp MZ-800s, ZX Spectrums, CPCs and PCs.

## How to start

1. After power on ECMON offers the choices 0–3 ("Zadej 0-3" means "enter 0-3"). Type `3` (CP/M from the RAM
   disk); the choices 0–2 wait for a floppy controller this machine does not have.

```
3
```

2. The start prints "PMD32-SD nenalezen" (not found) and "INIT HDD YXYX…": the machine has no SD card and no IDE
   disk, that is fine. Then type:

```
BOMBNET
```

3. The game shows in the CGA-1V window. In the menu the up and down arrows pick a line, left and right change its
   value, space starts the game.

## Controls

- Arrows and space (set A), or W A S D and E (set B). One press of an arrow moves the player one whole cell.
- A joystick is smoother: Ctrl+F8 turns on the joystick from the PC keyboard (arrows, space = fire).
- ESC leaves a match, Ctrl+C returns to CP/M.

## Network play

1. In the menu set NETWORK to HOST: the game opens a room and shows its code. The other player sets JOIN and types
   the code.
2. Both confirm READY with space or fire. The host may be READY any time, the game waits for the others.
3. The DPI-1L card connects to the relay `ws://api.mzpico.com/net` only after HOST or JOIN is chosen. The
   connection is not encrypted, the relay sees the IP address and the room code. Without the internet the game
   shows NO CONNECTION, local play still works.
4. One room can hold a real SAPI-1 with DPI-1L, another SAPIemu, a Sharp MZ-800 (also the web version at
   [mzpico.com](https://mzpico.com)), a ZX Spectrum, a CPC and a PC.

If a network match is aborted during the message at the end of a round, the game freezes and only Ctrl+C helps.
This is a bug of the BomberNet core.

## More

- The port comes from the repository [SAPI-BomberNet](https://github.com/mlukasek/SAPI-BomberNet). The game is
  [BomberNet](https://github.com/MZPico/BomberNet) by Martin Matyáš after Bomber Man © 1983 Hudson Soft.
