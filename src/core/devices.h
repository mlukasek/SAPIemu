#pragma once
// Interfaces between cards and the outside world: CPU, display, keyboard, serial line.
#include <stdint.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include "core/card.h"

struct CpuRegs {
    uint16_t af, bc, de, hl, ix, iy, sp, pc;
    uint16_t af2, bc2, de2, hl2;
    uint8_t i, r, im;
    bool iff1, iff2, halted;
};

// A keyboard attached to the parallel ports of a CPU card (connector X2 on JPR-1ZI).
// Host keys that do not produce text, independent of the frontend toolkit.
enum HostKey {
    HK_NONE = 0,
    HK_ENTER, HK_BACKSPACE, HK_TAB, HK_ESCAPE,
    HK_UP, HK_DOWN, HK_LEFT, HK_RIGHT, HK_HOME, HK_END, HK_PGUP, HK_PGDN, HK_INSERT, HK_DELETE,
    HK_PAUSE, HK_F1, HK_F2, HK_F3, HK_F4, HK_F6, HK_F7,
    HK_KP0, HK_KP1, HK_KP2, HK_KP3, HK_KP4, HK_KP5, HK_KP6, HK_KP7, HK_KP8, HK_KP9,
    HK_KP_DOT, HK_KP_ENTER,
};
enum HostMod { HM_SHIFT = 1, HM_CTRL = 2 };

class KeyboardDevice {
public:
    virtual ~KeyboardDevice() {}
    virtual const char* type_name() const = 0;
    virtual void reset() {}
    virtual void advance(uint64_t now) {}
    // Levels presented on the CPU card input ports (P0-IN = port 01h, P1-IN = port 02h).
    virtual uint8_t p0_in() { return 0xFF; }
    virtual uint8_t p1_in() { return 0xFF; }
    // P0-OUT latch written by the CPU (ACK, beeper, rows, LEDs...).
    virtual void p0_out(uint8_t value) {}
    // Host typed a Unicode character (text input, MCP); control characters
    // (CR, ESC, Ctrl+letter...) included. Characters the keyboard cannot
    // produce are ignored.
    virtual void type_char(uint32_t cp) = 0;
    // Host pressed a key without text (cursor keys, F-keys, keypad...). Returns
    // false when this keyboard has no such key.
    virtual bool host_key(int key, int mods) { return false; }
    virtual size_t pending() const = 0;
    virtual void clear() = 0;
    // Short state for the status bar (LEDs...), empty if none.
    virtual std::string status() const { return std::string(); }
};

// Base class of CPU cards (bus masters).
class CpuCard : public Card {
public:
    explicit CpuCard(const char* type_name) : Card(type_name) {}
    // Execute up to `cycles` CPU clock cycles. Returns the number executed; stops
    // early when a breakpoint is hit or a single step completes.
    virtual int run(int cycles) = 0;
    virtual void get_regs(CpuRegs& r) = 0;
    virtual void set_regs(const CpuRegs& r) = 0;
    // Memory as seen by the CPU (including on-card ROM), without side effects.
    virtual uint8_t peek(uint16_t addr) = 0;
    virtual KeyboardDevice* keyboard() { return nullptr; }
    // Replace the keyboard on the CPU card connector (takes ownership, nullptr = none).
    virtual void set_keyboard(KeyboardDevice* k) { delete k; }
    virtual uint32_t clock_hz() const = 0;

    const uint8_t* bp_exec = nullptr;   // 64K map of execution breakpoints (owned by Machine)
    bool step_mode = false;             // stop at the next instruction boundary
    int step_skip = 0;                  // boundaries to pass before a step stops
    bool stop_hit = false;              // run() stopped because of a breakpoint or step
    bool at_boundary = false;           // stopped right after the opcode fetch of instr_pc
    uint16_t instr_pc = 0;              // address of the instruction being executed
};

// A text display card (AND-1Z).
class DisplayDevice {
public:
    virtual ~DisplayDevice() {}
    virtual int screen_width() const = 0;    // pixels
    virtual int screen_height() const = 0;
    // Dot clock of the video signal: pixel aspect on a PAL TV (square pixels at 7.375 MHz).
    virtual double dot_clock_hz() const { return 10e6; }
    // Render into an ARGB8888 buffer of screen_width*screen_height.
    virtual void render(uint32_t* argb, uint32_t fg, uint32_t bg) = 0;
    // Visible text, one string per row, UTF-8.
    virtual std::vector<std::string> text_rows() = 0;
};

// Byte stream between an emulated serial port and the host (terminal window,
// TCP, MCP...). Thread safe. Data sent by SAPI is kept in a history buffer so
// that several host readers (terminal window, MCP) can each read it with their
// own position.
class SerialChannel {
public:
    explicit SerialChannel(const std::string& n) : name(n) {}
    std::string name;
    // A terminal is connected on the host side (DCD active). Software such as
    // MikroMon or ECMON uses DCD to decide whether to use the serial console.
    std::atomic<bool> connected{ true };   // DCD
    // The terminal is ready to receive: CTS and DSR active. Independent of DCD:
    // e.g. ECMON with the console on the screen (DCD inactive) still sends files
    // with RTS/CTS handshake (DOPC.COM) and waits for CTS.
    std::atomic<bool> cts{ true };
    // Host side never sends faster than SAPI reads: the next character is
    // delivered only after the previous one was taken (no overrun). Off = like a
    // real terminal sending at full line speed (characters may be lost).
    std::atomic<bool> pacing{ true };
    // Host side connections from the machine description ("serial" object of the
    // card: tcp / com), started by Machine::start_bridges().
    nlohmann::json bridge_cfg;
    int baud = 9600;   // line speed, default for a host COM port

    // emulated side
    void to_host(uint8_t b);
    bool from_host(uint8_t& b);

    // host side
    void host_send(const uint8_t* data, size_t n);
    // Read data sent by SAPI from stream position `pos` (updated); returns bytes read.
    size_t host_read(uint64_t& pos, std::string& out, size_t max);
    // Same with the channel's default reader position (headless runner, MCP).
    size_t host_receive(std::string& out, size_t max);
    uint64_t host_end(void) const;          // current end of the stream
    size_t host_pending_rx() const;         // bytes not yet taken by SAPI

private:
    mutable std::mutex mtx;
    std::deque<uint8_t> history;   // SAPI -> host, positions [base, base+size)
    uint64_t base = 0;
    uint64_t default_pos = 0;
    std::deque<uint8_t> rx_q;      // host -> SAPI
};
