# Magnetic tape RMP-1 and IZOT SM 5300

The IZOT SM 5300 tape unit with the RMP-1 controller and J. Bíba's CP/M. The reels turn in the Media window, the
lamps light and a map of the tape is on the right.

## Try it

1. View -> Media: the drive with the sample tape SAPI01.
2. List the tape:

```
B:DIRMT 0
```

3. `B:MTDAT`, function 1 copies a file from the tape to the floppy (tape `SAPI01`, file `2`, block `800`, record
   `80`, name `MTDATMAN`, target `A:MTDAT.MAN`).
