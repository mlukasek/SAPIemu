#pragma once
// PMD32-SD on an 8255 of a card (IDE-1, DPP-1): the "pmd32sd" object of the card's description
// (SD card directory, working copy, drives A-D, locked card, start-up delay) and its status.
#include "chips/pmd32sd.h"
#include "json.hpp"

#include <stdint.h>
#include <string>

// Configure from `p` (the "pmd32sd" object) and power the device on at `now`; probe = only
// the decoding is wanted (no SD card copy). `who` names the card in the log.
bool pmd32_configure(Pmd32Sd& pmd, const nlohmann::json& p, bool probe, uint64_t now, const std::string& who,
                     std::string& err);
// SD card and drives for describe() (MCP, status).
void pmd32_describe(Pmd32Sd& pmd, nlohmann::json& out);
