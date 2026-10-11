; HW bench: the FXS4 freeze (L. Lasota 2026-10-10): an interrupt taken while the PC is in the window of a graphics
; card. The INTA cycle puts the PC on the address bus with STSTB; the card pulls RDY low and waits for /MR or
; /MW, which never come; the CPU stays in wait states (and RESET, sampled with /M1, does not help).
; Here: the loop JR $ runs from the CGA-1V video RAM at C100h (MAP1 = MAP2 = H), IM 1, the receive interrupt of
; DSM-1V channel A (the PC sends bytes). Marker OUT (F7h) right before the jump. If the handler at 38h runs, it
; sends "NOFREEZE" CR LF on the line. FREEZE_AND (assemble with it 1): the loop in the AND-1V at F900h instead
; (MAP1 = L). pasmo freeze.asm freeze.bin

FREEZE_AND equ 0

BASE    equ 10h
MARK    equ 0F7h
        if FREEZE_AND
LOOPAT  equ 0F900h
MAPV    equ 00h
        else
LOOPAT  equ 0C100h
MAPV    equ 0C0h
        endif

        org 0
        di
        jp start

        ds 38h-$,0FFh
        ; RST 38h: we got here, so no freeze
        in a,(BASE+2)
        xor a
        out (BASE),a        ; receive interrupt off
        ld hl,nofreeze
        jp report

start:  ld sp,0BF00h
        out (BASE+3),a      ; UART: reset, 8N1, a RAM read ends the reset
        xor a
        out (BASE),a
        ld a,0Dh
        out (BASE+1),a
        ld a,(8000h)
        ld a,MAPV           ; the window of the card
        out (63h),a
        ld a,18h            ; JR $ in the card's memory
        ld (LOOPAT),a
        ld a,0FEh
        ld (LOOPAT+1),a
        im 1
        in a,(BASE+2)       ; drop what came, receive interrupt on
        ld a,80h
        out (BASE),a
        out (MARK),a        ; marker: now into the window
        ei
        jp LOOPAT

report: ld a,(hl)
        or a
        jr z,done
rp1:    in a,(BASE+1)
        and 40h
        jr z,rp1
        ld a,(hl)
        out (BASE+2),a
        inc hl
        jr report
done:   jr done

nofreeze: db "NOFREEZE",13,10,0

        ds 2048-$,0FFh
