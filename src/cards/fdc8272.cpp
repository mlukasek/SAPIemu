#include "cards/fdc8272.h"
#include "core/bus.h"
#include "core/ihex.h"
#include "core/log.h"
#include "core/state.h"

#include <stdio.h>
#include <string.h>

using nlohmann::json;

// bus cycles a DMA transfer takes from the CPU (HOLD; the 8257 cycle S1-S4)
static const uint32_t DMA_HOLD_CYCLES = 4;

Fdc8272Card::~Fdc8272Card()
{
    if (!trial) flush();
}

bool Fdc8272Card::configure_common(const json& cfg, const char* default_decoder, const char* prom_name,
                                   std::string& err)
{
    std::string dec;
    if (!cfg_string(cfg, "decoder", dec, default_decoder, err)) return false;
    std::vector<uint8_t> p;
    if (!rom_load(dec, p, 256, 0xFF, 0, err)) return false;
    if (p.size() != 256) {
        err = dec + ": the " + prom_name + " PROM (74S287) has 256 entries";
        return false;
    }
    memcpy(prom, p.data(), 256);
    if (!cfg_bool(cfg, "working_copy", work_copy, true, err)) return false;
    if (!cfg_bool(cfg, "rdy_ground", rdy_ground, type == "FDC-1", err)) return false;   // MFC-1A: from the drives
    long long v;
    if (!cfg_bool(cfg, "motor_line", motor_line, false, err)) return false;
    if (!cfg_number(cfg, "spinup_ms", v, 500, err)) return false;
    if (v < 0 || v > 5000) {
        err = "spinup_ms must be 0-5000";
        return false;
    }
    spinup_ms = (uint32_t)v;
    for (int i = 0; i < DRIVES; i++) turning_from[i] = motor_line ? UINT64_MAX : 0;
    static const char* const keys[DRIVES] = { "drive_a", "drive_b", "drive_c", "drive_d" };
    for (int i = 0; i < DRIVES; i++) {
        std::string image;
        bool wp = false;
        if (cfg.contains(keys[i]) && cfg[keys[i]].is_object()) {
            const json& d = cfg[keys[i]];
            if (!cfg_string(d, "image", image, "", err)) return false;
            if (!cfg_bool(d, "write_protect", wp, false, err)) return false;
            if (!cfg_letter(d, drives[i].cpm_cfg, err)) return false;
        }
        drives[i].image = image;
        drives[i].write_protect = wp;
        if (!image.empty() && !probe && !floppy_insert(i, image, wp, err)) return false;
    }
    fdc.host = this;
    return true;
}

bool Fdc8272Card::floppy_insert(int i, const std::string& image, bool wp, std::string& err)
{
    if (i < 0 || i >= DRIVES) {
        err = "no such drive";
        return false;
    }
    {   // read first: a bad image leaves the disk in the drive
        FloppyDrive probe;
        if (!probe.load(image, work_copy, err)) return false;
    }
    floppy_eject(i);
    FloppyDrive& d = drives[i];
    if (!d.load(image, work_copy, err)) return false;
    d.write_protect = wp;
    LOGI("%s: drive %c: %s%s", name.c_str(), 'A' + i, d.path.c_str(), wp ? " (write protected)" : "");
    return true;
}

void Fdc8272Card::floppy_eject(int i)
{
    if (i < 0 || i >= DRIVES) return;
    drives[i].unload(name.c_str());
}

void Fdc8272Card::flush()
{
    for (int i = 0; i < DRIVES; i++) {
        std::string err;
        if (!drives[i].save(err)) LOGE("%s: %s", name.c_str(), err.c_str());
    }
}

bool Fdc8272Card::unsaved()
{
    for (int i = 0; i < DRIVES; i++)
        if (drives[i].unsaved()) return true;
    return false;
}

void Fdc8272Card::fdc_reading(int unit)
{
    if (FloppyDrive* d = fdc_drive(unit)) d->seen_as(bus->cpm_drive);
}

void Fdc8272Card::reset_chips(void)
{
    for (FloppyDrive& d : drives) d.cpm_seen = 0;   // another system may come
    fdc.hz = bus->cpu_hz;
    fdc.reset(bus->now);
    dma.reset();
    fdc_in_reset = false;
}

void Fdc8272Card::set_fdc_reset(bool on)
{
    if (on == fdc_in_reset) return;
    advance(bus->now);
    fdc_in_reset = on;
    fdc.reset(bus->now);   // held in reset, or released: the chip starts from idle
}

void Fdc8272Card::set_motors(uint8_t mask)
{
    advance(bus->now);
    for (int i = 0; i < DRIVES; i++) {
        bool on = (mask >> i) & 1, was = (motor_mask >> i) & 1;
        if (!motor_line) turning_from[i] = 0;
        else if (!on) turning_from[i] = UINT64_MAX;
        else if (!was) turning_from[i] = bus->now + (uint64_t)bus->cpu_hz * spinup_ms / 1000;
    }
    motor_mask = mask;
    if (!fdc_in_reset) fdc.motor_changed(bus->now);
}

void Fdc8272Card::advance(uint64_t now)
{
    if (!fdc_in_reset) fdc.advance(now);
}

bool Fdc8272Card::fdc_dma(bool to_memory, uint8_t& byte, bool& tc)
{
    uint16_t addr;
    if (!dma.transfer(2, addr, tc)) return false;
    int saved_wait = bus->wait;   // a DMA cycle is not part of the CPU access under way
    int cyc = dma.cycle(2);
    if (to_memory) {
        if (cyc == I8257::WRITE) bus->mem_write(addr, byte);
    } else {
        byte = cyc == I8257::READ ? bus->mem_read(addr) : 0xFF;
    }
    bus->wait = saved_wait;
    bus->hold += DMA_HOLD_CYCLES;
    return true;
}

void Fdc8272Card::describe(json& out)
{
    json drv = json::array();
    for (int i = 0; i < DRIVES; i++) {
        const FloppyDrive& d = drives[i];
        drv.push_back({ { "unit", i }, { "image", d.image }, { "path", d.path },
                        { "inserted", d.present() }, { "write_protect", d.write_protect },
                        { "modified", d.dirty }, { "track", d.track },
                        { "turning", fdc_turning_from(i) != UINT64_MAX && bus->now >= fdc_turning_from(i) },
                        { "cpm", d.cpm_cfg ? std::string(1, d.cpm_cfg) : d.cpm_seen ? std::string(1, d.cpm_seen) : std::string() },
                        { "cpm_seen", d.cpm_seen ? std::string(1, d.cpm_seen) : std::string() } });
    }
    out["motor_line"] = motor_line;
    out["motors"] = motor_mask;
    out["drives"] = drv;
    out["unit"] = fdc.current_unit();
    out["busy"] = fdc.busy();
    out["head_loaded"] = fdc.head_loaded();
}

std::string Fdc8272Card::live_status()
{
    char b[64];
    snprintf(b, sizeof(b), "8272 %s, unit %c%s", fdc.busy() ? "busy" : "idle", 'A' + fdc.current_unit(),
             fdc.head_loaded() ? ", head loaded" : "");
    return b;
}

void Fdc8272Card::state_common(StateIO& io)
{
    // the floppies are referred to (file, protection, head), not stored
    for (int i = 0; i < DRIVES; i++) {
        FloppyDrive& dr = drives[i];
        std::string img = dr.image;
        io.media("floppy", dr.path);
        io.v(img);
        bool wp = dr.write_protect;
        io.v(wp);
        io.e(dr.track);
        if (io.loading && io.ok() && img != dr.image) {
            std::string err;
            if (img.empty()) floppy_eject(i);
            else if (!floppy_insert(i, img, wp, err)) LOGW("state: floppy %s: %s", img.c_str(), err.c_str());
        }
        if (io.loading) dr.write_protect = wp;
    }
    fdc.state(io);
    dma.state(io);
    io.v(fdc_in_reset);
    io.v(motor_mask);
    for (int i = 0; i < DRIVES; i++) io.v(turning_from[i]);
    // DMA cycles not yet taken from the CPU (only the disk controller holds the bus)
    uint32_t h = bus->hold;
    io.v(h);
    if (io.loading) bus->hold = h;
}
