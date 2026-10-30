# ZPS-2 with MIKOS V4

ZPS-2 (1985) added the RAM-1 memory and MIKOS, a cassette operating system: a monitor with commands for the memory
and for files on the tape recorder.

## Try it

1. Click the screen window. The screen shows `MIKOS-1 V4.0F +`, `SAPI-1 V4.0` and the prompt, a dot.
2. A monitor command is one letter, parameters are separated by a space (the screen shows `=` instead):
   - `Q` shows the device assignment: `C=C R=1 P=1 L=L`;
   - `D0 1F` and Enter dumps the memory from 0000 to 001F (the start of the monitor).

```
D0 1F
```

3. `K` and another letter works with the tape: `.K_D` directory, `.K_L` load a program, `.K_S` save.

## More

- The commands of MIKOS and the tape: [MIKOS](../../docs/software/MIKOS.md) (Czech).
