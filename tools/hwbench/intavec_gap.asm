; HW bench (intavec_gap: how long the data bus holds its last byte before INTA; interrupt from the DSM-1V
; receiver, the PC sends bytes; loops: LD A,I (1 internal T after its last fetch), INC BC (2), JR (5), ADD HL,BC (7)): which byte does the Z80 read in the INTA cycle when no card drives a vector (JPR-1V)?
; IM 2 with I = 31h; the table at 3100h-3200h holds t[j] = j (t[256] = 08h), so the vector d leads to the
; handler at (d+1)*256 + d (FFh: 08FFh). A stub "LD A,d / JP report" sits at each of these addresses (ROM for
; d = 00h-06h, RAM-1V and the AND-1V video RAM for the rest; d = 2Fh-34h collide with the table: unknown).
; The interrupt: timer 1 of the YM3812 (MPH-1V, 50h) through IEN D0 to /INT0, 80 us after the start.
; Four loops wait for it: JR $ (FEh?), NOPs (00h?), LD A,(9876h) with 5Ah there (5Ah?), LD (9876h),A with
; A = A5h (A5h?). Each result goes as "T<n>=<hex>" CR LF on DSM-1V channel A (10h, 9600 Bd, COM13).
; Not in the comms window: the stubs use 0400h-07FFh. pasmo intavec.asm intavec.bin

IVEC    equ 31h
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
        cp 2Fh
        jr c,st2
        cp 35h
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
        in a,(UART+2)       ; DSM-1V: drop the byte waiting, receive interrupt on (IER)
        ld a,80h
        out (UART),a
        ld a,(TESTNO)
        ei
        or a
        jr z,t0
        dec a
        jr z,t1
        dec a
        jr z,t2
        jr t3
t0:     rept 20             ; LD A,I: ED 57, 1 internal T after the fetch of 57h
        ld a,i
        endm
        jr t0
t1:     rept 30             ; INC BC: 03h, 2 internal T
        inc bc
        endm
        jr t1
t2:     rept 30             ; ADD HL,BC: 09h, 7 internal T
        add hl,bc
        endm
        jr t2
t3:     jr t3               ; JR $: FEh, 5 internal T
; the vector came: A = its low byte
report: di
        ld c,a
        xor a
        out (UART),a        ; receive interrupt off
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
