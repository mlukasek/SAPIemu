# MikroMon 1.3

The smallest set: the JPR-1ZI CPU card (Z80), the AND-1Z display and the EKL-1 keyboard. The memory holds MikroMon
by Libor Lasota, a small monitor to view and write the memory and the ports and to run machine code.

## Try it

1. Click the screen window. MikroMon reports `MikroMon 2kB V1.3`.
2. `?` and Enter lists the commands: `M adr` memory, `> adr byte` write, `JUMP adr` run, `IN`, `OUT`, `LOAD`...
3. Try `M F000` (the start of MikroMon in memory).

## More

- Load a program with File -> Load program into memory and run it with `JUMP address`.
- Debug -> Debugger steps the program (F11) and stops at breakpoints.
