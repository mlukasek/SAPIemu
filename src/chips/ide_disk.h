#pragma once
// ATA (IDE) disk backed by a raw image file (.img, 512-byte sectors, no MBR
// required). Implements the task file registers and the PIO commands used by
// SAPI software: IDENTIFY, READ/WRITE SECTORS (LBA28 and CHS), and accepts the
// usual housekeeping commands. Commands complete immediately.
#include <stdint.h>
#include <stdio.h>
#include <string>

class IdeDisk {
public:
    IdeDisk();
    ~IdeDisk();

    bool open(const std::string& path, bool readonly, std::string& err);
    void close(void);
    bool present(void) const { return file != nullptr; }
    uint32_t sector_count(void) const { return sectors; }
    const std::string& path(void) const { return image_path; }
    bool is_readonly(void) const { return readonly; }

    void reset(void);   // hardware or software reset
    // cs1 = command block (/CS1FX), otherwise control block (/CS3FX); reg = DA2..DA0
    uint16_t read_reg(bool cs1, int reg);
    void write_reg(bool cs1, int reg, uint16_t value);
    void flush(void);

    // status bits
    enum { ST_BSY = 0x80, ST_DRDY = 0x40, ST_DF = 0x20, ST_DSC = 0x10, ST_DRQ = 0x08, ST_ERR = 0x01 };
    enum { ER_ABRT = 0x04, ER_IDNF = 0x10, ER_UNC = 0x40 };

private:
    void command(uint8_t cmd);
    bool current_lba(uint32_t& lba) const;
    void advance_address(void);
    bool load_sector(uint32_t lba);
    bool store_sector(uint32_t lba);
    void identify(void);
    void abort_cmd(uint8_t err);

    FILE* file = nullptr;
    std::string image_path;
    bool readonly = false;
    uint32_t sectors = 0;
    int cyls = 0, heads = 16, spt = 63;

    uint8_t error = 0, features = 0, count = 0, lba0 = 0, lba1 = 0, lba2 = 0, head = 0;
    uint8_t status = 0, devctl = 0;
    uint8_t buffer[512];
    int buf_pos = 0;
    enum Xfer { XFER_NONE, XFER_READ, XFER_WRITE } xfer = XFER_NONE;
    int remaining = 0;          // sectors left in the current command
};
