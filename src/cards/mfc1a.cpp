#include "cards/mfc1a.h"
#include "core/bus.h"
#include "core/state.h"

using nlohmann::json;

// PROM DD9 outputs (active low)
static const uint8_t Y_DMA = 0x01, Y_FDC = 0x02, Y_SEL = 0x04, Y_REG = 0x08;

bool Mfc1a::configure(const json& cfg, std::string& err)
{
    if (!configure_common(cfg, "roms/mfc-1a/MFC-1A_DD9_74S287.bin", "DD9", err)) return false;
    std::string r;
    if (!cfg_string(cfg, "reset_d5", r, "pulse", err)) return false;
    if (r != "pulse" && r != "level") {
        err = "reset_d5 must be pulse or level";
        return false;
    }
    reset_level = r == "level";
    return true;
}

void Mfc1a::reset()
{
    reset_chips();
    reg = 0;
}

int Mfc1a::irq_lines()
{
    advance(bus->now);
    return (reg & 0x10) && !fdc_in_reset && fdc.interrupt() ? IRQ_INT0 : 0;   // D4: DD12A
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
        bool rise = (data & 0x20) && !(reg & 0x20);
        reg = data;
        if (reset_level) {
            set_fdc_reset((reg & 0x20) != 0);   // D5: DD4B, RES72
        } else if (rise) {
            set_fdc_reset(true);    // a reset pulse: the chip starts from idle
            set_fdc_reset(false);
        }
        if (reset_level && (reg & 0x20)) reg &= (uint8_t)~0x40;   // DD23B clears DD23A (D6): no drive
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
}
