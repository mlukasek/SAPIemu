#pragma once
// MFC-1A - mini floppy controller of SAPI-1 / SAPI-1.5 (Tesla Liberec 1989), one board, see
// docs/desky/MFC-1A.md.
//
// 8272 (uPD765), DMA 8257 channel 2 and the address decoder PROM DD9 (74S287, inputs A0-A7,
// outputs D0 = /CS 8257, D1 = /CS 8272, D2 = /SEL (the card answers), D3 = drive register);
// the standard PROM gives 04H, 05H, 08H 8257 (channel 2 address and count, mode/status),
// 0CH/0DH 8272 (status, data), 0FH the drive register (write only, WREG = /IOW OR /REG
// clocks DD11 74LS174 and DD23A 74LS74, /RES clears DD11), by the schematic:
//   D0 D1 (Q1 Q2) = selected drive, DD19 74145 -> /SEL0-3;
//   D2 D3 (Q3 Q4) = drive with its motor on, DD18 74145 -> /MO0-3;
//   D4 (DD11 pin 14, Q6) = INT enable, DD12A: INT = INT of the 8272 AND Q6;
//   D5 (pin 13, Q5) = reset of the 8272, DD4B: RES72 = Q5 OR RES -> 8272 RESET;
//   D6 (DD23A) = drives enabled (DD19); DD23B (D = RES72, clock RCLK) clears DD23A while the
//   8272 is in reset, so no drive is selected then.
// The comments of the EC1045 BIOS ECBIM8 (driver MFC-1 V281211JB) have D4 and D5 swapped.
// The DS0/DS1 outputs of the 8272 are not used: the register selects the drive whose
// signals (READY, INDEX, TRACK 0, WP, data) the 8272 sees. The board serves 5.25" and 8"
// drives; emulated are 8" SS SD drives (IBM 3740). READY comes from the drive (a disk is in),
// or is grounded as for the Consul 7113 (parameter rdy_ground).
//
// D5 (reset): by the schematic it holds the 8272 in reset while 1. The BIOS ECBIM8 writes FFh
// at its initialization and would hang there; reset_d5 = "pulse" (default, a DEVIATION from the
// schematic) only resets the chip on the rising edge, so that BIOS works. reset_d5 = "level" is
// the schematic. In pulse mode the drive stays selected (with DD23B the reset would deselect
// it, and ECBIM8 would then wait in RECALIBRATE for the TRACK 0 of no drive).
#include "cards/fdc8272.h"

class Mfc1a : public Fdc8272Card {
public:
    Mfc1a() : Fdc8272Card("MFC-1A") {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int irq_lines() override;
    void state(StateIO& io) override;
    int decode_io(uint8_t port, const char** what) override;

    // I8272Host: the drive register selects the drive
    FloppyDrive* fdc_drive(int unit) override { return (reg & 0x40) ? &drives[reg & 3] : nullptr; }
    bool fdc_ready(int unit) override { return (reg & 0x40) && (rdy_ground || drives[reg & 3].present()); }

private:
    uint8_t reg = 0;       // 0FH
    bool reset_level = false;   // D5 holds the reset (schematic) instead of a pulse
};
