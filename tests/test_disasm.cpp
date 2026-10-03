// Z80 disassembler: prefixes, displacements, relative jumps, lengths.
#include <string.h>
#include "core/disasm.h"
#include "test_util.h"

struct Case {
    uint16_t addr;
    uint8_t bytes[4];
    int len;
    const char* text;
};

static const Case k_cases[] = {
    { 0x0000, { 0x00 }, 1, "NOP" },
    { 0x0000, { 0x3E, 0x41 }, 2, "LD A,41h" },
    { 0x0000, { 0xC3, 0x00, 0xF0 }, 3, "JP 0F000h" },
    { 0x0100, { 0x18, 0xFE }, 2, "JR 0100h" },
    { 0x0100, { 0x10, 0x05 }, 2, "DJNZ 0107h" },
    { 0x0000, { 0xCD, 0x84, 0xF7 }, 3, "CALL 0F784h" },
    { 0x0000, { 0xFF }, 1, "RST 38h" },
    { 0x0000, { 0xD3, 0x10 }, 2, "OUT (10h),A" },
    { 0x0000, { 0xDB, 0xA0 }, 2, "IN A,(0A0h)" },
    { 0x0000, { 0x76 }, 1, "HALT" },
    { 0x0000, { 0x08 }, 1, "EX AF,AF'" },
    { 0x0000, { 0xCB, 0x7F }, 2, "BIT 7,A" },
    { 0x0000, { 0xCB, 0x06 }, 2, "RLC (HL)" },
    { 0x0000, { 0xED, 0xB0 }, 2, "LDIR" },
    { 0x0000, { 0xED, 0x5E }, 2, "IM 2" },
    { 0x0000, { 0xED, 0x4B, 0x34, 0x12 }, 4, "LD BC,(1234h)" },
    { 0x0000, { 0xED, 0x78 }, 2, "IN A,(C)" },
    { 0x0000, { 0xED, 0x71 }, 2, "OUT (C),0" },
    { 0x0000, { 0xDD, 0x7E, 0x05 }, 3, "LD A,(IX+05h)" },
    { 0x0000, { 0xFD, 0x36, 0xFE, 0x41 }, 4, "LD (IY-02h),41h" },
    { 0x0000, { 0xDD, 0x66, 0x01 }, 3, "LD H,(IX+01h)" },
    { 0x0000, { 0xDD, 0x26, 0x12 }, 3, "LD IXH,12h" },
    { 0x0000, { 0xDD, 0x21, 0x00, 0x80 }, 4, "LD IX,8000h" },
    { 0x0000, { 0xDD, 0xE9 }, 2, "JP (IX)" },
    { 0x0000, { 0xFD, 0x09 }, 2, "ADD IY,BC" },
    { 0x0000, { 0xDD, 0xCB, 0x03, 0xC6 }, 4, "SET 0,(IX+03h)" },
    { 0x0000, { 0xFD, 0xCB, 0xFF, 0x46 }, 4, "BIT 0,(IY-01h)" },
    { 0x0000, { 0x22, 0x02, 0xFD }, 3, "LD (0FD02h),HL" },
};

int main(void)
{
    for (const Case& c : k_cases) {
        uint8_t mem[4] = { 0, 0, 0, 0 };
        memcpy(mem, c.bytes, 4);
        // the reader maps addr..addr+3 onto the 4 test bytes
        struct Ctx { const uint8_t* b; uint16_t base; } ctx = { mem, c.addr };
        DisasmResult d = z80_disasm(c.addr, [](void* u, uint16_t a) -> uint8_t {
            Ctx* x = (Ctx*)u;
            uint16_t off = (uint16_t)(a - x->base);
            return off < 4 ? x->b[off] : 0;
        }, &ctx);
        if (d.text != c.text || d.length != c.len)
            fprintf(stderr, "expected '%s' (%d) got '%s' (%d)\n", c.text, c.len, d.text.c_str(), d.length);
        CHECK(d.text == c.text);
        CHECK_EQ(d.length, c.len);
    }
    // call_like detection for step over
    uint8_t call[3] = { 0xCD, 0x00, 0x10 };
    struct Ctx2 { const uint8_t* b; } c2 = { call };
    DisasmResult d = z80_disasm(0, [](void* u, uint16_t a) -> uint8_t { return a < 3 ? ((Ctx2*)u)->b[a] : 0; }, &c2);
    CHECK(d.call_like);
    CHECK(d.has_target && d.target == 0x1000);
    return TEST_RESULT();
}
