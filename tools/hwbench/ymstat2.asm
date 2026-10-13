; HW bench (ymstat2): how often does the YM3812 status of MPH-1V read with bit 2 set (C4h seen 2 of 11 times on
; 2026-10-11)? Histograms of 16384 status reads each:
;   A  single IN A,(56h), timer idle (status 00h expected), a random gap of 0-7 DJNZ between the reads
;   B  the same after timer 1 ran out (C0h expected)
;   C  256 INI (four back-to-back), 64 times, after timer 1 ran out
; The PicoROM watches A0-A10 only: every I/O keeps A10 (bit 2 of the high byte) at 0, else it is taken as an access
; to the comms window 0400h-07FFh (07xxh sends a byte) and a burst of them jams the PicoROM firmware.
; Each histogram has 16-bit counters: low bytes at base+v, high bytes at base+100h+v (A 9000h, B 9200h, C 9400h).
; Sent over the PicoROM comms channel: '*', then for each histogram the non-zero entries as 6 hex digits
; (value, count high, count low) and '.' after each histogram. pasmo ymstat2.asm ymstat2.bin

YMA     equ 56h             ; address (write) / status (read)
YMD     equ 57h
W       equ 0400h
ACTIVE  equ W+04h
OUTSEQ  equ W+10h
OUTAREA equ W+300h
BUF     equ 9800h

        org 0
        di
        ld sp,0BF00h
        ld b,40             ; 130 us: the YM is out of reset
dl:     djnz dl
        ld hl,9000h         ; clear the histograms
        ld de,9001h
        ld bc,0600h-1
        ld (hl),0
        ldir
        ld a,04h            ; reset the timer flags (IRQ reset), timers stopped
        ld bc,0800h+YMA     ; A8-A15 = 08h: A10 = 0 (see the header)
        out (c),a
        ld b,2
r1:     djnz r1
        ld a,80h
        ld bc,0800h+YMD
        out (c),a
        ld b,10
r2:     djnz r2
        ld c,90h            ; histogram A
        call single
        ld a,02h            ; timer 1 = FFh: overflow after 80 us
        ld bc,0800h+YMA     ; A8-A15 = 08h: A10 = 0 (see the header)
        out (c),a
        ld b,2
d1:     djnz d1
        ld a,0FFh
        ld bc,0800h+YMD
        out (c),a
        ld b,10             ; the YM wants about 23 us after a data write
d2:     djnz d2
        ld a,04h            ; start timer 1
        ld bc,0800h+YMA     ; A8-A15 = 08h: A10 = 0 (see the header)
        out (c),a
        ld b,2
d3:     djnz d3
        ld a,01h
        ld bc,0800h+YMD
        out (c),a
        ld de,154           ; 1 ms
d4:     dec de
        ld a,d
        or e
        jr nz,d4
        ld c,92h            ; histogram B
        call single
        ld a,64             ; histogram C: blocks of 256 INI, four back-to-back (B = F3h..F0h)
blk:    push af
        ld hl,BUF
        ld c,YMA
        ld e,64
blk4:   ld b,0F3h           ; B = F3h..F0h: A10 = 0
        ini
        ini
        ini
        ini
        dec e
        jr nz,blk4
        ld de,BUF
        ld b,0
hb:     ld a,(de)
        ld l,a
        ld h,94h
        inc (hl)
        jr nz,hb1
        inc h
        inc (hl)
hb1:    inc de
        djnz hb
        pop af
        dec a
        jr nz,blk
        jp sendall

; 16384 single status reads into the histogram with the high byte C
single:
        ld de,16384
sg:     ld a,r              ; a gap of 0-7 DJNZ turns
        and 7
        inc a
        ld b,a
sgd:    djnz sgd
        ld a,0F0h           ; A8-A15 = F0h: A10 = 0
        in a,(YMA)
        ld l,a
        ld h,c
        inc (hl)
        jr nz,sg1
        inc h
        inc (hl)
sg1:    dec de
        ld a,d
        or e
        jr nz,sg
        ret

sendall:
wact:   ld a,(ACTIVE)
        cp 1
        jr nz,wact
        ld e,'*'
        call put
        ld h,90h
        call sendh
        ld h,92h
        call sendh
        ld h,94h
        call sendh
        jr $

; the non-zero entries of the histogram at H*100h, then '.'
sendh:  ld l,0
sh:     ld a,(hl)
        inc h
        or (hl)
        jr z,sh1
        ld a,l
        call hex2
        ld a,(hl)
        call hex2
        dec h
        ld a,(hl)
        call hex2
        inc h
sh1:    dec h
        inc l
        jr nz,sh
        ld e,'.'
        jp put

hex2:   push af
        rrca
        rrca
        rrca
        rrca
        call hexc
        pop af
; send the hex digit of the low nibble of A, or E (put)
hexc:   and 0Fh
        add a,'0'
        cp '9'+1
        jr c,hex1
        add a,'A'-'9'-1
hex1:   ld e,a
put:    push hl
        ld d,high OUTAREA   ; send E
        ld a,(OUTSEQ)
        ld c,a
        ld a,(de)
pw:     ld a,(OUTSEQ)
        cp c
        jr z,pw
        pop hl
        ret

        ds 0400h-$,0FFh
        ds 0400h,0
