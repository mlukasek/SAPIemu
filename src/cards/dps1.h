#pragma once
// DPS-1 - instrument bus card (Tesla Liberec / Elstroj / Eltos-DIZ, 6XK 199 52): SAPI-1 as the controller
// of the IMS-2 bus (IEEE 488), docs/desky/DPS-1.md. Fixed ports 30h-37h:
//   30h-33h D2C (8255, mode 0): PA in  IFC-I REN-I EOI-I ATN-I SRQ-I ERR PON ZS (bit 0-7)
//                               PB in  V0-V3 (multi-line message from the PROM) EOIS RDY DAC DAV
//                               PC out IFC-O REN-O EOI-O ATN-O SRQ-O CEN RER LIST/TALK
//   34h-37h D1C (8255): PA = DIO in (listener: mode 1, strobed by DAV), PB = DIO out (talker: mode 1,
//                       /OBF B starts the handshake, /ACK B = NDAC), PC6 IESRQ, PC7 IEZS
// The logic is the one of the schematic (Tesla scan, the replica of EC1045, netlist in docs):
//   - the drivers MH8641 (inverting, open collector) work when the flip-flop PON is 0 (RER = 0 clears it,
//     /RES sets it); ATN, EOI, IFC, REN only with CEN = 1 (the controller);
//   - talker: /OBF B starts the shift register 74164 on F2: 0.5 us later the flip-flop ERR takes "NDAC and
//     NRFD both released" (nobody listens), 2.5 us later DAV is set when NRFD is released and no ERR;
//     NDAC released (all accepted) is /ACK B: /OBF B high, DAV off;
//   - listener: DAV strobes the data into port A (STB A), IBF A drives NRFD, NDAC = not (IBF A and DAV);
//     with LIST and EOI the strobe is held (the parallel poll);
//   - ZS (change of state) on an edge of ATN, EOI, REN, IFC, on SRQ and ERR going on; RER = 1 clears ZS, ERR;
//   - PROM D3D 512 x 4 (74S571): DIO with ATN -> the number of a multi-line message, latched by DAV with
//     EOI (74174) for PB;
//   - /INT0 = ZS and IEZS, or SRQ and IESRQ, or INTR A, or INTR B.
// One assumption against the schematic: LIST/TALK (D2C PC7) blocks the own NDAC of the card when it talks,
// as the manual and BASIMS say ("0EH ... zablokovani vlastniho signalu NDAC"). In the schematic it gates only
// the EOI strobe; with the card holding NDAC while it talks, no byte would ever be acknowledged.
#include <stdint.h>
#include <vector>
#include "chips/i8255.h"
#include "core/card.h"
#include "core/ims2.h"

class Dps1 : public Card, public Ims2Node {
public:
    Dps1();
    ~Dps1() override;

    // Card
    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int irq_lines() override;
    void state(StateIO& io) override;
    void describe(nlohmann::json& out) override;
    int decode_io(uint8_t port, const char** what) override;

    // Ims2Node
    const char* node_name() const override { return "DPS-1"; }
    void lines_changed(uint64_t t) override;
    uint64_t next_event() const override;
    void on_event(uint64_t t) override;

    // for the window
    bool pon_ff(void) const { return pon; }
    bool controller(void) const { return cen; }
    bool listening(void) const { return list; }
    bool cable(void) const { return ims != nullptr; }

private:
    struct Port : public I8255Peripheral {
        Dps1* card = nullptr;
        int which = 0;   // 0 = D2C (control), 1 = D1C (data)
        uint8_t pins_a() override;
        uint8_t pins_b() override;
        uint8_t pins_c() override;
        void outputs_changed(int port, uint8_t value) override;
    };
    I8255 d2c, d1c;
    Port p2, p1;
    std::vector<uint8_t> prom;   // D3D, 512 x 4
    bool jp1 = true;             // IFC from the RESET button (/RTL)
    bool jp2 = false;            // parallel poll answer on DIO1

    // flip-flops and latches
    bool pon = true;             // drivers off
    bool err = false;
    bool zs = false;
    bool davo = false;           // own DAV (the latch of D2A)
    bool obfb = false;           // /OBF B low: a byte to send
    uint64_t obf_t = 0;          // when /OBF B went low (the shift register runs from here)
    bool qa = false, qe = false; // outputs of the shift register
    uint8_t vbits = 0;           // D4D: message number V0-V3
    bool eois = false;           // D4D: EOI at the start of DAV
    uint8_t prev_ctl = 0;        // received lines, for the edges
    bool prev_err = false;
    uint64_t ifc_until = 0;      // JP1: IFC while RESET
    // port C pins seen by the logic
    bool cen = false, list = false, rer = true;

    void sync(void);             // the bench up to now, attach to its bus
    void evaluate(uint64_t t);   // the gates and flip-flops, the drives on the bus
    void changed_by_cpu(void);
    uint64_t us(double v) const;
};
