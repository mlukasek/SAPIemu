"""Make media/rkd-1/cm5400/BIRKD54.MAC (not an original, docs/desky/RKD-1.md): the 3K BIOS of J. Biba
(bi2612E4 8x512, V261211JB) with C: = ZRMD-1 4 MB (+ SYSTEM, as BIOS-1K V271111JB on r1_zrmd4m_271111) and
D: to G: = RKD-1 with the CM 5400 (12 x 512 B, FORMRKD format), the driver RKD.MAC of Tesla Elstroj 87-02-13
changed to the sector header of FORMRKD.COM. The changes are made here on the original sources, so they stay
visible. Then tools/rkd5400_bios.py assembles it and makes the boot disk.
python tools/rkd5400_bios_src.py [BIOS3K RKD.MAC OUT]   (default: the sources in ../SAPI_DOKU)"""
import os
import re
import sys

_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_doku = os.path.join(os.path.dirname(_root), 'SAPI_DOKU', 'SAPIMAN')
if len(sys.argv) < 4:
    sys.argv[1:] = [
        os.path.join(_doku, 'CPM_SYSTÉMY', 'ZMENA_CPM', 'FUNKCNI', 'JEN 8 PALCU', 'RPD A MFC', '3K RPD MFC 8 512 261211',
                     'bi2612E4 8x512.txt'),
        os.path.join(_doku, 'SAPI1_ARCHIV', 'ze SAPI', 'zdrojemercl', 'rkd.txt'),
        os.path.join(_root, 'media', 'rkd-1', 'cm5400', 'BIRKD54.MAC'),
    ]
orig = open(sys.argv[1], 'rb').read().decode('latin-1').replace('\r\n', '\n').split('\n')
rkd = open(sys.argv[2], 'rb').read().decode('latin-1').replace('\r\n', '\n').split('\n')


def find(lines, pat, start=0):
    for i in range(start, len(lines)):
        if re.match(pat, lines[i]):
            return i
    raise SystemExit('not found: ' + pat)


def block(lines, a, b):   # lines a .. b-1
    return lines[a:b]


out = []
HEADER = r"""TITLE	BIOS JPR1-Z 3 KB RKD-1 CM 5400 V091026 (SAPIemu)
;
; BIOS-3K for the CP/M of J. Biba with the cartridge disk RKD-1 and the Bulgarian drive CM 5400.
; NOT AN ORIGINAL: made 2026-10-09 for the emulator SAPIemu (docs/desky/RKD-1.md), because no CP/M for
; RKD-1 with the CM 5400 was found and FORMRKD of J. Biba could format disks no system could use.
; Made from:
;  - BIOS JPR1-Z 3 KB DLOUHY BIOS V260811JB, EDIT D: E: BLOK/DEBLOK 512/8 261211JB (bi2612E4 8x512.txt,
;    SAPI_DOKU\SAPIMAN\CPM_SYSTEMY\ZMENA_CPM\FUNKCNI\JEN 8 PALCU\RPD A MFC\3K RPD MFC 8 512 261211): the frame,
;    A: B: (RPD-1 through the monitor), the blocking/deblocking of 512 B sectors, boot and warm boot;
;  - CBIOS JPR-1(Z) V271111_ZRMD_JB (bi271111EC_ZRMD4.txt): C: = ZRMD-1 4 MB, with the DPB of the boot disk
;    r1_zrmd4m_271111 ("C: ZRMD1/4 MB + SYSTEM": OFF 32, DSM 1021), so C: keeps its files;
;  - RKD.MAC 87-02-13 (Tesla ELSTROJ & JaMeSoft) as linked into FORMRKD.COM v1.1 of J. Biba: the header of a
;    sector is only the track (track * 32, low byte first), so the disks formatted by FORMRKD read here.
;    One fix of the original: RK_SEEK returned with BC left on the stack on an error.
; D: E: = the fixed disk (lower, upper surface), F: G: = the cartridge (lower, upper), as FORMRKD names them.
; 204 tracks x 12 sectors x 512 B, 2 KB blocks, 256 directory entries, 1224 KB per surface.
;
;************************
.Z80
VERZE	EQU	13
;
MEMORY	EQU	0F800H		;VELIKOST PAMETI 62 KB
CBIOSL	EQU	0C00H		;DELKA CBIOS NA DISKETE (MAX 13 SEKTORU)
				;+ DIRBUF+ALL+CHK JE MAX 15 SEKTORU
				;DALSICH 6 SEKTORU NA STOPE 76
				;A: B: 8" SS SD RPD-1
				;C: ZRMD-1/4MB + SYSTEM
				;D: E: F: G: RKD-1 CM 5400 12x512
"""
out += HEADER.rstrip('\n').split('\n')
i_mon = find(orig, r'MONITL\s+EQU')
i_dpbase = find(orig, r'DP_BASE::')
out += block(orig, i_mon, i_dpbase + 1)
out += [
    '\tDW SKEW81, 0,0,0,DIRBUF,DPB_81, CHK_A,ALL_A',
    '\tDW SKEW81, 0,0,0,DIRBUF,DPB_81, CHK_B,ALL_B',
    '\tDW 0,0,0,0,      DIRBUF,DPB_ZR,CHK_C,ALL_C',
    '\tDW 0,0,0,0,      DIRBUF,DPB_RF,CHK_D,ALL_D\t;RKD pevny dolni',
    '\tDW 0,0,0,0,      DIRBUF,DPB_RF,CHK_E,ALL_E\t;RKD pevny horni',
    '\tDW 0,0,0,0,      DIRBUF,DPB_RV,CHK_F,ALL_F\t;RKD vymenny dolni',
    '\tDW 0,0,0,0,      DIRBUF,DPB_RV,CHK_G,ALL_G\t;RKD vymenny horni',
]
i_skew81 = find(orig, r'SKEW81::')
i_skew85 = find(orig, r'SKEW85::')
out += [';----------------------------'] + block(orig, i_skew81, i_skew85)
i_dpb81 = find(orig, r'DPB_81::')
i_dpb85 = find(orig, r'DPB_85::')
out += block(orig, i_dpb81 - 2, i_dpb85)
out += r""";RKD-1 CM 5400 pevny disk: fyz 12x512x204, log 48x128x204, AB=2k, 256 polozek adresare
DPB_RF::
	DW	48	;SEKTORU / STOPU
	DB	4	;BLOCK SHIFT
	DB	15	;BLOCK MASK
	DB	0	;EXTENT MASK
	DW	611	;CISLO MAX. ALOK. SKUPINY -1
	DW	255	;POLOZEK V ADRESARI -1
	DB	11110000B ;BIT MAPA ALOK. SKUP. PRO ADRESAR
	DB	00000000B
	DW	0	;PEVNY DISK, BEZ KONTROLY ADRESARE
	DW	0	;SYSTEMOVE STOPY NEJSOU
;----------------------------
;RKD-1 CM 5400 kazeta (vymenna): jako pevny, s kontrolou adresare
DPB_RV::
	DW	48	;SEKTORU / STOPU
	DB	4	;BLOCK SHIFT
	DB	15	;BLOCK MASK
	DB	0	;EXTENT MASK
	DW	611	;CISLO MAX. ALOK. SKUPINY -1
	DW	255	;POLOZEK V ADRESARI -1
	DB	11110000B ;BIT MAPA ALOK. SKUP. PRO ADRESAR
	DB	00000000B
	DW	64	;BYTU PRO KONTROLU ADRESARE
	DW	0	;SYSTEMOVE STOPY NEJSOU
;----------------------------
; ZRMD-1 RAMDISK 4Mb, 4 kB bloky, 1024 polozek adresare, 32 stop (8 kB) systemu
; (jako bootovaci disketa r1_zrmd4m_271111 "C: ZRMD1/4 MB + SYSTEM")
DPB_ZR::
	DW	2	;SPT
	DB	5	;BSH
	DB	31	;BLM
	DB	1	;EXM
	DW	1021	;DSM
	DW	1023	;DRM
	DB	0FFH	;AL0
	DB	0	;AL1
	DW	0	;CKS
	DW	32	;OFF
""".rstrip('\n').split('\n')
i_cpm = find(orig, r';\*+$', find(orig, r'DPB_RM::'))
i_read = find(orig, r'READ::')
sel = block(orig, i_cpm, i_read)
sel = [l.replace('\tCP\t5\n', '') for l in sel]
k = [j for j, l in enumerate(sel) if re.match(r'\tCP\t5\s*$', l)]
assert len(k) == 1
sel[k[0]] = '\tCP\t7\t\t;A: az G:'
out += sel
i_rrmd = find(orig, r'RRMD::')
rd = block(orig, i_read, i_rrmd - 2)
rd = [l.replace('JR\tZ,RRMD\t\t;C: RMD', 'JR\tZ,RDRMD\t\t;C: ZRMD').replace('JP\tNC,READ_MFC\t;D: E:', 'JP\tNC,READ_RKD\t;D: az G:')
      for l in rd]
assert any('RDRMD' in l for l in rd) and any('READ_RKD' in l for l in rd)
out += rd
out += r""";----------------------------
; ZRMD-1 (z BIOS-1K V271111_ZRMD_JB)
Z_R_W	EQU	60H		;BASE ZRMD1
RDRMD::
	CALL	RMD.1
	INIR
	XOR	A
	RET
WRRMD::
	CALL	RMD.1
	OTIR
	XOR	A
	RET
RMD.1::
	LD	C,Z_R_W+1	; bazova adresa
	LD	A,(LOG_SECTOR)	; sektor 0 nebo 1
	RRCA			; sektor do do B7
	OUT	(C),A		; nastaveni citacu A0-A7
	LD	HL,(LOG_TRACK)	; cislo stopy do HL
	INC	C		; port pro A8 - A15
	OUT	(C),L		; zapis nizsiho byte stopy
	INC	C		; port pro A16 - A19
	OUT	(C),H		; zapis vyssiho byte stopy
	LD	HL,(DMA_ADR)	; zdrojova/cilova adresa do HL
	LD	C,Z_R_W		; C=datovy port R/W
	LD	B,80H		; B=128 bytu
	RET
""".rstrip('\n').split('\n')
i_write = find(orig, r'WRITE::')
i_subttl = find(orig, r'SUBTTL OVLADAC MFC')
wr = block(orig, i_write - 7, i_subttl - 1)
wr = [l.replace('JR\tZ,WRMD\t\t;C: RMD', 'JR\tZ,WRRMD\t\t;C: ZRMD').replace('JP\tNC,WRITE_MFC\t;D: E:', 'JP\tNC,WRITE_RKD\t;D: az G:')
      for l in wr]
assert any('WRRMD' in l for l in wr) and any('WRITE_RKD' in l for l in wr)
out += wr
out += [';*******************************', 'SUBTTL BLOK/DEBLOK 512 B PRO RKD-1 (z V261211JB)', ';*******************************']
i_readmfc = find(orig, r'READ_MFC::')
i_basic = find(orig, r';\*+$', find(orig, r'\.2\.fill_buf::'))
deb = block(orig, i_readmfc - 1, i_basic)
text = '\n'.join(deb)
text = text.replace('READ_MFC::', 'READ_RKD::').replace('WRITE_MFC::', 'WRITE_RKD::')
old_flush = re.search(r'\tld\ta,\(buf_disk\)\t;hodnoty pro zapis\n.*?\tcall\twr_MFC[^\n]*\n', text, re.S).group(0)
text = text.replace(old_flush, '\tld\ta,(buf_disk)\t;fyz. disk (medium 0-3)\n\tld\td,a\n\tld\ta,(buf_track)\n\tld\te,a\n'
                    '\tld\ta,(buf_sector)\n\tld\tc,a\t\t;sektor 0-11\n\tcall\twr_RKD\t\t;zapis RKD-1\n')
old_fill = re.search(r'\tld\ta,d\n\tld\t\(DISK\),A\t;vyber disk\n.*?\tcall\trd_MFC[^\n]*\n', text, re.S).group(0)
text = text.replace(old_fill, '\tcall\trd_RKD\t\t;cti RKD-1 (D, E, C)\n')
assert 'MFC' not in text.replace('V261211JB', ''), [l for l in text.split('\n') if 'MFC' in l]
out += text.split('\n')
# the driver
i_init = find(rkd, r';funkce\s+RK_INIT')
i_dseg = find(rkd, r'\s+DSEG')
i_defs = find(rkd, r';\s+DEFINICE')
i_init0 = find(rkd, r'RK_INIT:')
drv_defs = block(rkd, i_defs - 1, i_init - 1)
drv = block(rkd, i_init - 1, i_dseg)
dt = '\n'.join(drv)
# RK_SEEK: no BC left on the stack on an error (fix)
seek = re.search(r'RK_SEEK:\n\tpush\tbc\n\tld\tc,a\n\tWAIT_BUSY[^\n]*\n\tret\tnz\n(;-+\n)\twait_seek\n\tret\tnz\n', dt)
assert seek, 'RK_SEEK'
dt = dt.replace(seek.group(0), 'RK_SEEK:\n\tpush\tbc\n\tld\tc,a\n\tWAIT_BUSY\t\t;cekej na pripravenost radice\n'
                '\tjr\tnz,.9.rk_seek\t;SAPIemu: oprava, BC ze zasobniku\n' + seek.group(1) + '\twait_seek\n'
                '\tjr\tnz,.9.rk_seek\n')
dt = dt.replace('\tXOR\tA\t\t;operace OK\n\tRET\t\t\t;hotovo\n\n;****', '\tXOR\tA\t\t;operace OK\n\tRET\t\t\t;hotovo\n'
                '.9.rk_seek:\n\tpop\tbc\t\t;SAPIemu: chyba, zasobnik v poradku\n\tret\n\n;****', 1)
assert '.9.rk_seek:' in dt
# header as FORMRKD.COM: only the track, low byte first
n = 0
for a, b in (('\tWDATA\tD\t\t', '\tWDATA\tE\t\t'), ('\tWDATA\tE\t\t', '\tWDATA\tD\t\t')):
    pass
w = re.search(r'\tWDATA\tD\t\t;[^\n]*\n\tWDATA\tE\t\t;[^\n]*\n', dt)
assert w, 'WDATA'
dt = dt.replace(w.group(0), '\tWDATA\tE\t\t;SAPIemu: jako FORMRKD, nizsi byte zahlavi prvni\n\tWDATA\tD\n')
r = re.search(r'(\tRDATA\t\t\t;[^\n]*\n)\tCP\tD\t\t;([^\n]*)\n(\tCHECK\tZ,MARK_ERR[^\n]*\n;=+\n\tRDATA\t\t\t;[^\n]*\n)\tCP\tE\t\t;([^\n]*)\n', dt)
assert r, 'RDATA'
dt = dt.replace(r.group(0), r.group(1) + '\tCP\tE\t\t;SAPIemu: jako FORMRKD, nizsi byte prvni\n' + r.group(3) + '\tCP\tD\n')
pm = re.search(r'PREP_MARK:\n.*?\n\tSET\t7,D[^\n]*\n\tRET[^\n]*\n', dt, re.S)
assert pm, 'PREP_MARK'
dt = dt.replace(pm.group(0), r"""PREP_MARK:
; SAPIemu: jako ovladac ve FORMRKD.COM (J. Biba): zahlavi je jen stopa * 32,
; bez sektoru, media a priznaku 512 B (RKD.MAC 87-02-13 je pridava)
	PUSH	BC
	LD	B,3
	LD	C,E
	LD	E,0
	OR	A
.1.PREP_MARK:
	RR	D
	RR	E
	DJNZ	.1.PREP_MARK
	POP	BC
	RET
""")
out += [';*******************************', 'SUBTTL OVLADAC RKD-1 (RKD.MAC 87-02-13 Tesla ELSTROJ & JaMeSoft, upraveny)',
        ';*******************************']
out += drv_defs
out += dt.split('\n')
out += r""";*******************************
; fyzicke cteni / zapis sektoru 512 B do/z buf_8
; I: D = medium 0-3 (D: az G:), E = stopa, C = sektor 0-11
; O: A=0,Z OK; A=1,NZ chyba
rd_RKD::
	XOR	A		;cteni
	JR	RKD_IO
wr_RKD::
	LD	A,1		;zapis
RKD_IO::
	PUSH	HL
	PUSH	DE
	PUSH	BC
	PUSH	AF
	LD	A,D
	CALL	RK_SEL		;medium
	LD	A,E
	CALL	RK_SEEK		;najeti, dokonci se v RK_RW
	JR	NZ,RKD_ER
	POP	AF
	POP	BC
	PUSH	BC
	LD	D,E		;stopa
	LD	E,C		;sektor
	LD	HL,buf_8
	LD	BC,512
	CALL	RK_RW
	JR	NZ,RKD_E1
	POP	BC
	POP	DE
	POP	HL
	XOR	A
	RET
RKD_ER::
	POP	AF
RKD_E1::
	POP	BC
	POP	DE
	POP	HL
	LD	A,1
	OR	A
	RET
""".rstrip('\n').split('\n')
i_wboot_hdr = find(orig, r';\*+$', find(orig, r'DTL::'))
i_vars = find(orig, r'RESTAB::')
tail = block(orig, i_wboot_hdr, i_vars)
tail = [l.replace('\tCALL\tINIT_MFC', '\tCALL\tRK_INIT\t\t;RKD-1') for l in tail]
assert any('RK_INIT' in l for l in tail)
out += tail
i_stack = find(orig, r'\s+ds\s+50')
i_signon = find(orig, r'SIGNON::')
out += block(orig, i_stack, i_signon)
out += ['M_SH::\tDS\t1\t\t;RKD: posunute cislo media', 'track::\tDS\t1\t\t;RKD: stopa pro restore pri chybe']
out += r"""SIGNON::
	DB 0DH,0AH,'JPR-1Z 62K CP/M V2.2'
	DB 0DH,0AH,'BIOS-3K RKD SAPIemu (V261211JB)'
	DB 0DH,0AH,'A,B: RPD1; C: ZRMD1/4 MB+SYS'
	DB 0DH,0AH,'D-G: RKD1 CM5400 12x512'
	DB 0DH,0AH,'CON: AND1Z + C259.11'
	DB 0DH,0AH,'LST: CENTRONIX'
	DB 0""".split('\n')
i_dead = find(orig, r';-+$', i_signon)
i_chk = find(orig, r'CHK_A::')
out += block(orig, i_dead, i_chk)
out += r"""CHK_A::
	DS	16
CHK_B::
	DS	16
CHK_C::
CHK_D::
CHK_E::
	DS	0
CHK_F::
	DS	64
CHK_G::
	DS	64
ALL_A::
	DS	32
ALL_B::
	DS	32
ALL_C::
	DS	128
ALL_D::
	DS	77
ALL_E::
	DS	77
ALL_F::
	DS	77
ALL_G::
	DS	77

DIRBUF::
	DS	128

buf_8::	ds	512
	ds	1
BIOS_END::

	.DEPHASE

	END	BIOS""".split('\n')
open(sys.argv[3], 'wb').write(('\r\n'.join(out) + '\r\n\x1a').encode('latin-1'))
print('lines', len(out))
