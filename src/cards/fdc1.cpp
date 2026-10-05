#include "cards/fdc1.h"
#include "core/bus.h"
#include "core/ihex.h"
#include "core/log.h"
#include "core/state.h"

#include <stdio.h>
#include <string.h>

using nlohmann::json;

// bus cycles a DMA transfer takes from the CPU (HOLD; the 8257 cycle S1-S4)
static const uint32_t DMA_HOLD_CYCLES = 4;

// PROM D5B outputs (active low)
static const uint8_t Y_DMA = 0x01, Y_FDC = 0x02, Y_FDCB = 0x08;

Fdc1::~Fdc1()
{
    flush();
}

bool Fdc1::configure(const json& cfg, std::string& err)
{
    std::string dec;
    if (!cfg_string(cfg, "decoder", dec, "roms/fdc-1/FDC-1A_D5B_74S287.bin", err)) return false;
    std::vector<uint8_t> p;
    if (!rom_load(dec, p, 256, 0xFF, 0, err)) return false;
    if (p.size() != 256) {
        err = dec + ": the D5B PROM (74S287) has 256 entries";
        return false;
    }
    memcpy(prom, p.data(), 256);
    if (!cfg_bool(cfg, "working_copy", work_copy, true, err)) return false;
    if (!cfg_bool(cfg, "rdy_ground", rdy_ground, true, err)) return false;
    static const char* const sw_keys[DRIVES] = { "sa2_a", "sa2_b", "sa2_c", "sa2_d" };
    for (int i = 0; i < DRIVES; i++) {
        std::string t;
        if (!cfg_string(cfg, sw_keys[i], t, "8sd", err)) return false;
        if (t == "8sd") sa2[i] = 2;
        else if (t == "8dd") sa2[i] = 3;
        else if (t == "5sd") sa2[i] = 0;
        else if (t == "5dd") sa2[i] = 1;
        else {
            err = std::string(sw_keys[i]) + " must be 8sd, 8dd, 5sd or 5dd";
            return false;
        }
    }
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

bool Fdc1::floppy_insert(int i, const std::string& image, bool wp, std::string& err)
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

void Fdc1::floppy_eject(int i)
{
    if (i < 0 || i >= DRIVES) return;
    drives[i].unload(name.c_str());
}

void Fdc1::flush()
{
    for (int i = 0; i < DRIVES; i++) {
        std::string err;
        if (!drives[i].save(err)) LOGE("%s: %s", name.c_str(), err.c_str());
    }
}

void Fdc1::reset()
{
    fdc.hz = bus->cpu_hz;
    fdc.reset(bus->now);
    dma.reset();
    ctrl = 0;
    motors = 0;
}

void Fdc1::advance(uint64_t now)
{
    if (!(ctrl & 0x04)) fdc.advance(now);
}

bool Fdc1::fdc_ready(int unit)
{
    return rdy_ground || drives[unit & 3].present();
}

bool Fdc1::fdc_dma(bool to_memory, uint8_t& byte, bool& tc)
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

int Fdc1::irq_lines()
{
    advance(bus->now);
    return (ctrl & 0x08) && !(ctrl & 0x04) && fdc.interrupt() ? IRQ_INT0 : 0;
}

bool Fdc1::io_read(uint8_t port, uint8_t& data)
{
    uint8_t y = prom[port];
    if (!(y & Y_DMA)) {
        advance(bus->now);
        data = dma.read(port & 15);
        return true;
    }
    if (!(y & Y_FDC)) {
        if (ctrl & 0x04) {
            data = 0x00;   // held in reset
            return true;
        }
        data = fdc.read(port & 1, bus->now);
        return true;
    }
    if (!(y & Y_FDCB)) {
        if (port & 1) return false;   // 0FH is write only
        data = (uint8_t)(0xFC | sa2[ctrl & 3]);
        return true;
    }
    return false;
}

bool Fdc1::io_write(uint8_t port, uint8_t data)
{
    uint8_t y = prom[port];
    if (!(y & Y_DMA)) {
        advance(bus->now);
        dma.write(port & 15, data);
        return true;
    }
    if (!(y & Y_FDC)) {
        if (!(ctrl & 0x04)) fdc.write(port & 1, data, bus->now);
        return true;
    }
    if (!(y & Y_FDCB)) {
        if (port & 1) {
            motors = data;
            return true;
        }
        advance(bus->now);
        bool was_reset = (ctrl & 0x04) != 0;
        ctrl = data;
        if (ctrl & 0x04) {
            if (!was_reset) fdc.reset(bus->now);
        } else if (was_reset) {
            fdc.reset(bus->now);   // RESET released: the chip starts from idle
        }
        return true;
    }
    return false;
}

int Fdc1::decode_io(uint8_t port, const char** what)
{
    uint8_t y = prom[port];
    if (!(y & Y_DMA)) { *what = "FDC-1A 8257 DMA"; return DEC_READ | DEC_WRITE; }
    if (!(y & Y_FDC)) { *what = (port & 1) ? "FDC-1A 8272 data" : "FDC-1A 8272 status"; return DEC_READ | DEC_WRITE; }
    if (!(y & Y_FDCB)) {
        if (port & 1) { *what = "FDC-1B motors, head load"; return DEC_WRITE; }
        *what = "FDC-1B control / switches SA2";
        return DEC_READ | DEC_WRITE;
    }
    return 0;
}

void Fdc1::describe(json& out)
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

std::string Fdc1::live_status()
{
    char b[64];
    snprintf(b, sizeof(b), "8272 %s, unit %c%s", fdc.busy() ? "busy" : "idle", 'A' + fdc.current_unit(),
             fdc.head_loaded() ? ", head loaded" : "");
    return b;
}

void Fdc1::state(StateIO& io)
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
    io.v(ctrl);
    io.v(motors);
    // DMA cycles not yet taken from the CPU (only this card holds the bus)
    uint32_t h = bus->hold;
    io.v(h);
    if (io.loading) bus->hold = h;
}
