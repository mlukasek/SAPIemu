// ZEXDOC / ZEXALL instruction exerciser on the vendored Z80 core with a minimal
// CP/M BDOS (functions 2 and 9). Slow: built for release configurations only.
#include <stdio.h>
#include <string.h>
#include <string>
#include "core/util.h"
#include "z80.h"
#include "test_util.h"

static int run_zex(const char* file)
{
    static uint8_t mem[65536];
    memset(mem, 0, sizeof(mem));
    std::vector<uint8_t> com;
    if (!file_read_all(data_path(std::string("tests/data/zex/") + file), com)) {
        fprintf(stderr, "cannot read %s\n", file);
        return 1;
    }
    memcpy(&mem[0x100], com.data(), com.size());
    mem[0x0000] = 0x76;   // warm boot -> HALT
    mem[0x0005] = 0xD3;   // BDOS: OUT (0),A ; RET  (word at 0006h = C900h = top of TPA)
    mem[0x0006] = 0x00;
    mem[0x0007] = 0xC9;

    z80_t cpu;
    uint64_t pins = z80_init(&cpu);
    pins = z80_prefetch(&cpu, 0x0100);
    std::string out;
    uint64_t ticks = 0;
    while (!(pins & Z80_HALT)) {
        pins = z80_tick(&cpu, pins);
        ticks++;
        if (pins & Z80_MREQ) {
            uint16_t a = Z80_GET_ADDR(pins);
            if (pins & Z80_RD) { Z80_SET_DATA(pins, mem[a]); }
            else if (pins & Z80_WR) { mem[a] = Z80_GET_DATA(pins); }
        } else if ((pins & Z80_IORQ) && (pins & Z80_WR)) {
            if (cpu.c == 2) {
                out += (char)cpu.e;
                putchar(cpu.e);
            } else if (cpu.c == 9) {
                for (uint16_t a = cpu.de; mem[a] != '$'; a++) { out += (char)mem[a]; putchar(mem[a]); }
            }
            fflush(stdout);
        }
    }
    printf("\n%s: %llu T-states\n", file, (unsigned long long)ticks);
    bool ok = out.find("ERROR") == std::string::npos && out.find("Tests complete") != std::string::npos;
    return ok ? 0 : 1;
}

int main(int argc, char** argv)
{
    data_root_init();
    const char* which = argc > 1 ? argv[1] : "zexdoc.com";
    return run_zex(which);
}
