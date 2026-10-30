#pragma once
// UV EPROM 2716 (2 KB) and 2732 (4 KB) in a socket: the chip is evaluated from the levels of its pins, by the
// modes of the data sheets (Intel 2716, 2732), so a programmer or a socket wired for the other type behaves as
// the real chip would. A0-A10 (pins 8-1, 23, 22, 19), D0-D7 and the supply are the same on both; pins 18, 20
// and 21 differ:
//   2716: 18 = /CE/PGM, 20 = /OE, 21 = Vpp (5 V reading, 25 V programming)
//     read            /CE/PGM L, /OE L, Vpp 5 V  -> D out
//     standby         /CE/PGM H, Vpp 5 V         -> high Z
//     program         /CE/PGM H pulse (50 ms), /OE H, Vpp 25 V -> D in
//     program verify  /CE/PGM L, /OE L, Vpp 25 V -> D out
//     program inhibit /CE/PGM L, /OE H, Vpp 25 V -> high Z
//   2732: 18 = /CE, 20 = /OE/Vpp, 21 = A11
//     read            /CE L, /OE/Vpp L           -> D out (program verify is the same)
//     standby         /CE H                      -> high Z (with /OE/Vpp 25 V: program inhibit)
//     program         /CE L pulse (50 ms), /OE/Vpp 25 V -> D in
//     output disabled /CE L, /OE/Vpp 5 V         -> high Z
// Levels: 0 V = L, 5 V = H, 25 V = H with the programming voltage. A 2716 with 0 V on Vpp does not drive its
// outputs (assumption after DSE.DOC "bez 30 V nefunguje ani cteni", docs/desky/DSE-1.md). Programming clears
// bits only (1 -> 0); the length of the pulse is the business of the programmer (Dse1 measures it).
// The image is a plain binary file: 2048 B = 2716, 4096 B = 2732.
#include <stdint.h>
#include <string>
#include <vector>

enum EpromLevel { EPL_L = 0, EPL_H = 1, EPL_VPP = 2 };   // 0 V, 5 V, 25 V

// Levels on the pins of the socket whose function depends on the type, the address on A0-A10.
struct EpromPins {
    uint16_t a = 0;       // A0-A10
    uint8_t p18 = EPL_H;  // EpromLevel
    uint8_t p20 = EPL_H;
    uint8_t p21 = EPL_L;
};

enum EpromMode {
    EPM_EMPTY = 0,        // no chip in the socket
    EPM_STANDBY,
    EPM_READ,             // outputs on (2716 also program verify at 25 V)
    EPM_OUTPUT_OFF,       // selected, outputs off (2716 at 25 V: program inhibit)
    EPM_PROGRAM,          // the programming pulse is on
    EPM_NO_VPP,           // 2716 with 0 V on Vpp: outputs off (assumption)
};

struct Eprom27 {
    enum { NONE = 0, T2716 = 2716, T2732 = 2732 };
    int type = NONE;
    std::vector<uint8_t> data;
    std::string image;    // as configured (machine description)
    std::string path;     // the file used (working copy)
    bool dirty = false;

    static int type_of_size(size_t n);   // T2716, T2732, NONE for another size
    bool loaded(void) const { return type != NONE; }
    // read the image at `file` (resolved path); false + err for a missing file or another size
    bool load(const std::string& file, std::string& err);
    bool save(std::string& err);   // writes `path` when dirty
    void unload(void);

    int mode(const EpromPins& p) const;   // EpromMode
    // the cell the pins address (2732: A11 = pin 21)
    uint16_t cell(const EpromPins& p) const;
    // D0-D7 driven by the chip: true + the byte
    bool output(const EpromPins& p, uint8_t& d) const;
    // 25 V on a pin that is not the programming one (2716: 18 or 20, 2732: 18 or 21): the pin number, else 0
    int overvoltage(const EpromPins& p) const;
    // a programming pulse long enough: the cell gets AND d (bits 1 -> 0 only); true when the content changed
    bool program(uint16_t addr, uint8_t d);
};
