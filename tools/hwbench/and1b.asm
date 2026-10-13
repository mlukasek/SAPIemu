; HW bench (and1b): AND-1B at F800h (S2 5-6 and 7-8 open), INT1 50 Hz (S2 1-2), in place of the AND-1V. The
; picture is compared between a photo of the monitor and SAPIemu (and1b_emu.sapi). /INT1 is the NMI of JPR-1V: a
; RETN at 0066h. The analyzer measures the INT1 pulses (/INT1 on a probe).
; Screen (character code = ASCII & 3Fh, D7-D6 = attribute):
;   every row: columns 0-1 the row number, 2-31 dots, 32-39 the digits 0-7
;   folding of columns 32-63 (the schematic: 40-63 mirror 32-39; rows 24-31 columns 8q+k = row 8q+j column 32+k):
;     rows 0-7:   written again at columns 40-47 with 'A'+row  -> the visible 32-39 show the letter if 40-47 mirror
;     rows 8-15:  written again at columns 56-63 with '#'      -> 32-39 show '#' if 56-63 mirror 32-39
;     rows 16-23: written through row 24+j, columns 16-23, '@' -> 32-39 of row 16+j show '@' (the invisible alias)
;   attributes in columns 4-31: row 2 "BLINK" (01), row 3 "UNDERLINE" (10), row 4 "DOUBLE" (11), row 5 "NORMAL"
; pasmo and1b.asm and1b.bin

VRAM    equ 0F800h

        org 0
        di
        ld sp,0BF00h
        jp start
        ds 0066h-$,0FFh
        retn                ; /INT1 = NMI on JPR-1V

start:  ld d,0              ; the basic picture
row:    ld e,0
        call addr
        ld a,d              ; the row number in decimal
        ld c,'0'-1
r10:    inc c
        sub 10
        jr nc,r10
        add a,10+'0'
        ld b,a
        ld a,c
        and 3Fh
        ld (hl),a
        inc hl
        ld a,b
        and 3Fh
        ld (hl),a
        inc hl
        ld b,30             ; columns 2-31: dots
dots:   ld (hl),'.' & 3Fh
        inc hl
        djnz dots
        ld b,8              ; columns 32-39: digits 0-7
        ld a,'0' & 3Fh
dig:    ld (hl),a
        inc a
        inc hl
        djnz dig
        inc d
        ld a,d
        cp 24
        jr nz,row

        ld d,0              ; rows 0-7: columns 40-47 = 'A'+row
f1:     ld e,40
        call addr
        ld a,d
        add a,'A' & 3Fh
        ld b,8
f1c:    ld (hl),a
        inc hl
        djnz f1c
        inc d
        ld a,d
        cp 8
        jr nz,f1

        ld d,8              ; rows 8-15: columns 56-63 = '#'
f2:     ld e,56
        call addr
        ld b,8
f2c:    ld (hl),'#' & 3Fh
        inc hl
        djnz f2c
        inc d
        ld a,d
        cp 16
        jr nz,f2

        ld d,24             ; rows 24-31, columns 16-23 = '@' (aliases of rows 16-23, columns 32-39)
f3:     ld e,16
        call addr
        ld b,8
f3c:    ld (hl),'@' & 3Fh
        inc hl
        djnz f3c
        inc d
        ld a,d
        cp 32
        jr nz,f3

        ld de,0204h         ; the attributes
        ld hl,t_blink
        ld c,40h
        call text
        ld de,0304h
        ld hl,t_under
        ld c,80h
        call text
        ld de,0404h
        ld hl,t_double
        ld c,0C0h
        call text
        ld de,0504h
        ld hl,t_norm
        ld c,00h
        call text
        jr $

; HL = VRAM + D * 64 + E
addr:   ld h,0
        ld l,d
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        ld a,l
        add a,e
        ld l,a
        ld a,h
        add a,high VRAM
        ld h,a
        ret

; the text at HL (0-terminated) to row D, column E with the attribute C; dots up to column 31
text:   push hl
        call addr
        ex de,hl
        pop hl
tx:     ld a,(hl)
        or a
        ret z
        and 3Fh
        or c
        ld (de),a
        inc hl
        inc de
        jr tx

t_blink:  db "BLINK BLINK",0
t_under:  db "UNDERLINE UNDERLINE",0
t_double: db "D O U B L E",0         ; double width: the next cell is not shown
t_norm:   db "NORMAL NORMAL",0

        ds 0400h-$,0FFh
        ds 0400h,0
