#pragma once
// Editing machine descriptions (machines/*.sapi) as JSON documents: parameter
// access by path, defaults from the card catalog, layout checks and writing a
// commented .sapi file (JSON with comments). Used by the configurator window and MCP; Machine only
// reads the result.
#include <string>
#include <vector>
#include "json.hpp"
#include "cards/catalog.h"

// Value at a dotted path ("serial.tcp.port", "write_enable.0"), nullptr if absent.
const nlohmann::json* cfg_find(const nlohmann::json& obj, const char* path);
// Remove the value at a path (no-op if absent).
void cfg_erase(nlohmann::json& obj, const char* path);

// Values compare equal, numbers also when written as text ("10h" == 16).
bool cfg_values_equal(const nlohmann::json& a, const nlohmann::json& b);
// Default of a parameter as JSON.
nlohmann::json param_default(const CardParam* p);
// Current value of a parameter of a card (its default when absent).
nlohmann::json card_param_value(const nlohmann::json& card, const CardParam* p);
// Set a parameter; null removes it. Arrays are created filled with the defaults
// of their sibling parameters, a PK_SECTION value true/false adds/removes the object.
void card_set_param(nlohmann::json& card, const CardInfo* ci, const CardParam* p, const nlohmann::json& v);
// The section a parameter belongs to (nullptr if none) and whether it is enabled.
const CardParam* param_section(const CardInfo* ci, const CardParam* p);
bool card_param_active(const nlohmann::json& card, const CardInfo* ci, const CardParam* p);
// Index of the option matching the value, -1 if none (custom value).
int param_option_index(const CardParam* p, const nlohmann::json& v);

// A new card with all its parameters at their defaults.
nlohmann::json card_new_config(const CardInfo* ci, int slot);

// Bus of the machine: number of slots, whether the CPU card must sit in X1.
int machine_cfg_slots(const nlohmann::json& cfg);
bool machine_cfg_cpu_in_x1(const nlohmann::json& cfg);
// Index in cfg["cards"] of the card covering `slot`, -1 if empty.
int machine_cfg_card_at(const nlohmann::json& cfg, int slot);
int card_cfg_width(const nlohmann::json& card);
// Port of a serial card for the panel and lists: "#10"; empty if none / port disabled.
std::string card_port_label(const nlohmann::json& card);
// "DMR-1 #10" or just the type.
std::string card_display_name(const nlohmann::json& card);
// Ports of the serial connectors of a card, in the order of its panel (DSM-1V: channel A
// on K3, B on K2; DSM-1, DMR-1: the terminal); "" for a disabled channel.
std::vector<std::string> card_connector_ports(const nlohmann::json& card);

struct CfgProblem {
    int slot;          // 0 = whole machine
    bool error;        // false = warning (e.g. card not emulated)
    std::string en, cs;
};
std::vector<CfgProblem> machine_cfg_problems(const nlohmann::json& cfg);

// Commented text of a machine description (.sapi file, JSON with comments).
std::string machine_cfg_to_jsonc(const nlohmann::json& cfg);
