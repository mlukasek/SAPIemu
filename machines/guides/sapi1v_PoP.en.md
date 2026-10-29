# SAPI-1 V: Prince of Persia

The game Prince of Persia (Jordan Mechner, 1989) on the V set with the CGA-1V colour graphics and YM3812 sound. The
game logic is converted from the Apple II, the pictures and the music come from the DOS version. The whole game and
its data are on the RAM disk A:.

## How to start

1. After power on ECMON offers the choices 0–3 ("Zadej 0-3" means "enter 0-3"). Type `3` (CP/M from the RAM
   disk). The choices 0–2 wait for a floppy controller this machine does not have, and the machine stays stuck
   there. A reset alone does not help, ECMON remembers the choice: press `3` right after the reset (F12), or use
   Machine -> Switch off and on (Ctrl+F12).

```
3
```

2. At the prompt `A>` start the game:

```
POP
```

3. The game shows in the CGA-1V window, the music plays at 100 % speed. A key or fire skips the intro and starts
   the game.
4. Ctrl-Q returns to CP/M.

## Controls

- Arrows or J, L, I, K: run, jump up, crouch; U and O jump diagonally.
- Space: careful step, grab a ledge, strike with the sword, drink. Shift with a direction works as the direction
  with the button.
- ESC pause (in the pause ESC steps one frame, Enter goes on), T the time left.
- Joystick: Ctrl+F8 turns on the joystick from the keyboard (arrows, space = fire).

## More

- The port comes from the repository [SAPI-Prince-of-Persia](https://github.com/mlukasek/SAPI-Prince-of-Persia), on
  a real SAPI-1 it runs as fast as the original.

## Copyright

A demonstration of historic technology: an unofficial, non-commercial port of the game to a 40-year-old
Czechoslovak computer. Prince of Persia © Jordan Mechner, graphics and music of the DOS version © Brøderbund
Software, the rights to publish Prince of Persia games are held by Ubisoft. The SAPIemu licence (GPLv3) does not
cover the game data. At the request of the copyright holders I will change or remove anything. Contact: Martin
Lukášek, [8bity.cz](https://www.8bity.cz).
