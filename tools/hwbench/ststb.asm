; HW bench: STSTB of the JPR-1V and the AND-1V wait at I/O with A8-A15 = F8h, and STSTB at INTA.
; Each part is framed by OUT (F7h) markers (/IOW); a part's window runs from the end of its start marker to the
; start of its end marker, so it holds the body plus the M1 and operand read of the end marker.
; P1 8x NOP                  memory cycles only (ROM, no /MR)
; P2 4x IN A,(F7h)           I/O reads, A8-A15 = 00h
; P3 4x OUT (C),A  B = F8h   I/O writes with A8-A15 = F8h (AND-1V window F800-FFFF)
; P4 4x IN A,(C)   B = F8h   I/O reads with A8-A15 = F8h
; P5 one interrupt (IM 1) from the DSM-1V receiver (the PC sends bytes), PC in the ROM: the INTA cycle
; Everything runs from the ROM (0000-07FF), outside any graphics window. pasmo ststb.asm ststb.bin

BASE    equ 10h
MARK    equ 0F7h

wait    macro n
        local dl
        ld de,n
dl:     dec de
        ld a,d
        or e
        jr nz,dl
        endm

        org 0
        di
        jp start

        ds 38h-$,0FFh
        ; RST 38h: take the byte, receive interrupt off, no EI
        push af
        in a,(BASE+2)
        xor a
        out (BASE),a
        out (MARK),a    ; marker: the handler ran
        pop af
        ret

start:  ld sp,0BF00h
        wait 1540       ; 10 ms
        out (BASE+3),a  ; UART for the receive interrupt
        xor a
        out (BASE),a
        ld a,0Dh
        out (BASE+1),a
        ld a,(8000h)    ; /MR ends the UART reset

        wait 154        ; P1
        out (MARK),a
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        out (MARK),a

        wait 154        ; P2
        out (MARK),a
        in a,(MARK)
        in a,(MARK)
        in a,(MARK)
        in a,(MARK)
        out (MARK),a

        ld bc,0F800h+MARK
        wait 154        ; P3 (wait keeps BC)
        out (MARK),a
        out (c),a
        out (c),a
        out (c),a
        out (c),a
        out (MARK),a

        wait 154        ; P4
        out (MARK),a
        in a,(c)
        in a,(c)
        in a,(c)
        in a,(c)
        out (MARK),a

        wait 154        ; P5
        in a,(BASE+2)   ; drop what came
        im 1
        ld a,80h        ; IER
        out (BASE),a
        ei
        out (MARK),a
p5:     jr p5           ; until the interrupt; the handler returns here, the loop goes on

        ds 2048-$,0FFh
