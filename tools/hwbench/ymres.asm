; HW bench (ymres): the YM3812 status of MPH-1V (56h) during and right after its reset (/RST is held by a
; 74LS164 for about 36 us after /IRES). Straight from the reset, 96 reads by INI in groups of four (16 T apart,
; 87 T per group: 21.75 T = 5.4 us per read on average at 4 MHz), covering about 0-520 us.
; Read k (from 0) starts its I/O cycle at 27 + 87 * (k / 4) + 16 * (k % 4) T after the first fetch.
; The I/O keeps A10 = 0 (B = F3h..F0h): the PicoROM watches A0-A10 only.
; Results at 9000h (96 bytes), sent over the PicoROM channel as '*' and hex digits. pasmo ymres.asm ymres.bin

N       equ 96
W       equ 0400h
ACTIVE  equ W+04h
OUTSEQ  equ W+10h
OUTAREA equ W+300h

        org 0
        ld hl,9000h         ; 10 T
        ld c,56h            ;  7 T
        ld e,N/4            ;  7 T
rd:     ld b,0F3h           ;  7 T
        ini                 ; 16 T each
        ini
        ini
        ini
        dec e               ;  4 T
        jr nz,rd            ; 12 T
        di
        ld sp,0BF00h
wact:   ld a,(ACTIVE)
        cp 1
        jr nz,wact
        ld a,'*'
        call send
        ld hl,9000h
        ld b,N
nx:     ld a,(hl)
        rrca
        rrca
        rrca
        rrca
        call hexc
        ld a,(hl)
        call hexc
        inc hl
        djnz nx
        jr $

hexc:   and 0Fh
        add a,'0'
        cp '9'+1
        jr c,send
        add a,'A'-'9'-1
send:   push de             ; A over the comms channel
        push bc
        ld d,high OUTAREA
        ld e,a
        ld a,(OUTSEQ)
        ld c,a
        ld a,(de)
sw:     ld a,(OUTSEQ)
        cp c
        jr z,sw
        pop bc
        pop de
        ret

        ds 0400h-$,0FFh
        ds 0400h,0
