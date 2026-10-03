// Table driven Z80 disassembler following "Decoding Z80 Opcodes" (x/y/z/p/q fields).
#include "core/disasm.h"

#include <stdio.h>

static const char* const k_r[8] = { "B", "C", "D", "E", "H", "L", "(HL)", "A" };
static const char* const k_rp[4] = { "BC", "DE", "HL", "SP" };
static const char* const k_rp2[4] = { "BC", "DE", "HL", "AF" };
static const char* const k_cc[8] = { "NZ", "Z", "NC", "C", "PO", "PE", "P", "M" };
static const char* const k_alu[8] = { "ADD A,", "ADC A,", "SUB ", "SBC A,", "AND ", "XOR ", "OR ", "CP " };
static const char* const k_rot[8] = { "RLC", "RRC", "RL", "RR", "SLA", "SRA", "SLL", "SRL" };
static const char* const k_im[8] = { "0", "0", "1", "2", "0", "0", "1", "2" };
static const char* const k_bli[4][4] = {
    { "LDI", "CPI", "INI", "OUTI" },
    { "LDD", "CPD", "IND", "OUTD" },
    { "LDIR", "CPIR", "INIR", "OTIR" },
    { "LDDR", "CPDR", "INDR", "OTDR" },
};

std::string hex8(uint8_t v)
{
    char b[8];
    snprintf(b, sizeof(b), "%s%02Xh", v >= 0xA0 ? "0" : "", v);
    return b;
}

std::string hex16(uint16_t v)
{
    char b[8];
    snprintf(b, sizeof(b), "%s%04Xh", v >= 0xA000 ? "0" : "", v);
    return b;
}

namespace {

struct Dis {
    disasm_read_fn read;
    void* user;
    uint16_t start;
    uint16_t pc;
    int idx = 0;            // 0 = HL, 1 = IX, 2 = IY
    bool have_disp = false;
    int8_t disp = 0;
    DisasmResult res;

    uint8_t fetch() { return read(user, pc++); }
    uint16_t fetch16() { uint8_t l = fetch(); uint8_t h = fetch(); return (uint16_t)(l | (h << 8)); }

    const char* hl() const { return idx == 0 ? "HL" : (idx == 1 ? "IX" : "IY"); }

    std::string mem_hl()
    {
        if (idx == 0) return "(HL)";
        if (!have_disp) { disp = (int8_t)fetch(); have_disp = true; }
        char b[16];
        int d = disp;
        snprintf(b, sizeof(b), "(%s%c%02Xh)", hl(), d < 0 ? '-' : '+', d < 0 ? -d : d);
        return b;
    }

    // register operand; `plain_hl` keeps H/L unprefixed (instruction also uses (IX+d))
    std::string reg(int i, bool plain_hl = false)
    {
        if (i == 6) return mem_hl();
        if (idx && !plain_hl && (i == 4 || i == 5))
            return std::string(idx == 1 ? "IX" : "IY") + (i == 4 ? "H" : "L");
        return k_r[i];
    }

    std::string rp(int p) { return p == 2 ? hl() : k_rp[p]; }
    std::string rp2(int p) { return p == 2 ? hl() : k_rp2[p]; }

    std::string rel()
    {
        int8_t e = (int8_t)fetch();
        uint16_t t = (uint16_t)(pc + e);
        res.has_target = true;
        res.target = t;
        return hex16(t);
    }

    std::string abs16(bool is_target)
    {
        uint16_t v = fetch16();
        if (is_target) { res.has_target = true; res.target = v; }
        return hex16(v);
    }

    std::string decode_cb(void);
    std::string decode_ed(void);
    std::string decode_main(uint8_t op);
};

std::string Dis::decode_cb(void)
{
    uint8_t op;
    std::string m;
    if (idx) {
        // DD CB d op
        disp = (int8_t)fetch();
        have_disp = true;
        op = fetch();
        m = mem_hl();
    } else {
        op = fetch();
    }
    int x = op >> 6, y = (op >> 3) & 7, z = op & 7;
    std::string operand = idx ? m : reg(z);
    std::string extra = (idx && z != 6) ? std::string(",") + k_r[z] : "";   // undocumented copy
    switch (x) {
    case 0: return std::string(k_rot[y]) + " " + operand + extra;
    case 1: return "BIT " + std::to_string(y) + "," + operand;
    case 2: return "RES " + std::to_string(y) + "," + operand + extra;
    default: return "SET " + std::to_string(y) + "," + operand + extra;
    }
}

std::string Dis::decode_ed(void)
{
    uint8_t op = fetch();
    int x = op >> 6, y = (op >> 3) & 7, z = op & 7, p = y >> 1, q = y & 1;
    if (x == 1) {
        switch (z) {
        case 0: return y == 6 ? "IN (C)" : std::string("IN ") + k_r[y] + ",(C)";
        case 1: return y == 6 ? "OUT (C),0" : std::string("OUT (C),") + k_r[y];
        case 2: return std::string(q ? "ADC HL," : "SBC HL,") + k_rp[p];
        case 3:
            if (q == 0) return "LD (" + abs16(false) + ")," + k_rp[p];
            return std::string("LD ") + k_rp[p] + ",(" + abs16(false) + ")";
        case 4: return "NEG";
        case 5: return y == 1 ? "RETI" : "RETN";
        case 6: return std::string("IM ") + k_im[y];
        default: {
            static const char* const k7[8] = { "LD I,A", "LD R,A", "LD A,I", "LD A,R", "RRD", "RLD", "NOP*", "NOP*" };
            return k7[y];
        }
        }
    }
    if (x == 2 && z <= 3 && y >= 4) {
        if (y >= 6) res.call_like = true;   // repeating block instruction
        return k_bli[y - 4][z];
    }
    return "DB 0EDh," + hex8(op);
}

std::string Dis::decode_main(uint8_t op)
{
    int x = op >> 6, y = (op >> 3) & 7, z = op & 7, p = y >> 1, q = y & 1;
    switch (x) {
    case 0:
        switch (z) {
        case 0:
            if (y == 0) return "NOP";
            if (y == 1) return "EX AF,AF'";
            if (y == 2) { res.call_like = true; return "DJNZ " + rel(); }
            if (y == 3) return "JR " + rel();
            return std::string("JR ") + k_cc[y - 4] + "," + rel();
        case 1:
            if (q == 0) return "LD " + rp(p) + "," + abs16(false);
            return std::string("ADD ") + hl() + "," + rp(p);
        case 2:
            switch (y) {
            case 0: return "LD (BC),A";
            case 1: return "LD A,(BC)";
            case 2: return "LD (DE),A";
            case 3: return "LD A,(DE)";
            case 4: return "LD (" + abs16(false) + ")," + hl();
            case 5: return std::string("LD ") + hl() + ",(" + abs16(false) + ")";
            case 6: return "LD (" + abs16(false) + "),A";
            default: return "LD A,(" + abs16(false) + ")";
            }
        case 3: return std::string(q ? "DEC " : "INC ") + rp(p);
        case 4: return "INC " + reg(y);
        case 5: return "DEC " + reg(y);
        case 6: {
            std::string dst = reg(y);   // fetches the displacement before n
            return "LD " + dst + "," + hex8(fetch());
        }
        default: {
            static const char* const k7[8] = { "RLCA", "RRCA", "RLA", "RRA", "DAA", "CPL", "SCF", "CCF" };
            return k7[y];
        }
        }
    case 1:
        if (y == 6 && z == 6) return "HALT";
        {
            bool mem = (y == 6 || z == 6);
            std::string dst = reg(y, mem);
            std::string src = reg(z, mem);
            return "LD " + dst + "," + src;
        }
    case 2:
        return std::string(k_alu[y]) + reg(z);
    default:
        switch (z) {
        case 0: return std::string("RET ") + k_cc[y];
        case 1:
            if (q == 0) return "POP " + rp2(p);
            if (p == 0) return "RET";
            if (p == 1) return "EXX";
            if (p == 2) return std::string("JP (") + hl() + ")";
            return std::string("LD SP,") + hl();
        case 2: return std::string("JP ") + k_cc[y] + "," + abs16(true);
        case 3:
            switch (y) {
            case 0: return "JP " + abs16(true);
            case 1: return decode_cb();
            case 2: return "OUT (" + hex8(fetch()) + "),A";
            case 3: return "IN A,(" + hex8(fetch()) + ")";
            case 4: return std::string("EX (SP),") + hl();
            case 5: return "EX DE,HL";
            case 6: return "DI";
            default: return "EI";
            }
        case 4: res.call_like = true; return std::string("CALL ") + k_cc[y] + "," + abs16(true);
        case 5:
            if (q == 0) return "PUSH " + rp2(p);
            if (p == 0) { res.call_like = true; return "CALL " + abs16(true); }
            return "?";   // prefixes are handled by the caller
        case 6: return std::string(k_alu[y]) + hex8(fetch());
        default:
            res.call_like = true;
            res.has_target = true;
            res.target = (uint16_t)(y * 8);
            return "RST " + hex8((uint8_t)(y * 8));
        }
    }
}

} // namespace

DisasmResult z80_disasm(uint16_t addr, disasm_read_fn read, void* user)
{
    Dis d;
    d.read = read;
    d.user = user;
    d.start = addr;
    d.pc = addr;
    d.res.addr = addr;
    d.res.call_like = false;
    d.res.has_target = false;
    d.res.target = 0;

    uint8_t op = d.fetch();
    std::string text;
    if (op == 0xDD || op == 0xFD) {
        uint8_t next = read(user, d.pc);
        if (next == 0xDD || next == 0xFD || next == 0xED) {
            text = "NOP*";   // prefix without effect
        } else {
            d.idx = (op == 0xDD) ? 1 : 2;
            op = d.fetch();
            text = (op == 0xCB) ? d.decode_cb() : d.decode_main(op);
        }
    } else if (op == 0xED) {
        text = d.decode_ed();
    } else if (op == 0xCB) {
        text = d.decode_cb();
    } else {
        text = d.decode_main(op);
    }
    d.res.text = text;
    d.res.length = (int)(uint16_t)(d.pc - addr);
    return d.res;
}
