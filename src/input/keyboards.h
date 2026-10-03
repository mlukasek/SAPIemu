#pragma once
// Keyboards attached to connector X2 of the CPU card (see docs/desky/klavesnice.md).
#include <string>
#include "core/devices.h"

KeyboardDevice* keyboard_create(const std::string& type);
bool keyboard_configure(KeyboardDevice* kbd, const nlohmann::json& cfg, uint32_t cpu_hz, std::string& err);
