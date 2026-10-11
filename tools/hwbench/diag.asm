; HW bench diagnostic ROM: reads the DSM-1V UART registers step by step and "taps" each value on /IOR
; (no serial line needed): one IN of the register, then a marker of 5 reads of the unused port F7h, then 8 groups
; MSB first, bit 1 = 3 reads, bit 0 = 1 read; groups 100 us apart, values 1 ms apart. At the end it sends 55h
; ('U') every 2 ms forever without waiting for TBRE, so that TxD shows whether the transmitter works.
; Values: 1 status after /RES, 2 modem after /RES, 3 status after the UART reset (OUT +3), 4 status after
; 8N1, 5 modem after 8N1, 6 status right after two chars back to back (TBRE 0 expected), 7 status 3 ms later.
; pasmo diag.asm diag.bin

BASE    equ 10h         ; DSM-1V channel A (10h) or B (14h)
DUMMY   equ 0F7h        ; a port nobody uses: only the /IOR pulse matters

        org 0
        di
        ld sp,0BF00h    ; RAM-1V (the hole is at F800 after reset)
        in a,(BASE+1)
        call emit       ; 1
        in a,(BASE)
        call emit       ; 2
        out (BASE+3),a  ; UART reset
        in a,(BASE+1)
        call emit       ; 3
        xor a
        out (BASE),a
        ld a,0Dh        ; 8N1
        out (BASE+1),a
        in a,(BASE+1)
        call emit       ; 4
        in a,(BASE)
        call emit       ; 5
        ld a,55h
        out (BASE+2),a  ; two chars back to back: the 2nd waits in the buffer
        out (BASE+2),a
        in a,(BASE+1)
        call emit       ; 6
        ld de,462       ; 3 ms
        call delay
        in a,(BASE+1)
        call emit       ; 7
        ld de,15000     ; 100 ms
        call delay
tx:     ld a,55h
        out (BASE+2),a
        ld de,300       ; 2 ms
        call delay
        jr tx

; tap the value in A on /IOR
emit:   ld c,a
        ld de,31        ; 200 us
        call delay
        ld b,5          ; marker
em1:    in a,(DUMMY)
        djnz em1
        ld de,31
        call delay
        ld b,8
em2:    rlc c
        in a,(DUMMY)
        jr nc,em3
        in a,(DUMMY)
        in a,(DUMMY)
em3:    ld de,15        ; 100 us
        call delay
        djnz em2
        ld de,154       ; 1 ms
        call delay
        ret

; DE x 26 T (6.5 us at 4 MHz)
delay:  dec de
        ld a,d
        or e
        jr nz,delay
        ret

        ds 2048-$,0FFh
