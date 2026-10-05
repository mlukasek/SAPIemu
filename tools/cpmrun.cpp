// cpmrun - runs a CP/M 2.2 program (.COM) on the PC, e.g. the Microsoft assembler M80 and
// linker L80 to build the SAPI-1 system sources (docs/software/ECMON.md).
//
//   cpmrun [-d DIR] PROGRAM[.COM] [command tail...]
//
// The Z80 core of the emulator runs the program in a 64 KB TPA; a minimal BDOS and BIOS
// trap to the PC: the console goes to stdout/stdin, files are in DIR (default: current
// directory) for every drive letter, names are matched case-insensitively. Implemented BDOS
// functions: 0-40 as far as compilers and assemblers use them (sequential and random files,
// search with ?, delete, rename, file size). stdin is the keyboard: its characters are keys
// typed ahead (console status reports them). The exit code is 0, or 2 when the program
// cannot be started.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <filesystem>
#include "z80.h"
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <sys/select.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static uint8_t mem[65536];
static fs::path dir;
static uint16_t dma = 0x80;
static uint8_t cur_drive = 0;
static bool trace = false;   // environment CPMRUN_TRACE: BDOS calls on stderr

// memory map of the traps
static const uint16_t BDOS_ENTRY = 0xFE06;   // OUT (BDOS_PORT),A ; RET
static const uint16_t BIOS_BASE = 0xFF00;    // 17 entries: OUT (n),A ; RET (n = entry)
static const uint16_t DPB_ADDR = 0xFE40;
static const uint16_t ALV_ADDR = 0xFE60;
static const uint8_t BDOS_PORT = 0xFE;

// FCB offsets
enum { F_DR = 0, F_NAME = 1, F_EXT = 9, F_EX = 12, F_S2 = 14, F_RC = 15, F_CR = 32, F_R0 = 33 };

static std::vector<fs::path> search_list;
static size_t search_pos;

// 8.3 name from an FCB (attribute bits cleared), "NAME.EXT"
static std::string fcb_name(uint16_t fcb)
{
    std::string n, e;
    for (int i = 0; i < 8; i++) {
        char c = (char)(mem[(uint16_t)(fcb + F_NAME + i)] & 0x7F);
        if (c != ' ') n += c;
    }
    for (int i = 0; i < 3; i++) {
        char c = (char)(mem[(uint16_t)(fcb + F_EXT + i)] & 0x7F);
        if (c != ' ') e += c;
    }
    return e.empty() ? n : n + "." + e;
}

static std::string upper(std::string s)
{
    for (auto& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

// splits a host file name into an 8.3 FCB name (11 chars, space padded); false if it does not fit
static bool host_to_83(const std::string& name, char out[11])
{
    std::string s = upper(name);
    size_t dot = s.rfind('.');
    std::string n = dot == std::string::npos ? s : s.substr(0, dot);
    std::string e = dot == std::string::npos ? "" : s.substr(dot + 1);
    if (n.empty() || n.size() > 8 || e.size() > 3 || n.find('.') != std::string::npos) return false;
    memset(out, ' ', 11);
    memcpy(out, n.data(), n.size());
    memcpy(out + 8, e.data(), e.size());
    return true;
}

static bool match_fcb(uint16_t fcb, const char name[11])
{
    for (int i = 0; i < 11; i++) {
        char p = (char)(mem[(uint16_t)(fcb + F_NAME + i)] & 0x7F);
        if (p != '?' && toupper((unsigned char)p) != name[i]) return false;
    }
    return true;
}

// host files matching the FCB (with ? wildcards)
static std::vector<fs::path> find_files(uint16_t fcb)
{
    std::vector<fs::path> r;
    std::error_code ec;
    for (auto& de : fs::directory_iterator(dir, ec)) {
        if (!de.is_regular_file()) continue;
        char n83[11];
        if (host_to_83(de.path().filename().string(), n83) && match_fcb(fcb, n83)) r.push_back(de.path());
    }
    return r;
}

// existing host file for the FCB name, or the path to create (upper case)
static fs::path file_path(uint16_t fcb, bool* exists)
{
    std::vector<fs::path> f = find_files(fcb);
    *exists = !f.empty();
    return f.empty() ? dir / fcb_name(fcb) : f[0];
}

static long file_records(const fs::path& p)
{
    std::error_code ec;
    uintmax_t sz = fs::file_size(p, ec);
    return ec ? 0 : (long)((sz + 127) / 128);
}

static long seq_record(uint16_t fcb)
{
    long ext = (long)(mem[(uint16_t)(fcb + F_S2)] & 0x3F) * 32 + (mem[(uint16_t)(fcb + F_EX)] & 0x1F);
    return ext * 128 + (mem[(uint16_t)(fcb + F_CR)] & 0x7F);
}

static void set_rc(uint16_t fcb, long total)
{
    long ext = (long)(mem[(uint16_t)(fcb + F_S2)] & 0x3F) * 32 + (mem[(uint16_t)(fcb + F_EX)] & 0x1F);
    long rc = total - ext * 128;
    mem[(uint16_t)(fcb + F_RC)] = (uint8_t)(rc < 0 ? 0 : rc > 128 ? 128 : rc);
}

static void set_seq_record(uint16_t fcb, long rec)
{
    mem[(uint16_t)(fcb + F_CR)] = (uint8_t)(rec & 127);
    mem[(uint16_t)(fcb + F_EX)] = (uint8_t)((rec >> 7) & 31);
    mem[(uint16_t)(fcb + F_S2)] = (uint8_t)((rec >> 12) & 0x3F);
}

static long random_record(uint16_t fcb)
{
    return mem[(uint16_t)(fcb + F_R0)] | (mem[(uint16_t)(fcb + F_R0 + 1)] << 8) |
           ((long)(mem[(uint16_t)(fcb + F_R0 + 2)] & 3) << 16);
}

// 0 = ok, 1 = end of file
static int read_record(uint16_t fcb, long rec)
{
    bool ex;
    fs::path p = file_path(fcb, &ex);
    if (!ex || rec >= file_records(p)) return 1;
    FILE* f = fopen(p.string().c_str(), "rb");
    if (!f) return 1;
    uint8_t buf[128];
    memset(buf, 0x1A, sizeof(buf));
    fseek(f, rec * 128, SEEK_SET);
    size_t n = fread(buf, 1, 128, f);
    fclose(f);
    if (n == 0) return 1;
    for (int i = 0; i < 128; i++) mem[(uint16_t)(dma + i)] = buf[i];
    return 0;
}

// 0 = ok, 2 = disk full (cannot write)
static int write_record(uint16_t fcb, long rec)
{
    bool ex;
    fs::path p = file_path(fcb, &ex);
    FILE* f = fopen(p.string().c_str(), ex ? "r+b" : "w+b");
    if (!f) return 2;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    while (size < rec * 128) {   // a gap before the record: fill with zeros
        fputc(0, f);
        size++;
    }
    fseek(f, rec * 128, SEEK_SET);
    uint8_t buf[128];
    for (int i = 0; i < 128; i++) buf[i] = mem[(uint16_t)(dma + i)];
    size_t n = fwrite(buf, 1, 128, f);
    fclose(f);
    return n == 128 ? 0 : 2;
}

// fills the directory entry at the DMA address for a search result
static uint8_t search_result(void)
{
    while (search_pos < search_list.size()) {
        fs::path p = search_list[search_pos++];
        char n83[11];
        if (!host_to_83(p.filename().string(), n83)) continue;
        memset(&mem[dma], 0, 32);
        for (int i = 0; i < 11; i++) mem[(uint16_t)(dma + 1 + i)] = (uint8_t)n83[i];
        long recs = file_records(p);
        mem[(uint16_t)(dma + 12)] = (uint8_t)(recs > 0 ? ((recs - 1) >> 7) & 31 : 0);
        long rc = recs - (recs > 0 ? ((recs - 1) >> 7) << 7 : 0);
        mem[(uint16_t)(dma + 15)] = (uint8_t)rc;
        for (int i = 16; i < 32; i++) mem[(uint16_t)(dma + i)] = 1;   // allocation: used
        return 0;
    }
    return 0xFF;
}

// a key waiting: stdin is the keys typed ahead (a pipe or file: its next character, not taken),
// a terminal: a key pressed
static bool console_ready(void)
{
#ifdef _WIN32
    if (_isatty(_fileno(stdin))) return _kbhit() != 0;
#else
    if (isatty(0)) {
        fd_set s;
        FD_ZERO(&s);
        FD_SET(0, &s);
        timeval t = { 0, 0 };
        return select(1, &s, nullptr, nullptr, &t) > 0;
    }
#endif
    int c = getchar();
    if (c == EOF) return false;
    ungetc(c, stdin);
    return true;
}

static int read_console(void)
{
    int c = getchar();
    return c == EOF ? 0x1A : (c == '\n' ? '\r' : c);
}

// returns false when the program ends
static bool bdos(z80_t* cpu)
{
    uint8_t fn = cpu->c;
    uint16_t de = cpu->de;
    uint16_t result = 0;
    if (trace && (fn > 12 || fn == 1 || fn == 6 || fn == 10 || fn == 11)) fprintf(stderr, "[BDOS %d DE=%04X %s]\n", fn, de, fn >= 15 && fn != 26 ? fcb_name(de).c_str() : "");
    switch (fn) {
    case 0: return false;
    case 1: result = (uint16_t)read_console(); putchar(result); break;
    case 2: putchar(cpu->e); break;
    case 3: result = 0x1A; break;   // reader: end of file
    case 4: case 5: break;          // punch, list: dropped
    case 6:
        if (cpu->e == 0xFF) result = console_ready() ? (uint16_t)read_console() : 0;
        else if (cpu->e == 0xFE) result = console_ready() ? 0xFF : 0;
        else putchar(cpu->e);
        break;
    case 7: case 8: break;          // IOBYTE
    case 9:
        for (uint16_t a = de; mem[a] != '$'; a++) putchar(mem[a]);
        break;
    case 10: {
        uint8_t max = mem[de];
        int n = 0, c;
        if (feof(stdin)) {   // nothing more to answer an interactive prompt: stop
            fprintf(stderr, "\ncpmrun: end of input at a console line read, stopped\n");
            return false;
        }
        while ((c = getchar()) != EOF && c != '\n') {
            if (c == '\r') continue;
            if (n < max) mem[(uint16_t)(de + 2 + n++)] = (uint8_t)c;
        }
        mem[(uint16_t)(de + 1)] = (uint8_t)n;
        break;
    }
    case 11: result = console_ready() ? 0xFF : 0; break;
    case 12: result = 0x0022; break;
    case 13: dma = 0x80; cur_drive = 0; break;
    case 14: cur_drive = cpu->e & 15; break;
    case 15: {   // open
        bool ex;
        fs::path p = file_path(de, &ex);
        if (!ex) { result = 0xFF; break; }
        mem[(uint16_t)(de + F_S2)] = 0;
        set_rc(de, file_records(p));
        break;
    }
    case 16: break;   // close
    case 17:
        search_list = find_files(de);
        search_pos = 0;
        result = search_result();
        break;
    case 18: result = search_result(); break;
    case 19: {   // delete
        std::vector<fs::path> f = find_files(de);
        std::error_code ec;
        for (auto& p : f) fs::remove(p, ec);
        result = f.empty() ? 0xFF : 0;
        break;
    }
    case 20: {   // read sequential
        long rec = seq_record(de);
        result = (uint16_t)read_record(de, rec);
        if (result == 0) {
            set_seq_record(de, rec + 1);
            bool ex;
            set_rc(de, file_records(file_path(de, &ex)));
        }
        break;
    }
    case 21: {   // write sequential
        long rec = seq_record(de);
        result = (uint16_t)write_record(de, rec);
        if (result == 0) {
            set_seq_record(de, rec + 1);
            bool ex;
            set_rc(de, file_records(file_path(de, &ex)));
        }
        break;
    }
    case 22: {   // make
        bool ex;
        fs::path p = file_path(de, &ex);
        if (ex) { result = 0xFF; break; }   // CP/M programs delete first
        FILE* f = fopen((dir / fcb_name(de)).string().c_str(), "wb");
        if (!f) { result = 0xFF; break; }
        fclose(f);
        mem[(uint16_t)(de + F_S2)] = 0;
        mem[(uint16_t)(de + F_RC)] = 0;
        break;
    }
    case 23: {   // rename: new name at DE+16
        bool ex;
        fs::path p = file_path(de, &ex);
        if (!ex) { result = 0xFF; break; }
        std::error_code ec;
        fs::rename(p, dir / fcb_name((uint16_t)(de + 16)), ec);
        result = ec ? 0xFF : 0;
        break;
    }
    case 24: result = 1; break;   // only A: logged in
    case 25: result = cur_drive; break;
    case 26: dma = de; break;
    case 27: result = ALV_ADDR; break;
    case 28: case 30: break;
    case 29: result = 0; break;
    case 31: result = DPB_ADDR; break;
    case 32: result = 0; break;   // user 0
    case 33: {   // read random
        long rec = random_record(de);
        set_seq_record(de, rec);
        result = (uint16_t)read_record(de, rec);
        break;
    }
    case 34: case 40: {   // write random
        long rec = random_record(de);
        set_seq_record(de, rec);
        result = (uint16_t)write_record(de, rec);
        break;
    }
    case 35: {   // file size
        bool ex;
        long recs = file_records(file_path(de, &ex));
        mem[(uint16_t)(de + F_R0)] = (uint8_t)recs;
        mem[(uint16_t)(de + F_R0 + 1)] = (uint8_t)(recs >> 8);
        mem[(uint16_t)(de + F_R0 + 2)] = (uint8_t)(recs >> 16);
        break;
    }
    case 36: {   // set random record
        long rec = seq_record(de);
        mem[(uint16_t)(de + F_R0)] = (uint8_t)rec;
        mem[(uint16_t)(de + F_R0 + 1)] = (uint8_t)(rec >> 8);
        mem[(uint16_t)(de + F_R0 + 2)] = (uint8_t)(rec >> 16);
        break;
    }
    case 37: break;
    default:
        fprintf(stderr, "cpmrun: BDOS function %d not implemented\n", fn);
        break;
    }
    cpu->hl = result;
    cpu->a = (uint8_t)result;
    cpu->b = (uint8_t)(result >> 8);
    fflush(stdout);
    return true;
}

// returns false when the program ends
static bool bios(z80_t* cpu, uint8_t entry)
{
    if (trace && entry != 4) fprintf(stderr, "[BIOS %d]\n", entry);
    switch (entry) {
    case 0: case 1: return false;                         // boot, warm boot
    case 2: cpu->a = console_ready() ? 0xFF : 0; break;   // console status
    case 3: cpu->a = (uint8_t)read_console(); break;      // console in
    case 4: putchar(cpu->c); fflush(stdout); break;       // console out
    default: cpu->a = 0; break;
    }
    return true;
}

static bool ccp_delim(char c)
{
    return c == 0 || c == ' ' || c == '=' || c == '_' || c == '.' || c == ':' || c == ';' || c == '<' || c == '>';
}

// FCB at addr from a command line word as the CP/M 2.2 CCP fills it (drive, name, extension up
// to its delimiters)
static void parse_fcb(uint16_t addr, const std::string& w)
{
    memset(&mem[addr], 0, 16);
    memset(&mem[addr + 1], ' ', 11);
    const char* s = w.c_str();
    if (s[0] && s[1] == ':') {
        mem[addr] = (uint8_t)(s[0] - 'A' + 1);
        s += 2;
    }
    for (int i = 0; *s && !ccp_delim(*s); s++, i++)
        if (i < 8) mem[addr + 1 + i] = (uint8_t)(*s == '*' ? '?' : *s);
    if (*s == '.')
        for (int i = 0, k = 1; s[k] && !ccp_delim(s[k]); k++, i++)
            if (i < 3) mem[addr + 9 + i] = (uint8_t)(s[k] == '*' ? '?' : s[k]);
}

int main(int argc, char** argv)
{
    int a = 1;
    dir = fs::current_path();
    trace = getenv("CPMRUN_TRACE") != nullptr;
    if (a + 1 < argc && strcmp(argv[a], "-d") == 0) {
        dir = argv[a + 1];
        a += 2;
    }
    if (a >= argc) {
        fprintf(stderr, "usage: cpmrun [-d DIR] PROGRAM[.COM] [command tail...]\n");
        return 2;
    }
    std::string prog = argv[a++];
    fs::path pp = prog;
    if (!pp.has_extension()) pp += ".COM";
    if (pp.is_relative() && !fs::exists(pp)) pp = dir / pp;
    FILE* f = fopen(pp.string().c_str(), "rb");
    if (!f) {
        fprintf(stderr, "cpmrun: cannot open %s\n", pp.string().c_str());
        return 2;
    }
    size_t n = fread(&mem[0x100], 1, BDOS_ENTRY - 0x100, f);
    fclose(f);
    if (n == 0) return 2;

    // command tail as the CCP leaves it: upper case, at 0080h with its length
    std::string tail;
    std::vector<std::string> words;
    for (; a < argc; a++) {
        tail += ' ';
        tail += upper(argv[a]);
    }
    for (size_t i = 0, j; i < tail.size(); i = j) {
        while (i < tail.size() && tail[i] == ' ') i++;
        j = tail.find(' ', i);
        if (j == std::string::npos) j = tail.size();
        if (j > i) words.push_back(tail.substr(i, j - i));
    }
    if (tail.size() > 127) tail.resize(127);
    mem[0x80] = (uint8_t)tail.size();
    memcpy(&mem[0x81], tail.data(), tail.size());
    parse_fcb(0x5C, words.size() > 0 ? words[0] : "");
    parse_fcb(0x6C, words.size() > 1 ? words[1] : "");

    // page zero, BDOS and BIOS traps
    mem[0x0000] = 0xC3; mem[0x0001] = (uint8_t)(BIOS_BASE + 3); mem[0x0002] = BIOS_BASE >> 8;
    mem[0x0005] = 0xC3; mem[0x0006] = (uint8_t)BDOS_ENTRY; mem[0x0007] = BDOS_ENTRY >> 8;
    mem[BDOS_ENTRY] = 0xD3; mem[BDOS_ENTRY + 1] = BDOS_PORT; mem[BDOS_ENTRY + 2] = 0xC9;
    for (int i = 0; i < 17; i++) {
        mem[BIOS_BASE + 3 * i] = 0xD3;
        mem[BIOS_BASE + 3 * i + 1] = (uint8_t)i;
        mem[BIOS_BASE + 3 * i + 2] = 0xC9;
    }
    static const uint8_t dpb[15] = { 64, 0, 4, 15, 0, 0xFF, 0x07, 0xFF, 0x00, 0, 0, 2, 0 };   // 8 MB, 2 KB blocks
    memcpy(&mem[DPB_ADDR], dpb, sizeof(dpb));

    z80_t cpu;
    uint64_t pins = z80_init(&cpu);
    cpu.sp = BDOS_ENTRY;
    mem[--cpu.sp] = 0x00;   // a RET from the program goes to 0000h (warm boot)
    mem[--cpu.sp] = 0x00;
    pins = z80_prefetch(&cpu, 0x0100);
    for (;;) {
        pins = z80_tick(&cpu, pins);
        if (pins & Z80_MREQ) {
            uint16_t ad = Z80_GET_ADDR(pins);
            if (pins & Z80_RD) {
                Z80_SET_DATA(pins, mem[ad]);
            } else if (pins & Z80_WR) {
                mem[ad] = Z80_GET_DATA(pins);
            }
        } else if ((pins & Z80_IORQ) && (pins & Z80_WR)) {
            uint8_t port = (uint8_t)Z80_GET_ADDR(pins);
            bool go = port == BDOS_PORT ? bdos(&cpu) : bios(&cpu, port);
            if (!go) break;
        } else if ((pins & Z80_IORQ) && (pins & Z80_RD)) {
            Z80_SET_DATA(pins, 0xFF);
        }
        if (pins & Z80_HALT) {
            fprintf(stderr, "cpmrun: HALT at %04X\n", (unsigned)(cpu.pc - 1));
            break;
        }
    }
    fflush(stdout);
    return 0;
}
