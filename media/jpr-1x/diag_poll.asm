; Counter 00-99 on the diagnostic board (K3, rev 2.0) of JPR-1S and JPR-1X, polled
; (no interrupts). Button "+" = P2-IN0, "-" = P2-IN7 (0 = pressed), one press = one
; step. Left display = tens (P2-OUT), right display = units (P1-OUT).
; Start from the Falcon monitor: JUMP 4400. ESC (the LF key of the ANK-1) ends it.
; python tools/asm8080.py media/jpr-1x/diag_poll.asm media/jpr-1x/diag_poll.hex

P0      EQU 2400H       ; keyboard: P0-OUT rows, P0-IN columns
P1      EQU 2800H       ; P1-OUT: the right display
P2      EQU 2C00H       ; P2-OUT: the left display; P2-IN: the buttons
MASKLED EQU 200EH       ; MIKROBIOS: LEDs and beeper for P0-OUT

        ORG 4400H
START:  XRA A
        STA COUNT
        CALL SHOW

LOOP:   LDA P2
        CMA
        ANI 81H         ; a button pressed?
        JZ KEY
        MOV B,A
        ANI 01H         ; "+"
        JZ NOPLUS
        LDA COUNT
        ADI 1           ; BCD + 1 (99 -> 00)
        DAA
        STA COUNT
NOPLUS: MOV A,B
        ANI 80H         ; "-"
        JZ NOMIN
        LDA COUNT
        ADI 99H         ; BCD - 1 (00 -> 99)
        DAA
        STA COUNT
NOMIN:  CALL SHOW
REL:    LDA P2          ; one step per press
        CMA
        ANI 81H
        JNZ REL
        CALL DELAY      ; the contacts bounce

KEY:    LDA MASKLED     ; keyboard row D4 (SHIFT A Q 1 LF CR P 0)
        ANI 0E0H
        ORI 0FH
        STA P0
        LDA P0
        ANI 10H         ; LF (ESC) pressed = 0
        JNZ LOOP
WREL:   LDA P0          ; wait for its release
        ANI 10H
        JZ WREL
        XRA A           ; the displays dark
        STA P1
        STA P2
        RET             ; back to the monitor

; COUNT (BCD) on the displays: tens left, units right.
SHOW:   LDA COUNT
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
        RET

SEG:    LXI H,FONT      ; A = digit 0-9 -> segments
        ADD L
        MOV L,A
        MVI A,0
        ADC H
        MOV H,A
        MOV A,M
        RET

DELAY:  LXI D,1000      ; about 20 ms at 2 MHz
DEL1:   DCX D
        MOV A,D
        ORA E
        JNZ DEL1
        RET

FONT:   DB 3FH,06H,5BH,4FH,66H,6DH,7DH,07H,7FH,6FH
COUNT:  DS 1
        END
