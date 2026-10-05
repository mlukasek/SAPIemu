#pragma once
// Intel 8257 DMA controller (KR580IK57), as on the FDC-1A board.
//
// Registers (A3-A0): 0/1 channel 0 address/count, 2/3 channel 1, 4/5 channel 2, 6/7 channel 3,
// 8 mode set (write) / status (read). Addresses and counts are written and read low byte
// first; the first/last flip-flop is cleared by writing the mode register.
// Count: bits 13-0 = number of transfers - 1, bits 15-14 the cycle: 00 verify, 01 write
// (I/O -> memory), 10 read (memory -> I/O).
// Mode: bits 3-0 enable channels 3-0, 4 rotating priority, 5 extended write, 6 TC stop
// (a channel is disabled after its terminal count), 7 autoload (channel 2 reloads from 3).
// Status: bits 3-0 TC reached on channels 3-0 (cleared by reading), 4 update flag.
#include <stdint.h>

class StateIO;

struct I8257 {
    enum Cycle { VERIFY = 0, WRITE = 1, READ = 2 };   // WRITE = to memory

    struct Channel {
        uint16_t addr = 0;
        uint16_t count = 0;   // with the cycle bits 15-14
    };
    Channel ch[4];
    uint8_t mode = 0;
    uint8_t status = 0;
    bool flip = false;    // first/last flip-flop: false = next byte is the low one

    void reset(void);     // /RESET: mode 0 (all channels disabled), flip-flop cleared
    uint8_t read(int reg);
    void write(int reg, uint8_t value);

    bool enabled(int c) const { return (mode >> c) & 1; }
    int cycle(int c) const { return ch[c].count >> 14; }
    // One DMA cycle on channel c (DREQ answered): the memory address in `addr`; `tc` is set
    // when this was the last transfer (TC output during the cycle). false: the channel is
    // disabled, no DACK.
    bool transfer(int c, uint16_t& addr, bool& tc);
    void state(StateIO& io);
};
