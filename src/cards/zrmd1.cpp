#include "cards/zrmd1.h"
#include "core/bus.h"
#include "core/ihex.h"
#include "core/log.h"
#include "core/util.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

using nlohmann::json;

static const uint32_t CHIP_SIZE = 512 * 1024;
// ZRD-1V rev 2.1: 74LS123 window (R6 100k, C25 680n, "20msec" in the schematic) and the
// MAX187 conversion time.
static const double WINDOW_SECONDS = 0.020;
static const double ADC_CONVERT_SECONDS = 8.5e-6;
// Autosave of a modified RAM disk after this much emulated time.
static const double AUTOSAVE_SECONDS = 5.0;

Zrmd1::Zrmd1(bool zrd1v) : Card(zrd1v ? "ZRD-1V" : "ZRMD-1"), is_zrd1v(zrd1v)
{
    if (zrd1v) rtc_enabled = false;
}

Zrmd1::~Zrmd1()
{
    flush();
}

static bool ends_with_hex(const std::string& p)
{
    std::string e = p.size() >= 4 ? p.substr(p.size() - 4) : "";
    for (char& c : e) c = (char)tolower((unsigned char)c);
    return e == ".hex";
}

bool Zrmd1::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_number(cfg, "base", v, 0x60, err)) return false;
    if (v < 0 || v > 0xE0 || (v & 0x03)) { err = "base must be a multiple of 4"; return false; }
    if (!is_zrd1v && v != 0x60) LOGW("%s: base %02Xh differs from the GAL decoder (60h)", name.c_str(), (int)v);
    base = (uint8_t)v;
    if (!cfg_number(cfg, "chips", v, 8, err)) return false;
    if (v < 1 || v > 8) { err = "chips must be 1..8 (512 KB each)"; return false; }
    chips = (int)v;
    if (!cfg_bool(cfg, "map_jp2", map_jp2, false, err)) return false;
    if (is_zrd1v) map_jp2 = false;   // ZRD-1V has no MAP outputs (mapping is on RAM-1V)
    if (is_zrd1v) {
        std::string rev;
        if (!cfg_string(cfg, "revision", rev, "2.1", err)) return false;
        if (rev == "2.1") rev2 = true;
        else if (rev == "1.1") rev2 = false;
        else { err = "revision must be 1.1 or 2.1"; return false; }
        if (!cfg_number(cfg, "battery_mv", v, 3000, err)) return false;
        if (v < 0 || v > 4095) { err = "battery_mv must be 0..4095 (MAX187 range)"; return false; }
        battery_mv = (int)v;
    } else {
        rev2 = false;
    }

    std::string mode_s;
    if (!cfg_string(cfg, "mode", mode_s, "rw", err)) return false;
    if (mode_s == "rw") mode = MODE_RW;
    else if (mode_s == "nosave") mode = MODE_NOSAVE;
    else if (mode_s == "readonly") mode = MODE_READONLY;
    else { err = "mode must be rw, nosave or readonly"; return false; }
    if (mode == MODE_RW && !battery_files_enabled()) mode = MODE_NOSAVE;   // tests: work/ stays untouched

    if (!is_zrd1v) {
        if (cfg.contains("write_enable")) {
            const json& we = cfg["write_enable"];
            if (!we.is_array() || we.size() != 4) { err = "write_enable must be an array of 4 booleans (S1-S4)"; return false; }
            for (int i = 0; i < 4; i++) write_enable[i] = we[i].get<bool>();
        }
        if (!cfg_bool(cfg, "rtc", rtc_enabled, true, err)) return false;
        std::string irq;
        if (!cfg_string(cfg, "rtc_int", irq, "none", err)) return false;
        if (irq == "none") rtc_irq = 0;
        else if (irq == "int0") rtc_irq = IRQ_INT0;
        else if (irq == "int1") rtc_irq = IRQ_INT1;
        else { err = "rtc_int must be none, int0 or int1 (JP3)"; return false; }
    }

    size_t size = (size_t)chips * CHIP_SIZE;
    std::string image;
    bool work_copy;
    if (!cfg_string(cfg, "image", image, "", err)) return false;
    if (!cfg_bool(cfg, "working_copy", work_copy, true, err)) return false;
    if (image.empty() || probe) {   // probe: no working copies just to look at the decoding
        mem.assign(size, 0xE5);   // empty CP/M directory pattern
        if (mode == MODE_RW) mode = MODE_NOSAVE;
        return true;
    }
    if (ends_with_hex(image)) {
        // Intel HEX images are read only sources: changes are kept in memory only
        if (!rom_load(image, mem, size, 0xE5, 0, err)) return false;
        if (mode == MODE_RW) {
            mode = MODE_NOSAVE;
            LOGW("%s: Intel HEX image, changes will not be saved", name.c_str());
        }
        image_path = data_path(image);
        return true;
    }
    if (!media_path(image, work_copy, image_path, err)) return false;
    std::vector<uint8_t> data;
    if (!file_read_all(image_path, data)) { err = "cannot read " + image_path; return false; }
    if (data.size() > size) {
        LOGW("%s: image is %u bytes, only %u fitted (chips=%d)", name.c_str(), (unsigned)data.size(),
             (unsigned)size, chips);
        data.resize(size);
    }
    mem.assign(size, 0xE5);
    for (size_t i = 0; i < data.size(); i++) mem[i] = data[i];
    LOGI("%s: %s (%s)", name.c_str(), image_path.c_str(),
         mode == MODE_RW ? "read/write" : (mode == MODE_NOSAVE ? "changes not saved" : "read only"));
    return true;
}

void Zrmd1::reset()
{
    // The SRAM and the RTC are battery backed; the address counters and the
    // port 63h latch are assumed to be cleared by /RES on ZRMD-1 (to be verified
    // on HW). ZRD-1V: CLR of the 74LS193 tied low, the 374 latches have no
    // clear, /RES only disables the decoder: the address survives a reset.
    if (!is_zrd1v) a_lo = a_mid = a_hi = 0;
    lock = false;   // rev 2.1: /IRES clears the RS latch of the write lock
}

bool Zrmd1::window(void) const
{
    return window_seen && bus->now - window_at < (uint64_t)(WINDOW_SECONDS * bus->cpu_hz);
}

// An access to 62h triggers the 74LS123 (retriggerable). Its /Q is /CS of the MAX187: a new
// conversion starts when /CS falls, i.e. when the window was closed.
void Zrmd1::trigger_window(void)
{
    if (!window()) {
        adc_start = bus->now;
        adc_clocks = 0;
        adc_code = (uint16_t)battery_mv;   // 4.096 V reference, 12 bits: 1 mV per step
    }
    window_seen = true;
    window_at = bus->now;
}

// DOUT of the MAX187 as seen during a read of 61h (SCLK high): low while converting, then high
// (end of conversion), then D11..D0 after each falling SCLK edge, zeros after them. Without
// /CS the output floats (reads 1).
uint8_t Zrmd1::adc_dout(void) const
{
    if (!window()) return 1;
    if (bus->now - adc_start < (uint64_t)(ADC_CONVERT_SECONDS * bus->cpu_hz)) return 0;
    if (adc_clocks == 0) return 1;
    if (adc_clocks > 12) return 0;
    return (uint8_t)((adc_code >> (12 - adc_clocks)) & 1);
}

void Zrmd1::advance(uint64_t now)
{
    if (dirty && mode == MODE_RW && now - dirty_since > (uint64_t)(AUTOSAVE_SECONDS * bus->cpu_hz)) save();
}

bool Zrmd1::writable(uint32_t addr) const
{
    if (mode == MODE_READONLY) return false;
    if (is_zrd1v) return true;
    return write_enable[(addr >> 20) & 3];   // S1..S4 per 1 MB (GAL mem.eqn, A20/A21)
}

int Zrmd1::decode_io(uint8_t port, const char** what)
{
    uint8_t off = (uint8_t)(port - base);
    if (off == 0) { *what = "RAM disk data"; return DEC_READ | DEC_WRITE; }
    if (off < 4 && is_zrd1v) {   // read: the undriven internal bus, loaded into the register too
        *what = off == 1 && rev2 ? "RAM disk address, battery ADC" : "RAM disk address";
        return DEC_READ | DEC_WRITE;
    }
    if (off < 4) { *what = "RAM disk address"; return DEC_WRITE; }
    if (rtc_enabled && off >= 0x10 && off < 0x20) { *what = "RTC"; return DEC_READ | DEC_WRITE; }
    return 0;
}

unsigned Zrmd1::panel_leds()
{
    if (!is_zrd1v || !rev2) return 0;   // ZRMD-1 and ZRD-1V rev 1.1 have no LEDs
    bool dsk = accessed && bus && bus->now - access_at < bus->cpu_hz / 50;   // 74LS123 IC21A, 20 ms
    return (lock ? 1u : 0u) | (dsk ? 2u : 0u);
}

std::string Zrmd1::live_status()
{
    if (!is_zrd1v || !rev2) return std::string();
    return lock ? "write lock: locked by the program (a write to 60h without 62h first), /RES unlocks"
                : "write lock: open";
}

bool Zrmd1::live_param(const char* path) const
{
    return !strcmp(path, "mode") || (!is_zrd1v && !strncmp(path, "write_enable.", 13)) ||
           (is_zrd1v && !strcmp(path, "battery_mv"));   // the battery runs down while it works
}

bool Zrmd1::set_live_param(const char* path, const json& v, std::string& err)
{
    if (!strncmp(path, "write_enable.", 13) && !is_zrd1v) {
        int i = path[13] - '0';
        if (i < 0 || i > 3 || !v.is_boolean()) { err = "write_enable.0-3 must be true or false (S1-S4)"; return false; }
        write_enable[i] = v.get<bool>();
        return true;
    }
    if (is_zrd1v && !strcmp(path, "battery_mv")) {
        if (!v.is_number_integer() || v.get<long long>() < 0 || v.get<long long>() > 4095) {
            err = "battery_mv must be 0..4095";
            return false;
        }
        battery_mv = v.get<int>();
        return true;
    }
    if (strcmp(path, "mode") != 0) return Card::set_live_param(path, v, err);
    std::string s = v.is_string() ? v.get<std::string>() : std::string();
    Mode m;
    if (s == "rw") m = MODE_RW;
    else if (s == "nosave") m = MODE_NOSAVE;
    else if (s == "readonly") m = MODE_READONLY;
    else { err = "mode must be rw, nosave or readonly"; return false; }
    if (m == MODE_RW && !battery_files_enabled()) m = MODE_NOSAVE;   // tests: work/ stays untouched
    if (mode == MODE_RW && m != MODE_RW) save();   // what was written so far is kept
    if (m == MODE_RW && mode != MODE_RW) {        // from now on the file follows the memory
        if (!dirty) dirty_since = bus ? bus->now : 0;
        dirty = true;
    }
    mode = m;
    return true;
}

char Zrmd1::panel_led_color(int n)
{
    if (!is_zrd1v) return 0;
    return n == 0 ? 'r' : 'g';   // D1 HLMP-3762 red WRITE PROTECT, D2 HLMP-3962 green ACCESS
}

bool Zrmd1::io_read(uint8_t port, uint8_t& data)
{
    uint8_t off = (uint8_t)(port - base);
    if (off == 0) {
        accessed = true;
        access_at = bus->now;
        uint32_t a = address();
        data = a < mem.size() ? mem[a] : 0xFF;   // no chip fitted: floating bus
        a_lo++;
        return true;
    }
    if (rtc_enabled && off >= 0x10 && off < 0x20) {
        data = (uint8_t)(0xF0 | rtc.read(off & 15, bus->now, bus->cpu_hz));   // D4-D7 not driven
        return true;
    }
    if (off < 4 && is_zrd1v) {
        // the transceiver drives the bus, but nothing drives the internal bus (only DOUT of the
        // MAX187 on D5 for 61h): FFh, and the address register takes it like a write
        accessed = true;
        access_at = bus->now;
        data = 0xFF;
        if (off == 1) {
            if (rev2) {
                if (!adc_dout()) data &= (uint8_t)~0x20;
                if (window() && bus->now - adc_start >= (uint64_t)(ADC_CONVERT_SECONDS * bus->cpu_hz))
                    adc_clocks++;   // SCLK falls at the end of the read
            }
            a_lo = data;
        } else if (off == 2) {
            if (rev2) trigger_window();
            a_mid = data;
        } else {
            a_hi = data;
        }
        return true;
    }
    return false;   // ZRMD-1: 61h-63h are write only
}

bool Zrmd1::io_write(uint8_t port, uint8_t data)
{
    uint8_t off = (uint8_t)(port - base);
    switch (off) {
    case 0: {
        accessed = true;
        access_at = bus->now;
        uint32_t a = address();
        if (rev2 && !lock && !window()) lock = true;   // a write outside the window: locked until /RES
        if (rev2 && lock) {
            a_lo++;   // the counter counts every access, the SRAM gets no /WE
            return true;
        }
        if (a < mem.size() && writable(a) && mem[a] != data) {
            mem[a] = data;
            if (!dirty) dirty_since = bus->now;
            dirty = true;
        }
        a_lo++;
        return true;
    }
    case 1:
        if (is_zrd1v) { accessed = true; access_at = bus->now; }
        a_lo = data;
        return true;
    case 2:
        if (is_zrd1v) { accessed = true; access_at = bus->now; }
        if (rev2) trigger_window();
        a_mid = data;
        return true;
    case 3:
        if (is_zrd1v) { accessed = true; access_at = bus->now; }
        a_hi = data;
        return true;
    default: break;
    }
    if (rtc_enabled && off >= 0x10 && off < 0x20) {
        rtc.write(off & 15, data, bus->now, bus->cpu_hz);
        return true;
    }
    return false;
}

int Zrmd1::irq_lines()
{
    if (!rtc_enabled || !rtc_irq) return 0;
    return rtc.irq(bus->now, bus->cpu_hz) ? rtc_irq : 0;
}

int Zrmd1::map_lines()
{
    if (!map_jp2) return 0;
    return ((a_hi & 0x40) ? MAP_1 : 0) | ((a_hi & 0x80) ? MAP_2 : 0);
}

bool Zrmd1::save(void)
{
    if (!dirty || mode != MODE_RW || image_path.empty()) return true;
    bool ok = file_write_all(image_path, mem.data(), mem.size());
    if (ok) {
        dirty = false;
        LOGI("%s: saved %s", name.c_str(), image_path.c_str());
    } else {
        LOGE("%s: cannot save %s", name.c_str(), image_path.c_str());
    }
    return ok;
}

void Zrmd1::flush()
{
    save();
}

void Zrmd1::describe(json& out)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02Xh", base);
    out["base"] = buf;
    out["size_kb"] = (int)(mem.size() / 1024);
    out["image"] = image_path;
    out["mode"] = mode == MODE_RW ? "rw" : (mode == MODE_NOSAVE ? "nosave" : "readonly");
    out["modified"] = dirty;
    snprintf(buf, sizeof(buf), "%06X", address());
    out["address"] = buf;
    if (!is_zrd1v) {
        out["write_enable"] = { write_enable[0], write_enable[1], write_enable[2], write_enable[3] };
        out["rtc"] = rtc_enabled;
    } else {
        out["revision"] = rev2 ? "2.1" : "1.1";
        if (rev2) {
            out["locked"] = lock;
            out["window_open"] = window();
            out["battery_mv"] = battery_mv;
        }
    }
}
