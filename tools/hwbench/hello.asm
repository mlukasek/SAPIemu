; HW bench test ROM for SAPI-1 V (JPR-1V + PicoROM, DSM-1V channel A at 10h, 9600 Bd by jumpers).
; Sends a banner, then a counter line about every 0.5 s. No RAM, no stack, no interrupts.
; pasmo hello.asm hello.bin

RAMREAD equ 1           ; 0: no memory read on the bus (the UART stays in reset on DSM-1V)
BASE    equ 10h         ; DSM-1V channel A (10h) or B (14h)
UCTL    equ BASE        ; modem control / status
UCRL    equ BASE+1      ; control register (write) / DR, TBRE (read)
UDATA   equ BASE+2
URST    equ BASE+3      ; any write resets the UART

        org 0
        di
        out (URST),a
        xor a
        out (UCTL),a
        ld a,0Dh        ; 8N1
        out (UCRL),a
        if RAMREAD
        ld a,(8000h)    ; a memory read on the bus (/MR) ends the UART reset (DSM-1V: MR held until /MR)
        endif
        ld hl,banner
        ld ix,next1
        jp puts
next1:
        ld de,0         ; line counter
again:
        ld hl,line
        ld ix,next2
        jp puts
next2:
        ld a,d          ; counter as 4 hex digits
        rrca
        rrca
        rrca
        rrca
        ld iy,h1
        jp hexd
h1:     ld a,d
        ld iy,h2
        jp hexd
h2:     ld a,e
        rrca
        rrca
        rrca
        rrca
        ld iy,h3
        jp hexd
h3:     ld a,e
        ld iy,h4
        jp hexd
h4:     ld hl,crlf
        ld ix,next3
        jp puts
next3:
        inc de
        ld bc,0         ; delay about 0.5 s at 4 MHz (65536 x 26 T)
dly:    dec bc
        ld a,b
        or c
        jp nz,dly
        jp again

; send the zero-terminated string at HL, return by JP (IX)
puts:   ld a,(hl)
        or a
        jr z,puts9
puts1:  in a,(UCRL)
        and 40h         ; TBRE
        jr z,puts1
        ld a,(hl)
        out (UDATA),a
        inc hl
        jr puts
puts9:  jp (ix)

; send the low nibble of A as a hex digit, return by JP (IY)
hexd:   and 0Fh
        add a,90h
        daa
        adc a,40h
        daa
        ld c,a
hexd1:  in a,(UCRL)
        and 40h
        jr z,hexd1
        ld a,c
        out (UDATA),a
        jp (iy)

banner: db 13,10,"SAPIemu HW test: hello from the PicoROM",13,10,0
line:   db "line ",0
crlf:   db 13,10,0

        ds 2048-$,0FFh
