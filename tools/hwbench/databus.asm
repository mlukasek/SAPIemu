; HW bench (databus): wiring check of the data bus probes. Writes known patterns to RAM (9000h) and reads each
; back: 01h 02h 04h 08h 10h 20h 40h 80h, FEh FDh FBh F7h EFh DFh BFh 7Fh, 55h AAh 00h FFh. databus_check.py
; samples D0-D7 at the rising /MW and /MR edges. pasmo databus.asm databus.bin

        org 0
        di
        ld hl,pat
        ld b,npat
nx:     ld a,(hl)           ; from the ROM: no /MR on the bus
        ld (9000h),a        ; /MW with the pattern
        ld a,(9000h)        ; /MR with the pattern back from the RAM
        inc hl
        djnz nx
        jr $

pat:    db 01h,02h,04h,08h,10h,20h,40h,80h
        db 0FEh,0FDh,0FBh,0F7h,0EFh,0DFh,0BFh,7Fh
        db 55h,0AAh,00h,0FFh
npat    equ $-pat

        ds 0400h-$,0FFh
        ds 0400h,0
