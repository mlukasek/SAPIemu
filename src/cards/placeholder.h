#pragma once
// A card from the catalog that is not emulated yet: it occupies its slot(s) and
// keeps its configuration, but never responds on the bus.
#include "core/card.h"

class PlaceholderCard : public Card {
public:
    explicit PlaceholderCard(const std::string& t) : Card("placeholder") { type = t; }
    void describe(nlohmann::json& out) override { out["emulated"] = false; }
    bool decode_known() override { return false; }
};
