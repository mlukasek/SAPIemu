#pragma once
// A complete SAPI-1 machine: bus + cards, created from a machine description
// (machines/*.jsonc). Not thread safe: drive it from one thread only.
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>
#include "json.hpp"
#include "core/bus.h"
#include "core/devices.h"

class Machine {
public:
    Machine();
    ~Machine();

    bool load_file(const std::string& path, std::string& err);
    bool load_json(const nlohmann::json& cfg, std::string& err);

    void reset(void);
    // Run for `cycles` CPU cycles or until a breakpoint; returns cycles executed.
    uint64_t run_cycles(uint64_t cycles);
    // Run for `seconds` of emulated time (or until a breakpoint).
    uint64_t run_seconds(double seconds);
    // Execute one instruction; returns false if there is no CPU.
    bool step(void);
    bool stopped_on_breakpoint(void) const { return break_hit; }
    void clear_break(void) { break_hit = false; }

    void set_breakpoint(uint16_t addr, bool enabled);
    bool has_breakpoint(uint16_t addr) const { return bp_exec[addr] != 0; }
    std::vector<uint16_t> breakpoints(void) const;

    uint8_t peek(uint16_t addr);            // CPU view, no side effects
    void poke(uint16_t addr, uint8_t value); // writes through the bus

    // Keyboard input (host text in UTF-8, converted to SAPI codes).
    bool type_text(const std::string& utf8);
    bool type_code(uint8_t code);

    void flush(void);   // persist RAM disks etc.

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
    bool break_hit = false;
};

// Card factory (cards/registry.cpp)
Card* card_create(const std::string& type);
std::vector<std::string> card_types(void);
