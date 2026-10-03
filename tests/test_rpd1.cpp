// RPD-1 floppy controller with MIKOS/D V5.0 (ZPS-2): the disk system boots from
// the system disk, DIR lists it, SAVE writes a file that LOAD reads back, BASIC
// loads from the disk and runs; without a disk the boot reports it.
#include <string.h>
#include "cards/rpd1.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

static const char* TEST_DISK = "work/test/rpd1_test.img";

static json machine_cfg(const char* image)
{
    std::string text;
    CHECK(file_read_text(data_path("machines/zps/ZPS-2_MIKOS_D.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"])
        if (c.value("type", std::string()) == "RPD-1") {
            if (image) c["drive_a"] = { { "image", image }, { "write_protect", false } };
            else c.erase("drive_a");
        }
    return cfg;
}

static Rpd1* find_rpd(Machine& m)
{
    for (auto& c : m.cards)
        if (Rpd1* r = dynamic_cast<Rpd1*>(c.get())) return r;
    return nullptr;
}

// the monitor prompt "." is the last line on the screen again
static bool run_until_prompt(Machine& m, double timeout_s)
{
    for (double t = 0; t < timeout_s; t += 0.1) {
        m.run_seconds(0.1);
        std::string s = screen_text(m);
        while (!s.empty() && (s.back() == '\n' || s.back() == ' ')) s.pop_back();
        size_t nl = s.find_last_of('\n');
        if (s.substr(nl == std::string::npos ? 0 : nl + 1) == ".") return true;
    }
    return false;
}

// a menu command of MIKOS/D: K, the first letter, the name
static void command(Machine& m, const char* letter, const char* name)
{
    m.type_text("K");
    CHECK(run_until_text(m, "RENAME FIND OPEN CLOSE", 3.0));
    m.run_seconds(0.3);
    m.type_text(letter);
    CHECK(run_until_text(m, "NAZEV:", 2.0));
    m.run_seconds(0.3);
    m.type_text(name);
}

static void no_disk(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(nullptr), err));
    CHECK(run_until_text(m, "NELZE ZAVEST DISKOVY SYSTEM", 5.0));
    printf("%s\n", screen_text(m).c_str());
}

static void mikos_d(void)
{
    // a fresh copy of the system disk, the user's working copy stays untouched
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path("media/rpd-1/MIKOS_D_V5.0_system.img"), img));
    CHECK_EQ((int)img.size(), Rpd1::IMAGE_SIZE);
    CHECK(file_write_all(data_path(TEST_DISK), img.data(), img.size()));

    Machine m;
    std::string err;
    CHECK(m.load_json(machine_cfg(TEST_DISK), err));
    Rpd1* rpd = find_rpd(m);
    CHECK(rpd != nullptr);
    if (!rpd) return;
    CHECK(run_until_text(m, "ZAVEDEN DISKOVY SYSTEM", 5.0));
    CHECK(run_until_text(m, "MONITOR V5.0\n.", 2.0));
    m.run_seconds(0.5);

    // directory
    command(m, "D", "\r");
    CHECK(run_until_text(m, "OBSAZENO 0167", 10.0));
    CHECK(screen_contains(m, "BAS5C   .SYS"));
    CHECK(screen_contains(m, "X2      .SYS"));
    m.run_seconds(0.5);

    // SAVE 4 KB, forget it, LOAD it back
    for (int i = 0; i < 0x1000; i++) m.bus.mem_write((uint16_t)(0x5000 + i), (uint8_t)(i * 13 + 7));
    command(m, "S", "TEST.BIN\r");
    CHECK(run_until_text(m, "TEST.BIN\n=", 5.0));
    m.run_seconds(0.3);
    m.type_text("5000 5FFF\r");
    CHECK(run_until_text(m, "=5FFF", 2.0));
    CHECK(run_until_prompt(m, 30.0));
    CHECK(rpd->drives[0].dirty);
    for (int i = 0; i < 0x1000; i++) m.bus.mem_write((uint16_t)(0x5000 + i), 0);
    m.run_seconds(0.5);
    command(m, "D", "\r");
    CHECK(run_until_text(m, "TEST    .BIN", 10.0));
    m.run_seconds(1.0);
    command(m, "L", "TEST.BIN\r");
    CHECK(run_until_prompt(m, 30.0));
    int bad = 0;
    for (int i = 0; i < 0x1000; i++)
        if (m.bus.mem_peek((uint16_t)(0x5000 + i)) != (uint8_t)(i * 13 + 7)) bad++;
    CHECK_EQ(bad, 0);

    // the file is on the disk image after a flush
    std::string e;
    CHECK(rpd->save(0, e));
    CHECK(file_read_all(data_path(TEST_DISK), img));
    bool found = false;
    for (size_t off = 2 * 26 * 128; off < 3 * 26 * 128; off += 32)   // the directory, skewed over track 2
        if (memcmp(&img[off + 1], "TEST    BIN", 11) == 0) found = true;
    CHECK(found);

    // BASIC from the disk (10 KB, takes some 20 s of emulated time)
    m.run_seconds(0.5);
    command(m, "L", "BAS5C.SYS\r");
    CHECK(run_until_prompt(m, 40.0));
    m.type_text("J");
    CHECK(run_until_text(m, "READY", 3.0));
    m.run_seconds(0.5);
    m.type_text("PRINT 12*34+5\r");
    CHECK(run_until_text(m, " 413", 5.0));
    printf("%s\n", screen_text(m).c_str());
}

int main(void)
{
    test_init();
    no_disk();
    mikos_d();
    return TEST_RESULT();
}
