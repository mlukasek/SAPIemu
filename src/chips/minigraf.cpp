#include "chips/minigraf.h"
#include "core/state.h"

#include <stdlib.h>

void Minigraf::power_on(void)
{
    Plotter::power_on();
    known = false;
}

void Minigraf::outputs_changed(int, uint8_t)
{
    lines_now();
}

void Minigraf::lines_now(void)
{
    // port B as an output (modes 0 and 1) drives the cable; as an input the 8255 lets its pins float
    // and the buffers of the DPP-1 (set to output) pass high levels
    uint8_t v = 0xFF;
    if (ppi && !(ppi->control() & 0x02)) v = ppi->out_b;
    set_lines(v);
}

void Minigraf::set_lines(uint8_t v)
{
    if (known && v == last) return;
    double t = now_s();
    if (!known) {   // the motors take this phase, nothing moves
        known = true;
        last = v;
        pen = (v & 0x80) != 0;
        if (pen) strokes.push_back({ x(), y() });
        generation++;
        return;
    }
    for (int axis = 0; axis < 2; axis++) {
        int sh = axis ? 3 : 0;
        int d = (((v >> sh) & 7) - ((last >> sh) & 7)) & 7;
        if (d == 0) continue;
        if (d == 4) {   // the opposite phase: the rotor stays (or jumps either way)
            lost++;
            continue;
        }
        int n = d < 4 ? d : d - 8;
        if (n > 1 || n < -1) lost++;   // a skipped phase: the motor follows the shortest way
        for (int i = 0; i < abs(n); i++) step(axis, n > 0 ? 1 : -1, t);
    }
    set_pen((v & 0x80) != 0);
    last = v;
}

void Minigraf::state(StateIO& io)
{
    io.v(last);
    io.v(known);
    io.v(carriage);
    io.v(paper_y);
    io.v(pen);
    io.v(last_step_t[0]);
    io.v(last_step_t[1]);
    io.v(steps);
    io.v(lost);
    io.v(stopped);
    io.v(fast);
    drawing_state(io);
}
