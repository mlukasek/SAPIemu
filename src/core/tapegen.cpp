#include "core/tapegen.h"
#include "core/ihex.h"
#include "core/util.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// The line: 2400 bit/s, one start bit, 8 data bits (LSB first), 2 stop bits, idle 1.
// The sound is the line XOR the 2400 Hz clock, a square wave like Dsm1::tape_record
// (first half of a bit = the level). 48 kHz gives exactly 20 samples per bit. Gaps
// are idle tone, never silence (as the original recordings).
static const int GEN_RATE = 48000;
static const int SAMPLES_PER_BIT = GEN_RATE / 2400;
static const int16_t GEN_AMPLITUDE = 12000;

// MIKOS (ms): lead tone, after the header, between 'B' blocks, before the last
// block, after it; as the original recordings (the ROM needs far less).
static const int MIKOS_LEAD_MS = 2000, MIKOS_HEADER_GAP_MS = 2000, MIKOS_BLOCK_GAP_MS = 210;
static const int MIKOS_EOF_GAP_MS = 1000, MIKOS_TAIL_MS = 1000;
static const uint32_t MIKOS_LOWEST = 0x4400;   // 4000-43FF is the RAM of MIKOS

// MIKROBASIC: SAVE writes 8.4 s of tone (LOAD needs 1.53 s without a character).
static const int MB_LEAD_MS = 8400, MB_TAIL_MS = 1500;
static const uint32_t MB_TXTBGN = 0x40EF;   // the program area, LOAD always loads here
static const uint32_t MB_TXTE = 0x47AB;     // its end without RAM n

struct Line {
    std::vector<uint8_t> bits;
    void tone(int ms) { bits.insert(bits.end(), (size_t)ms * 12 / 5, 1); }   // 2.4 bits per ms
    void byte(uint8_t v)
    {
        bits.push_back(0);
        for (int i = 0; i < 8; i++) bits.push_back((v >> i) & 1);
        bits.push_back(1);
        bits.push_back(1);
    }
    void bytes(const std::vector<uint8_t>& v) { for (uint8_t b : v) byte(b); }
};

static void render(const Line& l, std::vector<int16_t>& samples, int& rate)
{
    rate = GEN_RATE;
    samples.resize(l.bits.size() * SAMPLES_PER_BIT);
    int16_t* s = samples.data();
    for (uint8_t b : l.bits) {
        int16_t first = b ? GEN_AMPLITUDE : (int16_t)-GEN_AMPLITUDE;
        for (int i = 0; i < SAMPLES_PER_BIT / 2; i++) *s++ = first;
        for (int i = 0; i < SAMPLES_PER_BIT / 2; i++) *s++ = (int16_t)-first;
    }
}

const char* tape_format_name(int format)
{
    return format == TAPE_MIKROBASIC ? "mikrobasic" : "mikos";
}

bool tape_format_parse(const std::string& s, int& format)
{
    std::string l;
    for (char c : s) l += (char)tolower((unsigned char)c);
    if (l == "mikos") format = TAPE_MIKOS;
    else if (l == "mikrobasic") format = TAPE_MIKROBASIC;
    else return false;
    return true;
}

static std::string ext_of(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return std::string();
    std::string e;
    for (size_t i = dot + 1; i < path.size(); i++) e += (char)tolower((unsigned char)path[i]);
    return e;
}

bool tape_is_hex(const std::string& path)
{
    std::string e = ext_of(path);
    return e == "hex" || e == "ihx" || e == "txt";   // many HEX files of SAPI are .txt
}

bool tape_is_program(const std::string& path)
{
    return tape_is_hex(path) || ext_of(path) == "bin";
}

std::string tape_name_clean(const std::string& name)
{
    std::string out;
    for (char ch : name) {
        int c = toupper((unsigned char)ch);
        if (c < 0x20 || c > 0x5F) continue;   // what the screen codes of AND-1 can show
        if (out.empty() && c == ' ') continue;
        out += (char)c;
        if (out.size() == 11) break;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string tape_default_name(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot > 0) base = base.substr(0, dot);
    std::string n = tape_name_clean(base);
    return n.empty() ? std::string("PROGRAM") : n;
}

static uint8_t sum8(const uint8_t* p, size_t n)
{
    uint8_t s = 0;
    for (size_t i = 0; i < n; i++) s = (uint8_t)(s + p[i]);
    return s;
}

// ---------------------------------------------------------------- MIKOS

// A block: 10 10 10, type, 255 data bytes, the name (11), block, file, checksum of
// type..file, 10 00 (275 bytes).
static std::vector<uint8_t> mikos_block(uint8_t type, const uint8_t* data255, const uint8_t* name11,
                                        uint8_t block, uint8_t file)
{
    std::vector<uint8_t> b = { 0x10, 0x10, 0x10, type };
    b.insert(b.end(), data255, data255 + 255);
    b.insert(b.end(), name11, name11 + 11);
    b.push_back(block);
    b.push_back(file);
    b.push_back(sum8(&b[3], 269));
    b.push_back(0x10);
    b.push_back(0x00);
    return b;
}

bool tape_mikos_file(std::vector<uint8_t> mem, uint32_t& lo, int64_t entry_addr, std::vector<uint8_t>& s, std::string& err)
{
    uint32_t entry = entry_addr < 0 ? lo : (uint32_t)entry_addr;
    if (entry != lo) {   // .J jumps to the lowest address: a JMP to the entry goes before the program
        if (lo < MIKOS_LOWEST + 3) {
            err = "MIKOS: no room for the jump to the start address below the program (4400h at the lowest)";
            return false;
        }
        uint8_t jp[3] = { 0xC3, (uint8_t)entry, (uint8_t)(entry >> 8) };
        mem.insert(mem.begin(), jp, jp + 3);
        lo -= 3;
    }
    if (lo < MIKOS_LOWEST) {
        char b[96];
        snprintf(b, sizeof(b), "MIKOS: the program starts at %04Xh, it must be 4400h or higher (4000-43FF is MIKOS RAM)",
                 (unsigned)lo);
        err = b;
        return false;
    }
    uint32_t hi = lo + (uint32_t)mem.size() - 1;
    // the file: '<' end start '>' KS1 data KS2 1Ah, the addresses low byte first
    s = { 0x3C, (uint8_t)hi, (uint8_t)(hi >> 8), (uint8_t)lo, (uint8_t)(lo >> 8), 0x3E };
    s.push_back(sum8(&s[1], 5));
    s.insert(s.end(), mem.begin(), mem.end());
    s.push_back(sum8(mem.data(), mem.size()));
    s.push_back(0x1A);
    return true;
}

static bool build_mikos(std::vector<uint8_t> mem, uint32_t lo, const TapeProgram& p, const std::string& name,
                        Line& line, std::string& info, std::string& err)
{
    std::vector<uint8_t> s;
    if (!tape_mikos_file(mem, lo, p.entry, s, err)) return false;
    uint32_t hi = (uint32_t)s[1] | (uint32_t)s[2] << 8;   // the end address in the file
    mem.resize(s.size() - 9);                              // the size on the tape (with a jump 3 bytes more)

    // header data: the name in ASCII, a space, zeros; in every block the name as AND-1 screen codes
    uint8_t hd[255];
    memset(hd, 0, sizeof(hd));
    memcpy(hd, name.data(), name.size());
    hd[name.size()] = 0x20;
    uint8_t wrnm[11];
    for (int i = 0; i < 11; i++) wrnm[i] = (hd[i] & 0x3F) ? (uint8_t)(hd[i] & 0x3F) : 0x20;

    const uint8_t file = 1;
    line.tone(MIKOS_LEAD_MS);
    line.bytes(mikos_block(0x01, hd, wrnm, 1, file));
    line.tone(MIKOS_HEADER_GAP_MS);
    uint8_t block = 2;
    int blocks = 1;
    for (size_t i = 0; i < s.size(); i += 255, block++, blocks++) {
        size_t n = s.size() - i < 255 ? s.size() - i : 255;
        bool last = i + n >= s.size();
        uint8_t d[255];
        memset(d, 0, sizeof(d));
        memcpy(d, &s[i], n);
        if (i) line.tone(last ? MIKOS_EOF_GAP_MS : MIKOS_BLOCK_GAP_MS);
        line.bytes(mikos_block(last ? 0x04 : 0x42, d, wrnm, block, file));
    }
    line.tone(MIKOS_TAIL_MS);
    char b[160];
    snprintf(b, sizeof(b), "MIKOS \"%s\": %04X-%04Xh, %u B, %d blocks, .J starts %04Xh", name.c_str(),
             (unsigned)lo, (unsigned)hi, (unsigned)mem.size(), blocks, (unsigned)lo);
    info = b;
    return true;
}

// ---------------------------------------------------------------- MIKROBASIC

static bool build_mikrobasic(std::vector<uint8_t> mem, uint32_t lo, const TapeProgram& p, const std::string& name,
                             Line& line, std::string& info, std::string& err)
{
    char b[200];
    std::string what = "BASIC";
    if (lo != MB_TXTBGN) {
        // machine code above the program area start: a BASIC line CALLs it, RUN starts it
        uint32_t entry = p.entry < 0 ? lo : (uint32_t)p.entry;
        char call[24];
        snprintf(call, sizeof(call), "CALL %u", (unsigned)entry);
        std::vector<uint8_t> stub = { 1, 0 };
        stub.insert(stub.end(), call, call + strlen(call));
        stub.push_back(0x0D);
        const char* end_line = "END";
        stub.push_back(2);
        stub.push_back(0);
        stub.insert(stub.end(), end_line, end_line + 3);
        stub.push_back(0x0D);
        if (lo < MB_TXTBGN + stub.size()) {
            snprintf(b, sizeof(b), "MIKROBASIC: the program starts at %04Xh; it must start at 40EFh (a BASIC program) "
                                   "or at %04Xh or higher (machine code started by RUN)",
                     (unsigned)lo, (unsigned)(MB_TXTBGN + stub.size()));
            err = b;
            return false;
        }
        stub.resize(lo - MB_TXTBGN, 0);
        mem.insert(mem.begin(), stub.begin(), stub.end());
        snprintf(b, sizeof(b), "machine code from %04Xh, RUN = 1 CALL %u", (unsigned)lo, (unsigned)entry);
        what = b;
    }
    uint32_t end = MB_TXTBGN + (uint32_t)mem.size();   // TXTUNF after LOAD
    if (end > 0x10000) {
        err = "MIKROBASIC: the program does not fit into memory";
        return false;
    }
    // 'B', the name, CR, end (lo, hi), the program from 40EFh, checksum of name..program
    std::vector<uint8_t> r(name.begin(), name.end());
    r.push_back(0x0D);
    r.push_back((uint8_t)end);
    r.push_back((uint8_t)(end >> 8));
    r.insert(r.end(), mem.begin(), mem.end());
    uint8_t cs = sum8(r.data(), r.size());
    line.tone(MB_LEAD_MS);
    line.byte('B');
    line.bytes(r);
    line.byte(cs);
    line.tone(MB_TAIL_MS);
    snprintf(b, sizeof(b), "MIKROBASIC \"%s\": 40EF-%04Xh, %u B, %s%s", name.c_str(), (unsigned)(end - 1),
             (unsigned)mem.size(), what.c_str(), end > MB_TXTE ? ", beyond 47AAh (needs RAM n and more memory)" : "");
    info = b;
    return true;
}

// ---------------------------------------------------------------- program

bool tape_build(const uint8_t* image, uint32_t lo, uint32_t hi, const TapeProgram& p,
                std::vector<int16_t>& samples, int& rate, std::string& info, std::string& err)
{
    if (hi < lo || hi > 0xFFFF) {
        err = "the program does not fit into 64 KB";
        return false;
    }
    std::string name = tape_name_clean(p.name);
    if (name.empty()) name = "PROGRAM";
    std::vector<uint8_t> mem(image + lo, image + hi + 1);
    Line line;
    bool ok = p.format == TAPE_MIKROBASIC ? build_mikrobasic(mem, lo, p, name, line, info, err)
                                          : build_mikos(mem, lo, p, name, line, info, err);
    if (!ok) return false;
    render(line, samples, rate);
    return true;
}

struct HexImage {
    uint8_t* mem;
    int count;
    uint32_t lo, hi;
    bool over;   // a byte above 64 KB
};

static void hex_byte(void* user, uint32_t addr, uint8_t v)
{
    HexImage* h = (HexImage*)user;
    if (addr > 0xFFFF) {
        h->over = true;
        return;
    }
    h->mem[addr] = v;
    if (!h->count || addr < h->lo) h->lo = addr;
    if (!h->count || addr > h->hi) h->hi = addr;
    h->count++;
}

bool tape_load_image(const std::string& path, const TapeProgram& p, std::vector<uint8_t>& mem, uint32_t& lo,
                     uint32_t& hi, std::string& err)
{
    mem.assign(0x10000, 0);   // gaps of a HEX file are zeros
    if (tape_is_hex(path)) {
        std::string text;
        if (!file_read_text(path, text)) {
            err = "cannot read " + path;
            return false;
        }
        HexImage h = { mem.data(), 0, 0, 0, false };
        if (!ihex_parse(text, hex_byte, &h, err)) return false;
        if (!h.count || h.over) {
            err = h.over ? "the file does not fit into 64 KB" : "the file holds no data";
            return false;
        }
        lo = h.lo;
        hi = h.hi;
    } else {
        std::vector<uint8_t> d;
        if (!file_read_all(path, d)) {
            err = "cannot read " + path;
            return false;
        }
        if (p.address < 0) {
            err = "a binary file needs its load address";
            return false;
        }
        if (d.empty() || p.address + (long long)d.size() > 0x10000) {
            err = d.empty() ? "the file is empty" : "the file does not fit into memory at that address";
            return false;
        }
        lo = (uint32_t)p.address;
        hi = lo + (uint32_t)d.size() - 1;
        memcpy(&mem[lo], d.data(), d.size());
    }
    return true;
}

bool tape_from_file(const std::string& path, const TapeProgram& p, std::vector<int16_t>& samples,
                    int& rate, std::string& info, std::string& err)
{
    std::vector<uint8_t> mem;
    uint32_t lo, hi;
    if (!tape_load_image(path, p, mem, lo, hi, err)) return false;
    TapeProgram q = p;
    if (tape_name_clean(q.name).empty()) q.name = tape_default_name(path);
    return tape_build(mem.data(), lo, hi, q, samples, rate, info, err);
}
