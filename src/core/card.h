#pragma once
// Base class of all cards plugged into the SAPI-1 bus (ARB-1).
//
// Bus accesses are offered to cards in slot order. A card returns true when it
// responds to the access (drives data on a read, accepts a write). Time is
// measured in CPU clock cycles (Bus::now); cards that need time call
// advance(now) to catch up lazily.
#include <stdint.h>
#include <string>
#include "json.hpp"

struct Bus;
class SerialChannel;

enum IrqLine {
    IRQ_INT0 = 1,   // /INT0 -> Z80 /INT on JPR-1ZI
    IRQ_INT1 = 2,   // /INT1 -> Z80 /NMI on JPR-1ZI
};

enum MapLine {
    MAP_1 = 1,      // S1 = MAP1
    MAP_2 = 2,      // S2 = MAP2
};

class Card {
public:
    explicit Card(const char* type_name) : type(type_name) {}
    virtual ~Card() {}

    // Apply configuration (jumpers, ROM files...). Called once after creation.
    virtual bool configure(const nlohmann::json& cfg, std::string& err) { return true; }
    // /RES on the bus
    virtual void reset() {}
    // Bring internal state up to bus time `now` (CPU cycles).
    virtual void advance(uint64_t now) {}

    virtual bool mem_read(uint16_t addr, uint8_t& data) { return false; }
    virtual bool mem_write(uint16_t addr, uint8_t data) { return false; }
    virtual bool io_read(uint8_t port, uint8_t& data) { return false; }
    virtual bool io_write(uint8_t port, uint8_t data) { return false; }
    // Side-effect free read for debuggers (no wait states, no state change).
    virtual bool mem_peek(uint16_t addr, uint8_t& data) { return false; }
    // Interrupt acknowledge (/INTA); a card driving the vector returns true.
    virtual bool inta(uint8_t& data) { return false; }

    // Current level of the interrupt request lines driven by this card (IrqLine mask).
    virtual int irq_lines() { return 0; }
    // MAP lines driven by this card (MapLine mask).
    virtual int map_lines() { return 0; }

    // Serial channels provided by the card (index 0..n-1), nullptr past the end.
    virtual SerialChannel* serial_channel(int index) { return nullptr; }

    // Save battery-backed / persistent content (RAM disk...). Called on exit and on demand.
    virtual void flush() {}

    // Short status for UIs / MCP.
    virtual void describe(nlohmann::json& out) {}

    std::string type;   // e.g. "DMR-1"
    std::string name;   // unique instance name, defaults to type
    int slot = 0;
    Bus* bus = nullptr;
};

// Helpers for reading configuration values.
bool cfg_number(const nlohmann::json& cfg, const char* key, long long& out, long long def, std::string& err);
bool cfg_bool(const nlohmann::json& cfg, const char* key, bool& out, bool def, std::string& err);
bool cfg_string(const nlohmann::json& cfg, const char* key, std::string& out, const char* def, std::string& err);
