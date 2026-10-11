; HW bench diagnostic ROM 2: how the DSM-1V UART takes writes (the received bytes on the PC tell it).
; 1. init right after /RES (as hello.asm), then bytes 00h-27h every 2 ms without waiting: the first byte that
;    arrives shows when the UART takes writes after the reset;
; 2. "abcdefghij" 23 us apart (as hello.asm writes when TBRE stays 1);
; 3. "XY" back to back;
; 4. "KLMNOPQRST" each after waiting for TBRE;
; parts 50 ms apart, then it stops (JR $).
; pasmo diag2.asm diag2.bin

BASE    equ 10h

        org 0
        di
        ld sp,0BF00h
        out (BASE+3),a
        xor a
        out (BASE),a
        ld a,0Dh
        out (BASE+1),a
        ld c,0
p1:     ld a,c
        out (BASE+2),a
        ld de,300       ; 2 ms
        call delay
        inc c
        ld a,c
        cp 28h
        jr nz,p1
        ld de,7700      ; 50 ms
        call delay
        ld a,'a'
p2:     out (BASE+2),a
        ld de,3         ; about 23 us with the loop
        call delay2
        inc a
        cp 'k'
        jr nz,p2
        ld de,7700
        call delay
        ld a,'X'
        out (BASE+2),a
        ld a,'Y'
        out (BASE+2),a
        ld de,7700
        call delay
        ld c,'K'
p4:     in a,(BASE+1)
        and 40h
        jr z,p4
        ld a,c
        out (BASE+2),a
        inc c
        ld a,c
        cp 'U'
        jr nz,p4
        jr $

; DE x 26 T (6.5 us at 4 MHz), A lost
delay:  dec de
        ld a,d
        or e
        jr nz,delay
        ret

; the same, A kept
delay2: push af
d2:     dec de
        ld a,d
        or e
        jr nz,d2
        pop af
        ret

        ds 2048-$,0FFh
