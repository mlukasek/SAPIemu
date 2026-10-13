; HW bench (and1z): AND-1Z/89 at F800h in place of the AND-1V: how many rows does the monitor show (20 by the PROM
; and the manual, 24 by the CP/M documentation)? All 32 rows of the 2 KB are written: columns 0-1 the row number,
; column 2 ':', then the column ruler: row 0 the tens digit of each column, the other rows the units; '|' in column 63. The photo
; tells the first and the last visible row and column.
; The picture is compared between a photo of the monitor and SAPIemu (and1z_emu.sapi). pasmo and1z.asm and1z.bin

VRAM    equ 0F800h

        org 0
        di
        ld sp,0BF00h
        ld hl,VRAM
        ld d,0              ; row
row:    ld a,d              ; columns 0-1: the row number in decimal
        ld c,'0'-1
r10:    inc c
        sub 10
        jr nc,r10
        add a,10+'0'
        ld (hl),c
        inc hl
        ld (hl),a
        inc hl
        ld (hl),':'         ; column 2
        inc hl
        ld e,3              ; column
col:    ld a,e
        ld b,a
        ld c,0              ; row 0: the tens digit, else the units
t10:    cp 10
        jr c,t1
        sub 10
        inc c
        jr t10
t1:     ld b,a              ; B = units, C = tens
        ld a,d
        or a
        ld a,c
        jr z,put
        ld a,b
put:    add a,'0'
        ld (hl),a
        inc hl
        inc e
        ld a,e
        cp 63
        jr nz,col
        ld (hl),'|'
        inc hl
        inc d
        ld a,d
        cp 32
        jr nz,row
        jr $

        ds 0400h-$,0FFh
        ds 0400h,0
