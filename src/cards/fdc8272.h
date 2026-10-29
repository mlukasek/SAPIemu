#pragma once
// Common part of the disk controllers with the 8272 (uPD765) and the DMA controller 8257
// on channel 2: FDC-1 (docs/desky/FDC-1.md) and MFC-1A (docs/desky/MFC-1A.md).
//
// The board specific parts (address decoder PROM, control registers, which drive the
// signals of the 8272 reach, READY) are in the derived cards. Drives: 8" SS SD (IBM 3740),
// images as for RPD-1 (chips/floppy.h).
#include "core/card.h"
#include "chips/floppy.h"
#include "chips/i8257.h"
#include "chips/i8272.h"

class Fdc8272Card : public Card, public I8272Host {
public:
    explicit Fdc8272Card(const char* type) : Card(type) {}
    ~Fdc8272Card() override;

    void advance(uint64_t now) override;
    void flush() override;
    bool unsaved() override;
    void describe(nlohmann::json& out) override;
    std::string live_status() override;
    FloppyDrive* floppy_drive(int index) override { return index >= 0 && index < DRIVES ? &drives[index] : nullptr; }
    bool floppy_insert(int index, const std::string& image, bool wp, std::string& err) override;
    void floppy_eject(int index) override;

    // I8272Host: DMA channel 2 of the 8257
    bool fdc_dma(bool to_memory, uint8_t& byte, bool& tc) override;
    uint64_t fdc_turning_from(int unit) override { return turning_from[unit & 3]; }
    void fdc_reading(int unit) override;

    static const int DRIVES = 4;
    FloppyDrive drives[DRIVES];

protected:
    // configuration shared by the boards: decoder PROM (256 entries), drives, working copy,
    // /RDY grounded
    bool configure_common(const nlohmann::json& cfg, const char* default_decoder, const char* prom_name,
                          std::string& err);
    void reset_chips(void);
    // the 8272 is held in reset by a register bit of the board
    void set_fdc_reset(bool on);
    // MOTOR ON of the drives (bit per drive) from a board register; with motor_line the disk turns
    // spinup_ms after its motor is switched on and stops when it is switched off
    void set_motors(uint8_t mask);
    void state_common(StateIO& io);

    uint8_t prom[256];
    I8272 fdc;
    I8257 dma;
    bool fdc_in_reset = false;
    bool rdy_ground = true;
    bool work_copy = true;
    bool motor_line = false;          // the drives turn only with MOTOR ON (PC drives); else always (8" AC motors)
    uint32_t spinup_ms = 500;
    uint8_t motor_mask = 0;
    uint64_t turning_from[DRIVES] = {};
};
