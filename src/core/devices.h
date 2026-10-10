#pragma once
// Interfaces between cards and the outside world: CPU, display, keyboard, serial line.
#include <stdint.h>
#include <atomic>
#include <deque>
#include <string.h>
#include <mutex>
#include <string>
#include <vector>
#include "core/beeper.h"
#include "core/card.h"
#include "input/joyport.h"

class StateIO;

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
    HK_SHIFT,   // the Shift key itself (held keys in the game mode)
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
    // A device on both connectors of JPR-1 (X2 and X3: the cables of APS-5 with HRK-1): P2-IN
    // levels (ANDed in; on JPR-1 also the interrupt requests of the 3214), the P1-OUT and P2-OUT
    // latches, and /RTL (a watchdog resetting the whole machine).
    virtual uint8_t p2_in() { return 0xFF; }
    virtual void p1_out(uint8_t value) {}
    virtual void p2_out(uint8_t value) {}
    virtual bool rtl() { return false; }
    // Switching the machine off: a device that warns the computer of a power failure (the supply of
    // APS-5 through HRK-1) gets power_fail() and the machine runs this many seconds before it stops.
    virtual double power_fail_warning() const { return 0; }
    virtual void power_fail() {}
    // Host typed a Unicode character (text input, MCP); control characters
    // (CR, ESC, Ctrl+letter...) included. Characters the keyboard cannot
    // produce are ignored.
    virtual void type_char(uint32_t cp) = 0;
    // Host pressed a key without text (cursor keys, F-keys, keypad...). Returns
    // false when this keyboard has no such key.
    virtual bool host_key(int key, int mods) { return false; }
    // Keys held on the PC (a matrix scanned by the program, ANK-1): the key is down while the PC
    // key is held, no queue and no host autorepeat. `id` names the PC key (its scancode). The key
    // is the character it typed (cp) or a HostKey (hk, cp = 0); raw = the game mode: letters and
    // digits as they are, HK_SHIFT = the SHIFT key, no SHIFT added for symbols. Returns false when
    // the keyboard has no such key (or types text only: held_keys() false).
    virtual bool held_keys() const { return false; }
    virtual bool hold_key(int id, uint32_t cp, int hk, bool raw) { return false; }
    virtual void release_key(int id) {}
    virtual void release_all() {}
    virtual size_t pending() const = 0;
    virtual void clear() = 0;
    // Short state for the status bar (LEDs...), empty if none.
    virtual std::string status() const { return std::string(); }
    // "True to the hardware": no help for text typed on the PC (wait for the program, send a
    // missed key again); keys may be lost as on the real machine. False if the keyboard has
    // no such help (ANK-1 holds a key until the program reads it).
    virtual bool set_strict(bool on) { return false; }
    virtual void state(StateIO& io) {}   // save state (core/state.h)
    // the beeper (core/beeper.h): the kind this keyboard has, switched on or off
    virtual int beeper_kind() const { return Beeper::NONE; }
    void set_beeper(bool on) { beeper.kind = on ? beeper_kind() : Beeper::NONE; }
    // A joystick connector on the keyboard (ANK-1M2): what is plugged in (input/joyport.h JoyDevice),
    // -1 = the keyboard has none. set_joyport() returns false then.
    // A plotter behind the ports (the plotter of AR 1990 drives a Minigraf from P0-OUT)
    virtual Minigraf* minigraf() { return nullptr; }
    virtual int joyport() const { return -1; }
    virtual bool set_joyport(int dev) { return false; }
    Bus* bus = nullptr;   // set by the CPU card: the beeper sounds on its bus
    Beeper beeper;
};

// Base class of CPU cards (bus masters).
class CpuCard : public Card {
public:
    explicit CpuCard(const char* type_name) : Card(type_name) {}
    // While running: "keyboard.strict", "keyboard.beeper" and "keyboard.joyport" of the keyboard
    // on the connector.
    bool live_param(const char* path) const override
    {
        return !strcmp(path, "keyboard.strict") || !strcmp(path, "keyboard.beeper") || !strcmp(path, "keyboard.joyport");
    }
    bool set_live_param(const char* path, const nlohmann::json& v, std::string& err) override
    {
        if (!strcmp(path, "keyboard.joyport")) {
            KeyboardDevice* k = keyboard();
            int dev;
            if (!joy_device_parse(v, dev, err, path)) return false;
            if (!k || !k->set_joyport(dev)) { err = "this keyboard has no joystick connector (keyboard.joyport)"; return false; }
            return true;
        }
        if (!strcmp(path, "keyboard.beeper")) {
            KeyboardDevice* k = keyboard();
            if (!k || !v.is_boolean()) { err = "keyboard.beeper must be true or false"; return false; }
            k->set_beeper(v.get<bool>());
            return true;
        }
        if (strcmp(path, "keyboard.strict") != 0) return Card::set_live_param(path, v, err);
        KeyboardDevice* k = keyboard();
        if (!k || !v.is_boolean() || !k->set_strict(v.get<bool>())) {
            err = "this keyboard has no faithful mode to switch (keyboard.strict)";
            return false;
        }
        return true;
    }
    // Execute up to `cycles` CPU clock cycles. Returns the number executed; stops
    // early when a breakpoint is hit or a single step completes.
    virtual int run(int cycles) = 0;
    virtual void get_regs(CpuRegs& r) = 0;
    virtual void set_regs(const CpuRegs& r) = 0;
    // Memory as seen by the CPU (including on-card ROM), without side effects.
    virtual uint8_t peek(uint16_t addr) = 0;
    // Debugger / loader write into RAM on the CPU card itself (JPR-1, JPR-1S), which bus writes
    // of other masters do not reach. Ports and ROM are left alone.
    virtual void poke_local(uint16_t addr, uint8_t value) {}
    virtual KeyboardDevice* keyboard() { return nullptr; }
    // Replace the keyboard on the CPU card connector (takes ownership, nullptr = none).
    virtual void set_keyboard(KeyboardDevice* k) { delete k; }
    virtual uint32_t clock_hz() const = 0;       // the time base of the bus (cycles per second)
    // A TURBO switch on the CPU card that changes the CPU clock while running (JPR-1V).
    virtual bool has_cpu_turbo() const { return false; }
    virtual bool cpu_turbo() const { return false; }
    virtual void set_cpu_turbo(bool on) {}
    virtual uint32_t cpu_display_hz() const { return clock_hz(); }   // the actual CPU clock
    virtual uint32_t phi2_hz() const { return clock_hz(); }   // Φ2 TTL on the bus (pin 60), e.g. for RTC-1
    // The instruction set (the debugger disassembles the 8080/8085 opcodes that differ from the Z80).
    enum { ISA_Z80 = 0, ISA_8080, ISA_8085 };
    virtual int cpu_isa() const { return ISA_Z80; }
    // The diagnostic board on K3 (Libor Lasota, rev 2.0; JPR-1S, JPR-1X): two seven segment displays
    // on P2-OUT (left) and P1-OUT (right), segments a..g, dp = bits 0..7 (1 = lit), and two buttons:
    // 0 = "+" on P2-IN0, 1 = "-" on P2-IN7 (pressed = 0).
    virtual bool k3_diag() const { return false; }
    virtual uint8_t k3_display(int i) const { return 0; }
    virtual void k3_button(int b, bool down) {}
    virtual bool k3_button_down(int b) const { return false; }

    const uint8_t* bp_exec = nullptr;   // 64K map of execution breakpoints (owned by Machine)
    bool step_mode = false;             // stop at the next instruction boundary
    int step_skip = 0;                  // boundaries to pass before a step stops
    bool stop_hit = false;              // run() stopped because of a breakpoint or step
    bool at_boundary = false;           // stopped right after the opcode fetch of instr_pc
    uint16_t instr_pc = 0;              // address of the instruction being executed
};

// A display card with its own video output: text (AND-1, AND-1Z) or graphics (DGD-1).
class DisplayDevice {
public:
    virtual ~DisplayDevice() {}
    virtual int screen_width() const = 0;    // pixels
    virtual int screen_height() const = 0;
    // The console screen of the machine (text). A graphic display gets its own window.
    virtual bool text_display() const { return true; }
    // Dot clock of the video signal: pixel aspect on a PAL TV (square pixels at 7.375 MHz).
    virtual double dot_clock_hz() const { return 10e6; }
    // Render into an ARGB8888 buffer of screen_width*screen_height; a monochrome
    // picture uses its own ink colour on `bg`.
    virtual void render(uint32_t* argb, uint32_t bg) = 0;
    // Ink of a monochrome picture: the phosphor of the monitor (AND-1, AND-1Z, DGD-1)
    // or the colour jumpers of the card (AND-1V). It is the card parameter
    // ink_param() of the machine description; nullptr = a colour display (CGA-1V).
    virtual const char* ink_param() const { return nullptr; }
    virtual std::string ink() const { return std::string(); }   // current value
    // Change the ink while running (value as in the machine description).
    virtual bool set_ink(const std::string& value, std::string& err) { err = "no ink colour"; return false; }
    // Visible text, one string per row, UTF-8.
    virtual std::vector<std::string> text_rows() = 0;
};

// Phosphor of a monochrome monitor ("white", "green", "amber") as ARGB; false for an unknown name.
bool monitor_phosphor(const std::string& name, uint32_t& argb);
// Ink of a VGA card set by its colour jumpers /BLUE /RED /GREEN-HI /GREEN-LO (AND-1V, DGD-1V):
// "white", "green", "amber", "red", "blue", "cyan", "yellow", "magenta".
bool vga_ink(const std::string& name, uint32_t& argb);

// The monitor of a monochrome display card without colour jumpers: card parameter
// "monitor" (phosphor colour, not a part of the card).
struct MonitorInk {
    std::string name = "white";
    uint32_t argb = 0xFFE8E8E8u;
    bool configure(const nlohmann::json& cfg, std::string& err);
    bool set(const std::string& value, std::string& err);
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
    void host_drop_pending();               // forget them (a file send stopped)
    void state(StateIO& io);                // DCD, CTS and the bytes not yet taken by SAPI

private:
    mutable std::mutex mtx;
    std::deque<uint8_t> history;   // SAPI -> host, positions [base, base+size)
    uint64_t base = 0;
    uint64_t default_pos = 0;
    std::deque<uint8_t> rx_q;      // host -> SAPI
};
