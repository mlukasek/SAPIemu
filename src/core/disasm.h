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

// Hex formatting helpers used by the debugger UI ("0F3h" style).
std::string hex8(uint8_t v);
std::string hex16(uint16_t v);
