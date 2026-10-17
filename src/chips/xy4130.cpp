#include "chips/xy4130.h"
#include "core/card.h"
#include "core/state.h"

// bits of the cables: PEN, X/Y, +/-, STEP
static const int k_bits[2][4] = {
    { 0, 1, 6, 7 },   // mikos: JPR-1 X3 P2-OUT (sapi.cz doku2/xy4130.php)
    { 7, 2, 3, 0 },   // kres: BASIC EXP V5.0/G+SZ (sapi.cz doku2/bexpgsz.php)
};

void Xy4130::power_on(void)
{
    Plotter::power_on();
    ready_latch = false;
    pen_en = pen_on_at_power;
    pen_in = false;
    step_in = true;
}

void Xy4130::new_sheet(void)
{
    ready_latch = false;   // the paper buttons reset the latch (at the left stop it stays set)
    update_pen();
    Plotter::new_sheet();
}

void Xy4130::pen_button(void)
{
    pen_en = !pen_en;
    update_pen();
}

void Xy4130::update_pen(void)
{
    set_pen(pen_in && pen_en && ready());
}

void Xy4130::outputs_changed(int, uint8_t)
{
    lines_now();
}

void Xy4130::lines_now(void)
{
    // port B as an output drives the cable; as an input the lines float high
    uint8_t v = 0xFF;
    if (ppi && !(ppi->control() & 0x02)) v = ppi->out_b;
    set_lines(v);
}

void Xy4130::set_lines(uint8_t v)
{
    const int* b = k_bits[cable == CABLE_KRES ? 1 : 0];
    last = v;
    pen_in = (v >> b[0] & 1) != 0;
    bool st = (v >> b[3] & 1) != 0;
    if (st && !step_in) {   // the rising edge of STEP: one step of the axis and direction now set
        int axis = v >> b[1] & 1;
        bool dir = (v >> b[2] & 1) != 0;
        int d = axis == 0 ? (dir ? 1 : -1) : (dir ? -1 : 1);
        step(axis, d, now_s());
        if (carriage == 0) ready_latch = true;   // the left stop switch
    }
    step_in = st;
    update_pen();
}

uint8_t Xy4130::pins_b()
{
    int b = ready_bit();
    return b >= 0 && ready() ? (uint8_t)~(1u << b) : 0xFF;
}

bool xy4130_configure(Xy4130& xy, const nlohmann::json& sec, const char* def_cable, std::string& err)
{
    std::string c;
    if (!sec.is_object()) {
        err = "xy4130 must be an object";
        return false;
    }
    if (!cfg_string(sec, "cable", c, def_cable, err)) return false;
    if (c == "mikos") xy.cable = Xy4130::CABLE_MIKOS;
    else if (c == "kres") xy.cable = Xy4130::CABLE_KRES;
    else {
        err = "xy4130.cable must be \"mikos\" or \"kres\"";
        return false;
    }
    return cfg_bool(sec, "pen_on", xy.pen_on_at_power, false, err);
}

void Xy4130::state(StateIO& io)
{
    io.v(last);
    io.v(carriage);
    io.v(paper_y);
    io.v(pen);
    io.v(last_step_t[0]);
    io.v(last_step_t[1]);
    io.v(steps);
    io.v(lost);
    io.v(stopped);
    io.v(fast);
    io.v(ready_latch);
    io.v(pen_en);
    io.v(pen_in);
    io.v(step_in);
    drawing_state(io);
}
