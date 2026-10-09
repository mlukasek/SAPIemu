; ZRDBENCH.COM - speed of the RAM disk ZRD-1V (ports 60h-63h) against memory, and of CGA-1V memory, on SAPI-1 V.
; Prince of Persia reads its images and keeps its background buffers on the RAM disk; this checks that
; IN / INIR / OTIR there are as fast as memory (no wait states) at the CPU clock used (docs/hw-otazky.md).
;
; Time: 82C54 of MPH-1V, counter 0 (CLK0 = 55930.4 Hz), mode 2 from 65536, gate G0 on (IEN 28h), latched
; before and after each test (a test takes less than 1.17 s). The CPU clock is calibrated first with a loop
; of a known number of T-states (it runs from RAM-1V, which has no wait states), so the results are in
; T-states of the CPU, whatever the TURBO setting. Every test does 65536 units (bytes or loop turns):
; T per unit x 100 = counts * 2600 / calibration counts (the calibration loop is 26 T x 65536).
; The RAM disk test address is the last block of a 4 MB disk (3FF000h): 256 B are read there and only
; the same bytes are written back. Interrupts are off during the tests. Build: pasmo --bin zrdbench.asm.

BDOS:		equ 0005h
ZRD_DATA:	equ 060h
ZRD_A0:		equ 061h
ZRD_A8:		equ 062h
ZRD_A16:	equ 063h			; also the MAP register of RAM-1V (bits 6-7)
PIT0:		equ 050h
PITCW:		equ 053h
MIEN:		equ 054h
CGA:		equ 0C000h
CGA_CFG:	equ CGA+03FFBh
T_A16:		equ 03Fh			; 3FF000h
T_A8:		equ 0F0h

		org 0100h
		ld sp,stack
		ld de,msg_title
		call print
		ld a,i				; interrupts on before? (P/V = IFF2)
		push af
		di
		ld a,028h			; MPH-1V: gates G2, G0 on, no interrupt
		out (MIEN),a
		ld a,034h			; counter 0: LSB + MSB, mode 2, binary
		out (PITCW),a
		xor a
		out (PIT0),a
		out (PIT0),a
		; calibration
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
		; the tests
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
.done:		xor a				; CGA-1V unmapped, ZRD A16 0
		out (ZRD_A16),a
		pop af
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

; ZRD-1V address 3FF000h (A0 = 0)
zrd_addr:
		ld a,T_A16
		out (ZRD_A16),a
		ld a,T_A8
		out (ZRD_A8),a
		xor a
		out (ZRD_A0),a
		ret

; --- tests: 65536 units each -----------------------------------------------------------------------------
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

t_inir:		call zrd_addr			; INIR from 60h (A0-A7 wrap within the 256 B), 256 x 256 B
		ld ixl,0
.in_l:		ld hl,buf
		ld bc,0*256+ZRD_DATA
		inir
		dec ixl
		jr nz,.in_l
		ret

t_otir:		call zrd_addr			; OTIR to 60h: the 256 B read there are written back, 256 x 256 B
		ld hl,buf
		ld bc,0*256+ZRD_DATA
		inir
		ld ixl,0
.ot_l:		ld hl,buf
		ld bc,0*256+ZRD_DATA
		otir
		dec ixl
		jr nz,.ot_l
		ret

t_ldmem:	ld ixl,0			; loop ld a,(hl) / inc hl / djnz
.lm_o:		ld hl,buf
		ld b,0
.lm_l:		ld a,(hl)
		inc hl
		djnz .lm_l
		dec ixl
		jr nz,.lm_o
		ret

t_inport:	call zrd_addr			; loop in a,(c) / djnz (the address goes on by itself)
		ld c,ZRD_DATA
		ld ixl,0
.ip_o:		ld b,0
.ip_l:		in a,(c)
		djnz .ip_l
		dec ixl
		jr nz,.ip_o
		ret

t_cgaw:		ld a,0C0h			; LDIR memory -> CGA-1V (page B for the CPU, A shown), 256 x 256 B
		out (ZRD_A16),a
		ld a,006h
		ld (CGA_CFG),a
		ld ixl,0
.cw_l:		ld hl,buf
		ld de,CGA
		ld bc,256
		ldir
		dec ixl
		jr nz,.cw_l
		xor a
		out (ZRD_A16),a
		ret

t_cgar:		ld a,0C0h			; LDIR CGA-1V -> memory, 256 x 256 B
		out (ZRD_A16),a
		ld a,006h
		ld (CGA_CFG),a
		ld ixl,0
.cr_l:		ld hl,CGA
		ld de,buf
		ld bc,256
		ldir
		dec ixl
		jr nz,.cr_l
		xor a
		out (ZRD_A16),a
		ret

; routine, name, T per unit x 100 without wait states (counted, loop overhead included; the Python Z80
; model measures the same within 0.01)
tests:		dw t_ldir, n_ldir, 2118		; (10+10+10+5371+8+12) / 256
		dw t_inir, n_inir, 2114		; (10+10+5371+8+12) / 256
		dw t_otir, n_otir, 2122		; the same + the first INIR (5371 / 65536)
		dw t_ldmem, n_ldmem, 2613	; (10+7+26*255+21+8+12) / 256
		dw t_inport, n_inport, 2509	; (7+25*255+20+8+12) / 256
		dw t_cgaw, n_cgaw, 2118
		dw t_cgar, n_cgar, 2118
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

msg_title:	db 'ZRDBENCH: ZRD-1V and CGA-1V against memory (MPH-1V timer)',13,10,'$'
msg_cpu:	db 'CPU clock: $'
msg_mhz:	db ' MHz',13,10,'T-states per byte / loop turn (64 KB each):',13,10,'$'
msg_exp:	db '   (no waits: $'
msg_crlf:	db ')',13,10,'$'
msg_end:	db 'Done.',13,10,'$'
n_ldir:		db '  LDIR memory -> memory      $'
n_inir:		db '  INIR RAM disk -> memory    $'
n_otir:		db '  OTIR memory -> RAM disk    $'
n_ldmem:	db '  loop LD A,(HL) / INC HL    $'
n_inport:	db '  loop IN A,(C) from 60h     $'
n_cgaw:		db '  LDIR memory -> CGA-1V      $'
n_cgar:		db '  LDIR CGA-1V -> memory      $'

cnt_ref:	dw 0
dv_rem:		dw 0
cnt:		dw 0
t0:		dw 0
		ds 128
stack:
buf:		ds 256
buf2:		ds 256
