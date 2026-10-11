; HW bench: the CGA-1V loops of ZRDBENCH (media/zrd-1v/ZRDBENCH.ASM) exactly, timed by the analyzer instead of
; the 82C54: CGA config 06h (page B for the CPU, A shown), then 256 x LDIR 256 bytes.
; W1 buf -> CGA C000h (t_cgaw)     W2 CGA C000h -> buf (t_cgar)     W3, W4 the same with CGA config 00h
; The turn counter is B' (pasmo has no IXL): 21.19 T per byte without waits, ZRDBENCH 21.18.
; pasmo zrdlike.asm zrdlike.bin

MARK    equ 0F7h
CGA     equ 0C000h
CGA_CFG equ CGA+3FFBh
BUF     equ 8000h

wait    macro n
        local dl
        ld de,n
dl:     dec de
        ld a,d
        or e
        jr nz,dl
        endm

loopw   macro cv
        local lp
        ld a,cv
        ld (CGA_CFG),a
        exx
        ld b,0          ; 256 turns in B'
        exx
lp:     ld hl,BUF
        ld de,CGA
        ld bc,256
        ldir
        exx
        dec b
        exx
        jr nz,lp
        endm

loopr   macro cv
        local lp
        ld a,cv
        ld (CGA_CFG),a
        exx
        ld b,0
        exx
lp:     ld hl,CGA
        ld de,BUF
        ld bc,256
        ldir
        exx
        dec b
        exx
        jr nz,lp
        endm

        org 0
        di
        ld sp,0BF00h
        wait 1540
        ld a,0C0h       ; MAP1 = MAP2 = H: the CGA-1V at C000h
        out (63h),a
        wait 154
        out (MARK),a    ; W1
        loopw 06h
        out (MARK),a
        wait 154
        out (MARK),a    ; W2
        loopr 06h
        out (MARK),a
        wait 154
        out (MARK),a    ; W3
        loopw 00h
        out (MARK),a
        wait 154
        out (MARK),a    ; W4
        loopr 00h
        out (MARK),a
        xor a
        out (63h),a
        jr $

        ds 2048-$,0FFh
