#pragma once
// IDE-1 - two 8255: one bit-bangs an IDE disk (default 58h-5Bh), the other
// connects the PMD32-SD floppy emulator in mode 2 (default 5Ch-5Fh).
// See docs/desky/IDE-1.md.
#include "chips/i8255.h"
#include "chips/ide_disk.h"
#include "chips/pmd32sd.h"
#include "core/devices.h"

class Ide1 : public Card, public I8255Peripheral {
public:
    Ide1();
    ~Ide1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    void flush() override;
    void describe(nlohmann::json& out) override;
    int decode_io(uint8_t port, const char** what) override;

    // I8255Peripheral (IDE side)
    uint8_t pins_a() override { return data_lo; }
    uint8_t pins_b() override { return data_hi; }
    void outputs_changed(int port, uint8_t value) override;

    Pmd32Sd* pmd32(void) { return pmd_enabled ? &pmd : nullptr; }
    IdeDisk* disk(void) { return ide_enabled ? &ide : nullptr; }

private:
    bool ide_enabled = true, pmd_enabled = true;
    uint8_t base_ide = 0x58, base_pmd = 0x5C;
    I8255 ppi_ide, ppi_pmd;
    IdeDisk ide;
    Pmd32Sd pmd;
    uint8_t last_pc = 0;
    uint8_t data_lo = 0xFF, data_hi = 0xFF;   // data lines driven by the disk during /DIOR
};

// Create a raw disk image of `mb` megabytes filled with E5h (empty CP/M directory).
bool ide_create_image(const std::string& path, int mb, std::string& err);
