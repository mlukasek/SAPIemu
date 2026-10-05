// tools/cpmrun: a small CP/M program (built here, no assembler needed) prints through BDOS 9,
// shows its command tail, writes a file sequentially and reads it back.
#include "core/util.h"
#include "test_util.h"

#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

static const uint16_t MSG = 0x0180, FCB = 0x0190, DATA = 0x0200, BUF = 0x0280;

static void bdos(std::vector<uint8_t>& p, uint8_t fn, uint16_t de)
{
    uint8_t c[] = { 0x11, (uint8_t)de, (uint8_t)(de >> 8), 0x0E, fn, 0xCD, 0x05, 0x00 };   // LD DE / LD C / CALL 5
    p.insert(p.end(), c, c + sizeof(c));
}

static std::vector<uint8_t> program(void)
{
    std::vector<uint8_t> p;
    bdos(p, 9, MSG);      // "HELLO"
    bdos(p, 9, 0x0081);   // the command tail
    bdos(p, 22, FCB);     // make TEST.TXT
    bdos(p, 26, DATA);    // DMA
    bdos(p, 21, FCB);     // write sequential
    bdos(p, 16, FCB);     // close
    uint8_t clr[] = { 0xAF, 0x32, (uint8_t)(FCB + 12), (uint8_t)((FCB + 12) >> 8), 0x32, (uint8_t)(FCB + 32),
                      (uint8_t)((FCB + 32) >> 8) };   // XOR A, LD (EX),A, LD (CR),A
    p.insert(p.end(), clr, clr + sizeof(clr));
    bdos(p, 15, FCB);     // open
    bdos(p, 26, BUF);
    bdos(p, 20, FCB);     // read sequential
    bdos(p, 9, BUF);      // "FILE OK"
    p.push_back(0xC3);    // JP 0
    p.push_back(0x00);
    p.push_back(0x00);
    CHECK(p.size() <= MSG - 0x100);
    p.resize(0x200, 0);
    const char* msg = "HELLO\r\n$";
    memcpy(&p[MSG - 0x100], msg, strlen(msg));
    memcpy(&p[FCB - 0x100 + 1], "TEST    TXT", 11);
    memset(&p[DATA - 0x100], 0x1A, 128);
    const char* data = "\r\nFILE OK\r\n$";
    memcpy(&p[DATA - 0x100], data, strlen(data));
    return p;
}

int main(void)
{
    test_init();
    std::string dir = data_path("work/test/cpmrun");
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    std::vector<uint8_t> p = program();
    CHECK(file_write_all(dir + "/T.COM", p.data(), p.size()));
    std::string cmd = "\"\"" CPMRUN_EXE "\" -d \"" + dir + "\" T abc$\"";
#ifndef _WIN32
    cmd = "\"" CPMRUN_EXE "\" -d \"" + dir + "\" T 'abc$'";
#endif
    FILE* f = popen(cmd.c_str(), "r");
    CHECK(f != nullptr);
    std::string out;
    if (f) {
        char b[256];
        size_t n;
        while ((n = fread(b, 1, sizeof(b), f)) > 0) out.append(b, n);
        CHECK(pclose(f) == 0);
    }
    printf("%s\n", out.c_str());
    CHECK(out.find("HELLO") != std::string::npos);
    CHECK(out.find(" ABC") != std::string::npos);   // upper case, after a space, as the CCP leaves it
    CHECK(out.find("FILE OK") != std::string::npos);
    std::vector<uint8_t> t;
    CHECK(file_read_all(dir + "/TEST.TXT", t) && t.size() == 128 && memcmp(&t[2], "FILE OK", 7) == 0);
    return TEST_RESULT();
}
