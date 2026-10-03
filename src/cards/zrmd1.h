#pragma once
// ZRMD-1 / ZRD-1V - battery backed RAM disk, up to 8x 512 KB SRAM accessed
// through I/O ports only (see docs/desky/ZRMD-1.md).
//   base+0 (60h)  data read/write, low address byte counts up after each access
//                 (wraps within 256 bytes: the counter carry is not connected)
//   base+1 (61h)  write: A0-A7     base+2 (62h) write: A8-A15
//   base+3 (63h)  write: A16-A21 (b0-5), b6/b7 -> MAP1/MAP2 when JP2 is fitted
//   base+10h..1Fh (70-7Fh) RTC 72421 (ZRMD-1 only)
// Write protection: DIP switches S1-S4, one per 1 MB (ZRMD-1 only).
#include <vector>
#include "chips/rtc72421.h"
#include "core/devices.h"

class Zrmd1 : public Card {
public:
    explicit Zrmd1(bool zrd1v);
    ~Zrmd1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int irq_lines() override;
    int map_lines() override;
    int decode_io(uint8_t port, const char** what) override;
    void flush() override;
    void describe(nlohmann::json& out) override;

    enum Mode { MODE_RW, MODE_NOSAVE, MODE_READONLY };

private:
    uint32_t address(void) const { return ((uint32_t)(a_hi & 0x3F) << 16) | ((uint32_t)a_mid << 8) | a_lo; }
    bool writable(uint32_t addr) const;
    bool save(void);

    bool is_zrd1v;
    uint8_t base = 0x60;
    int chips = 8;                  // 512 KB SRAMs fitted
    std::vector<uint8_t> mem;
    std::string image_path;         // file actually used (working copy)
    Mode mode = MODE_RW;
    bool write_enable[4] = { true, true, true, true };   // S1-S4 ON = writes allowed
    bool map_jp2 = false;           // port 63h bits 6/7 drive MAP1/MAP2
    bool rtc_enabled = true;
    int rtc_irq = 0;                // JP3: 0 none, IRQ_INT0, IRQ_INT1
    Rtc72421 rtc;

    uint8_t a_lo = 0, a_mid = 0, a_hi = 0;
    bool dirty = false;
    uint64_t dirty_since = 0;
};
