# SAPI-1 V: games and demos

The V set with both graphics cards, CGA-1V and DGD-1V, and YM3812 sound. Everything is on the RAM disk A:.

## How to start

1. After power on ECMON offers the choices 0–3 ("Zadej 0-3" means "enter 0-3"). Type `3` (CP/M from the RAM
   disk). The choices 0–2 wait for a floppy controller this machine does not have, and the machine stays stuck
   there. A reset alone does not help, ECMON remembers the choice: press `3` right after the reset (F12), or use
   Machine -> Switch off and on (Ctrl+F12).

```
3
```

2. The start prints "PMD32-SD nenalezen" (not found) and "INIT HDD YXYX…": the machine has no SD card and no IDE
   disk, that is fine. At the prompt `A>` type the name of a program and Enter; the button Type into SAPI at the
   blocks below does it for you. `DIR` lists all files.
3. The games and demos show in the CGA-1V or DGD-1V window, the sound plays at 100 % speed. Joystick: Ctrl+F8 turns
   on the joystick from the PC keyboard (arrows, space = fire).
4. A program that does not end by itself is stopped by a reset, F12: ECMON remembers the choice 3 and starts CP/M
   again in a few seconds.

## Games

- Flappy on CGA-1V (dB-SOFT 1984, port from the MZ-800). Arrows and space (start and action), or the joystick and
  fire: the game is played with what started it. Backspace gives up the level, ESC returns to CP/M.

```
FLAPPY
```

- Flappy on DGD-1V (port by Libor Lasota). Played with the joystick: Ctrl+F8 turns it on from the PC keyboard
  (arrows, space = fire), fire on the title and in the scene selection. It does not end by itself, F12 stops it
  (CP/M starts again).

```
FLAPDGD
```

- Bomberman (Hudson Soft 1983, port from the MZ-700). The arrows walk, space starts the game and drops bombs, ESC
  returns to CP/M. With the joystick you can walk and drop bombs at once.

```
BOMBER
```

- Bratrstvo kočičí pracky, a text adventure in Czech (SAPI version by Libor Lasota). First choose the output: `3`
  is the AND-1V display with Czech letters. Commands are Czech words (SEVER = north, SEBER = take; short S, J, V, Z
  = north, south, east, west, R = look, I = inventory), POMOC = help, KONEC and `A` return to CP/M.

```
BKP
```

## Demos and music

- Bad Apple!!: a video from the RAM disk with music on the YM3812 (3:39). The picture flashes fast in places. A key
  ends the demo.

```
BADAPPLE
```

- Fuxoft Soundtrack IV, a music demo: keys A to Z choose the tunes, Enter speeds the music up, space changes the
  animation, ESC returns to CP/M.

```
FXS4
```

- A demo of the sound card with a picture on DGD-1V (Libor Lasota); `SNDDEMON` is a sound demo without a picture.
  Neither demo ends by itself, F12 stops them (CP/M starts again).

```
Y1STDEMO
```

- Pictures by Libor Lasota on CGA-1V: a castle, more are `REMBRAN1` and `REMBRAN2`. Tests of the cards: `CGATEST`,
  `MPHTEST`.

```
CASTLE
```

- VGM tunes and RAD modules (players by Libor Lasota), ESC stops the playing. `DIR *.VGM` and `DIR *.RAD` list the
  other tunes.

```
SPLAY HELLFIRE.VGM
```

```
RAT SMJOOP.RAD
```
