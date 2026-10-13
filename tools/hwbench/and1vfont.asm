; HW bench (and1vfont): which font is in the AND-1V rev 2.3 of the author (font_and1v22_1024 = type 1 or
; newfont_and1v22_1024 = type 2 from ..\SAPI-karty\AND-1V\rev2.3\font_and1v22)? All 256 codes on the screen,
; compared between a photo of the monitor and SAPIemu with each font (and1vfont_run.py).
; Row 0: the low hex digit over each column; rows 2-17: the high hex digit, then codes x0-xF two columns apart.
; Columns 40-63 of rows 2-17: a text sample. AND-1V at F800h (MAP1 = L after the reset). pasmo and1vfont.asm and1vfont.bin

VRAM    equ 0F800h

        org 0
        di
        ld sp,0BF00h
        ld hl,VRAM          ; clear the 20 visible rows
        ld bc,20*64
cl:     ld (hl),' '
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,cl
        ld hl,VRAM+4        ; row 0: 0 1 2 ... F
        ld b,16
        ld c,0
hd:     ld a,c
        call hexd
        ld (hl),a
        inc hl
        inc hl
        inc c
        djnz hd
        ld de,VRAM+2*64     ; rows 2-17
        ld c,0              ; the code
rw:     push de
        ld a,c
        rrca
        rrca
        rrca
        rrca
        call hexd
        ld (de),a
        inc de
        ld a,':'
        ld (de),a
        inc de
        inc de
        inc de
        ld b,16
cd:     ld a,c
        ld (de),a
        inc de
        inc de
        inc c
        djnz cd
        pop de
        ld hl,64
        add hl,de
        ex de,hl
        ld a,c
        or a
        jr nz,rw
        ld hl,txt           ; the text sample at row 3, column 40
        ld de,VRAM+3*64+40
tx:     ld a,(hl)
        or a
        jr z,done
        cp 0Dh
        jr nz,tx1
        ex de,hl            ; next row, column 40
        ld a,l
        and 0C0h
        add a,64+40
        ld l,a
        jr nc,tx2
        inc h
tx2:    ex de,hl
        inc hl
        jr tx
tx1:    ld (de),a
        inc hl
        inc de
        jr tx
done:   jr $

hexd:   and 0Fh             ; A = the hex digit of the low nibble
        add a,'0'
        cp '9'+1
        ret c
        add a,'A'-'9'-1
        ret

txt:    db "AND-1V rev 2.3",0Dh,0Dh
        db "The quick brown fox",0Dh
        db "jumps over the lazy",0Dh
        db "dog. 0123456789",0Dh,0Dh
        db "THE QUICK BROWN FOX",0Dh
        db "{|}~ [\\]^_ `@#$%&*",0
        ds 0400h-$,0FFh
        ds 0400h,0
