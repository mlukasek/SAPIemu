#pragma once
// SAPI-1 system bus (ARB-1 / ARB-1C). 8080-style bus: /MR, /MW, /IOR, /IOW,
// /INTA, RDY (wait), /INT0, /INT1, MAP1/MAP2. I/O addresses are A0-A7.
#include <stdint.h>
#include <vector>

class Card;

struct Bus {
    std::vector<Card*> cards;   // in slot order (not owned)
    uint64_t now = 0;           // CPU clock cycles since power-on
    uint32_t cpu_hz = 2000000;  // CPU clock, used by cards to convert time
    int wait = 0;               // wait states requested during the current access (RDY low)
    int map = 0;                // current MAP1/MAP2 levels (MapLine mask)
    bool warn_conflicts = true;
    uint64_t conflicts = 0;

    uint8_t mem_read(uint16_t addr);
    void mem_write(uint16_t addr, uint8_t data);
    uint8_t io_read(uint8_t port);
    void io_write(uint8_t port, uint8_t data);
    uint8_t inta(void);
    uint8_t mem_peek(uint16_t addr);

    void reset(void);
    void advance_all(void);
    int irq_lines(void);        // OR of all cards' interrupt requests
    void update_map(void);

    double seconds(void) const { return (double)now / (double)cpu_hz; }
};
