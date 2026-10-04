; ZRDTEST - test karty RAM disku ZRD-1V (rev 1.1 / 2.1) pod CP/M, porty 60h-63h.
; SAPIemu (docs/desky/ZRMD-1.md). Instrukce 8080, preklad: python tools/asm8080.py
; nebo M80/L80. Spusteni: ZRDTEST [takt CPU v MHz, 1-9, vychozi 4].
;
; 1. Mereni baterie (MAX187, rev 2.1): pristup na 62h spusti prevod, cteni 61h posouva
;    bity na D5 (konec prevodu, D11..D0, pak nuly).
; 2. Vedlejsi ucinek cteni 61h: co se precte a kam se tim posune adresa.
; 3. Zapis hned po 62h (uvnitr okna 74LS123).
; 4. Delka okna: zapis po 10, 11, ... 40 ms od pristupu na 62h. Rev 2.1 se na konci okna
;    ZAMKNE (odemkne RESET), rev 1.x zapisuje vzdy. Nakonec zapis po 100 ms (jako MikroMon).
; Pouziva jen bajty 3FFFF0h a 3FFFFFh (za daty CP/M, tez MikroMon) a vraci je.

BDOS    EQU     5
PDATA   EQU     60H
PLO     EQU     61H
PMID    EQU     62H
PHI     EQU     63H

        ORG     100H
START:  LXI     SP,STACK
        MVI     A,4
        STA     MHZ
        LDA     80H             ; command tail: " 2" = 2 MHz
        ORA     A
        JZ      NOARG
        LDA     82H
        SUI     '1'
        CPI     9
        JNC     NOARG
        INR     A
        STA     MHZ
NOARG:  LDA     MHZ             ; DE loops of 24 clocks per ms: MHz * 41 (-0.75 %)
        MOV     E,A
        MVI     D,0
        LXI     H,0
        MVI     B,41
MUL41:  DAD     D
        DCR     B
        JNZ     MUL41
        SHLD    CNTMS
        LXI     D,TITLE
        CALL    PRINT
        LDA     MHZ
        MOV     L,A
        MVI     H,0
        CALL    PDEC
        LXI     D,TMHZ
        CALL    PRINT

; ---- 1. battery
        IN      PMID            ; opens the window, starts the conversion
        MVI     B,40
BT0:    DCR     B
        JNZ     BT0             ; > 100 us
        IN      PLO             ; end of conversion: D5 = 1
        ANI     20H
        STA     EOC
        LXI     H,0
        MVI     B,12
BT1:    DAD     H
        IN      PLO
        ANI     20H
        JZ      BT2
        INX     H
BT2:    DCR     B
        JNZ     BT1
        SHLD    CODE
        IN      PLO             ; zeros after the LSB on the MAX187
        ANI     20H
        STA     TRAIL
        LXI     D,TBAT
        CALL    PRINT
        LDA     TRAIL
        ORA     A
        JZ      BT3
        LXI     D,TNOADC        ; only ones: no converter (rev 1.x)
        CALL    PRINT
        JMP     TEST2
BT3:    LDA     EOC
        ORA     A
        JNZ     BT4
        LXI     D,TNOEOC
        CALL    PRINT
BT4:    LHLD    CODE
        CALL    PDEC
        LXI     D,TMV
        CALL    PRINT

; ---- 2. a read of 61h: what it reads, where the address goes
TEST2:  LXI     H,0FFF0H
        CALL    RDBYTE
        STA     SAV0
        LXI     H,0FFFFH
        CALL    RDBYTE
        STA     SAVF
        LXI     H,0FFF0H
        MVI     B,55H
        CALL    WRBYTE
        LXI     H,0FFFFH
        MVI     B,0AAH
        CALL    WRBYTE
        LXI     D,TWR
        CALL    PRINT
        LXI     H,0FFF0H
        CALL    RDBYTE
        CPI     55H
        JNZ     WFAIL
        LXI     H,0FFFFH
        CALL    RDBYTE
        CPI     0AAH
        JNZ     WFAIL
        LXI     D,TOK
        CALL    PRINT
        LXI     H,0FFF0H        ; address 3FFFF0h, then IN 61h, then the data
        CALL    SETADR
        IN      PLO
        STA     R61
        IN      PDATA
        STA     X60
        LDA     R61             ; the byte at 3FFFxxh, xx = what 61h read
        MOV     L,A
        MVI     H,0FFH
        CALL    RDBYTE
        STA     Y60
        CALL    RESTOR
        LXI     D,TR61
        CALL    PRINT
        LDA     R61
        CALL    PHEX
        LXI     D,TX60
        CALL    PRINT
        LDA     X60
        CALL    PHEX
        LXI     D,TH
        CALL    PRINT
        LDA     X60
        CPI     55H
        JNZ     T2A
        LXI     D,TSAME         ; 3FFFF0h: the address did not change
        CALL    PRINT
        JMP     T3
T2A:    MOV     B,A
        LDA     Y60
        CMP     B
        JNZ     T2B
        LXI     D,TLOAD         ; 3FFFxxh: the register took what was read
        CALL    PRINT
        LDA     R61
        CALL    PHEX
        LXI     D,TLOAD2
        CALL    PRINT
        JMP     T3
T2B:    LXI     D,TOTHER
        CALL    PRINT

T3:

; ---- 3./4. the write window and the lock
        LXI     D,TWIN
        CALL    PRINT
        MVI     A,10
        STA     DMS
WIN1:   LXI     H,0FFFFH        ; OUT 61h, 62h (window), 63h
        CALL    SETADR
        LDA     DMS
        CALL    DELAY
        LDA     SAVF
        CMA
        OUT     PDATA           ; DMS ms after 62h
        LXI     H,0FFFFH
        CALL    RDBYTE
        MOV     B,A
        LDA     SAVF
        CMA
        CMP     B
        JNZ     LOCKED
        LXI     H,0FFFFH        ; written: put the byte back, next delay
        LDA     SAVF
        MOV     B,A
        CALL    WRBYTE
        LDA     DMS
        INR     A
        STA     DMS
        CPI     41
        JC      WIN1
        LXI     D,T40           ; still writing after 40 ms: try 100 ms
        CALL    PRINT
        MVI     A,100
        STA     DMS
        LXI     H,0FFFFH
        CALL    SETADR
        MVI     A,100
        CALL    DELAY
        LDA     SAVF
        CMA
        OUT     PDATA
        LXI     H,0FFFFH
        CALL    RDBYTE
        MOV     B,A
        LDA     SAVF
        CMA
        CMP     B
        JNZ     LOCKED
        LXI     H,0FFFFH
        LDA     SAVF
        MOV     B,A
        CALL    WRBYTE
        LXI     D,TNOLOCK
        CALL    PRINT
        JMP     DONE
LOCKED: LXI     D,TLOCK1
        CALL    PRINT
        LDA     DMS
        MOV     L,A
        MVI     H,0
        CALL    PDEC
        LXI     D,TLOCK2
        CALL    PRINT
        LDA     DMS
        DCR     A
        MOV     L,A
        MVI     H,0
        CALL    PDEC
        LXI     D,TLOCK3
        CALL    PRINT
        LDA     DMS
        MOV     L,A
        MVI     H,0
        CALL    PDEC
        LXI     D,TLOCK4
        CALL    PRINT
        JMP     DONE

WFAIL:  LXI     D,TWFAIL        ; writes do not work: locked already?
        CALL    PRINT
DONE:   LXI     D,TEND
        CALL    PRINT
        JMP     0

; ---- the two test bytes back
RESTOR: LXI     H,0FFF0H
        LDA     SAV0
        MOV     B,A
        CALL    WRBYTE
        LXI     H,0FFFFH
        LDA     SAVF
        MOV     B,A
        JMP     WRBYTE

; ---- address 3Fxxxxh: L = 61h, H = 62h (opens the window), 63h = 3Fh
SETADR: MOV     A,L
        OUT     PLO
        MOV     A,H
        OUT     PMID
        MVI     A,3FH
        OUT     PHI
        RET
RDBYTE: CALL    SETADR          ; A = byte at 3F:HL
        IN      PDATA
        RET
WRBYTE: CALL    SETADR          ; byte B to 3F:HL, right after 62h
        MOV     A,B
        OUT     PDATA
        RET

; ---- A ms (24 clocks per loop)
DELAY:  PUSH    B
        PUSH    D
        MOV     C,A
DL1:    LHLD    CNTMS
        XCHG
DL2:    DCX     D
        MOV     A,D
        ORA     E
        JNZ     DL2
        DCR     C
        JNZ     DL1
        POP     D
        POP     B
        RET

; ---- output
PRINT:  MVI     C,9
        JMP     BDOS
PCHAR:  PUSH    H
        MOV     E,A
        MVI     C,2
        CALL    BDOS
        POP     H
        RET
PHEX:   PUSH    PSW
        RAR
        RAR
        RAR
        RAR
        CALL    PNIB
        POP     PSW
PNIB:   ANI     0FH
        ADI     '0'
        CPI     '9'+1
        JC      PCHAR
        ADI     7
        JMP     PCHAR
PDEC:   MVI     B,0             ; HL decimal, no leading zeros
        LXI     D,10000
        CALL    PDIG
        LXI     D,1000
        CALL    PDIG
        LXI     D,100
        CALL    PDIG
        LXI     D,10
        CALL    PDIG
        MOV     A,L
        ADI     '0'
        JMP     PCHAR
PDIG:   MVI     C,0             ; one digit: HL / DE, the rest stays in HL
PD1:    MOV     A,L
        SUB     E
        MOV     L,A
        MOV     A,H
        SBB     D
        MOV     H,A
        JC      PD2
        INR     C
        JMP     PD1
PD2:    DAD     D
        MOV     A,C
        ORA     B
        RZ                      ; a leading zero
        MVI     B,1
        MOV     A,C
        ADI     '0'
        PUSH    B
        PUSH    D
        CALL    PCHAR
        POP     D
        POP     B
        RET

TITLE:  DB      13,10,'ZRDTEST 1.0 - RAM disk ZRD-1V na 60h, takt CPU $'
TMHZ:   DB      ' MHz',13,10,'$'
TBAT:   DB      '1. Baterie (MAX187): $'
TNOADC: DB      'nenalezen (61h cte jednicky, rev 1.x)',13,10,'$'
TNOEOC: DB      '(chybi konec prevodu) $'
TMV:    DB      ' mV',13,10,'$'
TWR:    DB      '2. Zapis hned po 62h: $'
TOK:    DB      'OK',13,10,'$'
TR61:   DB      '   adresa 3FFFF0h, IN 61h = $'
TX60:   DB      'h, IN 60h = $'
TH:     DB      'h',13,10,'$'
TSAME:  DB      '   -> cteni 61h adresu nezmenilo',13,10,'$'
TLOAD:  DB      '   -> cteni 61h nahralo do adresy $'
TLOAD2: DB      'h (prectenou hodnotu)',13,10,'$'
TOTHER: DB      '   -> adresa se zmenila jinak',13,10,'$'
TWIN:   DB      '3. Okno po 62h: zapis za 10..40 ms',13,10,'$'
T40:    DB      '   po 40 ms se porad zapisuje, zkousim 100 ms (jako MikroMon)',13,10,'$'
TNOLOCK: DB     '   zapis po 100 ms prosel: karta nema zamek (rev 1.x)',13,10,'$'
TLOCK1: DB      '   zapis po $'
TLOCK2: DB      ' ms neprosel: okno je $'
TLOCK3: DB      '..$'
TLOCK4: DB      ' ms (+-2 %)',13,10,'   Karta je ZAMCENA (rev 2.x), odemkne ji RESET.',13,10,'$'
TWFAIL: DB      'NEJDE (karta uz je zamcena? RESET)',13,10,'$'
TEND:   DB      'Konec.',13,10,'$'

MHZ:    DS      1
CNTMS:  DS      2
EOC:    DS      1
TRAIL:  DS      1
CODE:   DS      2
SAV0:   DS      1
SAVF:   DS      1
R61:    DS      1
X60:    DS      1
Y60:    DS      1
DMS:    DS      1
        DS      64
STACK:
        END
