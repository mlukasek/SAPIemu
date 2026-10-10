#include "plant/bs525.h"
#include "core/state.h"

const double BS525_LOADS[BS525_LOAD_COUNT] = { 0, 1000, 220, 100, 47, 22, 10 };

void Bs525::update(void)
{
    for (Bs525Output& o : out) {
        if (o.u_set < 0) o.u_set = 0;
        if (o.u_set > 30) o.u_set = 30;
        if (o.i_lim < 0.05) o.i_lim = 0.05;
        if (o.i_lim > 1.1) o.i_lim = 1.1;
        if (o.load < 0 || o.load >= BS525_LOAD_COUNT) o.load = 0;
        o.limiting = false;
        if (!power) {
            o.u = o.i = 0;
            continue;
        }
        double r = BS525_LOADS[o.load];
        if (r <= 0) {
            o.u = o.u_set;
            o.i = 0;
            continue;
        }
        double i = o.u_set / r;
        if (i > o.i_lim) {   // the limit holds the current
            o.i = o.i_lim;
            o.u = o.i_lim * r;
            o.limiting = true;
        } else {
            o.i = i;
            o.u = o.u_set;
        }
    }
}

void Bs525::state(StateIO& io)
{
    io.v(power);
    for (Bs525Output& o : out) {
        io.v(o.u_set);
        io.v(o.i_lim);
        io.v(o.meter_a);
        int32_t l = o.load;
        io.v(l);
        o.load = l;
    }
    update();
}
