// SAPI-1.5 ZPS-6 (machines/zps/ZPS-6.sapi): PKT-1 (Z80, Consul 262.3 on the 8255), RSW-1 (RAM with
// the B000-BFFF hole for MAP2, 8251, 8253, watchdog), MFC-1A with a 5.25" DS DD disk (.sp2) and
// the DBS-1A colour display.
// - both ROMs (EPROM dump, Melsoft ROMBIOS) boot CP/M 62K from the Melsoft disk 1, DIR lists it;
// - SAVE writes the disk (CBIOS deblocking of 256 B sectors, the directory on cylinder 1);
// - PKT-1 port 0Eh: MAP2 switches RSW-1 off at B000-BFFF and the DBS-1A video RAM on (write only);
// - RSW-1 8251 at 9600 Bd (8253 counter 0 = 8) sends and receives through the host channel;
// - the watchdog (counters 1 and 2) resets the system through /RTL when XT2 is fitted.
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"

#include <string.h>
#include "test_util.h"

using nlohmann::json;

// ZPS-6 with a copy of a Melsoft disk in drive A (no working copy, the test file is written)
static json zps6(const char* rom = nullptr, bool watchdog = false)
{
    std::string text;
    CHECK(file_read_text(data_path("machines/zps/ZPS-6.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path("media/sapi15/SAPI15_1.sp2"), img));
    CHECK(file_write_all(data_path("work/test/sapi15_a.sp2"), img.data(), img.size()));
    for (json& c : cfg["cards"]) {
        std::string ty = c.value("type", std::string());
        if (ty == "MFC-1A") {
            c["drive_a"] = { { "image", "work/test/sapi15_a.sp2" }, { "write_protect", false } };
            c["working_copy"] = false;
        }
        if (ty == "PKT-1" && rom) c["rom"] = rom;
        if (ty == "RSW-1" && watchdog) c["watchdog_reset"] = true;
    }
    return cfg;
}

static bool boot(Machine& m)
{
    bool ok = run_until_text(m, "\nA>", 30.0);
    std::string s = screen_text(m);
    CHECK(s.find("PKT-1 62K MON-V100690*LCH") != std::string::npos);
    CHECK(s.find("CBIOS V050690*LCH") != std::string::npos);
    return ok;
}

static void test_boot_dir_save(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(zps6(), err));
    CHECK(boot(m));
    m.type_text("DIR\r");
    CHECK(run_until_text(m, "SYSGEN", 10.0));
    m.run_seconds(0.3);
    for (int i = 0; i < 0x400; i++) m.bus.mem_write((uint16_t)(0x0100 + i), (uint8_t)(i * 5 + 1));
    m.type_text("SAVE 4 TEST.COM\r");
    m.run_seconds(3.0);
    m.type_text("DIR TEST.COM\r");
    CHECK(run_until_text(m, "TEST     COM", 10.0));
    m.flush();
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path("work/test/sapi15_a.sp2"), img));
    CHECK(img.size() == 327680);
    bool dir = false, data = false;
    for (size_t off = 8192; off + 12 <= img.size(); off += 32)   // directory entries on cylinder 1
        if (img[off] == 0 && !memcmp(&img[off + 1], "TEST    COM", 11)) dir = true;
    for (size_t off = 8192; off + 4 <= img.size(); off += 128)
        if (img[off] == 1 && img[off + 1] == 6 && img[off + 2] == 11 && img[off + 3] == 16) data = true;
    CHECK(dir);
    CHECK(data);
    printf("%s\n", screen_text(m).c_str());
}

static void test_rombios_melsoft(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(zps6("roms/pkt-1/PKT-1_ROMBIOS_MELSOFT.bin"), err));
    CHECK(boot(m));
}

static void test_map_and_serial(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(zps6(), err));
    CHECK(boot(m));
    // MAP2 = 1: RSW-1 leaves B000-BFFF, the DBS-1A video RAM takes writes there (not reads)
    m.bus.mem_write(0xB123, 0x5A);
    CHECK_EQ(m.bus.mem_read(0xB123), 0x5A);
    m.bus.io_write(0x0E, 0x03);
    CHECK((m.bus.map & MAP_2) != 0);
    CHECK_EQ(m.bus.mem_read(0xB123), 0xFF);   // nobody drives the bus
    CHECK_EQ(m.bus.mem_read(0xA000), m.bus.mem_read(0xA000));
    m.bus.mem_write(0xB000, 'Q');              // the first cell of the video RAM
    m.bus.io_write(0x0E, 0x01);
    CHECK_EQ(m.bus.mem_read(0xB123), 0x5A);   // the RAM kept its byte
    m.run_seconds(0.1);

    // RSW-1: 8253 counter 0 mode 3, 1.2288 MHz / 8 = 153.6 kHz = 9600 Bd x16; 8251 8N2, TxEN, RxE
    m.bus.io_write(0x1B, 0x36);
    m.bus.io_write(0x1C, 0x08);
    m.bus.io_write(0x1C, 0x00);
    for (int i = 0; i < 3; i++) m.bus.io_write(0x19, 0x00);
    m.bus.io_write(0x19, 0x40);
    m.bus.io_write(0x19, 0xCE);
    m.bus.io_write(0x19, 0x37);
    SerialChannel* ch = m.serials.empty() ? nullptr : m.serials[0];
    CHECK(ch != nullptr);
    if (!ch) return;
    CHECK_EQ(ch->baud, 9600);
    CHECK((m.bus.io_read(0x19) & 0x01) != 0);   // TxRDY
    m.bus.io_write(0x18, 'X');
    m.run_seconds(0.01);   // one character at 9600 Bd 8N2 = 1.15 ms
    std::string got;
    ch->host_receive(got, 16);
    CHECK(got == "X");
    const uint8_t y = 'Y';
    ch->host_send(&y, 1);
    m.run_seconds(0.01);
    CHECK((m.bus.io_read(0x19) & 0x02) != 0);   // RxRDY
    CHECK_EQ(m.bus.io_read(0x18), 'Y');
    CHECK((m.bus.io_read(0x19) & 0x02) == 0);
}

static void watchdog(bool fitted)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(zps6(nullptr, fitted), err));
    CHECK(boot(m));
    m.type_text("DIR\r");
    CHECK(run_until_text(m, "SYSGEN", 10.0));
    // counters 1 and 2 in mode 2, 10 x 10 clocks of 1.2288 MHz = 81 us, then enable
    m.bus.io_write(0x1B, 0x74);
    m.bus.io_write(0x1D, 10);
    m.bus.io_write(0x1D, 0);
    m.bus.io_write(0x1B, 0xB4);
    m.bus.io_write(0x1A, 10);
    m.bus.io_write(0x1A, 0);
    m.bus.io_write(0x1F, 0x01);
    m.run_seconds(0.2);
    std::string s = screen_text(m);
    if (fitted) {
        // the system restarted: the ROM cleared the screen and boots again
        CHECK(s.find("DIR") == std::string::npos);
        CHECK(run_until_text(m, "\nA>", 30.0));
    } else {
        CHECK(s.find("SYSGEN") != std::string::npos);
    }
}

int main(int argc, char** argv)
{
    test_init();
    if (argc > 1) {   // debugging: run a command, print the screen and the DBS-1A state
        Machine m;
        std::string err;
        CHECK(m.load_json(zps6(), err));
        boot(m);
        m.type_text(std::string(argv[1]) + "\r");
        m.run_seconds(argc > 2 ? atof(argv[2]) : 5.0);
        printf("%s\n", screen_text(m).c_str());
        for (auto& c : m.cards) {
            json d;
            c->describe(d);
            printf("%s %s\n", c->type.c_str(), d.dump().c_str());
        }
        if (argc > 4 && m.display) {   // dots of text row argv[3] from column argv[4], 16 characters
            int w = m.display->screen_width(), h = m.display->screen_height();
            std::vector<uint32_t> px((size_t)w * h);
            m.display->render(px.data(), 0xFF000000u);
            int row = atoi(argv[3]), col = atoi(argv[4]);
            for (int y = row * 12; y < row * 12 + 12 && y < h; y++) {
                for (int x = col * 8; x < col * 8 + 128 && x < w; x++) putchar((px[(size_t)y * w + x] & 0xFFFFFF) ? '#' : '.');
                putchar('\n');
            }
        }
        return 0;
    }
    test_boot_dir_save();
    test_rombios_melsoft();
    test_map_and_serial();
    watchdog(false);
    watchdog(true);
    return TEST_RESULT();
}
