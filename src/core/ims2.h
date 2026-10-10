#pragma once
// IMS-2 (the Czechoslovak IEEE 488 / HP-IB / IEC 625), the instrument bus of the DPS-1 card
// (docs/desky/DPS-1.md): 8 data lines DIO1-8, the handshake DAV, NRFD, NDAC and the management lines
// ATN, EOI, IFC, REN, SRQ. All lines are open collector with negative logic: a line is asserted (true)
// when any node pulls it low. Here a line is a bit, 1 = asserted, and the data byte is logical
// (DIO1 = bit 0, 1 = low on the cable).
//
// Nodes (the DPS-1, the instruments) drive their own masks; the bus is the OR of all of them. A node
// reacts at once to a change of the lines (gates, latches) in lines_changed() and keeps its own timed
// events (delays of the instrument, the T1 time of DPS-1) through next_event() / on_event(). Time is the
// bus time of the machine (CPU cycles, Bus::cpu_hz).
#include <stdint.h>
#include <string>
#include <vector>

class StateIO;

enum Ims2Line {
    IMS_DAV = 0x01,
    IMS_NRFD = 0x02,
    IMS_NDAC = 0x04,
    IMS_ATN = 0x08,
    IMS_EOI = 0x10,
    IMS_IFC = 0x20,
    IMS_REN = 0x40,
    IMS_SRQ = 0x80,
};

struct Ims2Bus;

class Ims2Node {
public:
    virtual ~Ims2Node() {}
    virtual const char* node_name() const = 0;
    // the lines changed (or the node was attached): react at once
    virtual void lines_changed(uint64_t t) {}
    // the next timed event of the node (bus time), UINT64_MAX = none
    virtual uint64_t next_event() const { return UINT64_MAX; }
    virtual void on_event(uint64_t t) {}

    uint8_t dio = 0;          // data this node drives (logical, 1 = asserted)
    uint8_t ctl = 0;          // control lines this node drives (Ims2Line)
    Ims2Bus* ims = nullptr;
};

// One transfer on the bus (a byte taken at the start of DAV) or a change of IFC / REN, for the
// bus monitor of the window.
struct Ims2Event {
    uint64_t t = 0;
    uint8_t byte = 0;
    uint8_t lines = 0;        // ATN, EOI at the byte; IFC / REN after a change
    bool transfer = true;     // false: a change of IFC or REN
    std::string from;         // the node driving DAV (the talker)
};

struct Ims2Bus {
    std::vector<Ims2Node*> nodes;
    uint8_t dio_lines = 0;    // OR of the nodes
    uint8_t ctl_lines = 0;
    uint64_t t = 0;           // the bus is settled up to this time
    double hz = 2000000.0;    // bus time units per second (Bus::cpu_hz)

    void attach(Ims2Node* n);
    void detach(Ims2Node* n);
    // loading a state: attach without resolving the lines (they come from the state; resolving them
    // with only some nodes attached would drop the drives of the others and the instruments would react)
    void attach_loaded(Ims2Node* n, bool first);
    // a node changed what it drives: resolve the lines, let the nodes react until it is stable
    void settle(uint64_t at);
    // run the timed events of the nodes up to `now`
    void advance(uint64_t now);
    uint64_t us(double microseconds) const { return (uint64_t)(microseconds * hz / 1e6 + 0.5); }
    bool asserted(uint8_t line) const { return (ctl_lines & line) != 0; }

    // monitor: a ring of the last transfers
    static const size_t LOG_SIZE = 256;
    std::vector<Ims2Event> log;
    size_t log_next = 0;
    uint64_t transfers = 0;   // all bytes since power on
    void log_copy(std::vector<Ims2Event>& out) const;   // oldest first
    void clear_log(void);
    void state(StateIO& io);

private:
    void record(uint8_t old_ctl);
    bool settling = false;
};

// The name of a byte sent with ATN (UNL, LAD 3, DCL...), or "" for a data byte.
std::string ims2_command_name(uint8_t b);
