#pragma once
// SAPI-1 system bus (ARB-1 / ARB-1C). 8080-style bus: /MR, /MW, /IOR, /IOW,
// /INTA, RDY (wait), /INT0, /INT1, MAP1/MAP2. I/O addresses are A0-A7.
#include <stdint.h>
#include <string>
#include <vector>

class Card;

// Watchpoint flags (Bus::mem_watch / io_watch)
enum WatchFlags { WATCH_READ = 1, WATCH_WRITE = 2 };

// Host game controller state (Bus::joystick), 1 = pressed. The order is that of the
// MPH-1V joystick port (D0..D7).
enum HostJoy {
    JOY_DOWN = 0x01, JOY_UP = 0x02, JOY_RIGHT = 0x04, JOY_LEFT = 0x08,
    JOY_FIRE1 = 0x10,   // FL
    JOY_FIRE2 = 0x20,   // FR
    JOY_BUTTON3 = 0x40, // L
    JOY_BUTTON4 = 0x80, // R
};

// Host mouse buttons (Bus::mouse_buttons), 1 = pressed.
enum MouseButton { MOUSE_LEFT = 1, MOUSE_RIGHT = 2 };

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
    uint32_t hold = 0;          // cycles a DMA controller held the bus (/HOLD); the CPU card adds
                                // them to the time before its next instruction
    int map = 0;                // current MAP1/MAP2 levels (MapLine mask)
    bool warn_conflicts = true;
    uint64_t conflicts = 0;
    uint16_t pc = 0;            // address of the current instruction (set by the CPU card)

    // sound: the sources (MPH-1V, the keyboard beeper) add into the samples by their index
    // (audio_index of the bus time, 14.31818 MHz / 288 = 49716 per second); after each run
    // audio_collect() mixes them into `audio` (mono) for the frontend or the CLI. With
    // audio_cap = 0 nobody listens and the samples are dropped; past audio_cap the oldest go.
    std::vector<int16_t> audio;
    uint32_t audio_rate = 49716;
    size_t audio_cap = 0;
    uint64_t audio_index(uint64_t t) const;    // the sample a bus time falls into
    double audio_pos(uint64_t t) const;        // the same with the fraction
    void audio_add(uint64_t index, int value);
    void audio_collect(void);                  // brings the cards up to now, mixes up to now
    void audio_restart(void);                  // after loading a state
    // recording (sapiemu-cli --audio-wav, MCP audio_record), independent of the player
    bool audio_recording = false;
    std::vector<int16_t> audio_rec;
    void audio_put(const int16_t* s, size_t n);
    void audio_record(bool on);   // starts a new recording / stops it (audio_rec is kept)
    // host game controllers (HostJoy bits) for the joystick ports of the cards
    uint8_t joystick[2] = { 0, 0 };
    // host mouse for the mouse ports: movement still to be sent (counts, +x right, +y down; the
    // card takes it at the rate of a real mouse), buttons (MouseButton)
    int mouse_dx = 0, mouse_dy = 0;
    uint8_t mouse_buttons = 0;
    bool mouse_connected = false;   // a card has a mouse on a connector: the GUI captures the host mouse

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
    void state(class StateIO& io);   // time, MAP lines, conflicts
    void io_log_copy(std::vector<IoLogEntry>& out, size_t max) const;

    double seconds(void) const { return (double)now / (double)cpu_hz; }

private:
    std::vector<int32_t> mix;   // samples from mix_base on, not mixed out yet
    uint64_t mix_base = 0;
    void watch_hit(const char* what, unsigned addr, uint8_t value);
    void log_io(uint8_t port, uint8_t value, bool write);
};
