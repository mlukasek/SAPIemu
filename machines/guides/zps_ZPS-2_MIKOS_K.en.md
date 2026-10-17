# Cassette data recorder KZD-1

The KZD-1 cassette data recorder (ZPA Košíře 1986) writes data digitally on a cassette and MIKOS/K uses it like a
slow disk. The drive is drawn in the Media window.

## Try it

1. View -> Media: the recorder with its cassette.
2. A new cassette has to be prepared: type `K` and `I` after the dot, a tape name and Enter (it takes a while, the
   tape winds).
3. `.K_S`, a name and `=5000 53FF` and Enter save a piece of memory; `.K_D` lists the directory.
4. `.K_Z` winds back to the start, `.K_L` and a name load a file.
