; HW bench: ZRD-1V rev 1.1, reads of 61h, 62h, 63h. The schematic: decoded, nothing drives the internal bus,
; and the read loads the address register like a write. What does the read return, and where does the address
; go? Only reads of the RAM disk (port 60h), nothing is written to it.
; Results (RAM 9000h, then over the PicoROM comms channel as '*' and hex digits):
;   9000  4 bytes at 03:41:10 (the reference)
;   9004  IN 61h -> v1, then 4 bytes from 60h            9009  IN 62h -> v2, 4 bytes     900E  IN 63h -> v3, 4 bytes
;   9013  candidates, 4 bytes each: 03:41:FF  03:41:61  03:41:00  03:FF:10  03:62:10  03:00:10  3F:41:10  23:41:10
;         00:41:10
; Each read test sets 03:41:10 first. pasmo zrdrd.asm zrdrd.bin

OUTBUF  equ 9000h
NRES    equ 4+3*5+9*4
W       equ 0400h
ACTIVE  equ W+04h
OUTSEQ  equ W+10h
OUTAREA equ W+300h

        org 0
        di
        ld sp,0BF00h
        ld ix,OUTBUF
        ld hl,addr0         ; the reference
        call setadr
        call get4
        ld hl,addr0         ; IN 61h
        call setadr
        in a,(61h)
        call put1
        call get4
        ld hl,addr0         ; IN 62h
        call setadr
        in a,(62h)
        call put1
        call get4
        ld hl,addr0         ; IN 63h
        call setadr
        in a,(63h)
        call put1
        call get4
        ld hl,cands         ; the candidates
        ld b,9
cn:     push bc
        call setadr
        call get4
        pop bc
        inc hl
        inc hl
        inc hl
        djnz cn
        jp sendall

; HL -> A16-21, A8-15, A0-7: set the address (63h, 62h, then 61h loads the counters)
setadr: ld a,(hl)
        out (63h),a
        inc hl
        ld a,(hl)
        out (62h),a
        inc hl
        ld a,(hl)
        out (61h),a
        dec hl
        dec hl
        ret

get4:   ld b,4
g4:     in a,(60h)
        call put1
        djnz g4
        ret

put1:   ld (ix+0),a
        inc ix
        ret

addr0:  db 03h,41h,10h
cands:  db 03h,41h,0FFh, 03h,41h,61h, 03h,41h,00h
        db 03h,0FFh,10h, 03h,62h,10h, 03h,00h,10h
        db 3Fh,41h,10h,  23h,41h,10h, 00h,41h,10h

sendall:
wact:   ld a,(ACTIVE)
        cp 1
        jr nz,wact
        ld a,'*'
        call send
        ld hl,OUTBUF
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
