; PicoROM comms test, SAPI -> PC only: sends 00h, 01h, ... FFh 16 times (4096 bytes), each after out_seq changed,
; then stops (the PC must read everything before it ends the session, else the PicoROM firmware blocks).
; pasmo pico_out.asm pico_out.bin

W       equ 0400h
ACTIVE  equ W+04h
OUTSEQ  equ W+10h
OUTAREA equ W+300h

        org 0
        di
wact:   ld a,(ACTIVE)
        cp 1
        jr nz,wact
        ld b,0
        ld h,16         ; 16 x 256 bytes
        ld d,high OUTAREA
loop:   ld a,(OUTSEQ)
        ld c,a
        ld e,b
        ld a,(de)       ; sends B
sw:     ld a,(OUTSEQ)
        cp c
        jr z,sw
        inc b
        jr nz,loop
        dec h
        jr nz,loop
        jr $

        ds 0400h-$,0FFh
        ds 0400h,0
