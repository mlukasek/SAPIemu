; HW bench (intblock): which instructions keep the Z80 of JPR-1V from taking an interrupt after them? Each test
; runs blocks of 40 equal instructions (then JP disp); the PC sends a byte every 3 ms, the DSM-1V receiver pulls
; /INT0, IM 1. intblock_run.py measures from the analyzer how long /INT0 waited for its INTA: about one
; instruction if the interrupt is taken after any of them, up to the whole block if not.
; Tests (8 interrupts each): 0 NOP, 1 LD A,I, 2 LD A,R, 3 LD I,A, 4 NEG, 5 IM 1, 6 LD A,(IX+0), 7 INC BC.
; No PicoROM comms session (the OUTs to the UART have A10 = 1). pasmo intblock.asm intblock.bin

UART    equ 10h
TEST    equ 9000h           ; the test number
COUNT   equ 9001h           ; interrupts left in this test
NINT    equ 8

        org 0
        jp start
        ds 0038h-$,0FFh
        push af             ; IM 1: drop the received byte, count
        in a,(UART+2)
        ld a,(COUNT)
        dec a
        jr nz,i1
        ld a,(TEST)
        inc a
        and 7
        ld (TEST),a
        ld a,NINT
i1:     ld (COUNT),a
        pop af
        ei
        reti

start:  di
        ld sp,0BF00h
        out (UART+3),a      ; UART: reset, 8N1, a RAM read ends the reset
        xor a
        out (UART),a
        ld a,0Dh
        out (UART+1),a
        ld a,(8000h)
        xor a
        ld (TEST),a
        ld a,NINT
        ld (COUNT),a
        ld ix,9100h
        ld a,0C0h           ; I for LD A,I (LD I,A writes it back)
        ld i,a
        in a,(UART+2)       ; drop a byte waiting, receive interrupt on
        ld a,80h
        out (UART),a
        im 1
        ei
disp:   ld a,(TEST)
        add a,a
        ld l,a
        ld h,0
        ld de,tab
        add hl,de
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a
        jp (hl)

tab:    dw t0,t1,t2,t3,t4,t5,t6,t7

t0:     rept 40
        nop
        endm
        jp disp
t1:     rept 40
        ld a,i
        endm
        jp disp
t2:     rept 40
        ld a,r
        endm
        jp disp
t3:     rept 40
        ld i,a
        endm
        jp disp
t4:     rept 40
        neg
        endm
        jp disp
t5:     rept 40
        im 1
        endm
        jp disp
t6:     rept 40
        ld a,(ix+0)
        endm
        jp disp
t7:     rept 40
        inc bc
        endm
        jp disp

        ds 0800h-$,0FFh
