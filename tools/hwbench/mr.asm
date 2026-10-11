; HW bench: the MR latch of the DSM-1V UART (channel A, 10h). Expected on the line: "CD" only.
; 1 UART reset + 8N1, 'A' at once (lost: MR held since the reset write)
; 2 a ROM read (JPR-1V gives no /MR for its ROM), 'B' (lost: MR still held)
; 3 a RAM read 8000h (/MR on the bus ends MR), 'C' (sent), 'D' after TBRE (sent)
; steps 2 ms apart, 10 ms after the reset; no stack. pasmo mr.asm mr.bin

BASE    equ 10h

gap     macro
        local dl
        ld de,308       ; 2 ms
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
        out (BASE+3),a  ; 1
        xor a
        out (BASE),a
        ld a,0Dh
        out (BASE+1),a
        ld a,'A'
        out (BASE+2),a
        gap
        ld a,(romdata)  ; 2
        ld a,'B'
        out (BASE+2),a
        gap
        ld a,(8000h)    ; 3
        ld a,'C'
        out (BASE+2),a
w3:     in a,(BASE+1)
        and 40h
        jr z,w3
        ld a,'D'
        out (BASE+2),a
        jr $

romdata: db 55h

        ds 2048-$,0FFh
