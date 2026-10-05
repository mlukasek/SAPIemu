#pragma once
// FDC-1 - floppy disk controller of SAPI-1, boards FDC-1A + FDC-1B (two slots), see
// docs/desky/FDC-1.md.
//
// FDC-1A: 8272 (uPD765) and the DMA controller 8257, channel 2 (DREQ2/DACK2, TC to the
// 8272). Ports by PROM D5B (74S287, inputs A0-A7, outputs D0 = /CS 8257, D1 = /CS 8272,
// D2 = buffers for both, D3 = FDC-1B registers); the standard PROM gives 04-0BH 8257,
// 0CH/0DH 8272 (status, data), 0EH/0FH FDC-1B.
// FDC-1B: data separator, drive interface and the registers
//   0EH write: D1 D0 = select S0-S3 (own signals on X3), D2 = 1 reset of the 8272, D3 = INT0
//              enable; read: the switches SA2 of the drive selected by D1 D0: D1 = 1 8" (S1-S4),
//              D0 = DD (S5-S8, the polarity is assumed); FORM6 checks D1 before formatting;
//   0FH write: D3-D0 motors M3-M0, D7-D4 head load H3-H0 (own signals on X3).
// The drives get /SEL0-3 from the 8272 (DS1 DS0 through a 74145). READY: on the boards as
// supplied /RDY (X3-20) is grounded for Consul 7113 drives, so all four units are ready.
// Drives: 8" SS SD (IBM 3740), images as for RPD-1 (chips/floppy.h).
#include "core/card.h"
#include "chips/floppy.h"
#include "chips/i8257.h"
#include "chips/i8272.h"

class Fdc1 : public Card, public I8272Host {
public:
    Fdc1() : Card("FDC-1") {}
    ~Fdc1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int irq_lines() override;
    void flush() override;
    void describe(nlohmann::json& out) override;
    std::string live_status() override;
    void state(StateIO& io) override;
    int decode_io(uint8_t port, const char** what) override;
    FloppyDrive* floppy_drive(int index) override { return index >= 0 && index < DRIVES ? &drives[index] : nullptr; }
    bool floppy_insert(int index, const std::string& image, bool wp, std::string& err) override;
    void floppy_eject(int index) override;

    // I8272Host
    FloppyDrive* fdc_drive(int unit) override { return &drives[unit & 3]; }
    bool fdc_ready(int unit) override;
    bool fdc_dma(bool to_memory, uint8_t& byte, bool& tc) override;

    static const int DRIVES = 4;
    FloppyDrive drives[DRIVES];

private:
    uint8_t prom[256];
    I8272 fdc;
    I8257 dma;
    uint8_t ctrl = 0;      // 0EH
    uint8_t motors = 0;    // 0FH
    uint8_t sa2[4] = {};   // switches SA2 per drive: D1 = 8", D0 = DD (read at 0EH)
    bool rdy_ground = true;
    bool work_copy = true;
};
