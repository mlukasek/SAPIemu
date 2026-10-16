// DPI-1L, the SD card part of the Repository API over a folder of the PC. The sequences A-E and G are the records
// of the real card (SAPI_DPI-firmware docs/sapiemu_dpi_repo.md, firmware 7767ed2), written the same way:
// command and parameters -> status (STSR + 4 bytes) | output; ?? = a byte of the date or time of a file.
// Then what the records do not cover: a saved state, RESET with a file open, a full card.
#include "cards/dpi1l.h"
#include "cards/dpi1l_fs.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

#include <string.h>
#include <filesystem>
#include <string>
#include <vector>

using nlohmann::json;
namespace fs = std::filesystem;

static const uint8_t B = 0xB0;

// hex bytes; "??" = any byte (mask 0)
static std::vector<uint8_t> hex(const std::string& s, std::vector<uint8_t>* mask = nullptr)
{
    std::vector<uint8_t> v;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == ' ' || s[i] == '|') { i++; continue; }
        bool any = s[i] == '?';
        v.push_back(any ? 0 : (uint8_t)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
        if (mask) mask->push_back(any ? 0 : 1);
        i += 2;
    }
    return v;
}

static std::string zeros(int n)
{
    std::string s;
    for (int i = 0; i < n; i++) s += " 00";
    return s;
}

// a string parameter: the name in KOI-8cs2 (from UTF-8) and its end
static std::string koi(const char* utf8, bool end = true)
{
    uint8_t k[256];
    size_t n = utf8_to_koi8(utf8, k, sizeof(k));
    std::string s;
    char b[4];
    for (size_t i = 0; i < n; i++) {
        snprintf(b, sizeof(b), " %02X", k[i]);
        s += b;
    }
    return end ? s + " 00" : s;
}

static std::string dump(const std::vector<uint8_t>& v, size_t status)
{
    std::string s;
    char b[4];
    for (size_t i = 0; i < v.size(); i++) {
        if (i == status) s += " |";
        snprintf(b, sizeof(b), " %02X", v[i]);
        s += b;
    }
    return s;
}

static void send(Machine& m, const std::string& cmd)
{
    std::vector<uint8_t> c = hex(cmd);
    m.bus.io_write(B, c[0]);
    for (size_t i = 1; i < c.size(); i++) m.bus.io_write(B + 1, c[i]);
}

static std::vector<uint8_t> status4(Machine& m)
{
    std::vector<uint8_t> st;
    m.bus.io_write(B, 0x03);
    for (int i = 0; i < 4; i++) st.push_back(m.bus.io_read(B));
    return st;
}

// command, status, output (DOUTRQ) as the DPI console shows it
static std::vector<uint8_t> run(Machine& m, const std::string& cmd)
{
    send(m, cmd);
    std::vector<uint8_t> got = status4(m);
    if (got[0] & 0x02) {
        int n = got[2];
        for (int i = 0; i < n; i++) got.push_back(m.bus.io_read(B + 1));
    }
    return got;
}

static bool same(const std::vector<uint8_t>& got, const std::string& want)
{
    std::vector<uint8_t> mask, w = hex(want, &mask);
    if (got.size() != w.size()) return false;
    for (size_t i = 0; i < w.size(); i++)
        if (mask[i] && got[i] != w[i]) return false;
    return true;
}

static void check(const std::vector<uint8_t>& got, const std::string& want, const std::string& what, int line,
                  size_t status = 4)
{
    if (!same(got, want)) {
        fprintf(stderr, "test_dpi1l_files.cpp:%d: %s ->%s, want %s\n", line, what.c_str(), dump(got, status).c_str(),
                want.c_str());
        g_test_failures++;
    }
}
#define REC(m, c, w) check(run(m, c), w, c, __LINE__)
#define STATUS(m, w) check(status4(m), w, "status", __LINE__)

// a data byte written, then the status
static void put(Machine& m, uint8_t v, const std::string& want, int line)
{
    m.bus.io_write(B + 1, v);
    check(status4(m), want, "B1h <-", line);
}
#define PUT(m, v, w) put(m, v, w, __LINE__)

// a data byte read, then the status
static void get(Machine& m, uint8_t v, const std::string& want, int line)
{
    uint8_t got = m.bus.io_read(B + 1);
    if (got != v) {
        fprintf(stderr, "test_dpi1l_files.cpp:%d: B1h -> %02X, want %02X\n", line, got, v);
        g_test_failures++;
    }
    check(status4(m), want, "B1h ->", line);
}
#define GET(m, v, w) get(m, v, w, __LINE__)

// the status, then a record of `n` bytes read from B1h
static void record(Machine& m, const std::string& st, int n, const std::string& want, int line)
{
    check(status4(m), st, "status", line);
    std::vector<uint8_t> r;
    for (int i = 0; i < n; i++) r.push_back(m.bus.io_read(B + 1));
    check(r, want, "record", line, 99);
}
#define RECORD(m, st, n, w) record(m, st, n, w, __LINE__)

static std::string folder(const char* name)
{
    std::string rel = std::string("work/test/") + name;
    std::error_code ec;
    fs::path p(std::u8string((const char8_t*)data_path(rel).c_str()));
    fs::remove_all(p, ec);              // a fresh SD card for every run
    return rel;
}

static fs::path host(const std::string& rel, const char* inside)
{
    return fs::path(std::u8string((const char8_t*)data_path(rel).c_str())) / fs::path(std::u8string((const char8_t*)inside));
}

static uint64_t host_size(const std::string& rel, const char* inside)
{
    std::error_code ec;
    uint64_t n = fs::file_size(host(rel, inside), ec);
    return ec ? (uint64_t)-1 : n;
}

static json machine(const std::string& sd, uint32_t sectors = 62309376)
{
    json cfg = { { "cards", json::array() } };
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    cfg["cards"].push_back({ { "slot", 2 },
                             { "type", "DPI-1L" },
                             { "net", { { "host", "" } } },
                             { "sd", { { "folder", sd }, { "sectors", sectors } } } });
    return cfg;
}

// FILINFO records of the real card (C.2, C.3), date and time left out
static const char* T = "?? ?? ?? ??";
static std::string filinfo(const std::string& size, const char* attr, const std::string& tail)
{
    return size + " " + T + " " + attr + " " + tail;
}
#define REC_A_TXT filinfo("07 00 00 00", "20", zeros(13) + " 05 41 2E 54 58 54" + zeros(27))
#define REC_B_BIN filinfo("7E 13 00 00", "20", zeros(13) + " 05 42 2E 42 49 4E" + zeros(27))
#define REC_C_BIN filinfo("64 00 00 00", "20", zeros(13) + " 05 43 2E 42 49 4E" + zeros(27))
#define REC_LONG filinfo("00 00 00 00", "20", "44 4C 4F 55 48 45 7E 31 2E 54 58 54 00 1F 44 6C 6F 75 68 D7 20 6A 6D D7 " \
                         "6E 6F 20 73 6F 75 62 6F 72 75 20 C3 C9 73 6C 6F 20 31 32 33 34 00")
#define REC_ZABA1 filinfo("00 00 00 00", "20", "5F 41 42 41 7E 31 2E 54 58 54 00 00 00 08 DA C1 62 61 2E 74 78 74" + zeros(24))
#define REC_ZABA2 filinfo("00 00 00 00", "20", "5F 41 42 41 7E 32 2E 54 58 54 00 00 00 08 DA 61 62 61 2E 74 78 74" + zeros(24))
#define REC_UNI filinfo("00 00 00 00", "20", "E8 4E 49 43 4F 44 45 2E 54 58 54 00 00 0B E8 6E 69 63 6F 64 D7 2E 74 78 74" + zeros(21))
#define REC_SUB filinfo("00 00 00 00", "10", zeros(13) + " 03 53 55 42" + zeros(29))

static const char* LONG_NAME = "Dlouhé jméno souboru číslo 1234567.txt";

static void test_records(void)
{
    std::string sd = folder("dpi1l_files");
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(sd), err));
    CHECK(fs::exists(host(sd, "DPI/DPI.INI")));   // as the firmware: /DPI and its configuration with the defaults

    // clocks on a fixed time
    host_time_fix(1760608800);                     // 2025-10-16 10:00:00 UTC
    REC(m, "00", "00 00 00 00");
    REC(m, "60 10 0A 2E", "00 60 00 00");
    REC(m, "61 0C 00 00", "00 61 00 00");
    REC(m, "62", "02 62 03 00 | 10 0A 2E");
    REC(m, "63", "02 63 03 00 | 0C 00 00");

    // A. basics (GETFREE: the size of the real card; free: this folder, the root, /DPI and DPI.INI used)
    REC(m, "00", "00 00 00 00");
    REC(m, "22", "02 22 02 00 | 2F 0D");
    REC(m, "20", "02 20 08 00 | 00 C4 B6 03 40 C3 B6 03");
    REC(m, "40" + koi("/EMUTEST"), "00 40 00 00");
    REC(m, "40" + koi("/EMUTEST"), "80 40 06 08");
    REC(m, "21" + koi("/EMUTEST"), "00 21 00 00");
    REC(m, "22", "02 22 09 00 | 2F 45 4D 55 54 45 53 54 0D");
    REC(m, "21" + koi("NENI"), "80 21 06 05");
    REC(m, "21" + koi(".."), "00 21 00 00");
    REC(m, "22", "02 22 02 00 | 2F 0D");
    REC(m, "21" + koi("/EMUTEST/.."), "00 21 00 00");
    REC(m, "22", "02 22 02 00 | 2F 0D");
    REC(m, "21" + koi("/EMUTEST"), "00 21 00 00");

    // B.1
    REC(m, "50 0A" + koi("A.TXT"), "30 50 00 00");
    for (uint8_t c : { 0x48, 0x45, 0x4C, 0x4C, 0x4F, 0x0D, 0x0A }) PUT(m, c, "30 F1 00 00");
    REC(m, "55", "32 55 04 00 | 07 00 00 00");
    REC(m, "56", "32 56 04 00 | 07 00 00 00");
    REC(m, "54", "00 54 00 00");
    REC(m, "30" + koi("A.TXT"), "02 30 37 00 | " + REC_A_TXT);
    REC(m, "30" + koi("a.txt"), "02 30 37 00 | " + REC_A_TXT);
    REC(m, "50 01" + koi("A.TXT"), "08 50 00 00");
    for (uint8_t c : { 0x48, 0x45, 0x4C, 0x4C, 0x4F, 0x0D }) GET(m, c, "08 F0 00 00");
    GET(m, 0x0A, "28 F0 00 00");
    GET(m, 0x00, "28 F0 00 00");
    GET(m, 0x00, "28 F0 00 00");
    REC(m, "51 00 02 00 00 00", "08 51 00 00");
    REC(m, "55", "0A 55 04 00 | 02 00 00 00");
    REC(m, "51 01 02 00 00 00", "08 51 00 00");
    REC(m, "55", "0A 55 04 00 | 05 00 00 00");
    REC(m, "51 02 01 00 00 00", "08 51 00 00");
    REC(m, "55", "0A 55 04 00 | 06 00 00 00");
    REC(m, "51 03 02 00 00 00", "08 51 00 00");
    REC(m, "55", "0A 55 04 00 | 04 00 00 00");
    REC(m, "51 04 01 00 00 00", "88 51 02 00");
    REC(m, "54", "00 54 00 00");

    // B.2: mode 0Ah writes only, reading gives ERROR 4
    REC(m, "50 0A" + koi("B.BIN"), "30 50 00 00");
    for (int i = 0; i < 5000; i++) m.bus.io_write(B + 1, (uint8_t)i);
    STATUS(m, "30 F1 00 00");
    CHECK_EQ(host_size(sd, "EMUTEST/B.BIN"), 4096);   // the first page went to the disk at the page change
    REC(m, "56", "32 56 04 00 | 88 13 00 00");
    REC(m, "55", "32 55 04 00 | 88 13 00 00");
    REC(m, "51 01 0A 00 00 00", "10 51 00 00");
    REC(m, "55", "12 55 04 00 | 7E 13 00 00");
    for (int i = 0; i < 10; i++) GET(m, 0x00, "90 55 04 00");
    REC(m, "52", "30 52 00 00");
    REC(m, "56", "32 56 04 00 | 7E 13 00 00");
    REC(m, "54", "00 54 00 00");
    REC(m, "30" + koi("B.BIN"), "02 30 37 00 | " + REC_B_BIN);

    // B.3: /RES keeps the file open, the page stays in the card
    REC(m, "50 0A" + koi("C.BIN"), "30 50 00 00");
    for (int i = 0; i < 100; i++) m.bus.io_write(B + 1, (uint8_t)i);
    m.reset();
    STATUS(m, "30 F1 00 00");
    REC(m, "30" + koi("C.BIN"), "32 30 37 00 | " + filinfo("00 00 00 00", "20", zeros(13) + " 05 43 2E 42 49 4E" + zeros(27)));
    REC(m, "56", "32 56 04 00 | 64 00 00 00");
    REC(m, "54", "00 54 00 00");
    REC(m, "30" + koi("C.BIN"), "02 30 37 00 | " + REC_C_BIN);

    // B.4
    REC(m, "50 01" + koi("A.TXT"), "08 50 00 00");
    PUT(m, 0x58, "88 50 04 00");
    REC(m, "54", "00 54 00 00");
    GET(m, 0x00, "80 54 04 00");
    REC(m, "52", "80 52 04 00");
    REC(m, "53", "80 53 04 00");
    REC(m, "54", "00 54 00 00");
    REC(m, "50 04" + koi("A.TXT"), "80 50 06 08");
    REC(m, "50 01" + koi("NENI.TXT"), "80 50 06 04");

    // C.1
    REC(m, "21" + koi("/EMUTEST"), "00 21 00 00");
    for (const char* n : { LONG_NAME, "žába.txt", "žaba.txt", "Ünïcødé.txt" }) {
        REC(m, "50 0A" + koi(n), "30 50 00 00");
        REC(m, "54", "00 54 00 00");
    }
    REC(m, "40" + koi("SUB"), "00 40 00 00");

    // C.2: the entries in the order they were made, short names of FatFs (CP437)
    REC(m, "41" + koi("."), "04 41 37 00");
    for (const std::string& r : { REC_A_TXT, REC_B_BIN, REC_C_BIN, REC_LONG, REC_ZABA1, REC_ZABA2, REC_UNI, REC_SUB })
        RECORD(m, "04 41 37 00", 55, r);
    STATUS(m, "00 41 00 00");
    REC(m, "42" + koi("."), "04 42 08 00");
    static const struct { const char* st; int n; const char* rec; } fl[] = {
        { "04 42 08 00", 8, "41 2E 54 58 54 0D 37 0D" },
        { "04 42 0B 00", 11, "42 2E 42 49 4E 0D 34 39 39 30 0D" },
        { "04 42 0A 00", 10, "43 2E 42 49 4E 0D 31 30 30 0D" },
        { "04 42 29 00", 41, "44 6C 6F 75 68 D7 20 6A 6D D7 6E 6F 20 73 6F 75 62 6F 72 75 20 C3 C9 73 6C 6F 20 31 32 "
                             "33 34 35 36 37 2E 74 78 74 0D 30 0D" },
        { "04 42 0B 00", 11, "DA C1 62 61 2E 74 78 74 0D 30 0D" },
        { "04 42 0B 00", 11, "DA 61 62 61 2E 74 78 74 0D 30 0D" },
        { "04 42 0E 00", 14, "E8 6E 69 63 6F 64 D7 2E 74 78 74 0D 30 0D" },
        { "04 42 07 00", 7, "53 55 42 2F 0D 30 0D" },
    };
    for (const auto& f : fl) RECORD(m, f.st, f.n, f.rec);
    STATUS(m, "00 42 00 00");
    REC(m, "42" + koi("."), "04 42 08 00");
    GET(m, 0x41, "04 42 07 00");
    REC(m, "43", "04 42 0B 00");
    for (size_t i = 1; i < sizeof(fl) / sizeof(fl[0]); i++) RECORD(m, fl[i].st, fl[i].n, fl[i].rec);
    STATUS(m, "00 42 00 00");
    REC(m, "00", "00 00 00 00");
    REC(m, "43", "80 43 02 00");

    // C.3
    REC(m, "21" + koi("/EMUTEST"), "00 21 00 00");
    REC(m, "30" + koi("ZABA.TXT"), "80 30 06 06");
    REC(m, "50 01" + koi("ZABA.TXT"), "80 50 06 06");
    REC(m, "54", "00 54 00 00");
    REC(m, "30" + koi("Dlouhé jméno souboru číslo 12"), "80 30 06 04");
    REC(m, "30" + koi(LONG_NAME), "02 30 37 00 | " + std::string(REC_LONG));
    REC(m, "30" + koi("UNICODE.TXT"), "02 30 37 00 | " + std::string(REC_UNI));
    REC(m, "30" + koi("Ünïcødé.txt"), "02 30 37 00 | " + std::string(REC_UNI));
    REC(m, "30" + koi("DLOUHE~1.TXT"), "02 30 37 00 | " + std::string(REC_LONG));   // the short name (not recorded)

    // C.4
    REC(m, "34" + koi("A.TXT") + koi("SUB/A2.TXT"), "00 34 00 00");
    REC(m, "34" + koi("B.BIN") + koi("C.BIN"), "80 34 06 08");
    REC(m, "34" + koi("NENI.TXT") + koi("X.TXT"), "80 34 06 04");
    REC(m, "31" + koi("SUB"), "80 31 06 07");
    REC(m, "31" + koi("NENI.TXT"), "80 31 06 04");
    REC(m, "40" + koi("SUB"), "80 40 06 08");
    REC(m, "32 01 01" + koi("C.BIN"), "80 32 01 00");
    REC(m, "30" + koi("C.BIN"), "02 30 37 00 | " + REC_C_BIN);
    REC(m, "32 00 01" + koi("C.BIN"), "80 32 01 00");
    REC(m, "33 00 00 00 00 00 00" + koi("2026-10-16 12:34:56"), "80 33 01 00");
    REC(m, "33 01 02 03 04 05 06" + koi("C.BIN"), "80 33 01 00");
    REC(m, "30" + koi("C.BIN"), "02 30 37 00 | " + REC_C_BIN);

    // D. /DPI and the configuration (the card has LOG.TXT there too, the emulator writes none)
    CHECK(file_write_all(data_path(sd + "/DPI/SECRET.INI"), "[wifi]\r\n", 8));
    REC(m, "41" + koi("/DPI"), "04 41 37 00");
    RECORD(m, "04 41 37 00", 55, filinfo("?? ?? 00 00", "20", zeros(13) + " 07 44 50 49 2E 49 4E 49" + zeros(25)));
    STATUS(m, "00 41 00 00");
    REC(m, "30" + koi("/DPI/SECRET.INI"), "80 30 06 04");
    REC(m, "34" + koi("/DPI") + koi("/DPI2"), "80 34 06 07");
    REC(m, "31" + koi("/DPI"), "80 31 06 07");
    REC(m, "50 01" + koi("/DPI/SECRET.INI"), "80 50 06 04");
    REC(m, "54", "00 54 00 00");
    REC(m, "92" + koi("net"), "04 92 50 00");
    RECORD(m, "04 92 50 00", 80, "72 65 6C 61 79" + zeros(11) + " 61 70 69 2E 6D 7A 70 69 63 6F 2E 63 6F 6D 3A 38 30" + zeros(47));
    RECORD(m, "04 92 50 00", 80, "74 72 61 6E 73 70 6F 72 74" + zeros(7) + " 77 73" + zeros(62));
    RECORD(m, "04 92 50 00", 80, "74 63 70 5F 64 65 66 61 75 6C 74" + zeros(69));
    STATUS(m, "00 92 00 00");
    REC(m, "92" + koi("repo"), "04 92 50 00");
    RECORD(m, "04 92 50 00", 80, "65 6E 61 62 6C 65" + zeros(10) + " 31" + zeros(63));
    RECORD(m, "04 92 50 00", 80, "70 6F 72 74" + zeros(12) + " 42 30 68" + zeros(61));
    STATUS(m, "00 92 00 00");
    REC(m, "92" + koi("neni"), "00 92 00 00");
    REC(m, "99" + koi("emutest") + koi("klic") + koi("hodnota"), "00 99 00 00");
    REC(m, "92" + koi("emutest"), "04 92 50 00");
    RECORD(m, "04 92 50 00", 80, "6B 6C 69 63" + zeros(12) + " 68 6F 64 6E 6F 74 61" + zeros(57));
    STATUS(m, "00 92 00 00");
    REC(m, "99" + koi("emutest") + koi("klic") + koi(""), "00 99 00 00");
    REC(m, "92" + koi("emutest"), "00 92 00 00");
    REC(m, "99" + koi("emutest") + koi("kl=ic") + koi("x"), "80 99 02 00");

    // E. RTC, BOOT, SDCARD (the clock stands still here)
    REC(m, "62", "02 62 03 00 | 10 0A 2E");
    REC(m, "63", "02 63 03 00 | 0C 00 00");
    REC(m, "60 1F 02 2E", "80 60 02 00");
    REC(m, "61 18 00 00", "80 61 02 00");
    REC(m, "62", "02 62 03 00 | 10 0A 2E");
    REC(m, "B0 FF", "02 B0 01 00 | 00");
    REC(m, "07", "00 07 00 00");
    STATUS(m, "00 07 00 00");
    CHECK(m.bus.rtl());                            // /RTL for 100 ms
    m.bus.now += m.bus.cpu_hz / 5 * m.bus.cpu_step;
    CHECK(!m.bus.rtl());
    REC(m, "B0 01", "80 B0 05 00");                // no USB to lend the card to
    REC(m, "B0 07", "80 B0 02 00");
    host_time_fix(0);

    // G. reading and writing in one file (mode 0Bh)
    REC(m, "40" + koi("/EMUTEST2"), "00 40 00 00");
    REC(m, "21" + koi("/EMUTEST2"), "00 21 00 00");
    REC(m, "50 0B" + koi("D.BIN"), "38 50 00 00");
    for (int i = 0; i < 5000; i++) m.bus.io_write(B + 1, (uint8_t)i);
    STATUS(m, "38 F1 00 00");
    REC(m, "56", "3A 56 04 00 | 88 13 00 00");
    REC(m, "51 01 0A 00 00 00", "18 51 00 00");
    REC(m, "55", "1A 55 04 00 | 7E 13 00 00");
    for (uint8_t c = 0x7E; c != 0x87; c++) GET(m, c, "18 F0 00 00");
    GET(m, 0x87, "38 F0 00 00");
    REC(m, "51 00 FE 0F 00 00", "18 51 00 00");
    for (uint8_t c : { 0xFE, 0xFF, 0x00, 0x01 }) GET(m, c, "18 F0 00 00");
    REC(m, "51 00 88 13 00 00", "38 51 00 00");
    GET(m, 0x00, "38 F0 00 00");
    REC(m, "52", "38 52 00 00");
    REC(m, "56", "3A 56 04 00 | 88 13 00 00");
    REC(m, "54", "00 54 00 00");
    REC(m, "30" + koi("D.BIN"), "02 30 37 00 | " + filinfo("88 13 00 00", "20", zeros(13) + " 05 44 2E 42 49 4E" + zeros(27)));
    REC(m, "31" + koi("D.BIN"), "00 31 00 00");
    REC(m, "00", "00 00 00 00");
    REC(m, "31" + koi("/EMUTEST2"), "00 31 00 00");

    // not in the records: a saved state keeps the open file and its page
    REC(m, "21" + koi("/EMUTEST"), "00 21 00 00");
    REC(m, "50 0A" + koi("E.BIN"), "30 50 00 00");
    for (char c : std::string("0123456789")) m.bus.io_write(B + 1, (uint8_t)c);
    std::vector<uint8_t> state;
    CHECK(m.save_state(state, "", err));
    for (char c : std::string("abcde")) m.bus.io_write(B + 1, (uint8_t)c);
    CHECK(m.load_state(state, err));
    REC(m, "55", "32 55 04 00 | 0A 00 00 00");
    REC(m, "22", "32 22 09 00 | 2F 45 4D 55 54 45 53 54 0D");
    REC(m, "54", "00 54 00 00");
    {
        std::vector<uint8_t> b;
        CHECK(file_read_all(data_path(sd + "/EMUTEST/E.BIN"), b));
        CHECK(std::string(b.begin(), b.end()) == "0123456789");
    }
    // RESET writes the open file back and goes to the root
    REC(m, "50 0A" + koi("F.BIN"), "30 50 00 00");
    m.bus.io_write(B + 1, 0x41);
    REC(m, "00", "00 00 00 00");
    CHECK_EQ(host_size(sd, "EMUTEST/F.BIN"), 1);
    REC(m, "22", "02 22 02 00 | 2F 0D");
    // the old /DPI.INI in the root is hidden as well
    CHECK(file_write_all(data_path(sd + "/DPI.INI"), "[wifi]\r\n", 8));
    REC(m, "41" + koi("/"), "04 41 37 00");
    RECORD(m, "04 41 37 00", 55, filinfo("00 00 00 00", "10", zeros(13) + " 03 44 50 49" + zeros(29)));
    RECORD(m, "04 41 37 00", 55, filinfo("00 00 00 00", "10", zeros(13) + " 07 45 4D 55 54 45 53 54" + zeros(25)));
    STATUS(m, "00 41 00 00");
}

// a card of 1 MB: writing stops with FR_DENIED when it is full
static void test_full(void)
{
    std::string sd = folder("dpi1l_full");
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(sd, 2048), err));
    REC(m, "20", "02 20 08 00 | 00 08 00 00 F4 07 00 00");   // 512 clusters of 2 KB, the root, /DPI, DPI.INI used
    REC(m, "50 0A" + koi("BIG.BIN"), "30 50 00 00");
    int at = -1;
    for (int i = 0; i < 1200 * 1024 && at < 0; i++) {
        m.bus.io_write(B + 1, (uint8_t)i);
        if ((i & 4095) == 0 && (status4(m)[0] & 0x80)) at = i;
    }
    CHECK_EQ(at, 1044480);
    STATUS(m, "B0 F1 06 07");
}

int main(void)
{
    test_init();
    test_records();
    test_full();
    return TEST_RESULT();
}
