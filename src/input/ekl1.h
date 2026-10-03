#pragma once
// EKL-1 - PS/2 keyboard converter (AT89C2051 firmware KEYB_REPAIR.a51),
// emulated at the port level. It behaves like a Consul 262.3 with the strobe
// latched until ACK: the firmware puts the inverted code on P1, STROBE=0, waits
// for ACK=1 and raises STROBE. Czech letters (Alt-lock mode) are sent as
// KOI-8 cs with swapped case, like the Czech Consul 259.11.
#include "input/parallel_kbd.h"

class Ekl1 : public ParallelKeyboard {
public:
    const char* type_name() const override { return "EKL-1"; }
    void type_char(uint32_t cp) override;
    bool host_key(int key, int mods) override;
};
