; XY4130_DEMO - ukazka kresleni ovladacem XY 4130 v EPROM za MIKOSem V5 (1800h), napsana pro SAPIemu.
; Napis a pod nim ramecek 100 x 60 mm s uhloprickami. Souradnice v krocich 0,1 mm, X v DE, Y v BC.
; Po startu stroje (nebo .G=1803) je pocatek X 0 osm milimetru od leveho dorazu a pero stoji na Y 2460,
; tam zacina text. Ovladac po presunu nebo care zacne dalsi text novou strankou (HOME: znovu na doraz
; a papir o 126 mm dal), proto je napis prvni.
; Spusteni: Soubor -> Nahrat program -> XY4130_DEMO.hex, v monitoru .G=8000 (navod docs/navod-xy4130.md).
; Preklad: python tools/asm8080.py media/mikos/XY4130_DEMO.asm media/mikos/XY4130_DEMO.hex
ZNAKP   EQU     1800H           ; znak z C (tiskarna: od mista, kde skoncil predchozi text)
AMOVA   EQU     1809H           ; presun zvednuteho pera na X (DE), Y (BC)
AVECTA  EQU     180CH           ; cara na X (DE), Y (BC)

        ORG     8000H
START:  LXI     H,TEXT          ; napis, CR LF a konec (0)
ZNAK:   MOV     A,M
        ORA     A
        JZ      RAM0
        MOV     C,A
        PUSH    H
        CALL    ZNAKP
        POP     H
        INX     H
        JMP     ZNAK
RAM0:   MVI     A,1
        STA     PERO            ; prvni bod presunem
        LXI     H,BODY          ; tabulka bodu: X, Y (slova), 0FFFFH konec
DALSI:  CALL    BOD
        JNZ     DALSI
        LXI     D,0             ; pero zvednute vlevo pod ramecek
        LXI     B,1500
        JMP     AMOVA           ; navrat do monitoru z ovladace

; dalsi bod z tabulky (HL), presunem nebo carou podle PERO; Z = konec tabulky
BOD:    MOV     E,M
        INX     H
        MOV     D,M
        INX     H
        MOV     A,D
        ANA     E
        CPI     0FFH
        RZ
        MOV     C,M
        INX     H
        MOV     B,M
        INX     H
        PUSH    H
        LDA     PERO
        ORA     A
        LXI     H,AVECTA
        JZ      BOD1
        LXI     H,AMOVA
BOD1:   CALL    VOLEJ
        XRA     A
        STA     PERO            ; dalsi body carou
        POP     H
        INR     A               ; NZ: bod byl
        RET
VOLEJ:  PCHL

PERO:   DB      1               ; 1 = presun, 0 = cara
BODY:   DW      0,1700, 1000,1700, 1000,2300, 0,2300, 0,1700, 1000,2300
        DW      1000,1700, 0,2300
        DW      0FFFFH
TEXT:   DB      'SAPI-1 A XY 4130',13,10,0
        END
