#pragma once
// SAPI-1 system bus (ARB-1 / ARB-1C). 8080-style bus: /MR, /MW, /IOR, /IOW,
// /INTA, RDY (wait), /INT0, /INT1, MAP1/MAP2. I/O addresses are A0-A7.
#include <stdint.h>
#include <string>
#include <vector>

class Card;

// Watchpoint flags (Bus::mem_watch / io_watch)
enum WatchFlags { WATCH_READ = 1, WATCH_WRITE = 2 };

struct IoLogEntry {
    uint64_t time;      // CPU cycles
    uint16_t pc;        // instruction that did the access
    uint8_t port;
    uint8_t value;
    bool write;
};

struct Bus {
    std::vector<Card*> cards;   // in slot order (not owned)
    uint64_t now = 0;           // CPU clock cycles since power-on
    uint32_t cpu_hz = 2000000;  // CPU clock, used by cards to convert time
    int wait = 0;               // wait states requested during the current access (RDY low)
    int map = 0;                // current MAP1/MAP2 levels (MapLine mask)
    bool warn_conflicts = true;
    uint64_t conflicts = 0;
    uint16_t pc = 0;            // address of the current instruction (set by the CPU card)

    // debugging
    uint8_t mem_watch[65536];   // WatchFlags per address
    uint8_t io_watch[256];      // WatchFlags per port
    bool any_mem_watch = false;
    bool break_request = false; // set by a watchpoint, consumed by the CPU card
    std::string break_reason;
    bool io_log_enabled = false;
    std::vector<IoLogEntry> io_log;   // ring buffer
    size_t io_log_next = 0;
    static const size_t IO_LOG_SIZE = 4096;

    Bus();
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

    void set_mem_watch(uint16_t addr, int flags);
    void io_log_copy(std::vector<IoLogEntry>& out, size_t max) const;

    double seconds(void) const { return (double)now / (double)cpu_hz; }

private:
    void watch_hit(const char* what, unsigned addr, uint8_t value);
    void log_io(uint8_t port, uint8_t value, bool write);
};
