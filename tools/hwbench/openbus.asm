; HW bench: what a read returns when no card drives the data bus (open bus).
;  1-3  IN A,(9Ah), IN A,(0A0h), IN A,(0C5h)      (operand = the port)
;  4    IN A,(C)  C = 9Ah, B = 5Ah                (ED 78; B on A8-A15)
;  5    IN B,(C)  C = 0A0h, B = 33h -> stored B   (ED 40)
;  6    IN A,(C)  C = 0C5h, B = 0FFh
;  7    LD A,(0C000h) with MAP1 = H, MAP2 = L: the 16 KB hole of RAM-1V, no CGA-1V (3A 00 C0)
;  8    LD A,(0C123h) the same                     (3A 23 C1)
;  9    LD A,(HL) HL = 0D0F0h, the same            (7E)
; 10    IN A,(9Ah) right after LD A,(8000h) (RAM holds 5Ah there)
; The values go over the PicoROM comms channel ('*' and hex digits). pasmo openbus.asm openbus.bin

RESV    equ 9000h
NV      equ 10
W       equ 0400h
ACTIVE  equ W+04h
OUTSEQ  equ W+10h
OUTAREA equ W+300h

        org 0
        di
        ld sp,0BF00h
        ld ix,RESV
        in a,(9Ah)
        ld (ix+0),a
        in a,(0A0h)
        ld (ix+1),a
        in a,(0C5h)
        ld (ix+2),a
        ld bc,5A9Ah
        in a,(c)
        ld (ix+3),a
        ld bc,33A0h
        in b,(c)
        ld (ix+4),b
        ld bc,0FFC5h
        in a,(c)
        ld (ix+5),a
        ld a,40h            ; MAP1 = H, MAP2 = L: RAM-1V leaves C000-FFFF, the CGA-1V needs MAP2 too
        out (63h),a
        ld a,(0C000h)
        ld (ix+6),a
        ld a,(0C123h)
        ld (ix+7),a
        ld hl,0D0F0h
        ld a,(hl)
        ld (ix+8),a
        xor a
        out (63h),a
        ld a,5Ah
        ld (8000h),a
        ld a,(8000h)
        in a,(9Ah)
        ld (ix+9),a
wact:   ld a,(ACTIVE)
        cp 1
        jr nz,wact
        ld a,'*'
        call put
        ld hl,RESV
        ld b,NV
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
        jr c,put
        add a,'A'-'9'-1
put:    push de
        push bc
        ld d,high OUTAREA
        ld e,a
        ld a,(OUTSEQ)
        ld c,a
        ld a,(de)
pw:     ld a,(OUTSEQ)
        cp c
        jr z,pw
        pop bc
        pop de
        ret

        ds 0400h-$,0FFh
        ds 0400h,0
