// Card factory: maps the "type" of a card in a machine description to its class.
#include "core/machine.h"
#include "cards/and1.h"
#include "cards/and1v.h"
#include "cards/and1b.h"
#include "cards/bgp1s.h"
#include "cards/cga1v.h"
#include "cards/and1z.h"
#include "cards/dgd1.h"
#include "cards/dmr1.h"
#include "cards/dsm1.h"
#include "cards/dpp1.h"
#include "cards/ide1.h"
#include "cards/jpr1.h"
#include "cards/jpr1zi.h"
#include "cards/mph1v.h"
#include "cards/placeholder.h"
#include "cards/ram1.h"
#include "cards/ram1v.h"
#include "cards/ram1z.h"
#include "cards/rem1.h"
#include "cards/rmd1.h"
#include "cards/rkd1.h"
#include "cards/rpd1.h"
#include "cards/fdc1.h"
#include "cards/mfc1a.h"
#include "cards/crm1.h"
#include "cards/dsp1.h"
#include "cards/rtc1.h"
#include "cards/adp1.h"
#include "cards/dmt1.h"
#include "cards/drv1a.h"
#include "cards/dbs1a.h"
#include "cards/pkt1.h"
#include "cards/rsw1.h"
#include "cards/zrmd1.h"

Card* card_create(const std::string& type)
{
    if (type == "JPR-1ZI") return new Jpr1zi();
    if (type == "JPR-1Z") return new Jpr1zi("JPR-1Z");
    if (type == "JPR-1V") return new Jpr1zi("JPR-1V");
    if (type == "DSM-1V") return new Dsm1v();
    if (type == "AND-1V") return new And1v();
    if (type == "AND-1B") return new And1b();
    if (type == "BGP-1S" || type == "BGP-1") return new Bgp1s();   // BGP-1 in the schematic and the BIOS
    if (type == "RAM-1V") return new Ram1v();
    if (type == "RAM-1Z") return new Ram1z();
    if (type == "RMD-1") return new Rmd1();
    if (type == "JPR-1") return new Jpr1();
    if (type == "JPR-1S") return new Jpr1("JPR-1S");
    if (type == "JPR-1X") return new Jpr1("JPR-1X");
    if (type == "JPR-1A") return new Jpr1("JPR-1A");
    if (type == "REM-1") return new Rem1();
    if (type == "RAM-1" || type == "RAM-1A") return new Ram1(type.c_str());
    if (type == "AND-1") return new And1();
    if (type == "AND-1A") return new And1("AND-1A");
    if (type == "DGD-1") return new Dgd1();
    if (type == "DGD-1V") return new Dgd1("DGD-1V");
    if (type == "CGA-1V") return new Cga1v();
    if (type == "DMR-1") return new Dmr1();
    if (type == "DSM-1") return new Dsm1();
    if (type == "ZRMD-1") return new Zrmd1(false);
    if (type == "IDE-1") return new Ide1();
    if (type == "DPP-1") return new Dpp1();
    if (type == "ZRD-1V") return new Zrmd1(true);
    if (type == "BDK-1") return new BlankCard(type);
    if (type == "RPD-1") return new Rpd1();
    if (type == "RKD-1") return new Rkd1();
    if (type == "FDC-1") return new Fdc1();
    if (type == "MFC-1A") return new Mfc1a();
    if (type == "PKT-1") return new Pkt1();
    if (type == "CRM-1") return new Crm1();
    if (type == "RTC-1") return new Rtc1();
    if (type == "ADP-1") return new Adp1();
    if (type == "DMT-1") return new Dmt1();
    if (type == "DRV-1A") return new Drv1a();
    if (type == "DSP-1") return new Dsp1();
    if (type == "RSW-1") return new Rsw1();
    if (type == "DBS-1A" || type == "DBS-1") return new Dbs1a();
    if (type == "MPH-1V") return new Mph1v();
    if (type == "AND-1Z-89" || type == "AND-1Z") {
        And1z* c = new And1z();
        c->type = type;
        return c;
    }
    return nullptr;
}

std::vector<std::string> card_types(void)
{
    return { "JPR-1ZI", "JPR-1", "JPR-1S", "JPR-1X", "DMR-1", "DSM-1", "REM-1", "RAM-1", "AND-1", "AND-1Z-89", "AND-1Z", "ZRMD-1", "ZRD-1V", "IDE-1", "BDK-1", "DGD-1", "RPD-1", "RAM-1A", "JPR-1A", "AND-1A", "JPR-1Z", "RAM-1Z", "RMD-1", "JPR-1V", "DSM-1V", "AND-1V", "AND-1B", "BGP-1S", "RAM-1V", "CGA-1V", "DGD-1V", "MPH-1V", "FDC-1", "MFC-1A" };
}
