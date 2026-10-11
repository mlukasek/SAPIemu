; HW bench: does /RES clear the address counters (61h) and the latches 62h, 63h of the ZRMD-1? And what do
; reads of 61h-64h, 6Fh give? Nothing is written to the RAM disk (port 60h is only read: the author's disk).
; Phase A (first reset): 4 bytes from each of the 8 addresses with A16-21 / A8-15 / A0-7 either 0 or the test
; value (03h, 41h, 10h), then the address 03:41:10 is set and not read, a flag goes to RAM, stop.
; The PC resets again. Phase B: 4 bytes from 60h at once (no address set), then IN 61h, 62h, 63h, 64h, 6Fh,
; the flag cleared; all of it over the PicoROM comms channel as '*' and hex digits.
; pasmo zrmd.asm zrmd.bin

FLAG    equ 9000h           ; 'ZRM1' after phase A (RAM-1V keeps it over /RES)
RESA    equ 9010h           ; phase A: 8 x 4 bytes
RESB    equ 9030h           ; phase B: 4 bytes, then the 5 port reads
NRES    equ 32+4+5
W       equ 0400h
ACTIVE  equ W+04h
OUTSEQ  equ W+10h
OUTAREA equ W+300h

        org 0
        di
        ld sp,0BF00h
        ld hl,(FLAG)
        ld de,'Z'+256*'R'
        or a
        sbc hl,de
        jr nz,phasea
        ld hl,(FLAG+2)
        ld de,'M'+256*'1'
        or a
        sbc hl,de
        jr nz,phasea

phaseb: ld hl,RESB          ; the counter as the reset left it
        ld bc,0460h
        inir
        in a,(61h)
        ld (hl),a
        inc hl
        in a,(62h)
        ld (hl),a
        inc hl
        in a,(63h)
        ld (hl),a
        inc hl
        in a,(64h)
        ld (hl),a
        inc hl
        in a,(6Fh)
        ld (hl),a
        ld hl,0
        ld (FLAG),hl
        ld (FLAG+2),hl
        jr sendall

phasea: ld hl,RESA
        ld iy,combos
        ld e,8
pa1:    ld a,(iy+2)         ; A0-7 (loads the counters)
        out (61h),a
        ld a,(iy+1)         ; A8-15
        out (62h),a
        ld a,(iy+0)         ; A16-21 (b6, b7 = 0: MAP1, MAP2 low)
        out (63h),a
        ld bc,0460h
        inir
        inc iy
        inc iy
        inc iy
        dec e
        jr nz,pa1
        ld a,10h            ; the test address, not read
        out (61h),a
        ld a,41h
        out (62h),a
        ld a,03h
        out (63h),a
        ld hl,'Z'+256*'R'
        ld (FLAG),hl
        ld hl,'M'+256*'1'
        ld (FLAG+2),hl
        jr $

combos: ; A16-21, A8-15, A0-7
        db 00h,00h,00h,  00h,00h,10h,  00h,41h,00h,  00h,41h,10h
        db 03h,00h,00h,  03h,00h,10h,  03h,41h,00h,  03h,41h,10h

sendall:
wact:   ld a,(ACTIVE)
        cp 1
        jr nz,wact
        ld a,'*'
        call put
        ld hl,RESA
        ld b,NRES
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
