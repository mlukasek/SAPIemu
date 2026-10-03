#pragma once
// Port and memory map of a machine description: which card answers which I/O
// port and memory address, from the cards' own decoders (PROM dumps, jumpers),
// with conflicts. The cards are created in probe mode, so no media files are
// touched. Cards not emulated yet appear with the ports set in the
// configurator (serial cards), otherwise as unknown.
#include <string>
#include <vector>
#include "json.hpp"
#include "core/machine_cfg.h"

struct MapUser {
    int card;            // index into MachineMap::cards
    int flags;           // DecodeFlags
    std::string what;    // "UART", "RAM"...
    bool operator==(const MapUser& o) const { return card == o.card && flags == o.flags && what == o.what; }
};

struct MapRange {
    int lo = 0, hi = 0;          // inclusive
    std::vector<MapUser> users;
    bool conflict = false;       // two cards drive the data bus on a read
    bool shared_write = false;   // a write goes to more than one card
};

struct MapCard {
    std::string name;    // "DMR-1 #10"
    int slot = 0;
    bool known = true;   // decoding known (emulated card, configured without error)
    bool declared = false;   // not emulated: ports from its configuration only
    std::string error;
};

struct MachineMap {
    std::vector<MapCard> cards;
    std::vector<MapRange> io;
    std::vector<MapRange> mem[4];   // index = MAP lines: bit 0 MAP1, bit 1 MAP2
    bool map_matters = false;       // the memory map depends on MAP1/MAP2
};

void machine_map_build(const nlohmann::json& cfg, MachineMap& out);
// Conflicts of a map as configuration problems (read conflict = error).
std::vector<CfgProblem> machine_map_problems(const MachineMap& m);
// Layout problems and map conflicts together.
std::vector<CfgProblem> machine_all_problems(const nlohmann::json& cfg);
// Readable text of the users of a range ("DMR-1 #10 UART, ...").
std::string map_range_users(const MachineMap& m, const MapRange& r);
