#include "cards/dps1.h"
#include <string.h>
#include "core/bus.h"
#include "core/plant.h"
#include "core/state.h"
#include "core/util.h"

using nlohmann::json;

static const char* DEFAULT_PROM = "roms/dps-1/DPS-1_D3D_74S571.bin";

Dps1::Dps1() : Card("DPS-1")
{
    p2.card = this;
    p2.which = 0;
    p1.card = this;
    p1.which = 1;
    d2c.dev = &p2;
    d1c.dev = &p1;
    prom.assign(512, 0x0F);
}

Dps1::~Dps1()
{
    if (ims) ims->detach(this);
}

bool Dps1::configure(const json& cfg, std::string& err)
{
    jp1 = cfg.value("jp1", true);
    jp2 = cfg.value("jp2", false);
    std::string file = cfg.value("prom", std::string(DEFAULT_PROM));
    if (!file.empty()) {
        std::vector<uint8_t> d;
        if (!file_read_all(data_path(file), d)) {
            err = "DPS-1: cannot read the PROM " + file;
            return false;
        }
        if (d.size() < 512) {
            err = "DPS-1: the PROM " + file + " must have 512 bytes";
            return false;
        }
        prom.assign(d.begin(), d.begin() + 512);
    }
    return true;
}

uint64_t Dps1::us(double v) const
{
    if (ims) return ims->us(v);
    double hz = bus ? (double)bus->cpu_hz : 2e6;
    return (uint64_t)(v * hz / 1e6 + 0.5);
}

void Dps1::reset()
{
    d2c.reset();
    d1c.reset();
    pon = true;            // /RES sets PON: the drivers are off until the program clears RER
    err = zs = false;      // RER floats high (R9) while the 8255 is reset
    davo = obfb = qa = qe = false;
    vbits = 0;
    eois = false;
    prev_err = false;
    uint64_t now = bus ? bus->now : 0;
    ifc_until = jp1 ? now + us(1000) : 0;   // the RESET button pulls /RTL: IFC through V1
    changed_by_cpu();
}

void Dps1::sync(void)
{
    if (!bus || !bus->plant) return;
    bus->plant->advance(bus->now);
    Ims2Bus* b = bus->plant->ims2();
    if (b && !ims) b->attach(this);   // the cable of X2 to the bench
}

void Dps1::advance(uint64_t now)
{
    // the timed events run in the bench (Plant::advance); without a bench the IFC of JP1 ends here
    if (!ims && ifc_until && now >= ifc_until) evaluate(now);
}

void Dps1::changed_by_cpu(void)
{
    uint64_t now = bus ? bus->now : 0;
    if (ims) {
        evaluate(ims->t > now ? ims->t : now);
        ims->settle(now);
    } else {
        evaluate(now);
    }
}

// ---------------------------------------------------------------- the logic of the card

void Dps1::evaluate(uint64_t t)
{
    uint8_t rx = ims ? ims->ctl_lines : 0;     // received lines (the terminations hold them released)
    uint8_t drx = ims ? ims->dio_lines : 0;
    bool dav = rx & IMS_DAV, dac = rx & IMS_NDAC, rdy = rx & IMS_NRFD;
    bool eoi_i = rx & IMS_EOI, atn_i = rx & IMS_ATN, srq_i = rx & IMS_SRQ;

    for (int round = 0; round < 4; round++) {
        // port C pins: what the 8255 drives, the rest float high (TTL inputs, R9 pull-up of RER)
        uint8_t drv2, drv1;
        uint8_t pc2 = d2c.pc_out(drv2) | (uint8_t)~drv2;
        uint8_t pc1 = d1c.pc_out(drv1) | (uint8_t)~drv1;
        cen = pc2 & 0x20;
        rer = pc2 & 0x40;
        list = pc2 & 0x80;
        if (!rer) pon = false;               // the PON latch (D7A): RER = 0 clears it
        if (rer) err = zs = false;           // /RER: CLR of ERR, PRE of the ZS flip-flop
        // the shift register 74164 runs while /OBF B is low
        bool ob = !(pc1 & 0x02);
        if (ob && !obfb) obf_t = t;
        obfb = ob;
        bool qa_now = obfb && t >= obf_t + us(0.5);
        if (qa_now && !qa && !rer && !dac && !rdy) err = true;   // nobody listens: NDAC and NRFD both high
        qa = qa_now;
        qe = obfb && t >= obf_t + us(2.5);
        // DAV-O, the latch of D2A: set after T1 with NRFD released, held while /OBF B and NDAC
        for (int k = 0; k < 3; k++) {
            bool n35 = !(obfb && davo && dac);
            bool n34 = !(!err && qe && !rdy);
            davo = !(n35 && n34);
        }
        // /ACK B = the received NDAC: all listeners accepted -> the 8255 sets /OBF B high
        bool again = false;
        if (!dac && d1c.strobed_out_b() && d1c.b_output_full()) {
            d1c.ack_b();
            again = true;
        }
        // STB A: the start of DAV, held low with LIST and EOI (parallel poll)
        if (d1c.strobed_in_a()) {
            if (dav && !(prev_ctl & IMS_DAV)) d1c.strobe_a(drx);
            else if (list && eoi_i && !d1c.a_input_full()) d1c.strobe_a(drx);
        }
        if (!again) break;
    }
    if (dav && !(prev_ctl & IMS_DAV)) {   // D4D clocked by DAV: the PROM message number and EOI
        vbits = prom[(atn_i ? 256 : 0) | drx] & 0x0F;
        eois = eoi_i;
    }
    // ZS: edges of ATN, EOI, IFC, REN; SRQ and ERR going on
    uint8_t changes = (uint8_t)(rx ^ prev_ctl);
    bool edge = (changes & (IMS_ATN | IMS_EOI | IMS_IFC | IMS_REN)) || (srq_i && !(prev_ctl & IMS_SRQ)) ||
                (err && !prev_err);
    if (edge && !rer) zs = true;
    prev_ctl = rx;
    prev_err = err;

    // the drivers
    uint8_t drv2, drv1;
    uint8_t pc2 = d2c.pc_out(drv2) | (uint8_t)~drv2;
    uint8_t pc1 = d1c.pc_out(drv1) | (uint8_t)~drv1;
    bool ibfa = pc1 & 0x20;
    uint8_t c = 0, d = 0;
    if (!pon && cen) {
        if (pc2 & 0x01) c |= IMS_IFC;
        if (pc2 & 0x02) c |= IMS_REN;
        if (pc2 & 0x04) c |= IMS_EOI;
        if (pc2 & 0x08) c |= IMS_ATN;
    }
    if (!pon) {
        if (pc2 & 0x10) c |= IMS_SRQ;
        if (davo) c |= IMS_DAV;
        if (ibfa) c |= IMS_NRFD;
        if (list && !(ibfa && dav)) c |= IMS_NDAC;   // own NDAC only as a listener (see dps1.h)
        if (obfb) d = d1c.out_b;
    }
    if (ifc_until && t >= ifc_until) ifc_until = 0;
    if (jp1 && ifc_until) c |= IMS_IFC;
    if (jp2 && atn_i && eoi_i && !cen) d |= 0x01;    // parallel poll answer through V2
    ctl = c;
    dio = d;
}

void Dps1::lines_changed(uint64_t t)
{
    evaluate(t);
}

uint64_t Dps1::next_event() const
{
    uint64_t e = UINT64_MAX;
    if (obfb && !qa) e = obf_t + us(0.5);
    else if (obfb && !qe) e = obf_t + us(2.5);
    if (ifc_until && ifc_until < e) e = ifc_until;
    return e;
}

void Dps1::on_event(uint64_t t)
{
    evaluate(t);
}

int Dps1::irq_lines()
{
    uint8_t drv1;
    uint8_t pc1 = d1c.pc_out(drv1) | (uint8_t)~drv1;
    bool srq_i = ims && ims->asserted(IMS_SRQ);
    bool iesrq = pc1 & 0x40, iezs = pc1 & 0x80;
    bool intr = (zs && iezs) || (srq_i && iesrq) || d1c.intr_a() || (pc1 & 0x01 && (drv1 & 0x01));
    return intr ? IRQ_INT0 : 0;
}

// ---------------------------------------------------------------- the 8255 inputs

uint8_t Dps1::Port::pins_a()
{
    Dps1* c = card;
    uint8_t rx = c->ims ? c->ims->ctl_lines : 0;
    if (which == 1) return c->ims ? c->ims->dio_lines : 0;   // D1C PA: DI1-DI8 (mode 0 talker reads them)
    uint8_t v = 0;
    if (rx & IMS_IFC) v |= 0x01;
    if (rx & IMS_REN) v |= 0x02;
    if (rx & IMS_EOI) v |= 0x04;
    if (rx & IMS_ATN) v |= 0x08;
    if (rx & IMS_SRQ) v |= 0x10;
    if (c->err) v |= 0x20;
    if (c->pon) v |= 0x40;
    if (c->zs) v |= 0x80;
    return v;
}

uint8_t Dps1::Port::pins_b()
{
    Dps1* c = card;
    if (which == 1) return 0xFF;
    uint8_t rx = c->ims ? c->ims->ctl_lines : 0;
    uint8_t v = c->vbits & 0x0F;
    if (c->eois) v |= 0x10;
    if (rx & IMS_NRFD) v |= 0x20;   // RDY: the received NRFD
    if (rx & IMS_NDAC) v |= 0x40;   // DAC: the received NDAC
    if (rx & IMS_DAV) v |= 0x80;
    return v;
}

uint8_t Dps1::Port::pins_c()
{
    Dps1* c = card;
    if (which == 0) return 0xFF;
    uint8_t rx = c->ims ? c->ims->ctl_lines : 0;
    uint8_t v = 0xFF;
    if (!(rx & IMS_NDAC)) v &= (uint8_t)~0x04;                           // /ACK B
    if (c->list && (rx & IMS_EOI)) v &= (uint8_t)~0x10;                  // /STB A held
    return v;
}

void Dps1::Port::outputs_changed(int port, uint8_t value)
{
    // the logic is evaluated after each access of the CPU (io_read / io_write)
}

// ---------------------------------------------------------------- the CPU side

bool Dps1::io_read(uint8_t port, uint8_t& data)
{
    if ((port & 0xF8) != 0x30) return false;
    sync();
    evaluate(bus ? bus->now : 0);
    I8255& p = (port & 4) ? d1c : d2c;
    data = p.read(port & 3);
    if (port == 0x34) changed_by_cpu();   // reading the data clears IBF A: NRFD released
    return true;
}

bool Dps1::io_write(uint8_t port, uint8_t data)
{
    if ((port & 0xF8) != 0x30) return false;
    sync();
    I8255& p = (port & 4) ? d1c : d2c;
    p.write(port & 3, data);
    changed_by_cpu();
    return true;
}

int Dps1::decode_io(uint8_t port, const char** what)
{
    if ((port & 0xF8) != 0x30) return 0;
    if (what) *what = (port & 4) ? "D1C 8255 (IMS-2 data)" : "D2C 8255 (IMS-2 control)";
    return DEC_READ | DEC_WRITE;
}

void Dps1::describe(json& out)
{
    out["cable"] = ims != nullptr;
    out["pon"] = pon;
    out["controller_cen"] = cen;
    out["listen"] = list;
    out["err"] = err;
    out["zs"] = zs;
    out["dav_out"] = davo;
    out["lines"] = ims ? ims->ctl_lines : 0;
    out["dio"] = ims ? ims->dio_lines : 0;
}

void Dps1::state(StateIO& io)
{
    d2c.state(io);
    d1c.state(io);
    io.v(pon);
    io.v(err);
    io.v(zs);
    io.v(davo);
    io.v(obfb);
    io.v(obf_t);
    io.v(qa);
    io.v(qe);
    io.v(vbits);
    io.v(eois);
    io.v(prev_ctl);
    io.v(prev_err);
    io.v(ifc_until);
    io.v(cen);
    io.v(list);
    io.v(rer);
    io.v(dio);
    io.v(ctl);
}
