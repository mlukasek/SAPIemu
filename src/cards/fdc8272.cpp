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
    flush();
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
    static const char* const keys[DRIVES] = { "drive_a", "drive_b", "drive_c", "drive_d" };
    for (int i = 0; i < DRIVES; i++) {
        std::string image;
        bool wp = false;
        if (cfg.contains(keys[i]) && cfg[keys[i]].is_object()) {
            const json& d = cfg[keys[i]];
            if (!cfg_string(d, "image", image, "", err)) return false;
            if (!cfg_bool(d, "write_protect", wp, false, err)) return false;
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

void Fdc8272Card::reset_chips(void)
{
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
        drv.push_back({ { "drive", std::string(1, (char)('A' + i)) }, { "image", d.image }, { "path", d.path },
                        { "inserted", d.present() }, { "write_protect", d.write_protect },
                        { "modified", d.dirty }, { "track", d.track } });
    }
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
    // DMA cycles not yet taken from the CPU (only the disk controller holds the bus)
    uint32_t h = bus->hold;
    io.v(h);
    if (io.loading) bus->hold = h;
}
