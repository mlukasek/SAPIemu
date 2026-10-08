#pragma once
// Keyboards attached to connector X2 of the CPU card (see docs/desky/klavesnice.md).
#include <string>
#include <vector>
#include "core/devices.h"

// Create a keyboard from its machine description object, e.g.
//   { "type": "EKL-1" }
//   { "type": "Consul 262.3", "latch_mod": true }     // 7474 modification
//   { "type": "Consul 259.11", "code": "koi7" }       // or "koi8cs"
//   { "type": "ANK-1" }
//   { "type": "ANK-1M2", "joyport": "joystick" }       // or none, mouse_atari, mouse_amiga
//                                                       // ("ANK-1M5", its predecessor, is the same)
//   { "type": "HRK-1" }                                // APS-5 front panel on X2 and X3 of JPR-1
// Returns nullptr and fills err on error.
KeyboardDevice* keyboard_from_config(const nlohmann::json& cfg, uint32_t cpu_hz, std::string& err);

// Ready-made choices for menus and MCP: name -> configuration.
struct KeyboardPreset {
    const char* name;
    const char* config;   // JSON
};
const std::vector<KeyboardPreset>& keyboard_presets(void);
KeyboardDevice* keyboard_from_preset(const std::string& name, uint32_t cpu_hz, std::string& err);
