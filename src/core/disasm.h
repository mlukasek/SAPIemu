#pragma once
// Z80 disassembler (Zilog mnemonics, hex numbers with 'h' suffix as in M80).
#include <stdint.h>
#include <string>

typedef uint8_t (*disasm_read_fn)(void* user, uint16_t addr);

struct DisasmResult {
    uint16_t addr;
    int length;             // 1..4 bytes
    std::string text;       // e.g. "LD A,(IX+05h)"
    bool call_like;         // CALL / RST / block repeat: "step over" runs to addr+length
    bool has_target;        // JP/JR/CALL/DJNZ/RST with a known target address
    uint16_t target;
};

DisasmResult z80_disasm(uint16_t addr, disasm_read_fn read, void* user);
// 8080 / 8085 code in Zilog mnemonics; the opcodes that are no Z80 instructions there: the 8080
// aliases (NOP, JP, RET, CALL) or the 8085 RIM, SIM and undocumented instructions (Intel names).
DisasmResult i8080_disasm(uint16_t addr, disasm_read_fn read, void* user, bool i8085);

// Hex formatting helpers used by the debugger UI ("0F3h" style).
std::string hex8(uint8_t v);
std::string hex16(uint16_t v);
