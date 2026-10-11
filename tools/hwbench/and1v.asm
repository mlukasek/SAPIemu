; HW bench: wait states of the AND-1V video RAM (F800h, MAP1 = L after the reset) against the emulator.
; Windows between OUT (F7h) markers:
; W1 LDIR 256 bytes RAM 8000h -> AND-1V F800h      W2 LDIR 256 bytes AND-1V F800h -> RAM 8100h
; W3 16 single writes LD (F800h),A, 13 us apart     W4 16 single reads LD A,(F800h), 13 us apart
; tools/hwbench/and1v_run.py counts the CPU clocks per window and the RDY low time of each access.
; pasmo and1v.asm and1v.bin

MARK    equ 0F7h
VRAM    equ 0F800h

wait    macro n
        local dl
        ld de,n
dl:     dec de
        ld a,d
        or e
        jr nz,dl
        endm

        org 0
        di
        ld sp,0BF00h
        wait 1540       ; 10 ms
        ld hl,8000h     ; the source: 00h..FFh
        ld b,0
fill:   ld (hl),l
        inc hl
        djnz fill
        wait 154
        out (MARK),a    ; W1
        ld hl,8000h
        ld de,VRAM
        ld bc,256
        ldir
        out (MARK),a
        wait 154
        out (MARK),a    ; W2
        ld hl,VRAM
        ld de,8100h
        ld bc,256
        ldir
        out (MARK),a
        wait 154
        out (MARK),a    ; W3
        ld b,16
w3:     ld (VRAM),a
        wait 2
        djnz w3
        out (MARK),a
        wait 154
        out (MARK),a    ; W4
        ld b,16
w4:     ld a,(VRAM)
        wait 2
        djnz w4
        out (MARK),a
        jr $

        ds 2048-$,0FFh
