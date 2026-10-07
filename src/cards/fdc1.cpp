#include "cards/fdc1.h"
#include "core/bus.h"
#include "core/state.h"

using nlohmann::json;

// PROM D5B outputs (active low)
static const uint8_t Y_DMA = 0x01, Y_FDC = 0x02, Y_FDCB = 0x08;

bool Fdc1::configure(const json& cfg, std::string& err)
{
    if (!configure_common(cfg, "roms/fdc-1/FDC-1A_D5B_74S287.bin", "D5B", err)) return false;
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
    if (!cfg_bool(cfg, "motor_hl", motor_hl, false, err)) return false;
    if (motor_hl && motor_line) {
        err = "motor_hl and motor_line exclude each other (MOTOR ON from /HL or from 0FH)";
        return false;
    }
    return true;
}

uint64_t Fdc1::fdc_turning_from(int unit)
{
    if (!motor_hl) return Fdc8272Card::fdc_turning_from(unit);
    if (!fdc.head_loaded()) return UINT64_MAX;
    return fdc.head_load_time() + (uint64_t)bus->cpu_hz * spinup_ms / 1000;
}

void Fdc1::reset()
{
    reset_chips();
    ctrl = 0;
    motors = 0;   // /RES clears the register (an assumption)
    set_motors(0);
}

int Fdc1::irq_lines()
{
    advance(bus->now);
    return (ctrl & 0x08) && !fdc_in_reset && fdc.interrupt() ? IRQ_INT0 : 0;
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
        data = fdc_in_reset ? 0x00 : fdc.read(port & 1, bus->now);
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
        if (!fdc_in_reset) fdc.write(port & 1, data, bus->now);
        return true;
    }
    if (!(y & Y_FDCB)) {
        if (port & 1) {
            motors = data;
            set_motors(data & 0x0F);   // M3-M0; H3-H0 (head load) are not used by PC drives
            return true;
        }
        ctrl = data;
        set_fdc_reset((ctrl & 0x04) != 0);
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

void Fdc1::state(StateIO& io)
{
    state_common(io);
    io.v(ctrl);
    io.v(motors);
}
