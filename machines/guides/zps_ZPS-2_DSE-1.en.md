# DSE-1 EPROM programmer

The DSE-1 board (Tesla Elstroj) programs 2716 and 2732 EPROMs and can simulate them in its 4 KB of RAM. The DSE
program by Ing. P. Navrátil is in the EPROMs of the REM-1 card from 1000h. A blank 2716 is in the socket of the
programmer. The Media window shows the socket, the LEDs of the cable, the programming voltage Vpp and the length of
the pulse.

## Try it

1. Click the screen window, type `G1000` after the dot and Enter. The menu of the DSE program appears.
2. Blank check: `K`, any key at `VLOZ EPROM` (insert the EPROM), `N` at `2732? A=ANO N`, then the range `0` Enter
   and `7FF` Enter. The program prints `CISTA  0000 - 07FF` (blank).
3. Data into RAM: `F` and `5000,50FF,55` Enter fill 5000-50FF with the byte 55h.
4. Programming: `P`, `5000` Enter, `50FF` Enter, the EPROM address `0` Enter, any key and `N`. During
   `PROGRAMUJI` the Media window shows LED1 lit, Vpp at 25 V and a pulse of 50 ms. After about 13 s the program
   prints `SHODNY OBSAH  0000 - 00FF` (the same content).
5. The programmed EPROM is the working copy `work\dse-1\prazdna_2716.bin`. In the Media window the button `...` ->
   Restore from the original erases it like UV light, New 2716 puts in another blank one.
