; HW bench: wait states of the CGA-1V video RAM, 12 KB blocks as ZRDBENCH (C000h with MAP1 = MAP2 = H).
; Windows between OUT (F7h) markers:
; W1 LDIR 3000h bytes RAM 8000h -> CGA-1V C000h    W2 LDIR 3000h bytes CGA-1V C000h -> RAM 8000h
; W3 16 single writes LD (C000h),A, 13 us apart     W4 16 single reads LD A,(C000h), 13 us apart
; tools/hwbench/and1v_run.py counts the CPU clocks per window and the RDY low time of each access.
; The CGA waits depend on the beam: compare the distribution. pasmo cga1v_long.asm cga1v_long.bin

MARK    equ 0F7h
VRAM    equ 0C000h

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
        ld a,0C0h       ; MAP1 = MAP2 = H: the CGA-1V at C000h (RAM-1V has its 16 KB hole there)
        out (63h),a
        wait 154
        out (MARK),a    ; W1
        ld hl,8000h
        ld de,VRAM
        ld bc,3000h
        ldir
        out (MARK),a
        wait 154
        out (MARK),a    ; W2
        ld hl,VRAM
        ld de,8000h
        ld bc,3000h
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
        xor a
        out (63h),a
        jr $

        ds 2048-$,0FFh
