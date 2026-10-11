; HW bench: the measurement of ZRDBENCH (media/zrd-1v/ZRDBENCH.ASM) in a ROM: the 82C54 of MPH-1V (counter 0,
; CLK0 55930.4 Hz, mode 2) times the calibration loop (26 T x 65536) and the tests, with the same loops
; (IXL counters, LDIR 256 B x 256). The tests run from RAM (8100h, copied there; the code is position
; independent) as ZRDBENCH runs from the CP/M TPA. Each test is also framed by OUT (F7h) markers for the
; analyzer, so the same run gives both measurements. '*' and the counts as hex digits (4 words, low byte
; first: calibration, memory -> memory, memory -> CGA-1V, CGA-1V -> memory) go over the PicoROM comms
; channel. T per unit = counts * 26 / cal.
; pasmo zrdtimer.asm zrdtimer.bin

MARK    equ 0F7h
PIT0    equ 050h
PITCW   equ 053h
MIEN    equ 054h
CGA     equ 0C000h
CGA_CFG equ CGA+3FFBh
RAMCODE equ 8100h
BUF     equ 8800h
RESULT  equ 8000h           ; the results: 4 words
W       equ 0400h           ; the comms window
ACTIVE  equ W+04h
OUTSEQ  equ W+10h
OUTAREA equ W+300h

latch   macro               ; HL = counter 0
        xor a
        out (PITCW),a
        in a,(PIT0)
        ld l,a
        in a,(PIT0)
        ld h,a
        endm

ldixl0  macro               ; ld ixl,0 (DD 2E 00), as ZRDBENCH
        db 0DDh,2Eh,0
        endm
decixl  macro               ; dec ixl (DD 2D)
        db 0DDh,2Dh
        endm

        org 0
        di
        ld sp,0BF00h
        ld de,1540          ; 10 ms
w0:     dec de
        ld a,d
        or e
        jr nz,w0
        ld a,28h            ; MPH-1V: gates G2, G0 on, no interrupt
        out (MIEN),a
        ld a,34h            ; counter 0: LSB + MSB, mode 2, binary
        out (PITCW),a
        xor a
        out (PIT0),a
        out (PIT0),a
        ld hl,ramcode       ; the tests to RAM, run them there
        ld de,RAMCODE
        ld bc,ramend-ramcode
        ldir
        call RAMCODE
wact:   ld a,(ACTIVE)       ; then the results over the comms channel
        cp 1
        jr nz,wact
        ; '*' and the 8 bytes as hex digits '0'-'F' (the PC drops anything else: the channel sometimes
        ; carries stray bytes, docs/zmeny-podle-hw.md)
        ld a,'*'
        call put
        ld hl,RESULT
        ld b,8
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

hexc:   and 0Fh             ; A = a nibble -> its hex digit on the channel
        add a,'0'
        cp '9'+1
        jr c,put
        add a,'A'-'9'-1
put:    push de             ; send A, wait until the PicoROM took it
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

; --- position independent: runs at RAMCODE ---------------------------------------------------------------
ramcode:
        ; calibration: 65536 x 26 T (- 5)
        latch
        push hl
        out (MARK),a
        ld bc,0
cal:    dec bc
        ld a,b
        or c
        jr nz,cal
        out (MARK),a
        latch
        ex de,hl
        pop hl
        or a
        sbc hl,de
        ld (RESULT),hl
        ; memory -> memory, 256 x LDIR 256 B
        latch
        push hl
        out (MARK),a
        ldixl0
t1:     ld hl,BUF
        ld de,BUF+100h
        ld bc,256
        ldir
        decixl
        jr nz,t1
        out (MARK),a
        latch
        ex de,hl
        pop hl
        or a
        sbc hl,de
        ld (RESULT+2),hl
        ; memory -> CGA-1V (MAP1 = MAP2 = H, config 06h as ZRDBENCH)
        ld a,0C0h
        out (63h),a
        ld a,06h
        ld (CGA_CFG),a
        latch
        push hl
        out (MARK),a
        ldixl0
t2:     ld hl,BUF
        ld de,CGA
        ld bc,256
        ldir
        decixl
        jr nz,t2
        out (MARK),a
        latch
        ex de,hl
        pop hl
        or a
        sbc hl,de
        ld (RESULT+4),hl
        ; CGA-1V -> memory
        latch
        push hl
        out (MARK),a
        ldixl0
t3:     ld hl,CGA
        ld de,BUF
        ld bc,256
        ldir
        decixl
        jr nz,t3
        out (MARK),a
        latch
        ex de,hl
        pop hl
        or a
        sbc hl,de
        ld (RESULT+6),hl
        xor a
        out (63h),a
        ret
ramend:

        ds 0400h-$,0FFh     ; the comms window follows
        ds 0400h,0
