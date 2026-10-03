// IDE-1: 8255 modes, IDE disk through the bit-banged 8255 (as the SAPI BIOS
// does it), PMD32-SD protocol through the mode 2 8255 (as the SAPI driver does).
#include <string.h>
#include "chips/i8255.h"
#include "chips/pmd32sd.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

// ---------------------------------------------------------------- 8255

static void test_i8255(void)
{
    I8255 p;
    p.reset();
    CHECK_EQ(p.read(0), 0xFF);          // inputs without a peripheral
    p.write(3, 0x80);                   // all outputs, mode 0
    p.write(0, 0x12);
    CHECK_EQ(p.read(0), 0x12);
    p.write(3, 0x0F);                   // set PC7
    CHECK_EQ(p.read(2) & 0x80, 0x80);
    p.write(3, 0x0E);                   // reset PC7
    CHECK_EQ(p.read(2) & 0x80, 0);

    p.write(3, 0xC4);                   // A mode 2, B mode 1 output (PMD32-SD setup)
    CHECK_EQ(p.read(2) & 0x80, 0x80);   // /OBF high (empty)
    CHECK(!p.intr_a());
    p.write(3, 0x0D);                   // INTE1 on
    CHECK(p.intr_a());                  // output buffer empty -> INTRA
    CHECK_EQ(p.read(2) & 0x08, 0x08);
    p.write(0, 0x55);                   // CPU writes: /OBF low, INTRA off
    CHECK(p.a_output_full());
    CHECK(!p.intr_a());
    CHECK_EQ(p.ack_a(), 0x55);          // peripheral takes it
    CHECK(p.intr_a());
    p.write(3, 0x0C);                   // INTE1 off
    p.write(3, 0x09);                   // INTE2 on
    CHECK(!p.intr_a());
    p.strobe_a(0xAA);                   // peripheral sends
    CHECK(p.intr_a());
    CHECK_EQ(p.read(2) & 0x20, 0x20);   // IBF
    CHECK_EQ(p.read(0), 0xAA);
    CHECK(!p.a_input_full());
    CHECK(!p.intr_a());
}

// ---------------------------------------------------------------- machine with IDE-1

static Machine* make_machine(const std::string& img)
{
    json cfg;
    cfg["cards"] = json::array();
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    cfg["cards"].push_back({
        { "slot", 2 }, { "type", "IDE-1" },
        { "ide", { { "image", img }, { "create_mb", 1 } } },
        { "pmd32sd", { { "sd_card", "media/pmd32sd" }, { "working_copy", false }, { "card_locked", true },
                       { "drives", { { { "image", "VZOR_2M.2MB" }, { "wp", true } } } } } },
    });
    Machine* m = new Machine();
    std::string err;
    if (!m->load_json(cfg, err)) {
        fprintf(stderr, "load: %s\n", err.c_str());
        delete m;
        return nullptr;
    }
    return m;
}

// IDE register access exactly like I_RD_16 / I_WR_16 in IDE.MAC (base 58h)
static uint16_t ide_rd(Bus& b, uint8_t reg)
{
    b.io_write(0x5A, reg);
    b.io_write(0x5A, (uint8_t)(reg | 0x40));
    uint8_t hi = b.io_read(0x59);
    uint8_t lo = b.io_read(0x58);
    b.io_write(0x5A, 0);
    return (uint16_t)(lo | (hi << 8));
}

static void ide_wr(Bus& b, uint8_t reg, uint16_t v)
{
    b.io_write(0x5B, 0x80);
    b.io_write(0x58, (uint8_t)v);
    b.io_write(0x59, (uint8_t)(v >> 8));
    b.io_write(0x5A, reg);
    b.io_write(0x5A, (uint8_t)(reg | 0x20));
    b.io_write(0x5A, reg);
    b.io_write(0x5B, 0x92);
}

static void test_ide(Machine& m)
{
    Bus& b = m.bus;
    b.io_write(0x5B, 0x92);
    ide_wr(b, 0x0E, 0xA0);
    CHECK_EQ(ide_rd(b, 0x0F) & 0xC0, 0x40);      // DRDY, not BSY
    // IDENTIFY: model name in words 27-46, byte swapped
    ide_wr(b, 0x0F, 0xEC);
    CHECK(ide_rd(b, 0x0F) & 0x08);               // DRQ
    char model[41] = { 0 };
    for (int w = 0; w < 256; w++) {
        uint16_t v = ide_rd(b, 0x08);
        if (w >= 27 && w < 47) { model[(w - 27) * 2] = (char)(v >> 8); model[(w - 27) * 2 + 1] = (char)v; }
        if (w == 60) CHECK_EQ(v, 2048);          // 1 MB image = 2048 sectors
    }
    CHECK(strncmp(model, "SAPIemu IDE disk", 16) == 0);
    CHECK_EQ(ide_rd(b, 0x0F) & 0x08, 0);

    // write LBA 5, read it back
    ide_wr(b, 0x0A, 1);
    ide_wr(b, 0x0B, 5); ide_wr(b, 0x0C, 0); ide_wr(b, 0x0D, 0); ide_wr(b, 0x0E, 0xE0);
    ide_wr(b, 0x0F, 0x30);
    for (int w = 0; w < 256; w++) ide_wr(b, 0x08, (uint16_t)(w * 3));
    CHECK_EQ(ide_rd(b, 0x0F) & 0x89, 0);         // not busy, no DRQ, no error
    ide_wr(b, 0x0B, 5);
    ide_wr(b, 0x0F, 0x20);
    bool same = true;
    for (int w = 0; w < 256; w++) if (ide_rd(b, 0x08) != (uint16_t)(w * 3)) same = false;
    CHECK(same);
    // another sector is untouched (E5 fill)
    ide_wr(b, 0x0B, 6);
    ide_wr(b, 0x0F, 0x20);
    CHECK_EQ(ide_rd(b, 0x08), 0xE5E5);
}

// ---------------------------------------------------------------- PMD32-SD host side

struct PmdHost {
    Bus& b;
    explicit PmdHost(Bus& bus) : b(bus) {}

    void run_us(int us) { b.now += (uint64_t)us * 2; b.advance_all(); }
    // wait for INTRA with INTE2 (input) / INTE1 (output), like the SAPI driver
    bool get(uint8_t& v, int timeout_ms = 50)
    {
        b.io_write(0x5F, 0x09);   // INTE2 on
        b.io_write(0x5F, 0x0C);   // INTE1 off
        for (int i = 0; i < timeout_ms * 50; i++) {
            if (b.io_read(0x5E) & 0x08) { v = b.io_read(0x5C); return true; }
            run_us(20);
        }
        return false;
    }
    bool put(uint8_t v, int timeout_ms = 50)
    {
        b.io_write(0x5F, 0x08);
        b.io_write(0x5F, 0x0D);
        for (int i = 0; i < timeout_ms * 50; i++) {
            if (b.io_read(0x5E) & 0x08) { b.io_write(0x5C, v); return true; }
            run_us(20);
        }
        return false;
    }
    // IsPresent: answer the device's AAh
    bool present(void)
    {
        uint8_t v;
        for (int tries = 0; tries < 10; tries++) {
            if (get(v, 30) && v == 0xAA) return put(0xAA);
        }
        return false;
    }
};

static void test_pmd32(Machine& m)
{
    Bus& b = m.bus;
    b.io_write(0x5F, 0xC4);
    PmdHost h(b);
    CHECK(h.present());

    // Q: read drive A (code 00h), sector 0, track 0
    uint8_t ack = 0, err = 0xFF, data[128], crc = 0, rcrc;
    uint8_t cmd[3] = { 'Q', 0x00, 0x00 };
    uint8_t x = 0;
    for (uint8_t c : cmd) { CHECK(h.put(c)); x ^= c; }
    CHECK(h.put(x));
    CHECK(h.get(ack));
    CHECK_EQ(ack, 0x33);
    CHECK(h.get(err));
    CHECK_EQ(err, 0);
    for (int i = 0; i < 128; i++) { CHECK(h.get(data[i])); crc ^= data[i]; }
    CHECK(h.get(rcrc));
    CHECK_EQ(rcrc, crc);
    std::vector<uint8_t> file;
    CHECK(file_read_all(data_path("media/pmd32sd/VZOR_2M.2MB"), file));
    CHECK(file.size() > 128 && memcmp(file.data(), data, 128) == 0);

    // A bad CRC is answered with NAK (send with timeout 0), but the firmware then
    // returns to its presentation loop and strobes AAh immediately, which
    // overwrites the NAK in the 8255 input latch: the host sees AAh, no ACK.
    CHECK(h.present());
    CHECK(h.put('Q')); CHECK(h.put(0)); CHECK(h.put(0)); CHECK(h.put(0x00));
    CHECK(h.get(ack));
    CHECK_EQ(ack, 0xAA);

    // T (write) to a write protected drive -> ERR 01
    CHECK(h.present());
    uint8_t w[3] = { 'T', 0x00, 0x00 };
    x = 0;
    for (uint8_t c : w) { CHECK(h.put(c)); x ^= c; }
    for (int i = 0; i < 128; i++) { CHECK(h.put(0x11)); x ^= 0x11; }
    CHECK(h.put(x));
    CHECK(h.get(ack));
    CHECK_EQ(ack, 0x33);
    CHECK(h.get(err));
    CHECK_EQ(err, 1);

    // P: parameters of drive A (2MB = 255 tracks x 64 sectors)
    CHECK(h.present());
    CHECK(h.put('P')); CHECK(h.put(0)); CHECK(h.put('P' ^ 0));
    CHECK(h.get(ack)); CHECK_EQ(ack, 0x33);
    CHECK(h.get(err)); CHECK_EQ(err, 0);
    uint8_t t, s, f;
    CHECK(h.get(t)); CHECK(h.get(s)); CHECK(h.get(f));
    CHECK_EQ(t, 255);
    CHECK_EQ(s, 64);

    // H (insert image) with a name leaving the SD card -> "not found" (06), no host access
    const char* evil = "../../media/pmd32sd/VZOR_2M.2MB";
    CHECK(h.present());
    x = 0;
    uint8_t len = (uint8_t)strlen(evil);
    CHECK(h.put('H')); x ^= 'H';
    CHECK(h.put(1)); x ^= 1;       // drive B
    CHECK(h.put(0)); x ^= 0;       // not write protected
    CHECK(h.put(len)); x ^= len;
    for (const char* p = evil; *p; p++) { CHECK(h.put((uint8_t)*p)); x ^= (uint8_t)*p; }
    CHECK(h.put(x));
    CHECK(h.get(ack)); CHECK_EQ(ack, 0x33);
    CHECK(h.get(err)); CHECK_EQ(err, 6);
}

static void test_safe_path(void)
{
    CHECK(Pmd32Sd::safe_path("VZOR_2M.2MB", false));
    CHECK(Pmd32Sd::safe_path("HRY/HRA.2MB", true));        // subdirectory on the card
    CHECK(!Pmd32Sd::safe_path("HRY/HRA.2MB", false));
    CHECK(!Pmd32Sd::safe_path("../x.SAP", true));
    CHECK(!Pmd32Sd::safe_path("HRY/../../x.SAP", true));
    CHECK(!Pmd32Sd::safe_path("/etc/x.SAP", true));
    CHECK(!Pmd32Sd::safe_path("C:/Users/Public/x.SAP", true));
    CHECK(!Pmd32Sd::safe_path("..\\x.SAP", true));
    CHECK(!Pmd32Sd::safe_path("\\\\server\\share\\x.P32", true));
    CHECK(!Pmd32Sd::safe_path("", true));
}

int main(void)
{
    test_init();
    test_i8255();
    test_safe_path();

    std::string dir = path_join(data_root(), "work/tests");
    make_dirs(dir);
    std::string img = path_join(dir, "ide_test.img");
    remove(img.c_str());
    Machine* m = make_machine(img);
    CHECK(m != nullptr);
    if (m) {
        test_ide(*m);
        test_pmd32(*m);
        delete m;
    }
    return TEST_RESULT();
}
