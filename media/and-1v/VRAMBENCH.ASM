; VRAMBENCH.COM - speed of the video memory of AND-1V (F800h) against memory on SAPI-1 V, SAPIemu project.
; Made after ZRDBENCH of the Prince of Persia port (media/zrd-1v): the same timer and the same arithmetic.
; AND-1V gives the CPU its memory in a window every character (10 dots of 25.175 MHz, 397 ns) and holds RDY
; low until the window ends (docs/desky/AND-1V.md); this measures the wait states of LDIR into and from it.
;
; Time: 82C54 of MPH-1V, counter 0 (CLK0 = 55930.4 Hz), mode 2 from 65536, gate G0 on (IEN 28h), latched
; before and after each test. The CPU clock is calibrated first with a loop of a known number of T-states
; (from RAM-1V, no wait states), so the results are in T-states of the CPU at either TURBO setting. Every test
; moves 65536 bytes: 256 x 256 B. The read test copies the first 256 B of the screen (rows 0-3) to a buffer,
; the write test writes the same bytes back, so the screen stays as it was. AND-1V must be at F800h with
; MAP1 = L (machines/sapi1v.sapi, the author's board). Interrupts are off during the tests.
; Build: pasmo --hex VRAMBENCH.ASM vrambench.hex (CP/M: SAVE 6 VRAMBENCH.COM).

BDOS:		equ 0005h
PIT0:		equ 050h
PITCW:		equ 053h
MIEN:		equ 054h
MAP:		equ 063h			; RAM-1V MAP register: bit 6 = MAP1
AND1V:		equ 0F800h

		org 0100h
		ld sp,stack
		ld de,msg_title
		call print
		ld a,i				; interrupts on before? (P/V = IFF2)
		push af
		di
		xor a				; MAP1 = L: AND-1V at F800h
		out (MAP),a
		ld a,028h			; MPH-1V: gates G2, G0 on, no interrupt
		out (MIEN),a
		ld a,034h			; counter 0: LSB + MSB, mode 2, binary
		out (PITCW),a
		xor a
		out (PIT0),a
		out (PIT0),a
		ld hl,t_cal
		call measure
		ld (cnt_ref),hl
		ld de,msg_cpu
		call print
		ld hl,(cnt_ref)			; MHz x 100 = 9530145 / counts (1703931 T * 55930.4 Hz / 10^4)
		ld b,h
		ld c,l
		ld de,0091h
		ld hl,6B21h			; DEHL = 9530145
		call div32
		call print_x100
		ld de,msg_mhz
		call print
		ld ix,tests
.next:		ld l,(ix+0)
		ld h,(ix+1)
		ld a,h
		or l
		jr z,.done
		push ix				; (the tests use IXl as a counter)
		call measure
		pop ix
		ld (cnt),hl
		ld e,(ix+2)			; name
		ld d,(ix+3)
		call print
		ld hl,(cnt)			; T per unit x 100 = counts * 2600 / cnt_ref
		ld de,2600
		call mul16			; DEHL
		ld bc,(cnt_ref)
		call div32
		call print_x100
		ld de,msg_exp
		call print
		ld l,(ix+4)			; expected without wait states
		ld h,(ix+5)
		call print_x100
		ld de,msg_crlf
		call print
		ld bc,6
		add ix,bc
		jr .next
.done:		pop af
		jp po,.di
		ei
.di:		ld de,msg_end
		call print
		jp 0

; HL = test routine -> HL = elapsed counts of the timer
measure:
		ld (.m_call+1),hl
		call read_t
		ld (t0),hl
.m_call:	call 0
		call read_t
		ex de,hl
		ld hl,(t0)
		or a
		sbc hl,de			; the counter counts down
		ret

read_t:
		xor a				; latch counter 0
		out (PITCW),a
		in a,(PIT0)
		ld l,a
		in a,(PIT0)
		ld h,a
		ret

; --- tests: 65536 bytes each ------------------------------------------------------------------------------
t_cal:		ld bc,0				; 65536 x 26 T (- 5)
.c_l:		dec bc
		ld a,b
		or c
		jr nz,.c_l
		ret

t_ldir:		ld ixl,0			; LDIR memory -> memory, 256 x 256 B
.ld_l:		ld hl,buf
		ld de,buf2
		ld bc,256
		ldir
		dec ixl
		jr nz,.ld_l
		ret

t_andr:		ld ixl,0			; LDIR AND-1V -> memory
.ar_l:		ld hl,AND1V
		ld de,buf
		ld bc,256
		ldir
		dec ixl
		jr nz,.ar_l
		ret

t_andw:		ld ixl,0			; LDIR memory -> AND-1V (the bytes just read: the screen stays)
.aw_l:		ld hl,buf
		ld de,AND1V
		ld bc,256
		ldir
		dec ixl
		jr nz,.aw_l
		ret

; routine, name, T per byte x 100 without wait states: (10+10+10+5371+8+12) / 256
tests:		dw t_ldir, n_ldir, 2118
		dw t_andr, n_andr, 2118
		dw t_andw, n_andw, 2118
		dw 0

; --- arithmetic --------------------------------------------------------------------------------------------
; DEHL = HL * DE
mul16:
		ld b,h
		ld c,l
		ld hl,0
		ld a,16
.mu_l:		add hl,hl
		rl e
		rl d
		jr nc,.mu_n
		add hl,bc
		jr nc,.mu_n
		inc de
.mu_n:		dec a
		jr nz,.mu_l
		ret

; HL = DEHL / BC (quotient < 65536); the remainder is kept in dv_rem
div32:
		push hl
		ld hl,0
		ld (dv_rem),hl
		pop hl
		ld a,32
.dv_l:		add hl,hl			; DEHL << 1, the top bit into the remainder
		rl e
		rl d
		push hl
		ld hl,(dv_rem)
		adc hl,hl
		jr c,.dv_big			; 17 bits: larger than BC
		sbc hl,bc			; (carry is 0 here)
		jr nc,.dv_s
		add hl,bc
		ld (dv_rem),hl
		pop hl
		jr .dv_n
.dv_big:	or a
		sbc hl,bc			; (the 17th bit goes away)
.dv_s:		ld (dv_rem),hl
		pop hl
		inc l				; quotient bit (shifted in at bit 0)
.dv_n:		dec a
		jr nz,.dv_l
		ret

; print HL / 100 with 2 decimals (HL = value x 100)
print_x100:
		ld de,100
		call div16			; HL = HL / 100, A = remainder
		push af
		call print_dec
		ld e,'.'
		call putc
		pop af
		ld l,a
		ld h,0
		ld de,10
		call div16
		push af
		ld a,l
		add a,'0'
		ld e,a
		call putc
		pop af
		add a,'0'
		ld e,a
		jp putc

; HL = HL / DE, A = remainder (DE < 256)
div16:
		ld b,16
		xor a
		ld c,a
.d16:		add hl,hl
		rla
		jr c,.d16s
		cp e
		jr c,.d16n
.d16s:		sub e
		inc l
.d16n:		djnz .d16
		ret

; print HL in decimal
print_dec:
		ld bc,0				; C = a digit printed
		ld de,10000
		call .pd
		ld de,1000
		call .pd
		ld de,100
		call .pd
		ld de,10
		call .pd
		ld a,l
		add a,'0'
		ld e,a
		jp putc
.pd:		xor a
.pd_l:		or a
		sbc hl,de
		jr c,.pd_e
		inc a
		jr .pd_l
.pd_e:		add hl,de
		or a
		jr nz,.pd_p
		bit 0,c
		ret z				; no leading zeros
.pd_p:		set 0,c
		add a,'0'
		push hl
		push bc
		ld e,a
		call putc
		pop bc
		pop hl
		ret

putc:		push hl
		push bc
		ld c,2
		call BDOS
		pop bc
		pop hl
		ret

print:		push ix
		ld c,9
		call BDOS
		pop ix
		ret

msg_title:	db 'VRAMBENCH: AND-1V (F800h) against memory (MPH-1V timer)',13,10,'$'
msg_cpu:	db 'CPU clock: $'
msg_mhz:	db ' MHz',13,10,'T-states per byte (64 KB each):',13,10,'$'
msg_exp:	db '   (no waits: $'
msg_crlf:	db ')',13,10,'$'
msg_end:	db 'Done.',13,10,'$'
n_ldir:		db '  LDIR memory -> memory      $'
n_andr:		db '  LDIR AND-1V -> memory      $'
n_andw:		db '  LDIR memory -> AND-1V      $'

cnt_ref:	dw 0
dv_rem:		dw 0
cnt:		dw 0
t0:		dw 0
		ds 128
stack:
buf:		ds 256
buf2:		ds 256
