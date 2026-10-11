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
class StateIO;
struct FloppyDrive;
class Pmd32Sd;
class Kzd1;
class Minigraf;
struct Printer;

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
    // STSTB in an INTA cycle with `addr` (the PC) on the address bus (JPR-1V): true when the card's wait logic
    // starts on it (STSTB with its select) and ends only with /MR or /MW, so RDY stays low for ever (measured
    // with CGA-1V and AND-1V 2026-10-11, docs/zmeny-podle-hw.md: the FXS4 freeze).
    virtual bool inta_holds_rdy(uint16_t addr) { return false; }

    // Current level of the interrupt request lines driven by this card (IrqLine mask).
    virtual int irq_lines() { return 0; }
    // MAP lines driven by this card (MapLine mask).
    virtual int map_lines() { return 0; }
    // The card pulls the bus /RTL low (the reset button line; RSW-1 watchdog): a CPU card that
    // watches it (PKT-1) resets the whole system.
    virtual bool rtl_line() { return false; }

    // Serial channels provided by the card (index 0..n-1), nullptr past the end.
    virtual SerialChannel* serial_channel(int index) { return nullptr; }
    // Floppy drives of a disk controller (index 0 = A), nullptr past the end. Disks are
    // changed through floppy_insert / floppy_eject, which keep the controller consistent.
    virtual FloppyDrive* floppy_drive(int index) { return nullptr; }
    // PMD32-SD floppy emulator connected through the card (IDE-1, DPP-1), nullptr if none.
    virtual Pmd32Sd* pmd32sd() { return nullptr; }
    // Printer connected to the card (JPR X3, DPP-1), nullptr if none.
    virtual Printer* printer() { return nullptr; }
    // KZD-1 cassette data recorder connected to the card (DPP-1 X2), nullptr if none.
    virtual Kzd1* kzd1() { return nullptr; }
    // Plotter ARITMA Minigraf 0507 connected to the card (DPP-1 port B), nullptr if none.
    virtual Minigraf* minigraf() { return nullptr; }
    virtual bool floppy_insert(int index, const std::string& image, bool write_protect, std::string& err)
    {
        err = "no such drive";
        return false;
    }
    virtual void floppy_eject(int index) {}
    // Removable media named by a parameter of the card (a disk, a tape, a cartridge): the Media window and
    // the configurator change them while running through Machine::set_live_param, so the machine
    // description keeps up. media_param: the parameter of unit `u` ("unit0.tape"), nullptr = no such unit;
    // media_change: put that image in ("" = take the medium out); media_discard: forget the changes still
    // in memory, the next eject writes nothing. Default: the floppy drives ("drive_a.image"...).
    virtual const char* media_param(int u);
    virtual bool media_change(int u, const std::string& image, std::string& err);
    virtual void media_discard(int u);

    // Save battery-backed / persistent content (RAM disk...). Called on exit and on demand.
    virtual void flush() {}

    // Short status for UIs / MCP.
    virtual void describe(nlohmann::json& out) {}
    // Save state (core/state.h): write / read every variable that changes while running, in
    // a fixed order. What the configuration sets up (ROMs, jumpers) is built again from the
    // machine description. Default: nothing.
    virtual void state(StateIO& io) {}
    // State worth seeing while running that is not a parameter (a lock set by the program...):
    // one line for the configurator, empty = nothing.
    virtual std::string live_status() { return std::string(); }

    // Address decoding for the port and memory map, without side effects:
    // DecodeFlags of the accesses the card answers, `what` = short name of the
    // function there ("UART", "RAM"...). `map` = state of the MAP1/MAP2 lines.
    virtual int decode_io(uint8_t port, const char** what) { return 0; }
    virtual int decode_mem(uint16_t addr, int map, const char** what) { return 0; }
    // false: the decoding is not known (card not emulated yet)
    virtual bool decode_known() { return true; }
    // LEDs on the front panel that follow the emulation: bit n = the n-th LED of the panel
    // drawing (configurator), 1 = lit.
    virtual unsigned panel_leds() { return 0; }
    // Colour of the n-th panel LED when the card sets it by its state: 'g' green, 'y' yellow,
    // 'r' red; 0 = the colour of the panel drawing.
    virtual char panel_led_color(int n) { return 0; }
    // Parameters that take effect while running (switches that can be flipped on the real card,
    // modes of the emulator): the configurator applies them at once, not at the next power cycle.
    virtual bool live_param(const char* path) const { return false; }
    // Apply such a parameter (value as in the machine description); false + err if not applied.
    virtual bool set_live_param(const char* path, const nlohmann::json& v, std::string& err)
    {
        err = std::string(path) + " cannot change while running";
        return false;
    }

    // power-on content of a memory of the card: `value`, or random bits (Bus::power_on_fill)
    void power_on_fill(uint8_t* p, size_t n, uint8_t value);

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
// "cpm": the CP/M letter of a drive ("C" or "C:"), out = 0 when not given
bool cfg_letter(const nlohmann::json& cfg, char& out, std::string& err);
bool cfg_string(const nlohmann::json& cfg, const char* key, std::string& out, const char* def, std::string& err);
