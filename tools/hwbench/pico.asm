; HW bench test ROM: the PicoROM comms channel (no serial line). 2 KB ROM: program 0000-03FF, comms window 0400-07FF.
; SAPI -> PC: a read of 0700h+x sends x; out_seq (0410h) goes up when the byte is taken (FIFO of 32: wait for it).
; PC -> SAPI: in_seq (040Ch) goes up when a new byte is in 0600h; reading 0600h takes it (pops the FIFO).
; The program waits for the PC (active, 0404h = 1), sends a banner, then echoes the bytes in upper case.
; No RAM, no stack. pasmo pico.asm pico.bin

W       equ 0400h       ; window (1 KB aligned)
ACTIVE  equ W+04h
INSEQ   equ W+0Ch
OUTSEQ  equ W+10h
INBYTE  equ W+200h
OUTAREA equ W+300h

        org 0
        di
wact:   ld a,(ACTIVE)
        cp 1
        jr nz,wact
        ld hl,banner
ban:    ld a,(hl)
        or a
        jr z,echo
        ld ix,ban1
        jr send
ban1:   inc hl
        jr ban

echo:   ld a,(INSEQ)
        ld b,a
ech1:   ld a,(INSEQ)
        cp b
        jr z,ech1
        ld b,a
        ld a,(INBYTE)   ; the byte, and the read takes it
        cp 'a'
        jr c,ech2
        cp 'z'+1
        jr nc,ech2
        sub 20h
ech2:   ld ix,ech1
        jr send

; send A, return by JP (IX); waits until the PicoROM took it (out_seq changed). DE, C used.
send:   ld d,high OUTAREA
        ld e,a
        ld a,(OUTSEQ)
        ld c,a
        ld a,(de)       ; the read sends the byte
sw:     ld a,(OUTSEQ)
        cp c
        jr z,sw
        jp (ix)

banner: db 13,10,"SAPI-1 V: hello over the PicoROM comms channel",13,10,0

        ds 0400h-$,0FFh ; the comms window follows (0400-07FF)
        ds 0400h,0
