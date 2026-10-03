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

// Address decoding reported for the port and memory map (Card::decode_*).
enum DecodeFlags {
    DEC_READ = 1,     // the card drives the data bus on a read
    DEC_WRITE = 2,    // the card takes writes
    DEC_ONCARD = 4,   // memory on the CPU card itself: reads do not go to the bus
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

    // Address decoding for the port and memory map, without side effects:
    // DecodeFlags of the accesses the card answers, `what` = short name of the
    // function there ("UART", "RAM"...). `map` = state of the MAP1/MAP2 lines.
    virtual int decode_io(uint8_t port, const char** what) { return 0; }
    virtual int decode_mem(uint16_t addr, int map, const char** what) { return 0; }
    // false: the decoding is not known (card not emulated yet)
    virtual bool decode_known() { return true; }

    std::string type;   // e.g. "DMR-1"
    std::string name;   // unique instance name, defaults to type
    int slot = 0;
    Bus* bus = nullptr;
    // Configured only to inspect its decoding (configurator map): configure()
    // must not open, create or copy media files.
    bool probe = false;
};

// Helpers for reading configuration values.
bool cfg_number(const nlohmann::json& cfg, const char* key, long long& out, long long def, std::string& err);
bool cfg_bool(const nlohmann::json& cfg, const char* key, bool& out, bool def, std::string& err);
bool cfg_string(const nlohmann::json& cfg, const char* key, std::string& out, const char* def, std::string& err);
