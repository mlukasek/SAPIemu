// Smoke test for the vendored Z80 core: run a tiny program and check T-state timing.
#include <string.h>
#include "z80.h"
#include "test_util.h"

int main(void)
{
    static uint8_t mem[65536];
    memset(mem, 0, sizeof(mem));
    // LD A,42h ; LD (8000h),A ; OUT (01h),A ; HALT
    const uint8_t prog[] = { 0x3E, 0x42, 0x32, 0x00, 0x80, 0xD3, 0x01, 0x76 };
    memcpy(mem, prog, sizeof(prog));

    z80_t cpu;
    uint64_t pins = z80_init(&cpu);
    int out_port = -1, out_val = -1;
    long ticks = 0;
    while (!(pins & Z80_HALT) && ticks < 1000) {
        pins = z80_tick(&cpu, pins);
        ticks++;
        if (pins & Z80_MREQ) {
            uint16_t a = Z80_GET_ADDR(pins);
            // Z80_SET_DATA expands to a {} block, braces are required before else
            if (pins & Z80_RD) { Z80_SET_DATA(pins, mem[a]); }
            else if (pins & Z80_WR) { mem[a] = Z80_GET_DATA(pins); }
        } else if ((pins & Z80_IORQ) && (pins & Z80_WR)) {
            out_port = Z80_GET_ADDR(pins) & 0xFF;
            out_val = Z80_GET_DATA(pins);
        }
    }
    CHECK_EQ(mem[0x8000], 0x42);
    CHECK_EQ(out_port, 0x01);
    CHECK_EQ(out_val, 0x42);
    // 7 (LD A,n) + 13 (LD (nn),A) + 11 (OUT) + 4 (HALT M1) = 35 T-states
    CHECK(ticks >= 35 && ticks <= 36);
    printf("ticks=%ld\n", ticks);
    return TEST_RESULT();
}
