#pragma once
// ZRMD-1 / ZRD-1V - battery backed RAM disk, up to 8x 512 KB SRAM accessed
// through I/O ports only (see docs/desky/ZRMD-1.md).
//   base+0 (60h)  data read/write, low address byte counts up after each access
//                 (wraps within 256 bytes: the counter carry is not connected)
//   base+1 (61h)  write: A0-A7     base+2 (62h) write: A8-A15
//   base+3 (63h)  write: A16-A21 (b0-5), b6/b7 -> MAP1/MAP2 when JP2 is fitted
//   base+10h..1Fh (70-7Fh) RTC 72421 (ZRMD-1 only)
// Write protection: DIP switches S1-S4, one per 1 MB (ZRMD-1 only).
// ZRD-1V rev 2.1 (parameter revision): a write lock and the battery voltage.
//   - 74LS123 (IC21B): every access to 62h opens a window of 20 ms; writes to 60h happen only
//     inside it. A write to 60h outside it is dropped and sets the lock (RS latch IC13C/B),
//     which only /RES clears (MikroMon locks the card this way at start).
//   - LEDs: red WRITE PROTECT = locked, green ACCESS = 20 ms after any access to 60h-63h.
//   - MAX187 (12 bit, 4.096 V reference) measures the backup battery: /CS is the 62h window,
//     each read of 61h clocks it, DOUT is D5. Code = millivolts.
//   - A read of 61h-63h loads the address register with what the undriven internal bus holds
//     (read as FFh; 61h also has DOUT on D5). Rev 1.1 the same, without DOUT.
#include <vector>
#include "chips/rtc72421.h"
#include "core/devices.h"

class Zrmd1 : public Card {
public:
    // ZRD-1V rev 2.1 panel: bit 0 ZAM = the write lock (red), bit 1 DSK = access to 60h-63h,
    // lit 20 ms (green). Rev 1.1 has no LEDs.
    unsigned panel_leds() override;
    char panel_led_color(int n) override;
    std::string live_status() override;
    // While running: the mode, and on ZRMD-1 the write enable switches S1-S4.
    bool live_param(const char* path) const override;
    bool set_live_param(const char* path, const nlohmann::json& v, std::string& err) override;
    explicit Zrmd1(bool zrd1v);
    ~Zrmd1() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
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
    bool locked(void) const { return lock; }

private:
    uint64_t access_at = 0;   // bus time of the last access (DSK LED: 60h on rev 1.1, 60h-63h on 2.1)
    bool accessed = false;
    bool rev2 = true;         // ZRD-1V rev 2.1 (lock, LEDs, MAX187); rev 1.1 otherwise
    bool lock = false;        // rev 2.1: writes locked until /RES
    uint64_t window_at = 0;   // rev 2.1: bus time of the last access to 62h (74LS123 trigger)
    bool window_seen = false;
    int battery_mv = 3000;    // rev 2.1: backup battery voltage (2x AAA)
    // MAX187: conversion started when the window opened, bits clocked out by reads of 61h
    uint64_t adc_start = 0;
    int adc_clocks = 0;
    uint16_t adc_code = 0;
    bool window(void) const;
    void trigger_window(void);
    uint8_t adc_dout(void) const;
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
