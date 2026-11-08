// SAPI-1.5 ZPS-6 (machines/zps/ZPS-6.sapi): PKT-1 (Z80, Consul 262.3 on the 8255), RSW-1 (RAM with
// the B000-BFFF hole for MAP2, 8251, 8253, watchdog), MFC-1A with a 5.25" DS DD disk (.sp2) and
// the DBS-1A colour display.
// - both ROMs (EPROM dump, Melsoft ROMBIOS) boot CP/M 62K from the Melsoft disk 1, DIR lists it;
// - SAVE writes the disk (CBIOS deblocking of 256 B sectors, the directory on cylinder 1);
// - PKT-1 port 0Eh: MAP2 switches RSW-1 off at B000-BFFF and the DBS-1A video RAM on (write only);
// - RSW-1 8251 at 9600 Bd (8253 counter 0 = 8) sends and receives through the host channel;
// - the watchdog (counters 1 and 2) resets the system through /RTL when XT2 is fitted;
// - the image formats .sp2 (40 x 2 x 16 x 256 B) and .sp1 (one side) are known by their size;
// - the EPROM for AND-1Z/AND-1V (machines/sapi15_and1z.sapi, sapi15_and1v.sapi) answers the control codes like the DBS-1 one.
#include "core/charset.h"
#include "chips/floppy.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"

#include <string.h>
#include "test_util.h"

using nlohmann::json;

// ZPS-6 with a copy of a Melsoft disk in drive A (no working copy, the test file is written)
static json zps6(const char* rom = nullptr, bool watchdog = false, const char* disk = "media/sapi15/SAPI15_1.sp2")
{
    std::string text;
    CHECK(file_read_text(data_path("machines/zps/ZPS-6.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path(disk), img));
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

static void test_formats(void)
{
    const FloppyGeom* g2 = floppy_geom_for_size(327680);
    const FloppyGeom* g1 = floppy_geom_for_size(163840);
    CHECK(g2 && g2->tracks == 40 && g2->heads == 2 && g2->sectors == 16 && g2->sector_size() == 256 && g2->mfm);
    CHECK(g1 && g1->tracks == 40 && g1->heads == 1 && g1->sectors == 16 && g1->sector_size() == 256 && g1->mfm);
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

// The EPROM for the displays AND-1Z and AND-1V (tools/pkt1_and.py) against the original for the DBS-1: the same
// control codes through the vector F009h give the same screen. The program is left in JMP $, so the cursor stays drawn
// (row 18, column 0, a cleared row: the character 127 of the AND driver is taken out of the text).
static std::string control_codes(const char* file, const char* rom)
{
    std::string text;
    CHECK(file_read_text(data_path(file), text));
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
    }
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    bool up = run_until_text(m, "\nA>", 30.0);
    CHECK(up);
    if (!up) return std::string();
    CHECK(screen_contains(m, rom));
    m.run_seconds(0.3);

    static const uint8_t prog[] = {
        0x21, 0x20, 0x01,   // LXI H,0120h
        0x7E,               // MOV A,M
        0xFE, 0xFF,         // CPI 0FFh
        0xCA, 0x13, 0x01,   // JZ 0113h
        0xE5,               // PUSH H
        0xCD, 0x09, 0xF0,   // CALL 0F009h
        0xE1,               // POP H
        0x23,               // INX H
        0xC3, 0x03, 0x01,   // JMP 0103h
        0x00,
        0xC3, 0x13, 0x01,   // JMP 0113h
    };
    std::string d;
    auto at = [&d](int row, int col) { d += '\x1B'; d += (char)(0x20 + row); d += (char)(0x20 + col); };
    d += '\x1F';                           // clear the screen
    d += "Hello\rJ";                       // CR: Jello
    d += "\n\rA\tB\tC";                    // LF, CR, tabs every 8
    at(3, 62); d += "ab\tZ";               // the second character wraps to the next row, the tab stays in it
    at(5, 10); d += "XY\b\bQ";             // BS
    at(6, 0); d += "\b";                   // BS from column 0 goes to the end of the previous row
    d += "W";
    at(7, 2); d += "ABCDEFGH"; at(7, 5); d += '\x1E';  // clear to the end of the row
    at(8, 0); d += '\x01'; d += '\r'; d += "after";    // 01h: the next code is a character
    d += '\x1D'; d += "Home";              // 1Dh
    at(10, 0); d += std::string(70, 'L');  // wraps
    at(19, 0); d += "last"; d += '\n';     // LF on the last row scrolls and goes to column 0
    d += "next";
    at(19, 63); d += 'x';                  // the last cell scrolls
    d += "corner";
    d += '\x05';                           // no scrolling: the end of a row keeps the position
    at(19, 63); d += "yz";
    d += '\x04';
    d += '\x1B'; d += (char)(0x20 + 18); d += ' '; d += '\x1E';  // rows 18 clear, cursor at its start
    CHECK(sizeof(prog) <= 0x20 && d.size() < 0x1E0);
    for (size_t i = 0; i < sizeof(prog); i++) m.bus.mem_write((uint16_t)(0x0100 + i), prog[i]);
    for (size_t i = 0; i < d.size(); i++) m.bus.mem_write((uint16_t)(0x0120 + i), (uint8_t)d[i]);
    m.bus.mem_write((uint16_t)(0x0120 + d.size()), 0xFF);
    m.type_text("SAVE 2 T.COM\r");
    m.run_seconds(3.0);
    m.type_text("T\r");
    m.run_seconds(3.0);
    std::string s = screen_text(m), cur = sapi_code_to_utf8(127);
    for (size_t p; (p = s.find(cur)) != std::string::npos;) s.erase(p, cur.size());
    return s;
}

// The repaired copies of the Melsoft disks (tools/sapi15_oprava.py): only the damaged sectors differ from the originals.
// WM.COM of the original disk 2 has a damaged start (BDOS ERR ON P: SELECT), the repaired one clears the screen and runs.
static void test_repaired_disks(void)
{
    static const struct { const char* orig; const char* opr; int sectors; } disks[] = {
        { "media/sapi15/SAPI15_2.sp2", "media/sapi15/SAPI15_2_opr.sp2", 41 },
        { "media/sapi15/SAPI15_3.sp2", "media/sapi15/SAPI15_3_opr.sp2", 7 },
        { "media/sapi15/SAPI15_4.sp2", "media/sapi15/SAPI15_4_opr.sp2", 27 },
    };
    for (const auto& d : disks) {
        std::vector<uint8_t> a, b;
        CHECK(file_read_all(data_path(d.orig), a));
        CHECK(file_read_all(data_path(d.opr), b));
        CHECK(a.size() == 327680 && b.size() == 327680);
        int n = 0;
        for (size_t i = 0; i + 256 <= a.size() && i + 256 <= b.size(); i += 256)
            if (memcmp(&a[i], &b[i], 256)) n++;
        CHECK(n == d.sectors);
    }
    Machine m;
    std::string err;
    CHECK(m.load_json(zps6(nullptr, false, "media/sapi15/SAPI15_2_opr.sp2"), err));
    CHECK(boot(m));
    m.type_text("WM NOVY.TXT\r");
    m.run_seconds(8.0);
    std::string s = screen_text(m);
    CHECK(s.find("BDOS ERR") == std::string::npos);
    CHECK(s.find("CBIOS") == std::string::npos);
}

static void test_and_eprom(void)
{
    static const char* DBS_ROM = "PKT-1 62K MON-V100690*LCH";
    static const char* AND_ROM = "PKT-1 62K MON-V100690*LCH/AND";
    std::string dbs = control_codes("machines/zps/ZPS-6.sapi", DBS_ROM);
    std::string z = control_codes("machines/sapi15_and1z.sapi", AND_ROM);
    std::string v = control_codes("machines/sapi15_and1v.sapi", AND_ROM);
    printf("--- DBS-1\n%s--- AND-1Z\n%s", dbs.c_str(), z.c_str());
    CHECK(!dbs.empty());
    CHECK(z == dbs);
    CHECK(v == dbs);
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
    test_formats();
    test_boot_dir_save();
    test_rombios_melsoft();
    test_map_and_serial();
    watchdog(false);
    watchdog(true);
    test_and_eprom();
    test_repaired_disks();
    return TEST_RESULT();
}
