#pragma once
// Consul keyboards on connector X2 of the CPU card (see docs/desky/klavesnice.md).
#include "input/parallel_kbd.h"

// Consul 259.11: KOI-7 (ASCII incl. lower case), full handshake with /AC, the
// CLEAR key on P1-IN7 (/CLR, active low). Variant "koi8cs": 8-bit KOI-8 cs with
// BIT8 on P1-IN7 instead of /CLR, as expected by ECMON ("CONSUL 259.11 KOI8 CS2").
class Consul25911 : public ParallelKeyboard {
public:
    Consul25911() { latched = true; seven_bit = true; }
    const char* type_name() const override { return koi8 ? "Consul 259.11 (KOI-8 cs)" : "Consul 259.11"; }
    void type_char(uint32_t cp) override;
    bool host_key(int key, int mods) override;
    void set_koi8(bool on) { koi8 = on; seven_bit = !on; }

private:
    bool koi8 = false;
};

// Consul 262.3 (LAT, finál 800.567): 8-bit codes, no acknowledge input, 1 ms
// strobe pulse. With the 7474 modification on JPR-1Z the strobe is latched
// until the computer acknowledges.
class Consul2623 : public ParallelKeyboard {
public:
    Consul2623() { latched = true; }
    const char* type_name() const override { return latched ? "Consul 262.3" : "Consul 262.3 (bez 7474)"; }
    void type_char(uint32_t cp) override;
    bool host_key(int key, int mods) override;
};
