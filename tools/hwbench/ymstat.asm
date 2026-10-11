; HW bench (ymstat): the YM3812 status 32x before and 32x after timer 1 ran out.
; Based on ymrst.asm: when after /RES does the YM3812 of MPH-1V (base 50h) take writes? (/RST is held by a 74LS164 for 8
; clocks of 224 kHz after /IRES, about 36 us.) After a delay set by the PC (byte DELAY, DJNZ units of 3.25 us at
; 4 MHz) it writes timer 1 = FFh (80 us) and starts it, then reads the status 1 ms later: D6 = T1 flag set means
; the writes were taken. The status goes over the PicoROM comms channel ('*' and two hex digits).
; No RAM before the status is read. pasmo ymrst.asm ymrst.bin; tools/hwbench/ymrst_run.py patches DELAY.

YMA     equ 56h             ; address (write) / status (read)
YMD     equ 57h
DELAY   equ 03F0h           ; patched by the PC
W       equ 0400h
ACTIVE  equ W+04h
OUTSEQ  equ W+10h
OUTAREA equ W+300h

        org 0
        di
        ld b,40             ; 130 us: the YM is out of reset
dl:     djnz dl
        ld hl,9000h         ; 32 status reads before the timer
        ld bc,2000h+YMA
        inir
        ld a,02h            ; timer 1 = FFh: overflow after 80 us
        out (YMA),a
        ld b,2
d1:     djnz d1
        ld a,0FFh
        out (YMD),a
        ld b,10             ; the YM wants about 23 us after a data write
d2:     djnz d2
        ld a,04h            ; start timer 1
        out (YMA),a
        ld b,2
d3:     djnz d3
        ld a,01h
        out (YMD),a
        ld de,154           ; 1 ms
d4:     dec de
        ld a,d
        or e
        jr nz,d4
        ld bc,2000h+YMA     ; 32 status reads after it ran out
        inir
        ld sp,0BF00h        ; the test is over: a stack in RAM for the sending
wact:   ld a,(ACTIVE)
        cp 1
        jr nz,wact
        ld e,'*'
        call put
        ld hl,9000h
        ld b,64
nx:     ld a,(hl)
        rrca
        rrca
        rrca
        rrca
        push bc
        call hexc
        ld a,(hl)
        call hexc
        pop bc
        inc hl
        djnz nx
        jr $

; send the hex digit of the low nibble of A, or E (put)
hexc:   and 0Fh
        add a,'0'
        cp '9'+1
        jr c,hex1
        add a,'A'-'9'-1
hex1:   ld e,a
put:    ld d,high OUTAREA   ; send E
        ld a,(OUTSEQ)
        ld c,a
        ld a,(de)
pw:     ld a,(OUTSEQ)
        cp c
        jr z,pw
        ret

        ds DELAY-$,0FFh
        db 0
        ds 0400h-$,0FFh
        ds 0400h,0
