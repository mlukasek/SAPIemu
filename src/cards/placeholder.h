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

// A board without any function on the bus (BDK-1, the prototyping board left
// empty for the user's own wiring): it only occupies its slot.
class BlankCard : public Card {
public:
    explicit BlankCard(const std::string& t) : Card(t.c_str()) {}
};
