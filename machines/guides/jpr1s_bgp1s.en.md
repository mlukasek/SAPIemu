# JPR-1S and graphics BGP-1S (8080)

Libor's CPU card JPR-1S (8080) with the BGP-1S colour graphics that draws like the PMD-85. The ANK-1M2 keyboard has
a joystick connector. The memory holds MIKROBIOS 1.02, PMD-85 programs are loaded as Intel HEX.

## Manic Miner

1. File -> Load program into memory (Intel HEX)... -> `media\jpr-1s\manic_miner.hex`.
2. Click the screen window and type:

```
JUMP 4400
```

3. The title screen shows the keys: **O** left, **P** right, **SHIFT** jump, **M** melody, **Enter** start.
4. For playing turn on Machine -> Keyboard -> Game mode (Ctrl+F7): keys are held by their position like on the
   real keyboard and Shift is SHIFT.

## More

- `media\jpr-1s\auto.hex` is the car race "auto" (also `JUMP 4400`).
- Manic Miner: Vít Libovický and Daniel Jenne, ViLiSoft and DaJeSoft 1985 (port from PMD-85), ported to SAPI-1
  with the BGP-1S by Libor Lasota.
