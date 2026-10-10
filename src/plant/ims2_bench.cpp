#include "plant/ims2_bench.h"
#include "core/bus.h"
#include "core/state.h"

using nlohmann::json;

Ims2Bench::~Ims2Bench()
{
    // the machine destroys its plant before its cards: the DPS-1 must not detach from a bus that is gone
    for (Ims2Node* n : ims.nodes) n->ims = nullptr;
}

bool Ims2Bench::configure(const json& cfg, std::string& err)
{
    if (cfg.contains("m1t330") && cfg["m1t330"].is_object()) {
        const json& m = cfg["m1t330"];
        m1t.address = m.value("address", 3);
        if (m1t.address < 0 || m1t.address > 30) {
            err = "m1t330.address must be 0-30";
            return false;
        }
        m1t.power = m.value("power", true);
    }
    if (cfg.contains("bs525") && cfg["bs525"].is_object()) {
        const json& b = cfg["bs525"];
        bs.power = b.value("power", true);
        bs.out[0].u_set = b.value("u1", 12.564);
        bs.out[0].i_lim = b.value("i1", 0.5);
        bs.out[1].u_set = b.value("u2", 5.0);
        bs.out[1].i_lim = b.value("i2", 0.5);
    } else {
        bs.out[0].u_set = 12.564;
        bs.out[1].u_set = 5.0;
    }
    std::string in = cfg.value("input", std::string("I"));
    if (in == "I") input = 0;
    else if (in == "II") input = 1;
    else if (in == "none") input = -1;
    else {
        err = "input must be I, II or none";
        return false;
    }
    bs.update();
    return true;
}

double Ims2Bench::input_v(void) const
{
    if (input < 0) return 0;
    return bs.out[input].u;
}

void Ims2Bench::changed(void)
{
    bs.update();
    m1t.vin = input_v();
}

void Ims2Bench::set_power_m1t(bool on)
{
    if (on == m1t.power) return;
    if (on) m1t.power_on(ims.t);
    else m1t.power_off();
}

void Ims2Bench::advance(uint64_t now)
{
    if (bus) ims.hz = (double)bus->cpu_hz;
    if (!started) {
        started = true;
        ims.t = now;
        ims.attach(&m1t);
        changed();
        if (m1t.power) m1t.power_on(now);
        else m1t.power_off();
    }
    m1t.vin = input_v();
    ims.advance(now);
}

void Ims2Bench::state(StateIO& io)
{
    io.v(started);
    int32_t in = input;
    io.v(in);
    input = in;
    bs.state(io);
    ims.state(io);
    m1t.state(io);
    // the voltmeter first, as when running (the DPS-1 attaches in its state, before the plant)
    if (io.loading && started) ims.attach_loaded(&m1t, true);
}

void Ims2Bench::describe(json& out)
{
    json m;
    m["address"] = m1t.address;
    m["power"] = m1t.power;
    m["display"] = m1t.display();
    m["unit"] = m1t.unit_pct() ? "%" : m1t.unit_mv() ? "mV" : m1t.unit_v() ? "V" : "";
    m["range"] = m1t.range;
    m["autorange"] = m1t.autorange;
    m["filter"] = m1t.filter;
    m["repeat"] = m1t.repeat;
    m["remote"] = m1t.remote;
    m["value_v"] = m1t.value;
    m["measurements"] = m1t.measured;
    out["m1t330"] = m;
    json b;
    b["power"] = bs.power;
    for (int k = 0; k < 2; k++) {
        const Bs525Output& o = bs.out[k];
        json c;
        c["u_set"] = o.u_set;
        c["i_lim"] = o.i_lim;
        c["u"] = o.u;
        c["i"] = o.i;
        c["limiting"] = o.limiting;
        c["load_ohm"] = BS525_LOADS[o.load];
        b[k == 0 ? "I" : "II"] = c;
    }
    out["bs525"] = b;
    out["input"] = input == 0 ? "I" : input == 1 ? "II" : "none";
    out["bus_transfers"] = ims.transfers;
}
