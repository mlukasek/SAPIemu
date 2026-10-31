#pragma once
// UV EPROM 2708 (1 KB), 2716 (2 KB) and 2732 (4 KB) in a socket: the chip is evaluated from the levels of its pins,
// by the modes of the data sheets (Intel 2716, 2732; Oki MSM2708AS, NTE2708), so a programmer or a socket wired for
// another type behaves as the real chip would. A0-A9 (pins 8-1, 23, 22), D0-D7, VCC (24) and VSS (12) are the same
// on all three; pins 18, 19, 20 and 21 differ:
//   2716: 18 = /CE/PGM, 19 = A10, 20 = /OE, 21 = Vpp (5 V reading, 25 V programming)
//     read            /CE/PGM L, /OE L, Vpp 5 V  -> D out
//     standby         /CE/PGM H, Vpp 5 V         -> high Z
//     program         /CE/PGM H pulse (50 ms), /OE H, Vpp 25 V -> D in
//     program verify  /CE/PGM L, /OE L, Vpp 25 V -> D out
//     program inhibit /CE/PGM L, /OE H, Vpp 25 V -> high Z
//   2732: 18 = /CE, 19 = A10, 20 = /OE/Vpp, 21 = A11
//     read            /CE L, /OE/Vpp L           -> D out (program verify is the same)
//     standby         /CE H                      -> high Z (with /OE/Vpp 25 V: program inhibit)
//     program         /CE L pulse (50 ms), /OE/Vpp 25 V -> D in
//     output disabled /CE L, /OE/Vpp 5 V         -> high Z
//   2708: 18 = PROGRAM, 19 = VDD +12 V, 20 = CS/WE, 21 = VBB -5 V (the supplies must be there, else high Z; VBB
//         before VCC and VDD and off last, Oki note 1: VCC is always on in a socket, so 0 V on VBB is flagged)
//     read            CS/WE L, PROGRAM 0 V       -> D out (5 V on PROGRAM: read too, outside the data sheet)
//     deselect        CS/WE H (5 V)              -> high Z
//     program         CS/WE 12 V, PROGRAM pulse 25 V (0.1-1 ms, N loops over all words, N x width >= 100 ms) -> D in
//     program inhibit CS/WE 12 V, PROGRAM 0 V    -> high Z
//     25 V on PROGRAM with CS/WE L               -> high Z (not in the data sheets: assumption)
// Levels: 0 V = L, 5 V = H, 25 V = H with the programming voltage, +12 V and -5 V (the 2708 supplies; on a logic pin
// +12 V is H, -5 V L). A 2716 with 0 V on Vpp does not drive its outputs (assumption after DSE.DOC "bez 30 V
// nefunguje ani cteni", docs/desky/DSE-1.md). Programming clears bits only (1 -> 0); the length of the pulse is the
// business of the programmer (Dse1 measures it).
// The image is a plain binary file: 1024 B = 2708, 2048 B = 2716, 4096 B = 2732.
#include <stdint.h>
#include <string>
#include <vector>

enum EpromLevel { EPL_L = 0, EPL_H = 1, EPL_VPP = 2, EPL_12 = 3, EPL_M5 = 4 };   // 0 V, 5 V, 25 V, +12 V, -5 V

// Levels on the pins of the socket whose function depends on the type, the address on A0-A9.
struct EpromPins {
    uint16_t a = 0;       // A0-A9
    uint8_t p18 = EPL_H;  // EpromLevel
    uint8_t p19 = EPL_L;  // A10 (2716, 2732), VDD (2708)
    uint8_t p20 = EPL_H;
    uint8_t p21 = EPL_L;
};

enum EpromMode {
    EPM_EMPTY = 0,        // no chip in the socket
    EPM_STANDBY,
    EPM_READ,             // outputs on (2716 also program verify at 25 V)
    EPM_OUTPUT_OFF,       // selected, outputs off (2716 at 25 V, 2708 at CS/WE 12 V: program inhibit)
    EPM_PROGRAM,          // the programming pulse is on
    EPM_NO_VPP,           // 2716 with 0 V on Vpp: outputs off (assumption)
    EPM_NO_SUPPLY,        // 2708 without +12 V on pin 19 or -5 V on pin 21: outputs off
};

// Overvoltage on a pin (bits of Eprom27::overvoltage): could damage a real chip
enum EpromOver {
    EPO_25V_P18 = 1,          // 25 V on pin 18 of a 2716 / 2732 (/CE/PGM, /CE)
    EPO_25V_OE = 2,           // 25 V on /OE (pin 20) of a 2716
    EPO_25V_A11 = 4,          // 25 V on A11 (pin 21) of a 2732
    EPO_12V_A10 = 8,          // +12 V on A10 (pin 19) of a 2716 / 2732
    EPO_12V_OE = 16,          // +12 V on /OE (pin 20) of a 2716
    EPO_M5V_P21 = 32,         // -5 V on pin 21 of a 2716 (Vpp) / 2732 (A11)
    EPO_VBB_PLUS = 64,        // 5 V or 25 V on VBB (pin 21) of a 2708, it needs -5 V
    EPO_25V_CSWE = 128,       // 25 V on CS/WE (pin 20) of a 2708
    EPO_NO_VBB = 256,         // 0 V on VBB (pin 21) of a 2708 while VCC (pin 24) has 5 V: VBB must come first (Oki)
};

struct Eprom27 {
    enum { NONE = 0, T2708 = 2708, T2716 = 2716, T2732 = 2732 };
    int type = NONE;
    std::vector<uint8_t> data;
    std::string image;    // as configured (machine description)
    std::string path;     // the file used (working copy)
    bool dirty = false;

    static int type_of_size(size_t n);   // T2708, T2716, T2732, NONE for another size
    static size_t size_of_type(int t);   // 1024, 2048, 4096, 0
    bool loaded(void) const { return type != NONE; }
    // read the image at `file` (resolved path); false + err for a missing file or another size
    bool load(const std::string& file, std::string& err);
    bool save(std::string& err);   // writes `path` when dirty
    void unload(void);

    int mode(const EpromPins& p) const;   // EpromMode
    // the cell the pins address (2716, 2732: A10 = pin 19; 2732: A11 = pin 21)
    uint16_t cell(const EpromPins& p) const;
    // D0-D7 driven by the chip: true + the byte
    bool output(const EpromPins& p, uint8_t& d) const;
    // voltages a real chip of this type could be damaged by: EpromOver bits, 0 = none
    unsigned overvoltage(const EpromPins& p) const;
    // a programming pulse long enough: the cell gets AND d (bits 1 -> 0 only); true when the content changed
    bool program(uint16_t addr, uint8_t d);
};
