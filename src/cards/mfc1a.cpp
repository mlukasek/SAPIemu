#include "cards/mfc1a.h"
#include "core/bus.h"
#include "core/state.h"

using nlohmann::json;

// PROM DD9 outputs (active low)
static const uint8_t Y_DMA = 0x01, Y_FDC = 0x02, Y_SEL = 0x04, Y_REG = 0x08;

bool Mfc1a::configure(const json& cfg, std::string& err)
{
    if (!configure_common(cfg, "roms/mfc-1a/MFC-1A_DD9_74S287.bin", "DD9", err)) return false;
    std::string w, r;
    if (!cfg_string(cfg, "wiring", w, "tesla", err)) return false;
    if (w != "tesla" && w != "ec1045") {
        err = "wiring must be tesla or ec1045";
        return false;
    }
    tesla = w == "tesla";
    reset_bit = tesla ? 0x10 : 0x20;
    int_bit = tesla ? 0x20 : 0x10;
    if (!cfg_string(cfg, "reset_mode", r, "level", err)) return false;
    if (r != "pulse" && r != "level") {
        err = "reset_mode must be pulse or level";
        return false;
    }
    reset_level = r == "level";
    return true;
}

void Mfc1a::reset()
{
    reset_chips();
    reg = 0;        // /RES clears DD11
    dd23 = false;   // and through RES72 and DD23B also DD23A
    update_motors();
}

// MO0n-MO3n: DD18 decodes D3 D2, enabled like the drive select (D6, wiring)
void Mfc1a::update_motors(void)
{
    set_motors(drives_on() ? (uint8_t)(1 << ((reg >> 2) & 3)) : 0);
}

int Mfc1a::irq_lines()
{
    advance(bus->now);
    return (reg & int_bit) && !fdc_in_reset && fdc.interrupt() ? IRQ_INT0 : 0;   // DD12A
}

bool Mfc1a::io_read(uint8_t port, uint8_t& data)
{
    uint8_t y = prom[port];
    if (y & Y_SEL) return false;
    if (!(y & Y_DMA)) {
        advance(bus->now);
        data = dma.read(port & 15);
        return true;
    }
    if (!(y & Y_FDC)) {
        data = fdc_in_reset ? 0x00 : fdc.read(port & 1, bus->now);
        return true;
    }
    return false;   // the drive register is write only, nothing drives the bus
}

bool Mfc1a::io_write(uint8_t port, uint8_t data)
{
    uint8_t y = prom[port];
    if (y & Y_SEL) return false;
    if (!(y & Y_DMA)) {
        advance(bus->now);
        dma.write(port & 15, data);
        return true;
    }
    if (!(y & Y_FDC)) {
        if (!fdc_in_reset) fdc.write(port & 1, data, bus->now);
        return true;
    }
    if (!(y & Y_REG)) {
        advance(bus->now);   // the 8272 has worked with the old drive until now
        bool rise = (data & reset_bit) && !(reg & reset_bit);
        reg = data;
        dd23 = (data & 0x40) != 0;
        if (reset_level) {
            set_fdc_reset((reg & reset_bit) != 0);   // RES72
            if (reg & reset_bit) dd23 = false;       // DD23B clears DD23A
        } else if (rise) {
            set_fdc_reset(true);    // a reset pulse: the chip starts from idle
            set_fdc_reset(false);
        }
        update_motors();
        return true;
    }
    return true;
}

int Mfc1a::decode_io(uint8_t port, const char** what)
{
    uint8_t y = prom[port];
    if (y & Y_SEL) return 0;
    if (!(y & Y_DMA)) { *what = "MFC-1A 8257 DMA"; return DEC_READ | DEC_WRITE; }
    if (!(y & Y_FDC)) { *what = (port & 1) ? "MFC-1A 8272 data" : "MFC-1A 8272 status"; return DEC_READ | DEC_WRITE; }
    if (!(y & Y_REG)) { *what = "MFC-1A drive register"; return DEC_WRITE; }
    *what = "MFC-1A";
    return DEC_WRITE;
}

void Mfc1a::state(StateIO& io)
{
    state_common(io);
    io.v(reg);
    io.v(dd23);
}
