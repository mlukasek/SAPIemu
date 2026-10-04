#!/usr/bin/env python3
"""Small two pass Intel 8080 assembler for test programs (CP/M .COM, Intel HEX output).

Usage: python tools/asm8080.py SOURCE.ASM OUTPUT.HEX [OUTPUT.COM]

Syntax as M80/ASM: labels with or without ':', ';' comments, ORG, EQU, DB (numbers, 'text'),
DW, DS, END. Numbers: 123, 0FFH, 1010B, 'c', $ = current address; expressions with + - * /
and parentheses (evaluated left to right with the usual precedence).
"""
import re
import sys

REG = {"B": 0, "C": 1, "D": 2, "E": 3, "H": 4, "L": 5, "M": 6, "A": 7}
RP = {"B": 0, "D": 1, "H": 2, "SP": 3}
RP_PUSH = {"B": 0, "D": 1, "H": 2, "PSW": 3}
IMPLIED = {"NOP": 0x00, "RLC": 0x07, "RRC": 0x0F, "RAL": 0x17, "RAR": 0x1F, "DAA": 0x27, "CMA": 0x2F,
           "STC": 0x37, "CMC": 0x3F, "HLT": 0x76, "RET": 0xC9, "XCHG": 0xEB, "XTHL": 0xE3, "SPHL": 0xF9,
           "PCHL": 0xE9, "DI": 0xF3, "EI": 0xFB, "RNZ": 0xC0, "RZ": 0xC8, "RNC": 0xD0, "RC": 0xD8,
           "RPO": 0xE0, "RPE": 0xE8, "RP": 0xF0, "RM": 0xF8}
ALU_R = {"ADD": 0x80, "ADC": 0x88, "SUB": 0x90, "SBB": 0x98, "ANA": 0xA0, "XRA": 0xA8, "ORA": 0xB0, "CMP": 0xB8}
ALU_I = {"ADI": 0xC6, "ACI": 0xCE, "SUI": 0xD6, "SBI": 0xDE, "ANI": 0xE6, "XRI": 0xEE, "ORI": 0xF6, "CPI": 0xFE}
ADDR16 = {"JMP": 0xC3, "JNZ": 0xC2, "JZ": 0xCA, "JNC": 0xD2, "JC": 0xDA, "JPO": 0xE2, "JPE": 0xEA, "JP": 0xF2,
          "JM": 0xFA, "CALL": 0xCD, "CNZ": 0xC4, "CZ": 0xCC, "CNC": 0xD4, "CC": 0xDC, "LDA": 0x3A, "STA": 0x32,
          "LHLD": 0x2A, "SHLD": 0x22}


class AsmError(Exception):
    pass


def split_operands(s):
    out, cur, q = [], "", False
    for ch in s:
        if ch == "'" :
            q = not q
        if ch == "," and not q:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def value(expr, syms, pc, final):
    e = expr.strip()
    toks = re.findall(r"'[^']*'|[A-Za-z_$?@][\w$?@]*|\d[0-9A-Fa-f]*[HhBbDdOoQq]?|[-+*/()]", e)
    if "".join(toks).replace(" ", "") != e.replace(" ", ""):
        raise AsmError("bad expression: " + expr)
    py = []
    for t in toks:
        if t[0] == "'":
            if len(t) != 3:
                raise AsmError("character constant must be one character: " + t)
            py.append(str(ord(t[1])))
        elif t == "$":
            py.append(str(pc))
        elif t[0].isdigit():
            u = t.upper()
            if u.endswith("H"):
                py.append(str(int(u[:-1], 16)))
            elif u.endswith("B") and all(c in "01" for c in u[:-1]):
                py.append(str(int(u[:-1], 2)))
            elif u.endswith(("O", "Q")):
                py.append(str(int(u[:-1], 8)))
            else:
                py.append(str(int(u.rstrip("D"))))
        elif t in "+-*/()":
            py.append("//" if t == "/" else t)
        else:
            u = t.upper()
            if u in syms:
                py.append(str(syms[u]))
            elif final:
                raise AsmError("undefined symbol " + t)
            else:
                py.append("0")
    return eval("".join(py)) & 0xFFFF


def assemble(lines):
    syms = {}
    for final in (False, True):
        pc, out, origin = 0x100, {}, None
        for no, raw in enumerate(lines, 1):
            line = raw
            # strip the comment (not inside quotes)
            q = False
            for i, ch in enumerate(line):
                if ch == "'":
                    q = not q
                if ch == ";" and not q:
                    line = line[:i]
                    break
            if not line.strip():
                continue
            try:
                m = re.match(r"^([A-Za-z_$?@][\w$?@]*):?\s*(.*)$", line) if not line[0].isspace() else None
                label, rest = (m.group(1).upper(), m.group(2)) if m else (None, line.strip())
                parts = rest.split(None, 1)
                op = parts[0].upper() if parts else ""
                args = split_operands(parts[1]) if len(parts) > 1 else []
                if op == "EQU":
                    syms[label] = value(args[0], syms, pc, final)
                    continue
                if label:
                    if not final and label in syms and syms[label] != pc:
                        raise AsmError("label defined twice: " + label)
                    syms[label] = pc
                if not op:
                    continue
                code = []
                if op == "ORG":
                    pc = value(args[0], syms, pc, final)
                    continue
                elif op == "END":
                    break
                elif op == "DB":
                    for a in args:
                        if a.startswith("'") and a.endswith("'") and len(a) > 3:
                            code += [ord(c) for c in a[1:-1]]
                        else:
                            code.append(value(a, syms, pc, final) & 0xFF)
                elif op == "DW":
                    for a in args:
                        v = value(a, syms, pc, final)
                        code += [v & 0xFF, v >> 8]
                elif op == "DS":
                    pc += value(args[0], syms, pc, final)
                    continue
                elif op in IMPLIED:
                    code = [IMPLIED[op]]
                elif op == "MOV":
                    code = [0x40 | REG[args[0].upper()] << 3 | REG[args[1].upper()]]
                elif op == "MVI":
                    code = [0x06 | REG[args[0].upper()] << 3, value(args[1], syms, pc, final) & 0xFF]
                elif op in ("INR", "DCR"):
                    code = [(0x04 if op == "INR" else 0x05) | REG[args[0].upper()] << 3]
                elif op in ALU_R:
                    code = [ALU_R[op] | REG[args[0].upper()]]
                elif op in ALU_I:
                    code = [ALU_I[op], value(args[0], syms, pc, final) & 0xFF]
                elif op in ("LXI", "INX", "DCX", "DAD"):
                    r = RP[args[0].upper()]
                    base = {"LXI": 0x01, "INX": 0x03, "DCX": 0x0B, "DAD": 0x09}[op]
                    code = [base | r << 4]
                    if op == "LXI":
                        v = value(args[1], syms, pc, final)
                        code += [v & 0xFF, v >> 8]
                elif op in ("PUSH", "POP"):
                    code = [(0xC5 if op == "PUSH" else 0xC1) | RP_PUSH[args[0].upper()] << 4]
                elif op in ("LDAX", "STAX"):
                    r = args[0].upper()
                    code = [{("LDAX", "B"): 0x0A, ("LDAX", "D"): 0x1A, ("STAX", "B"): 0x02, ("STAX", "D"): 0x12}[(op, r)]]
                elif op in ADDR16:
                    v = value(args[0], syms, pc, final)
                    code = [ADDR16[op], v & 0xFF, v >> 8]
                elif op in ("IN", "OUT"):
                    code = [0xDB if op == "IN" else 0xD3, value(args[0], syms, pc, final) & 0xFF]
                elif op == "RST":
                    code = [0xC7 | (value(args[0], syms, pc, final) & 7) << 3]
                else:
                    raise AsmError("unknown instruction " + op)
                for b in code:
                    out[pc] = b
                    pc += 1
            except (AsmError, KeyError, IndexError) as e:
                raise AsmError("line %d: %s: %s" % (no, raw.rstrip(), e))
    return out


def ihex(mem):
    lines, addrs = [], sorted(mem)
    i = 0
    while i < len(addrs):
        start = addrs[i]
        chunk = []
        while i < len(addrs) and addrs[i] == start + len(chunk) and len(chunk) < 16:
            chunk.append(mem[addrs[i]])
            i += 1
        rec = [len(chunk), start >> 8, start & 0xFF, 0] + chunk
        rec.append((-sum(rec)) & 0xFF)
        lines.append(":" + "".join("%02X" % b for b in rec))
    lines.append(":00000001FF")
    return "\r\n".join(lines) + "\r\n"


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    src = open(sys.argv[1], encoding="latin-1").read().splitlines()
    try:
        mem = assemble(src)
    except AsmError as e:
        print(sys.argv[1] + ": " + str(e))
        return 1
    open(sys.argv[2], "w", newline="").write(ihex(mem))
    if len(sys.argv) > 3:
        lo, hi = min(mem), max(mem)
        open(sys.argv[3], "wb").write(bytes(mem.get(a, 0) for a in range(lo, hi + 1)))
    lo, hi = min(mem), max(mem)
    print("%s: %04X-%04Xh, %d bytes, CP/M SAVE %d" % (sys.argv[2], lo, hi, hi - lo + 1, (hi - 0x100) // 256 + 1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
