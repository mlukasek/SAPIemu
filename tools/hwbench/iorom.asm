; HW bench (iorom): does the boot ROM of JPR-1V answer I/O reads? Its /CE comes from the address only (IC12
; 74LS688 on A11-A15), so IN A,(n) with A = 00h-07h (A8-A15) could read the ROM at A*256+n, fighting the card.
; Avoided: A = 04h-07h (the PicoROM comms window 0400h-07FFh).
; For each high byte H of the table and each port (56h = YM3812 status of MPH-1V, 9Ah = nobody), 8x IN A,(C)
; with B = H, then 8x IN A,(n) with A = H. Markers in the ROM: 0056h 3Ch, 009Ah E7h, 0156h 81h, 019Ah 18h;
; 0256h, 029Ah, 0356h, 039Ah are code bytes (see the binary).
; Results at 9000h, 16 bytes per (H, port), then the 8 ROM markers read as memory; sent over the PicoROM channel
; as '*' and hex digits. pasmo iorom.asm iorom.bin

NH      equ 8
NRES    equ NH*2*16+8
W       equ 0400h
ACTIVE  equ W+04h
OUTSEQ  equ W+10h
OUTAREA equ W+300h

        org 0
        jp start
        ds 0056h-$,0
        db 3Ch
        ds 009Ah-$,0
        db 0E7h
        ds 0156h-$,0
        db 81h
        ds 019Ah-$,0
        db 18h
        ds 0200h-$,0

start:  di
        ld sp,0BF00h
        ld ix,9000h
        ld hl,highs
        ld d,NH
nh:     ld e,(hl)           ; E = H
        ld c,56h
        call test
        ld c,9Ah
        call test
        inc hl
        dec d
        jr nz,nh
        ld a,(0056h)
        call put1
        ld a,(009Ah)
        call put1
        ld a,(0156h)
        call put1
        ld a,(019Ah)
        call put1
        ld a,(0256h)
        call put1
        ld a,(029Ah)
        call put1
        ld a,(0356h)
        call put1
        ld a,(039Ah)
        call put1
        jr sendall

; 8x IN A,(C) with B = E, then 8x IN A,(n) with A = E (n = C: 56h or 9Ah)
test:   ld b,e
        in a,(c)
        call put1
        in a,(c)
        call put1
        in a,(c)
        call put1
        in a,(c)
        call put1
        in a,(c)
        call put1
        in a,(c)
        call put1
        in a,(c)
        call put1
        in a,(c)
        call put1
        ld b,8
        ld a,c
        cp 56h
        jr nz,t9a
t56:    ld a,e
        in a,(56h)
        call put1
        djnz t56
        ret
t9a:    ld a,e
        in a,(9Ah)
        call put1
        djnz t9a
        ret

put1:   ld (ix+0),a
        inc ix
        ret

highs:  db 00h,01h,02h,03h,08h,10h,80h,0FFh

sendall:
wact:   ld a,(ACTIVE)
        cp 1
        jr nz,wact
        ld a,'*'
        call send
        ld hl,9000h
        ld de,NRES
nx:     ld a,(hl)
        rrca
        rrca
        rrca
        rrca
        call hexc
        ld a,(hl)
        call hexc
        inc hl
        dec de
        ld a,d
        or e
        jr nz,nx
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
