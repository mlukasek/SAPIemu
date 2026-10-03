// Card factory: maps the "type" of a card in a machine description to its class.
#include "core/machine.h"
#include "cards/and1z.h"
#include "cards/dmr1.h"
#include "cards/jpr1zi.h"

Card* card_create(const std::string& type)
{
    if (type == "JPR-1ZI") return new Jpr1zi();
    if (type == "DMR-1") return new Dmr1();
    if (type == "AND-1Z-89" || type == "AND-1Z") {
        And1z* c = new And1z();
        c->type = type;
        return c;
    }
    return nullptr;
}

std::vector<std::string> card_types(void)
{
    return { "JPR-1ZI", "DMR-1", "AND-1Z-89", "AND-1Z" };
}
