#pragma once
// MFC-1A - mini floppy controller of SAPI-1 / SAPI-1.5 (Tesla Liberec 1989), one board, see
// docs/desky/MFC-1A.md.
//
// 8272 (uPD765), DMA 8257 channel 2 and the address decoder PROM DD9 (74S287, inputs A0-A7,
// outputs D0 = /CS 8257, D1 = /CS 8272, D2 = /SEL (the card answers), D3 = drive register);
// the standard PROM gives 04H, 05H, 08H 8257 (channel 2 address and count, mode/status),
// 0CH/0DH 8272 (status, data), 0FH the drive register (write only, WREG = /IOW OR /REG clocks
// DD11 74LS174 and DD23A 74LS74; /RES clears DD11):
//   D1 D0 = selected drive (DD19 74145 -> /SEL0-3), D3 D2 = drive with its motor on (DD18
//   74145 -> /MO0-3), D7 unused, and by the wiring (parameter `wiring`):
//   "tesla"  (original Tesla schematic scan, the manual): D4 = reset of the 8272 (DD11 Q5 ->
//            DD4B OR RES -> RES72 -> RESET), D5 = INT enable (Q6 -> DD12A), D6 = 1 drives OFF
//            (DD23A Q to input C of DD18 and DD19: no select, no motor);
//   "ec1045" (redraw by EC1045, 2010, the replica): D5 = reset, D4 = INT enable, D6 = 1 drives
//            ON (DD23A /Q to the decoders) - what the BIOS ECBIM8 expects.
// DD23B (D = RES72, clock RCLK) clears DD23A while the 8272 is in reset.
// The DS0/DS1 outputs of the 8272 are not used: the register selects the drive whose
// signals (READY, INDEX, TRACK 0, WP, data) the 8272 sees. The board serves 5.25" and 8"
// drives: 8" SS SD and DD disks of chips/floppy.h (MFM 250 kbit/s). READY comes from the drive
// (a disk is in), or is grounded as for the Consul 7113 (parameter rdy_ground).
//
// Reset bit (parameter `reset_mode`): "level" = the schematic, 1 holds the 8272 in reset.
// The BIOS ECBIM8 writes FFh at its initialization (a RET is missing in INIT_MFC) and would
// hang there; "pulse" (a DEVIATION) only resets the chip on the rising edge and keeps the
// drive selected, so that BIOS works.
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
    FloppyDrive* fdc_drive(int unit) override { return drives_on() ? &drives[reg & 3] : nullptr; }
    uint64_t fdc_turning_from(int unit) override { return turning_from[reg & 3]; }
    // RDY tied active (rdy_ground) does not depend on the drive selection
    bool fdc_ready(int unit) override { return rdy_ground || (drives_on() && drives[reg & 3].present()); }
    // CLK 8 MHz for SD (FM), 4 MHz for DD (MFM), as in the manual: 250 kbit/s either way,
    // so 8" SD and DD disks, no HD.
    int fdc_kbps(int unit, bool mfm) override { return 250; }

private:
    bool drives_on(void) const { return tesla ? !dd23 : dd23; }
    void update_motors(void);

    uint8_t reg = 0;            // 0FH (DD11)
    bool dd23 = false;          // DD23A Q (D6)
    bool tesla = true;          // wiring of the register: original Tesla / EC1045 redraw
    bool reset_level = true;    // the reset bit holds the reset (schematic) instead of a pulse
    uint8_t reset_bit = 0x10, int_bit = 0x20;
};
