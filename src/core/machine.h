#pragma once
// A complete SAPI-1 machine: bus + cards, created from a machine description
// (machines/*.jsonc). Not thread safe: drive it from one thread only (see EmuHost).
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>
#include "json.hpp"
#include "core/bus.h"
#include "core/devices.h"
#include "core/disasm.h"
#include "serial/bridge.h"

enum StopReason {
    STOP_NONE = 0,
    STOP_BREAKPOINT,    // execution breakpoint
    STOP_WATCHPOINT,    // memory or I/O watchpoint
    STOP_STEP,          // single step finished
};

class Machine {
public:
    Machine();
    ~Machine();

    bool load_file(const std::string& path, std::string& err);
    bool load_json(const nlohmann::json& cfg, std::string& err);

    void reset(void);
    // Run for `cycles` CPU cycles or until a breakpoint/watchpoint; returns cycles executed.
    uint64_t run_cycles(uint64_t cycles);
    // Run for `seconds` of emulated time (or until a breakpoint).
    uint64_t run_seconds(double seconds);
    // Execute one instruction.
    bool step(void);
    // Execute one instruction; CALL/RST/DJNZ/block instructions run until they return
    // (at most `max_seconds` of emulated time).
    bool step_over(double max_seconds = 5.0);

    StopReason stop_reason(void) const { return stop; }
    const std::string& stop_text(void) const { return stop_msg; }
    bool stopped_on_breakpoint(void) const { return stop == STOP_BREAKPOINT || stop == STOP_WATCHPOINT; }
    void clear_break(void) { stop = STOP_NONE; stop_msg.clear(); }

    void set_breakpoint(uint16_t addr, bool enabled);
    bool has_breakpoint(uint16_t addr) const { return bp_exec[addr] != 0; }
    std::vector<uint16_t> breakpoints(void) const;
    void clear_breakpoints(void);
    void set_mem_watch(uint16_t addr, int flags) { bus.set_mem_watch(addr, flags); }
    void set_io_watch(uint8_t port, int flags) { bus.io_watch[port] = (uint8_t)flags; }

    uint8_t peek(uint16_t addr);             // CPU view, no side effects
    void poke(uint16_t addr, uint8_t value); // writes through the bus (no watchpoints)
    DisasmResult disassemble(uint16_t addr);

    // Load an Intel HEX text / binary file into memory through the bus.
    bool load_ihex_text(const std::string& text, std::string& err, int* bytes = nullptr);
    bool load_binary(const std::string& path, uint16_t addr, std::string& err, int* bytes = nullptr);

    // Keyboard input: host text in UTF-8 (the keyboard converts it to its codes
    // or key presses), keys without text (HostKey, HostMod).
    bool type_text(const std::string& utf8);
    bool host_key(int key, int mods);
    // Plug another keyboard (name from keyboard_presets()) into the CPU card.
    bool set_keyboard(const std::string& preset, std::string& err);

    void flush(void);   // persist RAM disks etc.

    // Host side serial connections (TCP server, COM port).
    bool start_bridges(std::string& err);   // from the channels' configuration
    void stop_bridges(void);
    void add_bridge(SerialBridge* b) { bridges.emplace_back(b); }
    void remove_bridges(SerialChannel* ch, const char* kind);
    std::vector<std::unique_ptr<SerialBridge>> bridges;

    CpuCard* cpu = nullptr;
    DisplayDevice* display = nullptr;
    std::vector<SerialChannel*> serials;
    KeyboardDevice* keyboard(void) { return cpu ? cpu->keyboard() : nullptr; }
    Card* find_card(const std::string& name);

    Bus bus;
    std::vector<std::unique_ptr<Card>> cards;
    std::string name;          // human readable name from the config
    std::string config_path;
    nlohmann::json config;

private:
    uint8_t bp_exec[65536];
    StopReason stop = STOP_NONE;
    std::string stop_msg;
};

// Card factory (cards/registry.cpp)
Card* card_create(const std::string& type);
std::vector<std::string> card_types(void);
