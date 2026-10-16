// DPI-1L, the SD card part of the Repository API over a folder of the PC: files, directories, names in KOI-8cs2,
// the page of 4 KB, the hidden /DPI files, GETCONFIG/SETCONFIG, RTC, BOOT, SDCARD, a full card, a saved state.
// Expected values follow the DPI firmware (common/repo.c, repo_fs.c); the records of the real card for files are
// still to come (SAPI_DPI-firmware docs/sapiemu_dpi.md section 10).
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

static std::vector<uint8_t> hex(const std::string& s)
{
    std::vector<uint8_t> v;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == ' ' || s[i] == '|') { i++; continue; }
        v.push_back((uint8_t)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
        i += 2;
    }
    return v;
}

// a string parameter: the name in KOI-8cs2 (from UTF-8) and 0Dh
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
    return end ? s + " 0D" : s;
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

static void rec(Machine& m, const std::string& cmd, const std::string& want, int line)
{
    std::vector<uint8_t> got = run(m, cmd), w = hex(want);
    if (got != w) {
        fprintf(stderr, "test_dpi1l_files.cpp:%d: %s ->%s, want%s\n", line, cmd.c_str(), dump(got, 4).c_str(),
                dump(w, 4).c_str());
        g_test_failures++;
    }
}
#define REC(m, c, w) rec(m, c, w, __LINE__)

static void expect_status(Machine& m, const std::string& want, int line)
{
    std::vector<uint8_t> got = status4(m), w = hex(want);
    if (got != w) {
        fprintf(stderr, "test_dpi1l_files.cpp:%d: status%s, want%s\n", line, dump(got, 4).c_str(), dump(w, 4).c_str());
        g_test_failures++;
    }
}
#define STATUS(m, w) expect_status(m, w, __LINE__)

static std::vector<uint8_t> read_n(Machine& m, int n)
{
    std::vector<uint8_t> v;
    for (int i = 0; i < n; i++) v.push_back(m.bus.io_read(B + 1));
    return v;
}

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

static json machine(const std::string& sd, int size_mb = 32)
{
    json cfg = { { "cards", json::array() } };
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    cfg["cards"].push_back({ { "slot", 2 },
                             { "type", "DPI-1L" },
                             { "net", { { "host", "" } } },
                             { "sd", { { "folder", sd }, { "size_mb", size_mb } } } });
    return cfg;
}

static const char* LONG_NAME = "Dlouhé jméno souboru číslo 1234567.txt";

static void test_files(void)
{
    std::string sd = folder("dpi1l_files");
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(sd), err));
    CHECK(fs::exists(host(sd, "DPI/DPI.INI")));   // as the firmware: /DPI and its configuration with the defaults

    REC(m, "00", "00 00 00 00");
    REC(m, "22", "02 22 02 00 | 2F 0D");
    REC(m, "20", "02 20 08 00 | 74 FD 00 00 6C FD 00 00");  // 32 MB, clusters of 2 KB: /DPI and DPI.INI used

    // directories
    REC(m, "40" + koi("EMUTEST"), "00 40 00 00");
    REC(m, "40" + koi("EMUTEST"), "80 40 06 08");           // FR_EXIST
    REC(m, "21" + koi("emutest"), "00 21 00 00");           // any case
    REC(m, "22", "02 22 09 00 | 2F 45 4D 55 54 45 53 54 0D");
    REC(m, "21" + koi("NENI"), "80 21 06 05");              // FR_NO_PATH
    REC(m, "21" + koi(".."), "00 21 00 00");
    REC(m, "22", "02 22 02 00 | 2F 0D");
    REC(m, "21" + koi("/EMUTEST/../EMUTEST"), "00 21 00 00");

    // a file: written, then its size in the directory only after CLOSE
    REC(m, "50 0A" + koi("A.TXT"), "30 50 00 00");          // write, EOF
    for (char c : std::string("HELLO\r\n")) m.bus.io_write(B + 1, (uint8_t)c);
    STATUS(m, "30 F1 00 00");                               // INTPUTC
    REC(m, "55", "32 55 04 00 | 07 00 00 00");              // TELL
    REC(m, "56", "32 56 04 00 | 07 00 00 00");              // SIZE
    std::vector<uint8_t> st = run(m, "30" + koi("A.TXT"));  // STAT of the open file: the old size
    CHECK(st.size() == 4 + 55 && st[0] == 0x32 && st[2] == 55);
    if (st.size() == 59) {
        CHECK_EQ(st[4], 0);
        CHECK_EQ(st[4 + 8], 0x20);                          // ARC
        CHECK_EQ(st[4 + 9], 0);                             // a plain 8.3 name: no altname
        CHECK_EQ(st[4 + 22], 5);
        CHECK(memcmp(&st[4 + 23], "A.TXT", 6) == 0);
    }
    CHECK_EQ(host_size(sd, "EMUTEST/A.TXT"), 0);            // the page is not on the disk yet
    REC(m, "54", "00 54 00 00");
    CHECK_EQ(host_size(sd, "EMUTEST/A.TXT"), 7);
    st = run(m, "30" + koi("a.txt"));
    CHECK(st.size() == 59 && st[4] == 7);

    // reading, EOF, SEEK
    REC(m, "50 01" + koi("a.txt"), "08 50 00 00");
    CHECK(read_n(m, 9) == hex("48 45 4C 4C 4F 0D 0A 00 00"));
    STATUS(m, "28 F0 00 00");                               // read, EOF, INTGETC
    REC(m, "51 00 02 00 00 00", "08 51 00 00");
    CHECK(read_n(m, 1) == hex("4C"));
    REC(m, "51 01 01 00 00 00", "08 51 00 00");
    CHECK(read_n(m, 1) == hex("0A"));
    REC(m, "51 03 0A 00 00 00", "08 51 00 00");             // before the start: 0
    REC(m, "55", "0A 55 04 00 | 00 00 00 00");
    REC(m, "51 04 00 00 00 00", "88 51 02 00");             // mode 4: BAD_PARAM
    m.bus.io_write(B + 1, 0x55);                            // a write to a file open for reading
    STATUS(m, "88 51 04 00");
    REC(m, "54", "00 54 00 00");
    CHECK_EQ(m.bus.io_read(B + 1), 0x00);                   // nothing open
    STATUS(m, "80 54 04 00");
    REC(m, "52", "80 52 04 00");                            // TRUNC, SYNC without a file
    REC(m, "53", "80 53 04 00");
    REC(m, "54", "00 54 00 00");                            // CLOSE without a file: OK
    REC(m, "50 04" + koi("A.TXT"), "80 50 06 08");          // CREATE_NEW: FR_EXIST
    REC(m, "50 01" + koi("NENI.TXT"), "80 50 06 04");       // FR_NO_FILE
    REC(m, "50 01" + koi("NENI/X.TXT"), "80 50 06 05");     // FR_NO_PATH

    // over the page of 4 KB
    REC(m, "50 0B" + koi("B.BIN"), "38 50 00 00");          // read, write, create always
    for (int i = 0; i < 5000; i++) m.bus.io_write(B + 1, (uint8_t)i);
    CHECK_EQ(host_size(sd, "EMUTEST/B.BIN"), 4096);         // the first page went to the disk at the page change
    REC(m, "56", "3A 56 04 00 | 88 13 00 00");
    st = run(m, "30" + koi("B.BIN"));
    CHECK(st.size() == 59 && st[4] == 0 && st[5] == 0);     // the directory entry: still 0
    REC(m, "51 01 0A 00 00 00", "18 51 00 00");
    CHECK(read_n(m, 10) == hex("7E 7F 80 81 82 83 84 85 86 87"));
    REC(m, "51 00 00 10 00 00", "18 51 00 00");
    REC(m, "52", "38 52 00 00");                            // TRUNC at 4096
    REC(m, "56", "3A 56 04 00 | 00 10 00 00");
    REC(m, "54", "00 54 00 00");
    CHECK_EQ(host_size(sd, "EMUTEST/B.BIN"), 4096);
    {
        std::vector<uint8_t> b;
        CHECK(file_read_all(data_path(sd + "/EMUTEST/B.BIN"), b));
        bool ok = b.size() == 4096;
        for (size_t i = 0; ok && i < b.size(); i++) ok = b[i] == (uint8_t)i;
        CHECK(ok);
    }

    // /RES with a file open: the file stays open, the page is not written
    REC(m, "50 0A" + koi("C.BIN"), "30 50 00 00");
    for (int i = 0; i < 100; i++) m.bus.io_write(B + 1, (uint8_t)i);
    m.reset();
    CHECK_EQ(host_size(sd, "EMUTEST/C.BIN"), 0);
    STATUS(m, "30 F1 00 00");
    REC(m, "54", "00 54 00 00");
    CHECK_EQ(host_size(sd, "EMUTEST/C.BIN"), 100);

    // names: long, Czech; directory records
    REC(m, "40" + koi("SUB"), "00 40 00 00");
    REC(m, "50 0A" + koi(LONG_NAME), "30 50 00 00");
    REC(m, "54", "00 54 00 00");
    CHECK(fs::exists(host(sd, (std::string("EMUTEST/") + LONG_NAME).c_str())));
    REC(m, "41" + koi("."), "04 41 37 00");                 // READDIR: a record of 55 B waits
    struct Want { const char* name; uint32_t size; uint8_t attr; const char* alt; };
    static const Want want[] = {
        { "A.TXT", 7, 0x20, "" }, { "B.BIN", 4096, 0x20, "" }, { "C.BIN", 100, 0x20, "" },
        { LONG_NAME, 0, 0x20, "DLOUH_~1.TXT" }, { "SUB", 0, 0x10, "" },
    };
    for (const Want& w : want) {
        std::vector<uint8_t> r = read_n(m, 55);
        uint32_t size = r[0] | r[1] << 8 | r[2] << 16 | (uint32_t)r[3] << 24;
        uint8_t k[256];
        size_t n = utf8_to_koi8(w.name, k, sizeof(k));
        if (n > 31) n = 31;
        bool ok = size == w.size && r[8] == w.attr && !strcmp((const char*)&r[9], w.alt) && r[22] == n &&
                  !memcmp(&r[23], k, n) && r[23 + n] == 0;
        if (!ok) fprintf(stderr, "test_dpi1l_files.cpp:%d: record of %s:%s\n", __LINE__, w.name, dump(r, 99).c_str());
        CHECK(ok);
    }
    STATUS(m, "00 41 00 00");                               // the end: the directory is closed
    REC(m, "42" + koi("."), "04 42 08 00");                 // FILELIST: "A.TXT" 0D "7" 0D
    CHECK(read_n(m, 8) == hex("41 2E 54 58 54 0D 37 0D"));
    STATUS(m, "04 42 0B 00");                               // "B.BIN" 0D "4096" 0D
    CHECK(read_n(m, 1) == hex("42"));
    REC(m, "43", "04 42 0A 00");                            // NEXT: the rest skipped, "C.BIN" 0D "100" 0D
    REC(m, "43", "04 42 29 00");                            // the long name in full (38), 0D, "0", 0D
    REC(m, "43", "04 42 07 00");                            // "SUB/" 0D "0" 0D
    REC(m, "43", "00 42 00 00");
    REC(m, "43", "80 43 02 00");                            // no directory open

    // look-up of names
    REC(m, "50 0A" + koi("žába.txt"), "30 50 00 00");
    REC(m, "50 0A" + koi("žaba.txt"), "30 50 00 00");       // OPEN closes the previous file
    REC(m, "54", "00 54 00 00");
    REC(m, "30" + koi("ZABA.TXT"), "80 30 06 06");          // two fit without the diacritics: FR_INVALID_NAME
    st = run(m, "30" + koi("ŽÁBA.TXT"));                    // KOI-8cs2 without regard to case
    CHECK(st.size() == 59 && st[0] == 0x02);
    {
        uint8_t k[256];
        utf8_to_koi8(LONG_NAME, k, sizeof(k));
        k[31] = 0;
        std::string cut;
        char b[4];
        for (int i = 0; i < 31; i++) {
            snprintf(b, sizeof(b), " %02X", k[i]);
            cut += b;
        }
        st = run(m, "30" + cut + " 0D");                    // the name cut to 31 bytes in a record
        CHECK(st.size() == 59 && st[0] == 0x02);
    }
    st = run(m, "30" + koi("DLOUH_~1.TXT"));                // the short name
    CHECK(st.size() == 59 && st[0] == 0x02);

    // rename, delete
    REC(m, "34" + koi("A.TXT") + koi("SUB/A2.TXT"), "00 34 00 00");
    CHECK(fs::exists(host(sd, "EMUTEST/SUB/A2.TXT")));
    REC(m, "34" + koi("B.BIN") + koi("C.BIN"), "80 34 06 08");   // FR_EXIST
    REC(m, "34" + koi("b.bin") + koi("B.BIN"), "00 34 00 00");   // only the case
    REC(m, "31" + koi("SUB"), "80 31 06 07");               // not empty: FR_DENIED
    REC(m, "31" + koi("NENI.TXT"), "80 31 06 04");
    REC(m, "40" + koi("SUB"), "80 40 06 08");
    REC(m, "32 01 01" + koi("C.BIN"), "80 32 01 00");       // CHMOD, UTIME: not in the DPI
    REC(m, "33 00 00 00 00 00 00" + koi("C.BIN"), "80 33 01 00");
    REC(m, "50 01" + koi("C.BIN"), "08 50 00 00");
    REC(m, "31" + koi("C.BIN"), "88 31 06 10");             // the open file: FR_LOCKED
    REC(m, "54", "00 54 00 00");
    REC(m, "31" + koi("C.BIN"), "00 31 00 00");
    CHECK(!fs::exists(host(sd, "EMUTEST/C.BIN")));

    // /DPI: SECRET.INI and the old /DPI.INI hidden, /DPI protected
    CHECK(file_write_all(data_path(sd + "/DPI/SECRET.INI"), "[wifi]\r\n", 8));
    CHECK(file_write_all(data_path(sd + "/DPI.INI"), "[wifi]\r\n", 8));
    REC(m, "41" + koi("/DPI"), "04 41 37 00");
    std::vector<uint8_t> r = read_n(m, 55);
    CHECK(r[22] == 7 && !memcmp(&r[23], "DPI.INI", 7));
    STATUS(m, "00 41 00 00");                               // only DPI.INI
    REC(m, "30" + koi("/DPI/SECRET.INI"), "80 30 06 04");
    REC(m, "50 01" + koi("/DPI/SECRET.INI"), "80 50 06 04");
    REC(m, "50 0A" + koi("/dpi/secret.ini"), "80 50 06 07");
    REC(m, "31" + koi("/DPI"), "80 31 06 07");
    REC(m, "34" + koi("/DPI") + koi("/X"), "80 34 06 07");
    REC(m, "41" + koi("/"), "04 41 37 00");                 // the root: DPI and EMUTEST, not DPI.INI
    r = read_n(m, 55);
    CHECK(r[22] == 3 && !memcmp(&r[23], "DPI", 3));
    r = read_n(m, 55);
    CHECK(r[22] == 7 && !memcmp(&r[23], "EMUTEST", 7));
    STATUS(m, "00 41 00 00");

    // GETCONFIG, SETCONFIG on /DPI/DPI.INI
    REC(m, "92" + koi("net"), "04 92 50 00");
    r = read_n(m, 80);
    CHECK(!strcmp((const char*)&r[0], "relay") && !strcmp((const char*)&r[16], "api.mzpico.com:80"));
    r = read_n(m, 80);
    CHECK(!strcmp((const char*)&r[0], "transport") && !strcmp((const char*)&r[16], "ws"));
    r = read_n(m, 80);
    CHECK(!strcmp((const char*)&r[0], "tcp_default") && r[16] == 0);
    STATUS(m, "00 92 00 00");
    REC(m, "92" + koi("neni"), "00 92 00 00");
    REC(m, "99" + koi("emutest") + koi("klic") + koi("hodnota"), "00 99 00 00");
    REC(m, "92" + koi("emutest"), "04 92 50 00");
    r = read_n(m, 80);
    CHECK(!strcmp((const char*)&r[0], "klic") && !strcmp((const char*)&r[16], "hodnota"));
    REC(m, "99" + koi("emutest") + koi("a=b") + koi("x"), "80 99 02 00");
    REC(m, "99" + koi("emutest") + koi("klic") + " 0D", "00 99 00 00");   // an empty value deletes it
    REC(m, "92" + koi("emutest"), "00 92 00 00");

    // RTC: the clock of the PC, moved by the program
    host_time_fix(1760608800);                              // 2025-10-16 10:00:00 UTC
    REC(m, "60 1F 02 2E", "80 60 02 00");                   // 31. 2.
    REC(m, "60 10 0A 2E", "00 60 00 00");                   // 16. 10. 2026
    REC(m, "62", "02 62 03 00 | 10 0A 2E");
    REC(m, "61 0C 00 00", "00 61 00 00");
    REC(m, "63", "02 63 03 00 | 0C 00 00");
    REC(m, "62", "02 62 03 00 | 10 0A 2E");
    REC(m, "61 18 00 00", "80 61 02 00");                   // 24:00
    host_time_fix(0);

    // BOOT: /RTL for 100 ms; SDCARD: no USB to lend the card to
    REC(m, "07", "00 07 00 00");
    CHECK(m.bus.rtl());
    m.bus.now += m.bus.cpu_hz / 5 * m.bus.cpu_step;
    CHECK(!m.bus.rtl());
    REC(m, "B0 FF", "02 B0 01 00 | 00");
    REC(m, "B0 01", "80 B0 05 00");
    REC(m, "B0 07", "80 B0 02 00");

    // a saved state keeps the open file and its page
    REC(m, "21" + koi("/EMUTEST"), "00 21 00 00");
    REC(m, "50 0A" + koi("D.BIN"), "30 50 00 00");
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
        CHECK(file_read_all(data_path(sd + "/EMUTEST/D.BIN"), b));
        CHECK(std::string(b.begin(), b.end()) == "0123456789");
    }

    // RESET closes the file (written back) and goes to the root
    REC(m, "50 0A" + koi("E.BIN"), "30 50 00 00");
    m.bus.io_write(B + 1, 0x41);
    REC(m, "00", "00 00 00 00");
    CHECK_EQ(host_size(sd, "EMUTEST/E.BIN"), 1);
    REC(m, "22", "02 22 02 00 | 2F 0D");
}

// a card of 1 MB: writing stops with FR_DENIED when it is full
static void test_full(void)
{
    std::string sd = folder("dpi1l_full");
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(sd, 1), err));
    REC(m, "50 0A" + koi("BIG.BIN"), "30 50 00 00");
    int at = -1;
    for (int i = 0; i < 1200 * 1024 && at < 0; i++) {
        m.bus.io_write(B + 1, (uint8_t)i);
        if ((i & 4095) == 0 && (status4(m)[0] & 0x80)) at = i;
    }
    CHECK(at > 900 * 1024 && at < 1100 * 1024);
    STATUS(m, "B0 F1 06 07");
}

int main(void)
{
    test_init();
    test_files();
    test_full();
    return TEST_RESULT();
}
