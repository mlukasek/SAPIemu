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
// The drives get /SEL0-3 from the 8272 (DS1 DS0 through a 74145), the head /SS1 from its HD.
// READY: on the boards as supplied /RDY (X3-20) is grounded for Consul 7113 drives, so all four
// units are ready. Disks: 8" SS SD and PC formats (chips/floppy.h), MFM HD with SA2 = 8".
#include "cards/fdc8272.h"

class Fdc1 : public Fdc8272Card {
public:
    Fdc1() : Fdc8272Card("FDC-1") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int irq_lines() override;
    void state(StateIO& io) override;
    int decode_io(uint8_t port, const char** what) override;

    // I8272Host: the 8272 selects the drive itself (DS1 DS0)
    FloppyDrive* fdc_drive(int unit) override { return &drives[unit & 3]; }
    bool fdc_ready(int unit) override { return rdy_ground || drives[unit & 3].present(); }
    // Clock of the 8272 by the switch SA2 S1-S4 of the drive (an assumption): 8" = 8 MHz (FM 250,
    // MFM 500 kbit/s: 8" disks and PC HD drives), 5.25" = 4 MHz (FM 125, MFM 250: DD drives).
    int fdc_kbps(int unit, bool mfm) override
    {
        int k = mfm ? 500 : 250;
        return (sa2[unit & 3] & 2) ? k : k / 2;
    }

private:
    uint8_t ctrl = 0;      // 0EH
    uint8_t motors = 0;    // 0FH
    uint8_t sa2[4] = {};   // switches SA2 per drive: D1 = 8", D0 = DD (read at 0EH)
};
