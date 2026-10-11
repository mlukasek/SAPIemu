; HW bench: which byte does the Z80 read in the INTA cycle when no card drives a vector (JPR-1V)?
; IM 2 with I = 31h; the table at 3100h-3200h holds t[j] = j (t[256] = 08h), so the vector d leads to the
; handler at (d+1)*256 + d (FFh: 08FFh). A stub "LD A,d / JP report" sits at each of these addresses (ROM for
; d = 00h-06h, RAM-1V and the AND-1V video RAM for the rest; d = 2Fh-34h collide with the table: unknown).
; The interrupt: timer 1 of the YM3812 (MPH-1V, 50h) through IEN D0 to /INT0, 80 us after the start.
; Four loops wait for it: JR $ (FEh?), NOPs (00h?), LD A,(9876h) with 5Ah there (5Ah?), LD (9876h),A with
; A = A5h (A5h?). Each result goes as "T<n>=<hex>" CR LF on DSM-1V channel A (10h, 9600 Bd, COM13).
; Not in the comms window: the stubs use 0400h-07FFh. pasmo intavec.asm intavec.bin

IVEC    equ 61h
TABLE   equ IVEC*256
TESTNO  equ 9000h
MEMV    equ 9876h
YMA     equ 56h
YMD     equ 57h
IEN     equ 54h
UART    equ 10h

        org 0
        jp start
        ds 0008h-$,0FFh
start:  di
        ld sp,0BF00h
        out (UART+3),a      ; UART: reset, 8N1, a RAM read ends the reset
        xor a
        out (UART),a
        ld a,0Dh
        out (UART+1),a
        ld a,(8000h)
        ld hl,TABLE         ; the vector table: t[j] = j
        xor a
tb:     ld (hl),a
        inc hl
        inc a
        jr nz,tb
        ld (hl),08h         ; t[256]: the vector FFh leads to 08FFh
        ld d,07h            ; the stubs in RAM for d = 07h..FEh (not 2Fh-34h), and FFh at 08FFh
st1:    ld a,d
        cp 5Fh
        jr c,st2
        cp 65h
        jr c,st3
st2:    ld l,d
        ld h,d
        inc h
        call stub
st3:    inc d
        ld a,d
        cp 0FFh
        jr nz,st1
        ld hl,08FFh
        call stub
        ld a,5Ah
        ld (MEMV),a
        xor a
        ld (TESTNO),a
        jp next
        ds 0100h-$,0FFh
        db 3Eh,00h,0C3h
        dw report

        ; next test (after the stub at 0100h)
next:   ld sp,0BF00h
        ld a,IVEC
        ld i,a
        im 2
        ld a,04h            ; YM: reset the IRQ flag, timer 1 = FFh, start it
        out (YMA),a
        ld a,80h
        call ymd
        ld a,02h
        out (YMA),a
        ld a,0FFh
        call ymd
        ld a,04h
        out (YMA),a
        ld a,01h
        call ymd
        ld a,01h            ; IEN: the YM IRQ to /INT0
        out (IEN),a
        ld a,(TESTNO)
        ei
        or a
        jr z,t0
        dec a
        jr z,t1
        dec a
        jr z,t2
        jr t3
t0:     jr t0               ; JR $
t1:     ds 60,0             ; NOPs
        jr t1
t2:     rept 20             ; LD A,(9876h)
        ld a,(MEMV)
        endm
        jr t2
t3:     ld a,0A5h
t3l:    rept 20             ; LD (9876h),A
        ld (MEMV),a
        endm
        jr t3l

        ds 0201h-$,0FFh
        db 3Eh,01h,0C3h
        dw report

; the vector came: A = its low byte
report: di
        ld c,a
        xor a
        out (IEN),a
        ld a,04h
        out (YMA),a
        ld a,80h            ; IRQ flag off, timers stopped
        call ymd
        ld a,'T'
        call tx
        ld a,(TESTNO)
        add a,'0'
        call tx
        ld a,'='
        call tx
        ld a,c
        rrca
        rrca
        rrca
        rrca
        call hex
        ld a,c
        call hex
        ld a,13
        call tx
        ld a,10
        call tx
        ld a,(TESTNO)
        inc a
        ld (TESTNO),a
        cp 4
        jp c,next
        jr $

        ds 0302h-$,0FFh
        db 3Eh,02h,0C3h
        dw report

stub:   ld (hl),3Eh         ; at HL: LD A,d / JP report
        inc hl
        ld (hl),d
        inc hl
        ld (hl),0C3h
        inc hl
        ld (hl),low report
        inc hl
        ld (hl),high report
        ret

ymd:    out (YMD),a         ; a YM data write, then the 23 us it needs
        push bc
        ld b,8
ym1:    djnz ym1
        pop bc
        ret

hex:    and 0Fh
        add a,'0'
        cp '9'+1
        jr c,tx
        add a,'A'-'9'-1
tx:     push af
tx1:    in a,(UART+1)
        and 40h
        jr z,tx1
        pop af
        out (UART+2),a
        ret

        ds 0403h-$,0FFh
        db 3Eh,03h,0C3h
        dw report
        ds 0504h-$,0FFh
        db 3Eh,04h,0C3h
        dw report
        ds 0605h-$,0FFh
        db 3Eh,05h,0C3h
        dw report
        ds 0706h-$,0FFh
        db 3Eh,06h,0C3h
        dw report
        ds 0800h-$,0FFh
