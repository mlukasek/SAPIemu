#pragma once
// A complete SAPI-1 machine: bus + cards, created from a machine description
// (machines/*.sapi, JSON with comments). Not thread safe: drive it from one thread only (see EmuHost).
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>
#include "json.hpp"
#include "core/bus.h"
#include "core/devices.h"
#include "core/plant.h"
#include "core/disasm.h"
#include "serial/bridge.h"

enum StopReason {
    STOP_NONE = 0,
    STOP_BREAKPOINT,    // execution breakpoint
    STOP_WATCHPOINT,    // memory or I/O watchpoint
    STOP_STEP,          // single step finished
};

// Save state formats (docs/stav.md): SAPIemu writes STATE_FORMAT and reads STATE_FORMAT_MIN..STATE_FORMAT. Format 2
// put the version of SAPIemu that saved it into the header; every later format keeps the start of the header ("SAPISTAT",
// the format, that version), so that any version can say who saved a state it does not read.
// A state() that reads other data than before: STATE_FORMAT + 1, and STATE_FORMAT_MIN up to it when the old data
// cannot be read any more (such a state is then refused before the running machine is touched).
enum { STATE_FORMAT = 2, STATE_FORMAT_MIN = 1 };

struct StateOrigin {
    uint32_t format = 0;
    std::string app;   // the version of SAPIemu that saved it ("0.7.0-beta"); empty in format 1 (the alphas before)
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
    // `lo`, `hi`: the lowest and highest address loaded (when bytes > 0).
    bool load_ihex_text(const std::string& text, std::string& err, int* bytes = nullptr, uint16_t* lo = nullptr,
                        uint16_t* hi = nullptr);
    bool load_binary(const std::string& path, uint16_t addr, std::string& err, int* bytes = nullptr);

    // Keyboard input: host text in UTF-8 (the keyboard converts it to its codes
    // or key presses), keys without text (HostKey, HostMod).
    bool type_text(const std::string& utf8);
    bool host_key(int key, int mods);
    // Plug another keyboard (name from keyboard_presets()) into the CPU card.
    bool set_keyboard(const std::string& preset, std::string& err);

    void flush(void);   // persist RAM disks etc.
    // Media with writes not saved yet (Card::unsaved), and saving just those (EmuHost::autosave); returns how many
    // cards saved.
    bool unsaved(void);
    int save_media(void);
    // Switch off as the mains goes away: the power fail warning runs first (APS-5), then flush().
    void power_down(void);
    // The power switch off and on in place (sapiemu-cli --power-cycle; the GUI and MCP build a new machine,
    // EmuHost::power_cycle): power_down, then built again from `config` as it runs (media put in since stay in).
    bool power_cycle(std::string& err);
    // Built again from a description in place (the cards, the plant and the displays made anew).
    bool rebuild(const nlohmann::json& cfg, std::string& err);

    // Save state (core/state.h, docs/stav.md): the machine description, the time of the bus and
    // the state of every card, the keyboard and the serial channels. `file` = the machine file
    // it came from (kept in the state). load_state needs a machine built from the same
    // description (EmuHost::load_state builds it); with `rebuild` it builds itself again
    // when the description differs (sapiemu-cli).
    bool save_state(std::vector<uint8_t>& out, const std::string& file, std::string& err);
    bool load_state(const std::vector<uint8_t>& in, std::string& err, bool rebuild = false);
    // The machine description and machine file stored in a state; a state of a format this SAPIemu does not read
    // (STATE_FORMAT_MIN..STATE_FORMAT) is refused here, before anything is built.
    static bool state_header(const std::vector<uint8_t>& in, nlohmann::json& cfg, std::string& file,
                             std::string& err);
    // Who saved a state: its format and the version of SAPIemu (also of a format not read here). False: not a state.
    static bool state_origin(const std::vector<uint8_t>& in, StateOrigin& origin);

    // Host side serial connections (TCP server, COM port).
    bool start_bridges(std::string& err);   // from the channels' configuration
    void stop_bridges(void);
    void add_bridge(SerialBridge* b) { bridges.emplace_back(b); }
    void remove_bridges(SerialChannel* ch, const char* kind);
    std::vector<std::unique_ptr<SerialBridge>> bridges;

    CpuCard* cpu = nullptr;
    DisplayDevice* display = nullptr;           // the console screen (first text display)
    std::vector<DisplayDevice*> displays;       // all display outputs in slot order
    std::vector<SerialChannel*> serials;
    KeyboardDevice* keyboard(void) { return cpu ? cpu->keyboard() : nullptr; }
    Card* find_card(const std::string& name);
    DisplayDevice* find_display(const std::string& card);   // "" = the console screen
    // Ink colour of a monochrome display while running (DisplayDevice::ink_param of
    // its card); the machine description `config` changes with it.
    bool set_display_ink(DisplayDevice* d, const std::string& value, std::string& err);
    // Parameters that take effect while running: Card::live_param and the ink of a display.
    // Setting one changes the card at once and the description `config` with it.
    bool live_param(Card* c, const char* path);
    bool set_live_param(Card* c, const char* path, const nlohmann::json& v, std::string& err);
    nlohmann::json* card_config(Card* c);   // the entry of the card in `config`, nullptr if none
    // A value the card has taken over already (a tape made in the Media window): into `config` only.
    void note_param(Card* c, const char* path, const nlohmann::json& v);
    // The card whose first slot is `from` goes to `to` while running, a card starting at `to` to
    // `from` (a swap). The slots of the bus are equal (conflicts are wired AND in any order),
    // so only the slot numbers change, here and in `config`. The caller checks the layout.
    bool move_card(int from, int to);
    // New slots for some cards at once (a permutation, e.g. back to the saved description).
    void set_card_slots(const std::vector<std::pair<Card*, int>>& to);

    Bus bus;
    std::vector<std::unique_ptr<Card>> cards;
    std::unique_ptr<Plant> plant;   // the process outside the computer (description "plant"), or none
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
