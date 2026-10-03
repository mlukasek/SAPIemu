// Card factory: maps the "type" of a card in a machine description to its class.
#include "core/machine.h"
#include "cards/and1.h"
#include "cards/and1z.h"
#include "cards/dmr1.h"
#include "cards/dsm1.h"
#include "cards/ide1.h"
#include "cards/jpr1.h"
#include "cards/jpr1zi.h"
#include "cards/rem1.h"
#include "cards/zrmd1.h"

Card* card_create(const std::string& type)
{
    if (type == "JPR-1ZI") return new Jpr1zi();
    if (type == "JPR-1") return new Jpr1();
    if (type == "REM-1") return new Rem1();
    if (type == "AND-1") return new And1();
    if (type == "DMR-1") return new Dmr1();
    if (type == "DSM-1") return new Dsm1();
    if (type == "ZRMD-1") return new Zrmd1(false);
    if (type == "IDE-1") return new Ide1();
    if (type == "ZRD-1V") return new Zrmd1(true);
    if (type == "AND-1Z-89" || type == "AND-1Z") {
        And1z* c = new And1z();
        c->type = type;
        return c;
    }
    return nullptr;
}

std::vector<std::string> card_types(void)
{
    return { "JPR-1ZI", "JPR-1", "DMR-1", "DSM-1", "REM-1", "AND-1", "AND-1Z-89", "AND-1Z", "ZRMD-1", "ZRD-1V", "IDE-1" };
}
