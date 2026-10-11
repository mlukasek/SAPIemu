; HW bench: TBRE of the DSM-1V UART (channel A, 10h) right after writes, and the ports +3 and +0.
; 1 one data write, then 32 status reads (INIR, 21 T = 5.25 us apart)
; 2 two data writes back to back, then 64 status reads
; 3 IN +3, IN +0, IN +1
; The 99 values go to RAM 8100h, then over the PicoROM comms channel (window 0400-07FF) when the PC opens it.
; The analyzer gives the time of each read (/IOR) against the writes (/IOW). pasmo tbre.asm tbre.bin

BASE    equ 10h
BUF     equ 8100h
W       equ 0400h
ACTIVE  equ W+04h
OUTSEQ  equ W+10h
OUTAREA equ W+300h

wait    macro n
        local dl
        ld de,n         ; n x 26 T (6.5 us at 4 MHz)
dl:     dec de
        ld a,d
        or e
        jr nz,dl
        endm

        org 0
        di
        wait 1540       ; 10 ms
        out (BASE+3),a
        xor a
        out (BASE),a
        ld a,0Dh
        out (BASE+1),a
        ld a,(8000h)    ; /MR ends the UART reset
        wait 308        ; 2 ms
        ld hl,BUF
        ld a,55h        ; 1
        out (BASE+2),a
        ld bc,2000h+BASE+1
        inir
        wait 462        ; 3 ms
        ld a,55h        ; 2
        out (BASE+2),a
        out (BASE+2),a
        ld bc,4000h+BASE+1
        inir
        wait 462
        in a,(BASE+3)   ; 3
        ld (hl),a
        inc hl
        in a,(BASE)
        ld (hl),a
        inc hl
        in a,(BASE+1)
        ld (hl),a
wact:   ld a,(ACTIVE)   ; the PC opened the channel
        cp 1
        jr nz,wact
        ld hl,BUF
        ld b,99
        ld d,high OUTAREA
send:   ld a,(OUTSEQ)
        ld c,a
        ld e,(hl)
        ld a,(de)       ; the read sends (HL)
sw:     ld a,(OUTSEQ)
        cp c
        jr z,sw
        inc hl
        djnz send
        jr $

        ds 0400h-$,0FFh
        ds 0400h,0
