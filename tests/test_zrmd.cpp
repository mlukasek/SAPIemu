// ZRMD-1 / ZRD-1V: port interface, address wrap, write protection, modes,
// saving, RTC, MAP lines.
#include <time.h>
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static Machine* make(const json& zcfg)
{
    json cfg;
    cfg["cards"] = json::array();
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    json z = zcfg;
    z["slot"] = 2;
    if (!z.contains("type")) z["type"] = "ZRMD-1";
    cfg["cards"].push_back(z);
    Machine* m = new Machine();
    std::string err;
    if (!m->load_json(cfg, err)) {
        fprintf(stderr, "load: %s\n", err.c_str());
        delete m;
        return nullptr;
    }
    return m;
}

static void set_addr(Bus& b, uint32_t a)
{
    b.io_write(0x61, (uint8_t)a);
    b.io_write(0x62, (uint8_t)(a >> 8));
    b.io_write(0x63, (uint8_t)(a >> 16));
}

static void test_ports(void)
{
    Machine* m = make({ { "chips", 8 } });
    CHECK(m != nullptr);
    if (!m) return;
    Bus& b = m->bus;
    set_addr(b, 0x0112FE);
    b.io_write(0x60, 'A');
    b.io_write(0x60, 'B');
    b.io_write(0x60, 'C');                // low byte wraps: 0112FE, 0112FF, 011200
    set_addr(b, 0x0112FE);
    CHECK_EQ(b.io_read(0x60), 'A');
    CHECK_EQ(b.io_read(0x60), 'B');
    CHECK_EQ(b.io_read(0x60), 'C');       // read from 011200, not 011300
    set_addr(b, 0x011300);
    CHECK_EQ(b.io_read(0x60), 0xE5);      // untouched (empty image = E5)
    CHECK_EQ(b.io_read(0x61), 0xFF);      // 61h-63h are write only: open bus
    CHECK_EQ(b.io_read(0x64), 0xFF);      // not decoded
    delete m;
}

static void test_protection(void)
{
    Machine* m = make({ { "write_enable", { true, false, true, true } } });
    if (!m) return;
    Bus& b = m->bus;
    set_addr(b, 0x0FFFFF); b.io_write(0x60, 0x11);   // S1 on: written
    set_addr(b, 0x100000); b.io_write(0x60, 0x22);   // S2 off: ignored
    set_addr(b, 0x200000); b.io_write(0x60, 0x33);   // S3 on
    set_addr(b, 0x0FFFFF); CHECK_EQ(b.io_read(0x60), 0x11);
    set_addr(b, 0x100000); CHECK_EQ(b.io_read(0x60), 0xE5);
    set_addr(b, 0x200000); CHECK_EQ(b.io_read(0x60), 0x33);
    delete m;

    m = make({ { "mode", "readonly" } });
    if (!m) return;
    set_addr(m->bus, 0x10); m->bus.io_write(0x60, 0x44);
    set_addr(m->bus, 0x10); CHECK_EQ(m->bus.io_read(0x60), 0xE5);
    delete m;

    // only 2 chips fitted: above 1 MB the bus floats
    m = make({ { "chips", 2 } });
    if (!m) return;
    set_addr(m->bus, 0x0FFFFF); CHECK_EQ(m->bus.io_read(0x60), 0xE5);
    set_addr(m->bus, 0x100000); CHECK_EQ(m->bus.io_read(0x60), 0xFF);
    delete m;
}

static void test_save(void)
{
    // test image in work/ (ignored by git)
    std::string dir = path_join(data_root(), "work/tests");
    std::string path = path_join(dir, "zrmd_rw.bin");
    std::vector<uint8_t> init(512 * 1024, 0x00);
    CHECK(make_dirs(dir));
    CHECK(file_write_all(path, init.data(), init.size()));
    Machine* m = make({ { "chips", 1 }, { "image", path }, { "mode", "rw" } });
    if (!m) return;
    set_addr(m->bus, 0x1234);
    m->bus.io_write(0x60, 0x5A);
    m->flush();
    std::vector<uint8_t> data;
    CHECK(file_read_all(path, data));
    CHECK_EQ(data.size(), 512 * 1024);
    if (data.size() > 0x1234) CHECK_EQ(data[0x1234], 0x5A);
    delete m;

    // nosave: the file stays unchanged
    m = make({ { "chips", 1 }, { "image", path }, { "mode", "nosave" } });
    if (!m) return;
    set_addr(m->bus, 0x1234);
    m->bus.io_write(0x60, 0x77);
    m->flush();
    delete m;
    CHECK(file_read_all(path, data));
    if (data.size() > 0x1234) CHECK_EQ(data[0x1234], 0x5A);
}

static void test_rtc_map(void)
{
    Machine* m = make({ { "map_jp2", true } });
    if (!m) return;
    Bus& b = m->bus;
    time_t t = time(NULL);
    struct tm lt;
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    int yy = (b.io_read(0x7B) & 0x0F) * 10 + (b.io_read(0x7A) & 0x0F);
    CHECK_EQ(yy, lt.tm_year % 100);
    CHECK_EQ(b.io_read(0x7B) & 0xF0, 0xF0);   // D4-D7 not driven
    int mo = (b.io_read(0x79) & 0x0F) * 10 + (b.io_read(0x78) & 0x0F);
    CHECK_EQ(mo, lt.tm_mon + 1);

    // set the year to 1999 while HOLD is active
    b.io_write(0x7D, 0x01);
    b.io_write(0x7B, 9);
    b.io_write(0x7A, 9);
    b.io_write(0x7D, 0x00);
    yy = (b.io_read(0x7B) & 0x0F) * 10 + (b.io_read(0x7A) & 0x0F);
    CHECK_EQ(yy, 99);

    b.io_write(0x63, 0x40);
    CHECK(b.map & MAP_1);
    b.io_write(0x63, 0x80);
    CHECK(!(b.map & MAP_1) && (b.map & MAP_2));
    b.io_write(0x63, 0x00);
    CHECK_EQ(b.map, 0);
    delete m;
}

int main(void)
{
    data_root_init();
    test_ports();
    test_protection();
    test_save();
    test_rtc_map();
    return TEST_RESULT();
}
