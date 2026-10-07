// RTC-1: the real time counter, the system 8253 (tick, interrupt, watchdog) and the fast
// bulk clocking of the 8253 it relies on.
#include "chips/i8253.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

// I8253::ticks() (jumps between OUT changes) must match the same number of single ticks
static void test_8253_bulk(void)
{
    static const uint8_t modes[] = { 0x34, 0x36, 0x35, 0x37, 0x30, 0x38 };   // counter 0: modes 2, 3 (bin, BCD), 0, 4
    static const uint16_t counts[] = { 2, 3, 5, 10, 99, 100, 1000, 2000, 0x1234 };
    static const uint64_t steps[] = { 1, 2, 3, 7, 50, 999, 1000, 1001, 40000 };
    for (uint8_t m : modes)
        for (uint16_t n : counts) {
            I8253 a, b;
            for (I8253* p : { &a, &b }) {
                p->write(3, m);
                p->write(0, (uint8_t)n);
                p->write(0, (uint8_t)(n >> 8));
            }
            uint32_t falls_b = 0;
            for (uint64_t s : steps) {
                uint32_t fa = a.ticks(0, s);
                uint32_t fb = 0;
                for (uint64_t i = 0; i < s; i++) {
                    bool o = b.out(0);
                    b.tick(0);
                    if (o && !b.out(0)) fb++;
                }
                falls_b += fb;
                CHECK_EQ(fa, fb);
                CHECK_EQ(a.out(0), b.out(0));
                a.write(3, 0x00);   // latch and compare the counts
                b.write(3, 0x00);
                CHECK_EQ(a.read(0), b.read(0));
                CHECK_EQ(a.read(0), b.read(0));
            }
            (void)falls_b;
        }
}

static const uint8_t A = 0xA0;   // RTC-1 at A0h (SA3, A7 jumper 1-2 + 3-4), as RTCZ of J. Biba

static json machine(const json& rtc)
{
    json cfg = { { "cards", json::array() } };
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    json c = rtc;
    c["slot"] = 2;
    c["type"] = "RTC-1";
    if (!c.contains("time")) c["time"] = "emulated";
    if (!c.contains("battery")) c["battery"] = false;
    cfg["cards"].push_back(c);
    return cfg;
}

static void run_s(Machine& m, double s)   // let the bus time go on (the cards follow it)
{
    m.bus.now += (uint64_t)(s * m.bus.cpu_hz);
    m.bus.advance_all();
}

static uint32_t read_rtc(Machine& m)
{
    return m.bus.io_read(A) | (m.bus.io_read(A + 1) << 8) | ((m.bus.io_read(A + 2) & 0x0F) << 16);
}

static void write_rtc(Machine& m, uint32_t v)   // WTR of the manual and of RTCZ
{
    m.bus.io_write(A + 3, 0x80);
    m.bus.io_write(A, (uint8_t)v);
    m.bus.io_write(A + 1, (uint8_t)(v >> 8));
    m.bus.io_write(A + 2, (uint8_t)((v >> 16) & 0x0F));
    m.bus.io_write(A + 3, 0x0D);   // PC6 = 1
    m.bus.io_write(A + 12, 0);
    m.bus.io_write(A + 3, 0xB3);
}

static void test_counter(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(json::object()), err));
    m.bus.io_write(A + 3, 0xB3);
    CHECK_EQ(read_rtc(m), 0);
    run_s(m, 5.0);
    CHECK_EQ(read_rtc(m), 2);            // +1 every 2 s
    write_rtc(m, 0x51234);
    CHECK_EQ(read_rtc(m), 0x51234);      // bit 19 through PC3 (jumper 9-11)
    run_s(m, 2.0);
    CHECK_EQ(read_rtc(m), 0x51235);
    // without PC6 the load is blocked
    m.bus.io_write(A + 3, 0x80);
    m.bus.io_write(A, 0);
    m.bus.io_write(A + 12, 0);
    m.bus.io_write(A + 3, 0x9B);
    CHECK_EQ(read_rtc(m), 0x51235);
    CHECK_EQ(m.bus.io_read(A + 3), 0xFF);
    CHECK_EQ(m.bus.io_read(A + 12), 0xFF);
    // /RES: the counter goes on (batteries), the 8255 is reset
    m.bus.reset();
    CHECK_EQ(read_rtc(m), 0x51235);
    // the realtime jumpers: bit 19 cleared by a load
    Machine r;
    CHECK(r.load_json(machine({ { "mode", "realtime" } }), err));
    write_rtc(r, 0xFFFFE);
    r.bus.io_write(A + 3, 0x9B);
    CHECK_EQ(read_rtc(r) & 0x7FFFF, 0x7FFFE);
}

static void test_watchdog(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(json::object()), err));
    m.bus.io_write(A + 3, 0xB3);
    m.bus.io_write(A + 11, 0xB7);        // counter 2 mode 3 BCD 2000: 1 kHz from Phi2 2 MHz
    m.bus.io_write(A + 10, 0x00);
    m.bus.io_write(A + 10, 0x20);
    m.bus.io_write(A + 11, 0x30);        // counter 0 mode 0, 100: 100 ms
    m.bus.io_write(A + 8, 100);
    m.bus.io_write(A + 8, 0);
    m.bus.io_write(A + 3, 0x0F);         // PC7 = 1
    run_s(m, 0.05);
    CHECK(!m.bus.rtl());
    run_s(m, 0.06);
    CHECK(m.bus.rtl());                   // the watchdog ran out
    m.bus.reset();                        // the CPU card answers with /RES: PC7 low
    CHECK(!m.bus.rtl());
    // restarted in time it never fires
    m.bus.io_write(A + 3, 0xB3);
    m.bus.io_write(A + 3, 0x0F);
    for (int i = 0; i < 10; i++) {
        m.bus.io_write(A + 11, 0x30);
        m.bus.io_write(A + 8, 100);
        m.bus.io_write(A + 8, 0);
        run_s(m, 0.08);
        CHECK(!m.bus.rtl());
    }
}

static void test_interrupt(void)
{
    std::string err;
    {   // async: /INT while OUT1 is high (counter 1 mode 0: after 10 ms)
        Machine m;
        CHECK(m.load_json(machine({ { "int", "int0" } }), err));
        m.bus.io_write(A + 11, 0xB7);
        m.bus.io_write(A + 10, 0x00);
        m.bus.io_write(A + 10, 0x20);
        m.bus.io_write(A + 11, 0x70);
        m.bus.io_write(A + 9, 10);
        m.bus.io_write(A + 9, 0);
        run_s(m, 0.005);
        CHECK_EQ(m.bus.irq_lines(), 0);
        run_s(m, 0.01);
        CHECK_EQ(m.bus.irq_lines(), IRQ_INT0);
    }
    {   // realtime: OUT1 (mode 2, 100 ms) strobes PA, INTR A until the CPU reads PA
        Machine m;
        CHECK(m.load_json(machine({ { "int", "int1" }, { "mode", "realtime" } }), err));
        m.bus.io_write(A + 3, 0xB3);
        m.bus.io_write(A + 11, 0xB7);
        m.bus.io_write(A + 10, 0x00);
        m.bus.io_write(A + 10, 0x20);
        m.bus.io_write(A + 11, 0x75);
        m.bus.io_write(A + 9, 0x00);
        m.bus.io_write(A + 9, 0x01);     // BCD 100
        m.bus.io_write(A + 3, 0x09);     // PC4 = INTE A
        run_s(m, 0.05);
        CHECK_EQ(m.bus.irq_lines(), 0);
        run_s(m, 0.06);
        CHECK_EQ(m.bus.irq_lines(), IRQ_INT1);
        m.bus.io_read(A);
        CHECK_EQ(m.bus.irq_lines(), 0);
        run_s(m, 0.1);
        CHECK_EQ(m.bus.irq_lines(), IRQ_INT1);
    }
    {   // no cable from XC2/14
        Machine m;
        CHECK(m.load_json(machine(json::object()), err));
        m.bus.io_write(A + 11, 0x70);
        m.bus.io_write(A + 9, 1);
        m.bus.io_write(A + 9, 0);
        run_s(m, 1.0);
        CHECK_EQ(m.bus.irq_lines(), 0);
    }
}

static void test_pc_time(void)
{
    // Tuesday 2026-10-06 13:30:10 local: 1 day + 13:30:10 after Monday 0:00 -> counter = seconds / 2
    struct tm lt = {};
    lt.tm_year = 2026 - 1900;
    lt.tm_mon = 9;
    lt.tm_mday = 6;   // Tuesday
    lt.tm_hour = 13;
    lt.tm_min = 30;
    lt.tm_sec = 10;
    lt.tm_isdst = -1;
    time_t t = mktime(&lt);
    host_time_fix(t);
    Machine m;
    std::string err;
    CHECK(m.load_json(machine({ { "time", "pc" } }), err));
    m.bus.io_write(A + 3, 0xB3);
    uint32_t want = (86400 + 13 * 3600 + 30 * 60 + 10) / 2;
    CHECK_EQ(read_rtc(m), want);
    host_time_fix(t + 10);
    CHECK_EQ(read_rtc(m), want + 5);
    write_rtc(m, 7);
    host_time_fix(t + 14);
    CHECK_EQ(read_rtc(m), 9);
    // battery: saved at power off, after an hour the counter went on
    battery_files_enable(true);
    std::string file = "work/test/rtc1_test.bin";
    file_remove(data_path(file));
    json cfg = machine({ { "time", "pc" }, { "battery", true }, { "file", file } });
    {
        Machine a;
        CHECK(a.load_json(cfg, err));
        a.bus.io_write(A + 3, 0xB3);
        write_rtc(a, 1000);
    }
    host_time_fix(t + 14 + 3600);
    {
        Machine b;
        CHECK(b.load_json(cfg, err));
        b.bus.io_write(A + 3, 0xB3);
        CHECK_EQ(read_rtc(b), 1000 + 1800);
    }
    file_remove(data_path(file));
    battery_files_enable(false);
    host_time_fix(0);
}

int main(void)
{
    test_init();
    test_8253_bulk();
    test_counter();
    test_watchdog();
    test_interrupt();
    test_pc_time();
    return TEST_RESULT();
}
