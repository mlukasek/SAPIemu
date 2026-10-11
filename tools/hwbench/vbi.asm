; HW bench: when does the VBI flag of the CGA-1V (STATUS D7, read at FFFBh with MAP1 = MAP2 = H) come against
; VSYNC (analyzer D14, VGA pin 14)? The program polls STATUS; when D7 = 1 it writes the marker OUT (F7h) and
; acknowledges (CONFIG = 80h: /IACK, page A, no interrupt). Poll loop about 30 T (7.5 us at 4 MHz).
; pasmo vbi.asm vbi.bin

MARK    equ 0F7h
STATUS  equ 0FFFBh

        org 0
        di
        ld sp,0BF00h
        ld a,0C0h           ; MAP1 = MAP2 = H: the CGA-1V at C000h
        out (63h),a
        ld a,80h            ; acknowledge a VBI left from before
        ld (STATUS),a
poll:   ld a,(STATUS)
        rla
        jr nc,poll
        out (MARK),a        ; VBI seen
        ld a,80h
        ld (STATUS),a       ; /IACK
        jr poll

        ds 2048-$,0FFh
