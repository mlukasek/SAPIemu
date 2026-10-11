; HW bench wiring check: a known pattern on each analyzer channel (work/hwtest/wire_check.py), no stack.
; A 3x OUT F7h (/IOW), B 5x IN F7h (/IOR), C 7x read 8000h (/MR), D 9x write 8000h (/MW),
; E OUT 63h 40h, 80h, C0h, 00h (MAP1, MAP2), F 16x read F800h = AND-1V (RDY waits),
; G DSM-1V channel A: "WIRE" CR LF (TxD), H receive interrupt on, take one byte, echo it, interrupt off (RxD, /INT0).
; Phases 1 ms apart (E: 2 ms), 10 ms after the reset. pasmo wire.asm wire.bin

BASE    equ 10h
DUMMY   equ 0F7h

gap     macro
        local dl
        ld de,154       ; 1 ms (26 T x 154 at 4 MHz)
dl:     dec de
        ld a,d
        or e
        jr nz,dl
        endm

        org 0
        di
        ld de,1540      ; 10 ms
w0:     dec de
        ld a,d
        or e
        jr nz,w0
        ld b,3          ; A
pa:     out (DUMMY),a
        djnz pa
        gap
        ld b,5          ; B
pb:     in a,(DUMMY)
        djnz pb
        gap
        ld b,7          ; C
pc:     ld a,(8000h)
        djnz pc
        gap
        ld b,9          ; D
pd:     ld (8000h),a
        djnz pd
        gap
        ld a,40h        ; E
        out (63h),a
        gap
        gap
        ld a,80h
        out (63h),a
        gap
        gap
        ld a,0C0h
        out (63h),a
        gap
        gap
        xor a
        out (63h),a
        gap
        gap
        ld b,16         ; F
pf:     ld a,(0F800h)
        djnz pf
        gap
        out (BASE+3),a  ; G: UART reset, 8N1, a memory read on the bus ends the reset
        xor a
        out (BASE),a
        ld a,0Dh
        out (BASE+1),a
        ld a,(8000h)
        ld hl,text
pg:     ld a,(hl)
        or a
        jr z,ph
pg1:    in a,(BASE+1)
        and 40h
        jr z,pg1
        ld a,(hl)
        out (BASE+2),a
        inc hl
        jr pg
ph:     in a,(BASE+2)   ; H: drop what came during the init
        ld a,80h        ; IER: /INT0 while a byte waits
        out (BASE),a
ph1:    in a,(BASE+1)
        and 80h         ; DR
        jr z,ph1
        in a,(BASE+2)
        ld c,a
ph2:    in a,(BASE+1)
        and 40h
        jr z,ph2
        ld a,c
        out (BASE+2),a
        xor a
        out (BASE),a    ; IER off
        jr $

text:   db "WIRE",13,10,0

        ds 2048-$,0FFh
