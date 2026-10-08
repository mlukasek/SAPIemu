; Counter 00-99 on the diagnostic board (K3, rev 2.0) of the JPR-1X, by interrupts.
; Button "+" (P2-IN0 = request R0 of the 3214), "-" (P2-IN7 = request R7). With the
; jumper RSTN/7 open (factory) every request is RST 7: MIKROBIOS jumps from 0038h to
; 2000h (INTadr), where this program puts its handler. One press = one step.
; Left display = tens (P2-OUT), right display = units (P1-OUT).
; Start from the Falcon monitor: JUMP 4400. ESC (the LF key of the ANK-1) ends it.
; python tools/asm8080.py media/jpr-1x/diag_int.asm media/jpr-1x/diag_int.hex

P0      EQU 2400H       ; keyboard: P0-OUT rows, P0-IN columns
P1      EQU 2800H       ; P1-OUT: the right display
P2      EQU 2C00H       ; P2-OUT: the left display; P2-IN: the buttons (0 = pressed)
I3214   EQU 3000H       ; 3214: the current status (a write enables the next interrupt)
INTADR  EQU 2000H       ; MIKROBIOS: RST 7 jumps here (3 bytes)
MASKLED EQU 200EH       ; MIKROBIOS: LEDs and beeper for P0-OUT

        ORG 4400H
START:  DI
        LHLD INTADR     ; keep the vector of MIKROBIOS
        SHLD OLDV
        LDA INTADR+2
        STA OLDV+2
        MVI A,0C3H      ; JMP ISR
        STA INTADR
        LXI H,ISR
        SHLD INTADR+1
        XRA A
        STA COUNT
        CALL SHOW
        MVI A,08H       ; 3214: D3 = 1, every level may interrupt (D2-D0 = /B2-/B0)
        STA I3214
        EI

LOOP:   LDA MASKLED     ; keyboard row D4 (SHIFT A Q 1 LF CR P 0)
        ANI 0E0H
        ORI 0FH
        STA P0
        LDA P0
        ANI 10H         ; LF (ESC) pressed = 0
        JNZ LOOP
WREL:   LDA P0          ; wait for its release
        ANI 10H
        JZ WREL
        DI
        LHLD OLDV       ; the vector of MIKROBIOS back
        SHLD INTADR
        LDA OLDV+2
        STA INTADR+2
        XRA A           ; the displays dark
        STA P1
        STA P2
        RET             ; back to the monitor

; Interrupt: which button, one step, wait for the release, the next interrupt.
ISR:    PUSH PSW
        PUSH H
        LDA P2
        MOV H,A
        ANI 01H         ; "+"
        JNZ ISR1
        LDA COUNT
        ADI 1           ; BCD + 1 (99 -> 00)
        DAA
        STA COUNT
ISR1:   MOV A,H
        ANI 80H         ; "-"
        JNZ ISR2
        LDA COUNT
        ADI 99H         ; BCD - 1 (00 -> 99)
        DAA
        STA COUNT
ISR2:   CALL SHOW
REL:    LDA P2          ; one step per press
        CMA
        ANI 81H
        JNZ REL
        CALL DELAY      ; the contacts bounce
        MVI A,08H       ; 3214: the next interrupt, every level
        STA I3214
        POP H
        POP PSW
        EI
        RET

; COUNT (BCD) on the displays: tens left, units right.
SHOW:   PUSH H
        LDA COUNT
        RRC
        RRC
        RRC
        RRC
        ANI 0FH
        CALL SEG
        STA P2
        LDA COUNT
        ANI 0FH
        CALL SEG
        STA P1
        POP H
        RET

SEG:    LXI H,FONT      ; A = digit 0-9 -> segments
        ADD L
        MOV L,A
        MVI A,0
        ADC H
        MOV H,A
        MOV A,M
        RET

DELAY:  PUSH D          ; about 20 ms at 2 MHz
        LXI D,1000
DEL1:   DCX D
        MOV A,D
        ORA E
        JNZ DEL1
        POP D
        RET

FONT:   DB 3FH,06H,5BH,4FH,66H,6DH,7DH,07H,7FH,6FH
COUNT:  DS 1
OLDV:   DS 3
        END
