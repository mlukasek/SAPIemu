// DSE-1: the 8255 and the register through io_write / io_read with the primitives of the Tesla program (WRSIM,
// RDSIM, RDEPR, PRGBYTE): the simulator RAM, the programming pulse (length, sampled data, bits 1 -> 0 only), the
// cables and EPROM types, Vpp, /RES, the ports and mirrors, MAP1 by JP1, Emil's copy EPROM -> RAM, the pulse
// statistics and the state. No ROM: the CPU card does not run (DSE_PART=unit).
// The programs in their machines (the EPROM read from its original in media/, nothing written):
// - DSE_PART=tesla: the program DSE of Tesla in machines/zps/ZPS-2_DSE-1.sapi (K, P, S, R);
// - DSE_PART=simprog: SIMPROG of Melsoft (disk 7) in a copy of ZPS-6 with a DSE-1 (P, 98 ms pulses at 2 MHz).
#include <stdlib.h>
#include <string.h>
#include "cards/dse1.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static const char* const F2716 = "work/test/dse1_2716.bin";   // 000-0FF blank, then i ^ 5Ah
static const char* const F2732 = "work/test/dse1_2732.bin";   // lower 2 KB a ^ 0Fh, upper 2 KB a ^ F0h
static const char* const FBAD = "work/test/dse1_bad.bin";     // 1000 B

static uint8_t p16(int a) { return (uint8_t)((a & 0x7FF) ^ 0x5A); }   // the 2716 above 0FFh
static uint8_t lo32(int a) { return (uint8_t)((a & 0x7FF) ^ 0x0F); }
static uint8_t hi32(int a) { return (uint8_t)((a & 0x7FF) ^ 0xF0); }

static void make_files(void)
{
    std::vector<uint8_t> a(2048), b(4096), c(1000, 0xFF);
    for (int i = 0; i < 2048; i++) a[i] = i < 0x100 ? 0xFF : p16(i);
    for (int i = 0; i < 4096; i++) b[i] = i < 2048 ? lo32(i) : hi32(i);
    CHECK(file_write_all(data_path(F2716), a.data(), a.size()));
    CHECK(file_write_all(data_path(F2732), b.data(), b.size()));
    CHECK(file_write_all(data_path(FBAD), c.data(), c.size()));
}

static json machine(const json& dse)
{
    json cfg = { { "cards", json::array() } };
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    json c = dse;
    c["slot"] = 2;
    c["type"] = "DSE-1";
    if (!c.contains("eprom")) c["eprom"] = F2716;
    cfg["cards"].push_back(c);
    return cfg;
}

static Dse1* dse(Machine& m)
{
    return dynamic_cast<Dse1*>(m.bus.cards[1]);
}

static void out(Machine& m, uint8_t p, uint8_t v)
{
    m.bus.io_write(p, v);
}

static uint8_t in(Machine& m, uint8_t p)
{
    return m.bus.io_read(p);
}

static void wait_ms(Machine& m, double ms)
{
    m.bus.now += (uint64_t)(ms * m.bus.cpu_hz / 1000.0);
}

// WRSIM: 8255 = 80h, PC = H, PB = L, PA = data, PC4 pulse by BSR 09h / 08h
static void sim_write(Machine& m, uint8_t b, uint16_t a, uint8_t d)
{
    out(m, b + 3, 0x80);
    out(m, b + 2, (uint8_t)(a >> 8));
    out(m, b + 1, (uint8_t)a);
    out(m, b, d);
    out(m, b + 3, 0x09);
    out(m, b + 3, 0x08);
}

// RDSIM: 8255 = 90h, PC = H | 20h (R), PB = L, IN PA
static uint8_t sim_read(Machine& m, uint8_t b, uint16_t a)
{
    out(m, b + 3, 0x90);
    out(m, b + 2, (uint8_t)((a >> 8) | 0x20));
    out(m, b + 1, (uint8_t)a);
    return in(m, b);
}

// RDEPR with the register as K/T set it (11h 2716, 10h 2732): 8255 = 90h, PC = H | 80h (R/W), PB = L, IN PA
static uint8_t eprom_read(Machine& m, uint8_t b, uint8_t r, uint16_t a)
{
    out(m, b + 4, r);
    out(m, b + 3, 0x90);
    out(m, b + 2, (uint8_t)((a >> 8) | 0x80));
    out(m, b + 1, (uint8_t)a);
    return in(m, b);
}

// PRGBYTE: register 13h, 8255 = 80h, PC = H, PB = L, PA = data, BSR 0Dh (PC6 = 1) repeated for `ms`, then
// `end` (BSR 0Ch, or a control word)
static void eprom_program(Machine& m, uint8_t b, uint16_t a, uint8_t d, double ms, uint8_t end = 0x0C)
{
    out(m, b + 4, 0x13);
    out(m, b + 3, 0x80);
    out(m, b + 2, (uint8_t)(a >> 8));
    out(m, b + 1, (uint8_t)a);
    out(m, b, d);
    out(m, b + 3, 0x0D);
    wait_ms(m, ms / 2);
    out(m, b + 3, 0x0D);   // the loop of the program writes it again: no new pulse
    wait_ms(m, ms / 2);
    out(m, b + 3, end);
}

static bool load(Machine& m, const json& cfg)
{
    std::string err;
    bool ok = m.load_json(machine(cfg), err);
    if (!ok) fprintf(stderr, "load: %s\n", err.c_str());
    CHECK(ok);
    CHECK(dse(m) != nullptr);
    return ok && dse(m);
}

static void test_ram(void)
{
    Machine m;
    if (!load(m, json::object())) return;
    Dse1* d = dse(m);
    CHECK_EQ(d->eprom().type, 2716);
    sim_write(m, 0xF0, 0x123, 0xA5);
    sim_write(m, 0xF0, 0xFFF, 0x5A);
    sim_write(m, 0xF0, 0x000, 0x3C);
    CHECK_EQ(sim_read(m, 0xF0, 0x123), 0xA5);
    CHECK_EQ(sim_read(m, 0xF0, 0xFFF), 0x5A);
    CHECK_EQ(sim_read(m, 0xF0, 0x000), 0x3C);
    CHECK_EQ(d->sim_ram()[0x123], 0xA5);
    // the control word clears PC: R goes away, nobody drives PD
    out(m, 0xF3, 0x90);
    out(m, 0xF2, 0x21);
    out(m, 0xF1, 0x23);
    CHECK_EQ(in(m, 0xF2), 0x21);
    CHECK_EQ(in(m, 0xF0), 0xA5);
    out(m, 0xF3, 0x90);
    CHECK_EQ(in(m, 0xF2), 0x00);
    CHECK_EQ(in(m, 0xF0), 0xFF);
    // the write is a level: while PC4 = 1 every new address gets PD
    out(m, 0xF3, 0x80);
    out(m, 0xF0, 0x77);
    out(m, 0xF2, 0x14);   // W, A8-A11 = 4
    out(m, 0xF1, 0x00);
    out(m, 0xF1, 0x01);
    out(m, 0xF0, 0x78);   // the data change: the cell follows
    out(m, 0xF3, 0x08);
    CHECK_EQ(d->sim_ram()[0x400], 0x77);
    CHECK_EQ(d->sim_ram()[0x401], 0x78);
    CHECK(d->warnings().empty());
    // reads of the control word and of the register: FFh; the register is write only
    CHECK_EQ(in(m, 0xF3), 0xFF);
    CHECK_EQ(in(m, 0xF4), 0xFF);
}

static void test_program_2716(void)
{
    Machine m;
    if (!load(m, json::object())) return;
    Dse1* d = dse(m);
    const std::vector<uint8_t>& e = d->eprom().data;
    eprom_program(m, 0xF0, 0x010, 0xA5, 50);
    CHECK_EQ(e[0x010], 0xA5);
    eprom_program(m, 0xF0, 0x010, 0x0F, 50);     // bits only 1 -> 0
    CHECK_EQ(e[0x010], 0x05);
    eprom_program(m, 0xF0, 0x011, 0x00, 0.5);    // too short
    CHECK_EQ(e[0x011], 0xFF);
    eprom_program(m, 0xF0, 0x012, 0xC3, 50, 0x80);   // ended by a control word (PA = 00h): PD before it counts
    CHECK_EQ(e[0x012], 0xC3);
    // the data change during the pulse: AND of what PD had
    out(m, 0xF4, 0x13);
    out(m, 0xF3, 0x80);
    out(m, 0xF2, 0x00);
    out(m, 0xF1, 0x13);
    out(m, 0xF0, 0xF0);
    out(m, 0xF3, 0x0D);
    wait_ms(m, 20);
    out(m, 0xF0, 0x3C);
    wait_ms(m, 30);
    out(m, 0xF3, 0x0C);
    CHECK_EQ(e[0x013], 0x30);
    CHECK(d->pulse_on() == false);
    // read back (K, T, C/P: register 11h), program verify at 25 V too
    CHECK_EQ(eprom_read(m, 0xF0, 0x11, 0x010), 0x05);
    CHECK_EQ(eprom_read(m, 0xF0, 0x11, 0x012), 0xC3);
    CHECK_EQ(eprom_read(m, 0xF0, 0x11, 0x123), p16(0x123));
    CHECK_EQ(eprom_read(m, 0xF0, 0x13, 0x010), 0x05);
    CHECK_EQ(eprom_read(m, 0xF0, 0x11, 0x810), 0x05);    // A11 is not connected for a 2716
    CHECK_EQ(eprom_read(m, 0xF0, 0x00, 0x010), 0xFF);    // Vpp 0 V: the 2716 does not read (assumption)
    CHECK_EQ(eprom_read(m, 0xF0, 0x10, 0x010), 0xFF);
    CHECK(d->eprom().dirty);
    CHECK(!d->unsaved());   // tests: no files written
    CHECK(d->warnings().empty());
}

static void test_2732(void)
{
    // a 2732 in the cable modified after DSE.DOC: reads with 10h (OE/Vpp = 0 V), not with 11h (5 V on /OE)
    {
        Machine m;
        if (!load(m, { { "x2_cable", "2732" }, { "eprom", F2732 } })) return;
        Dse1* d = dse(m);
        CHECK_EQ(d->eprom().type, 2732);
        CHECK_EQ(eprom_read(m, 0xF0, 0x10, 0x123), lo32(0x123));
        CHECK_EQ(eprom_read(m, 0xF0, 0x10, 0x923), hi32(0x123));
        CHECK_EQ(eprom_read(m, 0xF0, 0x11, 0x123), 0xFF);
        eprom_program(m, 0xF0, 0x900, 0x3C, 50);           // F0h AND 3Ch
        CHECK_EQ(d->eprom().data[0x900], 0x30);
        CHECK_EQ(eprom_read(m, 0xF0, 0x10, 0x900), 0x30);
        CHECK(d->warnings().empty());
    }
    // a 2732 in the cable as supplied (for 2716): A11 = Vpp, so 11h reads the upper 2 KB, 00h the lower one;
    // it never programs (OE/Vpp gets only logic levels) and 25 V goes to A11
    {
        Machine m;
        if (!load(m, { { "eprom", F2732 } })) return;
        Dse1* d = dse(m);
        CHECK_EQ(eprom_read(m, 0xF0, 0x11, 0x123), hi32(0x123));
        CHECK_EQ(eprom_read(m, 0xF0, 0x00, 0x123), lo32(0x123));
        eprom_program(m, 0xF0, 0x100, 0x00, 50);
        CHECK_EQ(d->eprom().data[0x100], lo32(0x100));
        CHECK_EQ(d->eprom().data[0x900], hi32(0x100));
        std::vector<std::string> w = d->warnings();
        CHECK(w.size() == 1 && w[0].find("A11") != std::string::npos);
    }
    // two sockets: each chip in its own
    {
        Machine m;
        if (!load(m, { { "x2_cable", "dual" }, { "eprom", F2732 } })) return;
        CHECK_EQ(dse(m)->socket_wiring(), (int)Dse1::CABLE_2732);
        CHECK_EQ(eprom_read(m, 0xF0, 0x10, 0x923), hi32(0x123));
        std::string err;
        CHECK(m.set_live_param(dse(m), "eprom", json(F2716), err));
        CHECK_EQ(dse(m)->socket_wiring(), (int)Dse1::CABLE_2716);
        CHECK_EQ(eprom_read(m, 0xF0, 0x11, 0x123), p16(0x123));
        CHECK((*m.card_config(dse(m)))["eprom"] == F2716);   // the description keeps up
    }
    // a 2716 in the 2732 cable: its Vpp pin gets PA11, its /OE the Vpp net
    {
        Machine m;
        if (!load(m, { { "x2_cable", "2732" } })) return;
        CHECK_EQ(eprom_read(m, 0xF0, 0x10, 0x123), 0xFF);              // Vpp pin = A11 = 0 V
        CHECK_EQ(eprom_read(m, 0xF0, 0x10, 0x923), p16(0x123));      // 5 V on its Vpp: reads
        eprom_program(m, 0xF0, 0x020, 0x00, 50);
        CHECK_EQ(dse(m)->eprom().data[0x020], 0xFF);
        std::vector<std::string> w = dse(m)->warnings();
        CHECK(w.size() == 1 && w[0].find("/OE") != std::string::npos);
    }
}

static void test_vpp(void)
{
    Machine m;
    if (!load(m, { { "vpp", "none" } })) return;
    Dse1* d = dse(m);
    eprom_program(m, 0xF0, 0x010, 0x00, 50);
    CHECK_EQ(d->eprom().data[0x010], 0xFF);
    CHECK_EQ(d->vpp_volts(), 0);
    CHECK_EQ(eprom_read(m, 0xF0, 0x11, 0x123), 0xFF);   // "bez 30 V nefunguje ani cteni"
    Machine a;
    if (!load(a, { { "vpp", "converter" } })) return;
    out(a, 0xF4, 0x01);
    CHECK_EQ(dse(a)->vpp_volts(), 5);
    out(a, 0xF4, 0x03);
    CHECK_EQ(dse(a)->vpp_volts(), 25);
    out(a, 0xF4, 0x02);   // U0V = 0 closes the regulator also with U25V
    CHECK_EQ(dse(a)->vpp_volts(), 0);
}

static void test_reset(void)
{
    Machine m;
    if (!load(m, json::object())) return;
    Dse1* d = dse(m);
    sim_write(m, 0xF0, 0xABC, 0x42);
    sim_write(m, 0xF0, 0xFFF, 0x24);
    out(m, 0xF3, 0x93);   // ONLINE of the Tesla program: PA, PB, PC0-3 inputs, PC = 20h (R), ONLA + ONLD
    out(m, 0xF2, 0x20);
    out(m, 0xF4, 0x3C);   // + LED1, LED2 / MAP
    CHECK(d->onla() && d->onld() && d->led1() && d->led2());
    CHECK_EQ(m.bus.map, (int)MAP_1);
    CHECK(d->warnings().empty());
    m.bus.reset();
    CHECK_EQ(d->reg(), 0);
    CHECK(!d->onla() && !d->onld() && !d->led1() && !d->led2());
    CHECK_EQ(m.bus.map, 0);
    CHECK_EQ(d->sim_ram()[0xABC], 0x42);
    CHECK_EQ(d->sim_ram()[0xFFF], 0x24);   // the write with floating lines is not modelled
    CHECK_EQ(in(m, 0xF0), 0xFF);
    CHECK_EQ(sim_read(m, 0xF0, 0xABC), 0x42);
    // /RES during a pulse ends it (and the run)
    out(m, 0xF4, 0x13);
    out(m, 0xF3, 0x80);
    out(m, 0xF1, 0x30);
    out(m, 0xF0, 0x81);
    out(m, 0xF3, 0x0D);
    CHECK(d->pulse_on());
    wait_ms(m, 50);
    m.bus.reset();
    CHECK(!d->pulse_on());
    CHECK(!d->run().active);
    CHECK_EQ(d->eprom().data[0x030], 0x81);
}

static void test_ports(void)
{
    Machine m;
    if (!load(m, { { "base", "20h" }, { "jp1_map", false } })) return;
    Dse1* d = dse(m);
    sim_write(m, 0x20, 0x555, 0x99);
    CHECK_EQ(sim_read(m, 0x20, 0x555), 0x99);
    out(m, 0x25, 0x10);
    CHECK(d->led1() && !d->led2());
    out(m, 0x27, 0x20);   // base+5..7 mirror the register
    CHECK(!d->led1() && d->led2());
    CHECK_EQ(m.bus.map, 0);   // JP1 open
    out(m, 0x26, 0x00);
    CHECK_EQ(d->reg(), 0);
    for (int p = 0x23; p <= 0x27; p++) CHECK_EQ(in(m, (uint8_t)p), 0xFF);
    const char* what = nullptr;
    CHECK_EQ(d->decode_io(0x27, &what), DEC_READ | DEC_WRITE);
    CHECK_EQ(d->decode_io(0x20, &what), DEC_READ | DEC_WRITE);
    CHECK_EQ(d->decode_io(0x28, &what), 0);   // base+8..F not decoded
    CHECK_EQ(d->decode_io(0xF0, &what), 0);
    out(m, 0x2C, 0x3F);
    CHECK_EQ(d->reg(), 0);
    // the default: F0h and JP1 fitted
    Machine f;
    if (!load(f, json::object())) return;
    out(f, 0xF5, 0x20);
    CHECK_EQ(f.bus.map, (int)MAP_1);
    CHECK_EQ(dse(f)->map_lines(), (int)MAP_1);
    out(f, 0xF4, 0x1F);
    CHECK_EQ(f.bus.map, 0);
}

// Emil's DSE4: the EPROM straight into the RAM (PC4 + PC7 with PA as input), then read from the RAM
static void test_copy(void)
{
    Machine m;
    if (!load(m, json::object())) return;
    Dse1* d = dse(m);
    out(m, 0xF4, 0x11);
    out(m, 0xF3, 0x90);
    out(m, 0xF2, 0x91);   // R/W, W, A8-A11 = 1
    for (int a = 0; a < 256; a++) out(m, 0xF1, (uint8_t)a);
    out(m, 0xF3, 0x08);   // W off
    for (int a = 0x100; a < 0x200; a++) CHECK_EQ(d->sim_ram()[a], d->eprom().data[a]);
    CHECK_EQ(sim_read(m, 0xF0, 0x1A5), p16(0x1A5));
    CHECK(d->warnings().empty());
    // RAM read and EPROM read at once: a clash on PD, wired AND, one warning
    out(m, 0xF2, 0xA1);
    out(m, 0xF1, 0xA5);
    CHECK_EQ(in(m, 0xF0), p16(0x1A5) & d->sim_ram()[0x1A5]);
    CHECK_EQ((int)d->warnings().size(), 1);
}

static void test_stats(void)
{
    Machine m;
    if (!load(m, json::object())) return;
    Dse1* d = dse(m);
    eprom_program(m, 0xF0, 0x000, 0x11, 50);
    eprom_program(m, 0xF0, 0x001, 0x22, 50);
    eprom_program(m, 0xF0, 0x002, 0x33, 50);
    eprom_program(m, 0xF0, 0x003, 0x44, 25);   // two pulses of 25 ms (DSE.COM style)
    eprom_program(m, 0xF0, 0x003, 0x44, 25);
    eprom_program(m, 0xF0, 0x004, 0x55, 0.5);
    CHECK(d->run().active);
    CHECK(d->last_pulse_ms() > 0.49 && d->last_pulse_ms() < 0.51);
    out(m, 0xF4, 0x11);   // Vpp 5 V: the run ends
    const Dse1::Run& r = d->run();
    CHECK(!r.active);
    CHECK_EQ(r.pulses, 5);
    CHECK_EQ(r.short_pulses, 1);
    CHECK_EQ(r.bytes, 4);
    CHECK(r.min_t * 1000.0 / m.bus.cpu_hz > 24.9 && r.min_t * 1000.0 / m.bus.cpu_hz < 25.1);
    CHECK(r.max_t * 1000.0 / m.bus.cpu_hz > 49.9 && r.max_t * 1000.0 / m.bus.cpu_hz < 50.1);
    CHECK(r.min_byte_ms > 49.9 && r.max_byte_ms < 50.1);
    CHECK_EQ(d->eprom().data[0x003], 0x44);
    CHECK_EQ(d->eprom().data[0x004], 0xFF);
    // 98 ms (SIMPROG of disk 7 at 2 MHz): programmed, noted in the log
    eprom_program(m, 0xF0, 0x005, 0x66, 98);
    out(m, 0xF4, 0x00);
    CHECK_EQ(d->run().bytes, 1);
    CHECK(d->run().max_byte_ms > 97.9);
    CHECK_EQ(d->eprom().data[0x005], 0x66);
    nlohmann::json j;
    d->describe(j);
    CHECK(j["run"]["pulses"] == 1);
    CHECK(j["eprom"]["type"] == 2716);
}

// The socket as a medium: a wrong size is refused (the chip stays), a new blank one, the empty socket
static void test_media(void)
{
    Machine m;
    if (!load(m, json::object())) return;
    Dse1* d = dse(m);
    std::string err;
    CHECK(!m.set_live_param(d, "eprom", json(FBAD), err));
    CHECK(err.find("2048") != std::string::npos);
    CHECK_EQ(d->eprom().type, 2716);
    std::string rel;
    CHECK(Dse1::new_blank(2732, "work/test", rel, err));
    CHECK(m.set_live_param(d, "eprom", json(rel), err));
    CHECK_EQ(d->eprom().type, 2732);
    CHECK_EQ(d->eprom().data.size(), 4096);
    CHECK(d->eprom().data[0xFFF] == 0xFF && d->eprom().data[0] == 0xFF);
    CHECK(m.set_live_param(d, "eprom", json(""), err));
    CHECK(!d->eprom().loaded());
    CHECK_EQ(eprom_read(m, 0xF0, 0x11, 0x123), 0xFF);
    file_remove(data_path(rel));
    CHECK(d->save_sim_ram(data_path("work/test/dse1_sim.bin"), err));
    std::vector<uint8_t> s;
    CHECK(file_read_all(data_path("work/test/dse1_sim.bin"), s));
    CHECK_EQ(s.size(), 4096);
    file_remove(data_path("work/test/dse1_sim.bin"));
}

// The state in the middle of a pulse: the copy runs on and programs the same
static void test_state(void)
{
    Machine m1;
    if (!load(m1, json::object())) return;
    sim_write(m1, 0xF0, 0x321, 0x5E);
    eprom_program(m1, 0xF0, 0x000, 0x11, 50);
    out(m1, 0xF4, 0x13);
    out(m1, 0xF3, 0x80);
    out(m1, 0xF1, 0x40);
    out(m1, 0xF0, 0xA0);
    out(m1, 0xF3, 0x0D);
    wait_ms(m1, 30);
    std::vector<uint8_t> st, b1, b2;
    std::string err;
    CHECK(m1.save_state(st, "", err));
    Machine m2;
    if (!load(m2, json::object())) return;
    CHECK(m2.load_state(st, err));
    for (Machine* m : { &m1, &m2 }) {
        wait_ms(*m, 20);
        out(*m, 0xF3, 0x0C);
        out(*m, 0xF4, 0x11);
    }
    CHECK_EQ(dse(m2)->eprom().data[0x040], 0xA0);
    CHECK_EQ(dse(m2)->eprom().data[0x000], 0x11);
    CHECK(dse(m1)->eprom().data == dse(m2)->eprom().data);
    CHECK_EQ(dse(m2)->sim_ram()[0x321], 0x5E);
    CHECK_EQ(dse(m2)->run().pulses, 2);
    CHECK(m1.save_state(b1, "", err));
    CHECK(m2.save_state(b2, "", err));
    CHECK(b1 == b2);
}

// ---------------------------------------------------------------- the programs in their machines

// A machine of machines/ with the EPROM of the socket read from its original (no working copy: nothing is written,
// the copy in work/ of the user is not read either); `edit` changes the description first
static bool load_file(Machine& m, const char* file, void (*edit)(json&) = nullptr)
{
    std::string text, err;
    CHECK(file_read_text(data_path(file), text));
    json cfg = json::parse(text, nullptr, false, true);
    if (edit) edit(cfg);
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "DSE-1") c["working_copy"] = false;
    bool ok = m.load_json(cfg, err);
    if (!ok) fprintf(stderr, "%s: %s\n", file, err.c_str());
    CHECK(ok);
    return ok;
}

static Dse1* find_dse(Machine& m)
{
    for (auto& c : m.cards)
        if (Dse1* d = dynamic_cast<Dse1*>(c.get())) return d;
    return nullptr;
}

// The last line of the screen with text: where the program waits for an answer
static std::string last_line(Machine& m)
{
    std::string s = screen_text(m);
    size_t e = s.find_last_not_of(" \r\n");
    if (e == std::string::npos) return std::string();
    size_t b = s.rfind('\n', e);
    b = b == std::string::npos ? 0 : b + 1;
    return s.substr(b, e + 1 - b);
}

// Waits until the program asks with `prompt` on the last line, then types `keys`: the keyboards lose a key typed
// before the program reads them (after OK the DSE program prints its menu line first)
static bool answer(Machine& m, const char* prompt, const char* keys, double timeout_s = 5.0)
{
    for (double t = 0; t < timeout_s; t += 0.05) {
        if (last_line(m).find(prompt) != std::string::npos) {
            m.run_seconds(0.3);
            m.type_text(keys);
            return true;
        }
        m.run_seconds(0.05);
    }
    fprintf(stderr, "NO PROMPT '%s':\n%s\n", prompt, screen_text(m).c_str());
    CHECK(false);
    return false;
}

static uint8_t pattern(int i) { return (uint8_t)(i * 37 + 11); }

// The program DSE of Tesla (MONITOR V4.0F, G1000) in machines/zps/ZPS-2_DSE-1.sapi: K the blank 2716, P 5000-50FF
// into the EPROM from 0000 (50 ms pulses, verified by the program), S the RAM into the simulator and ON LINE, R back.
static void test_tesla(void)
{
    Machine m;
    if (!load_file(m, "machines/zps/ZPS-2_DSE-1.sapi")) return;
    Dse1* d = find_dse(m);
    CHECK(d && d->eprom().type == Eprom27::T2716);
    if (!d) return;
    if (!run_until_text(m, "SAPI-1 V4.0", 5.0)) {
        fprintf(stderr, "NO MONITOR:\n%s\n", screen_text(m).c_str());
        CHECK(false);
        return;
    }
    m.run_seconds(0.3);
    m.type_text("G1000\r");
    // K: the EPROM is blank
    answer(m, "STISKNI KLAVESU:", "K");
    answer(m, "VLOZ EPROM", " ");
    answer(m, "2732? A=ANO", "N");
    answer(m, "DOLNI ADRESA EPROM=", "0\r");
    answer(m, "HORNI ADRESA EPROM=", "7FF\r");
    answer(m, "STISKNI KLAVESU:", "", 10.0);
    CHECK(screen_text(m).find("\nCISTA  0000 - 07FF") != std::string::npos);
    // P: 256 bytes from the RAM, about 13 s with the pulses of 50 ms; the program compares them after
    for (int i = 0; i < 0x800; i++) m.bus.mem_write((uint16_t)(0x5000 + i), pattern(i));
    answer(m, "STISKNI KLAVESU:", "P");
    answer(m, "DOLNI ADRESA RAM=", "5000\r");
    answer(m, "HORNI ADRESA RAM=", "50FF\r");
    answer(m, "DOLNI ADRESA EPROM=", "0\r");
    answer(m, "VLOZ EPROM", " ");
    answer(m, "2732? A=ANO", "N");
    bool ok = run_until_text(m, "SHODNY OBSAH  0000 - 00FF", 30.0);
    if (!ok) fprintf(stderr, "NOT PROGRAMMED:\n%s\n", screen_text(m).c_str());
    CHECK(ok);
    const std::vector<uint8_t>& e = d->eprom().data;
    int wrong = 0;
    for (int i = 0; i < 2048; i++)
        if (e[i] != (i < 0x100 ? pattern(i) : 0xFF)) wrong++;
    CHECK_EQ(wrong, 0);
    CHECK(d->eprom().dirty);
    answer(m, "STISKNI KLAVESU:", "", 5.0);
    const Dse1::Run& r = d->run();
    CHECK(!r.active);   // Vpp went back to 0 V
    CHECK_EQ(r.pulses, 256);
    CHECK_EQ(r.bytes, 256);
    CHECK_EQ(r.short_pulses, 0);
    CHECK(r.min_byte_ms > 49.0 && r.max_byte_ms < 51.0);
    CHECK(d->last_pulse_ms() > 49.0 && d->last_pulse_ms() < 51.0);
    CHECK_EQ(d->vpp_volts(), 0);
    // S: 5000-57FF into the simulator RAM 800-FFF (a 2716 sits at 800-FFF: SA11 of the probe is not wired), ON LINE
    answer(m, "STISKNI KLAVESU:", "S");
    answer(m, "DOLNI ADRESA RAM=", "5000\r");
    answer(m, "EPROM 4K", "N");
    answer(m, "STISKNI KLAVESU:", "", 10.0);
    CHECK(d->onla() && d->onld());
    wrong = 0;
    for (int i = 0; i < 0x800; i++)
        if (d->sim_ram()[0x800 + i] != pattern(i)) wrong++;
    CHECK_EQ(wrong, 0);
    // R: the simulator RAM 800-8FF back to 6000, compared by the program
    answer(m, "STISKNI KLAVESU:", "R");
    answer(m, "DOLNI ADRESA DSE=", "800\r");
    answer(m, "HORNI ADRESA DSE=", "8FF\r");
    answer(m, "DOLNI ADRESA RAM=", "6000\r");
    ok = run_until_text(m, "SHODNY OBSAH  0800 - 08FF", 10.0);
    if (!ok) fprintf(stderr, "NO RECALL:\n%s\n", screen_text(m).c_str());
    CHECK(ok);
    wrong = 0;
    for (int i = 0; i < 0x100; i++)
        if (m.peek((uint16_t)(0x6000 + i)) != pattern(i)) wrong++;
    CHECK_EQ(wrong, 0);
    CHECK(d->warnings().empty());
}

// ZPS-6 with the disk 1 (a copy: SAVE writes it) in A:, the disk 7 of Melsoft in B: and a DSE-1 at F0h
static const char* const ZPS6_A = "work/test/dse1_sapi15_a.sp2";

static void zps6_dse1(json& cfg)
{
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "MFC-1A") {
            c["drive_a"] = { { "image", ZPS6_A }, { "write_protect", false } };
            c["drive_b"] = { { "image", "media/sapi15/SAPI15_7.sp2" }, { "write_protect", true } };
            c["working_copy"] = false;
        }
    cfg["cards"].push_back({ { "slot", 6 }, { "type", "DSE-1" } });
}

// SIMPROG of Melsoft (CP/M, disk 7) in ZPS-6: P programs 0000-00FF of the 2716 from a file. The build on disk 7 is the
// one for 4 MHz (constant 07CEh): at the 2 MHz of ZPS-6 its pulses are 98 ms, the log notes it (the length is measured
// in the time of the bus, as on the hardware).
static void test_simprog(void)
{
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path("media/sapi15/SAPI15_1.sp2"), img));
    CHECK(file_write_all(data_path(ZPS6_A), img.data(), img.size()));
    {
        Machine m;
        if (!load_file(m, "machines/zps/ZPS-6.sapi", zps6_dse1)) return;
        Dse1* d = find_dse(m);
        CHECK(d && d->eprom().type == Eprom27::T2716);
        if (!d || !answer(m, "A>", "", 30.0)) return;
        for (int i = 0; i < 0x100; i++) m.bus.mem_write((uint16_t)(0x100 + i), pattern(i));
        m.type_text("SAVE 1 X.BIN\r");
        m.run_seconds(3.0);   // the prompt is on the last line while the command is typed too
        answer(m, "A>", "B:SIMPROG A:X.BIN\r", 10.0);
        answer(m, "epromtype: 2716", "P", 20.0);
        answer(m, "whole eprom? (y/n):", "N");
        answer(m, "low  address(xxxx):", "0000\r");
        answer(m, "high address(xxxx):", "00FF\r");
        answer(m, "give eprom to prog(ret):", "\r");
        bool ok = answer(m, "ok(ret):", "", 60.0);   // 256 x 98 ms
        if (!ok) fprintf(stderr, "SIMPROG:\n%s\n", screen_text(m).c_str());
        const std::vector<uint8_t>& e = d->eprom().data;
        int wrong = 0;
        for (int i = 0; i < 2048; i++)
            if (e[i] != (i < 0x100 ? pattern(i) : 0xFF)) wrong++;
        CHECK_EQ(wrong, 0);
        const Dse1::Run& r = d->run();
        CHECK(!r.active);
        CHECK_EQ(r.pulses, 256);
        CHECK_EQ(r.bytes, 256);
        CHECK(d->last_pulse_ms() > 97.0 && d->last_pulse_ms() < 99.0);
        CHECK(r.min_byte_ms > 55.0);   // outside 50 ms +-5: the note in the log
    }
    file_remove(data_path(ZPS6_A));
}

int main(void)
{
    test_init();
    const char* part = getenv("DSE_PART");   // ctest runs the parts in parallel (tests/CMakeLists.txt)
    if (!part || !strcmp(part, "unit")) {
        make_files();
        test_ram();
        test_program_2716();
        test_2732();
        test_vpp();
        test_reset();
        test_ports();
        test_copy();
        test_stats();
        test_media();
        test_state();
        file_remove(data_path(F2716));
        file_remove(data_path(F2732));
        file_remove(data_path(FBAD));
    }
    if (!part || !strcmp(part, "tesla")) test_tesla();
    if (!part || !strcmp(part, "simprog")) test_simprog();
    return TEST_RESULT();
}
