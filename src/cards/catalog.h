#pragma once
// Catalog of SAPI-1 cards and buses: what the machine configurator offers,
// with a description of every configuration parameter (jumpers, ROM files...).
// One description drives the configurator form, the comments of saved machine
// files and the MCP tools. Cards not emulated yet are listed too; a machine can
// hold them, but they do not respond on the bus (see PlaceholderCard).
//
// Texts have an English and a Czech variant (the UI picks one by language).
#include <stdint.h>
#include <string>
#include <vector>

enum CardClass {
    CC_CPU = 0,     // bus master, slot X1 (unless ARB-1C JP3 = 1-2)
    CC_MEMORY,
    CC_DISPLAY,
    CC_SERIAL,
    CC_DISK,
    CC_IO,
    CC_OTHER,
};

enum ParamKind {
    PK_CHOICE = 0,  // one of `options` (values are JSON texts)
    PK_INT,         // integer in [min, max]
    PK_HEX,         // number written as "XXh" (ports, addresses)
    PK_BOOL,
    PK_FILE,        // path relative to the data root (ROM, image, directory)
    PK_TEXT,
    PK_SECTION,     // optional object: present = enabled; its parameters follow with the path as prefix
    PK_KEYBOARD,    // keyboard on the CPU card connector (keyboard_presets())
};

struct ParamOption {
    const char* value;   // JSON text: "2", "\"2-3\"", "true"
    const char* en;
    const char* cs;
};

struct CardParam {
    const char* path;    // "baud", "serial.tcp.port", "write_enable.0" (array index)
    int kind;            // ParamKind
    const char* en;      // label, starts with the jumper / part name when there is one
    const char* cs;
    const char* def;     // default as JSON text; for PK_SECTION "true"/"false" = present by default
    const ParamOption* options;   // PK_CHOICE, terminated by { nullptr }
    long long min, max;  // PK_INT / PK_HEX
};

struct CardInfo {
    const char* type;    // "DMR-1"
    const char* panel;   // short label on the front panel: "DMR"
    const char* en;      // full title
    const char* cs;
    int cls;             // CardClass
    int width;           // slots occupied (RPD-1 = 2 boards)
    bool emulated;
    const CardParam* params;   // terminated by { nullptr }, may be nullptr
};

struct BusInfo {
    const char* type;    // "ARB-1C"
    const char* en;
    const char* cs;
    int slots;           // X1..Xn
    bool has_jp3;        // JP3 1-2: address buffers off, CPU card in any slot (PKT-1)
};

const std::vector<const CardInfo*>& card_catalog(void);
const CardInfo* card_info(const std::string& type);
const std::vector<const BusInfo*>& bus_catalog(void);
const BusInfo* bus_info(const std::string& type);
const char* card_class_name(int cls, bool czech);
