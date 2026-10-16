#include "cards/dpi1l_fs.h"
#include "core/log.h"
#include "core/state.h"
#include "core/util.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <set>

namespace fs = std::filesystem;

static const char* const SECRET_PATH = "/DPI/SECRET.INI";
static const char* const OLD_SECRET_PATH = "/DPI.INI";
static const char* const DPI_DIR = "/DPI";
static const char* const CONFIG_PATH = "/DPI/DPI.INI";
static const size_t NAME_FIELD = 32;           // FILINFO record: name at 23, 32 bytes (31 + 00)
static const size_t INI_MAX_SIZE = 4096;

// /DPI/DPI.INI written when it is missing (common/config.c, the same text)
static const char default_ini[] =
    "; DPI-1L configuration (SAPI_DPI-firmware docs/konfigurace.md). A missing key takes its default.\r\n"
    "; Wi-Fi belongs to /DPI/SECRET.INI (hidden from the Repository API):  [wifi]  ssid=...  password=\"...\"\r\n"
    "\r\n"
    "[bus]\r\n"
    "dram=0                  ; 1 = DRAM refreshed by the Z80 in the rack (RAM-1, RAM-1Z): short WAIT only\r\n"
    "\r\n"
    "[repo]                  ; Repository API (Unicard)\r\n"
    "enable=1\r\n"
    "port=B0h                ; command/status port, data on port+1\r\n"
    "\r\n"
    "[net]\r\n"
    "relay=api.mzpico.com:80 ; NET A0h-A9h, game relay (BomberNet, MZPico)\r\n"
    "transport=ws            ; ws = WebSocket (MZPico) | tcp = JSON lines (BomberNet relay.py, port 8766)\r\n"
    "tcp_default=            ; a.b.c.d:port, TCPOPEN target until the Z80 sets one (empty: 0.0.0.0:0)\r\n"
    "\r\n"
    "[time]                  ; system time: RTC card, Unicard GETTIME, file times\r\n"
    "ntp=pool.ntp.org        ; off = no NTP\r\n"
    "tz=CET-1CEST,M3.5.0,M10.5.0/3   ; POSIX time zone (Czech time with summer time)\r\n"
    "\r\n"
    "; Emulated cards: [card.dpp], [card.ide], [pmd32], [card.dsm], [card.zrd], [card.rtc], [card.rpd],\r\n"
    ";                 [card.rtc1], [card.dsp] (docs/karty/)\r\n";

// ---------------------------------------------------------------- KOI-8cs2 (common/koi8.c)

// Unicode code points of C0h-FFh, 0 = undefined position in the character generator (src/core/charset.cpp)
static const uint16_t koi[64] = {
    0x0000, 0x00E1, 0x0000, 0x010D, 0x010F, 0x011B, 0x0155, 0x0000,     // C0  - á - č ď ě ŕ -
    0x00FC, 0x00ED, 0x016F, 0x013A, 0x013E, 0x00F6, 0x0148, 0x00F3,     // C8  ü í ů ĺ ľ ö ň ó
    0x00F4, 0x00E4, 0x0159, 0x0161, 0x0165, 0x00FA, 0x0000, 0x00E9,     // D0  ô ä ř š ť ú - é
    0x00E0, 0x00FD, 0x017E, 0x0000, 0x02C7, 0x0000, 0x00AC, 0x0000,     // D8  à ý ž - ˇ - ¬ -
    0x00B4, 0x00C1, 0x0000, 0x010C, 0x010E, 0x011A, 0x0154, 0x0000,     // E0  ´ Á - Č Ď Ě Ŕ -
    0x00DC, 0x00CD, 0x016E, 0x0139, 0x013D, 0x00D6, 0x0147, 0x00D3,     // E8  Ü Í Ů Ĺ Ľ Ö Ň Ó
    0x00D4, 0x00C4, 0x0158, 0x0160, 0x0164, 0x00DA, 0x0000, 0x00C9,     // F0  Ô Ä Ř Š Ť Ú - É
    0x00C0, 0x00DD, 0x017D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     // F8  À Ý Ž - - - - -
};

// ASCII form of U+00C0-U+017F: the base letter; '_' = none. Two-character forms in 'two'.
static const char base_c0[] = "AAAAAAACEEEEIIII" "DNOOOOOxOUUUUYTs" "aaaaaaaceeeeiiii" "dnooooo_ouuuuyty";
static const char base_100[] = "AaAaAaCcCcCcCcDd" "DdEeEeEeEeEeGgGg" "GgGgHhHhIiIiIiIi" "IiIiJjKkkLlLlLlL"
                               "lLlNnNnNnnNnOoOo" "OoOoRrRrRrSsSsSs" "SsTtTtTtUuUuUuUu" "UuUuWwYyYZzZzZzs";

static const struct { uint16_t cp; const char* ascii; } two[] = {
    { 0x00C6, "AE" }, { 0x00E6, "ae" }, { 0x00DF, "ss" }, { 0x00DE, "Th" }, { 0x00FE, "th" },
    { 0x0152, "OE" }, { 0x0153, "oe" }, { 0x0132, "IJ" }, { 0x0133, "ij" },
    { 0x2018, "'" }, { 0x2019, "'" }, { 0x201A, "'" }, { 0x201C, "'" }, { 0x201D, "'" }, { 0x201E, "'" },
    { 0x2013, "-" }, { 0x2014, "-" }, { 0x2026, "..." }, { 0x00A0, " " },
};

static size_t put_utf8(uint32_t cp, char* out, size_t room)
{
    if (cp < 0x80) {
        if (room < 1) return 0;
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        if (room < 2) return 0;
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (room < 3) return 0;
    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
}

size_t koi8_to_utf8(const uint8_t* in, char* out, size_t size)
{
    size_t n = 0;
    for (; *in && n + 1 < size; in++) {
        uint32_t cp = *in;
        if (cp >= 0xC0) cp = koi[cp - 0xC0];
        else if (cp >= 0x80) cp = 0;
        size_t k = put_utf8(cp ? cp : '_', out + n, size - 1 - n);
        if (k == 0) break;
        n += k;
    }
    out[n] = 0;
    return n;
}

// next code point of a UTF-8 string, advancing *s; malformed bytes give 0xFFFD
static uint32_t next_cp(const char** s)
{
    const uint8_t* p = (const uint8_t*)*s;
    uint32_t c = *p++;
    int extra = 0;
    if (c >= 0xF0) { c &= 0x07; extra = 3; }
    else if (c >= 0xE0) { c &= 0x0F; extra = 2; }
    else if (c >= 0xC0) { c &= 0x1F; extra = 1; }
    else if (c >= 0x80) { *s = (const char*)p; return 0xFFFD; }
    while (extra--) {
        if ((*p & 0xC0) != 0x80) { *s = (const char*)p; return 0xFFFD; }
        c = (c << 6) | (*p++ & 0x3F);
    }
    *s = (const char*)p;
    return c;
}

size_t utf8_to_koi8(const char* in, uint8_t* out, size_t size)
{
    size_t n = 0;
    while (*in && n + 1 < size) {
        uint32_t cp = next_cp(&in);
        if (cp >= 0x20 && cp < 0x7F) {
            out[n++] = (uint8_t)cp;
            continue;
        }
        int code = -1;                                  // 1. KOI-8cs2 has it
        for (int i = 0; i < 64; i++)
            if (koi[i] == cp) { code = 0xC0 + i; break; }
        if (code >= 0) {
            out[n++] = (uint8_t)code;
            continue;
        }
        const char* ascii = nullptr;                    // 2. a sensible ASCII form
        for (size_t i = 0; i < sizeof(two) / sizeof(two[0]); i++)
            if (two[i].cp == cp) { ascii = two[i].ascii; break; }
        char one[2] = { 0, 0 };
        if (!ascii && cp >= 0xC0 && cp < 0x100) one[0] = base_c0[cp - 0xC0];
        if (!ascii && cp >= 0x100 && cp < 0x180) one[0] = base_100[cp - 0x100];
        if (!ascii && one[0]) ascii = one;
        if (!ascii) ascii = "_";                        // 3. anything else
        for (; *ascii && n + 1 < size; ascii++) out[n++] = (uint8_t)*ascii;
    }
    out[n] = 0;
    return n;
}

static uint8_t fold(uint8_t c)
{
    if (c >= 'a' && c <= 'z') return (uint8_t)(c - 0x20);
    if (c >= 0xC0 && c <= 0xDF) return (uint8_t)(c + 0x20);    // Czech lower case C0-DF -> upper case E0-FF
    return c;
}

uint8_t koi8_plain(uint8_t c)
{
    if (c >= 'a' && c <= 'z') return (uint8_t)(c - 0x20);
    if (c < 0xC0) return c;
    uint16_t cp = koi[c - 0xC0];
    char b = 0;
    if (cp >= 0xC0 && cp < 0x100) b = base_c0[cp - 0xC0];
    else if (cp >= 0x100 && cp < 0x180) b = base_100[cp - 0x100];
    if (b >= 'a' && b <= 'z') b = (char)(b - 0x20);
    return b && b != '_' ? (uint8_t)b : c;      // not a letter (semigraphics, signs): itself
}

int koi8_nplaincmp(const uint8_t* a, const uint8_t* b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        uint8_t x = koi8_plain(a[i]), y = koi8_plain(b[i]);
        if (x != y) return x < y ? -1 : 1;
        if (x == 0) return 0;
    }
    return 0;
}

int koi8_ncasecmp(const uint8_t* a, const uint8_t* b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        uint8_t x = fold(a[i]), y = fold(b[i]);
        if (x != y) return x < y ? -1 : 1;
        if (x == 0) return 0;
    }
    return 0;
}

// ---------------------------------------------------------------- names as FatFs sees them

// upper case of a code point (FatFs ff_wtoupper for the Latin letters)
static uint32_t cp_upper(uint32_t c)
{
    if (c >= 'a' && c <= 'z') return c - 0x20;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 0x20;
    if (c == 0xFF) return 0x178;
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177)) return c & ~1u;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (c & 1) ? c : c - 1;
    return c;
}

static std::u32string upper_name(const std::string& utf8)
{
    std::u32string u;
    const char* s = utf8.c_str();
    while (*s) u.push_back(cp_upper(next_cp(&s)));
    return u;
}

// FatFs drops trailing spaces and dots of a name
static std::string strip_name(const std::string& n)
{
    size_t e = n.size();
    while (e && (n[e - 1] == ' ' || n[e - 1] == '.')) e--;
    return n.substr(0, e);
}

// a name FatFs takes for a long file name
static bool valid_name(const std::string& n)
{
    if (n.empty()) return false;
    for (unsigned char c : n)
        if (c < 0x20 || c == 0x7F || strchr("\"*:<>?|\\/", c)) return false;
    return true;
}

// Short name 8.3 as FatFs makes it (an approximation: CP437 letters with marks become '_' here, the ~n numbers
// follow the order of the listing, not of the creation). has_alt = false: the name is a plain upper case 8.3 name,
// FatFs has no other form of it (altname empty).
static std::string make_sfn(const std::string& name, const std::set<std::string>& taken, bool& has_alt)
{
    std::u32string u;
    const char* s = name.c_str();
    while (*s) u.push_back(next_cp(&s));
    size_t start = 0;
    while (start < u.size() && (u[start] == '.' || u[start] == ' ')) start++;
    size_t dot = u.rfind(U'.');
    if (dot == std::u32string::npos || dot < start) dot = u.size();
    std::string body, ext;
    bool lossy = start > 0, lower = false, upper = false;
    for (size_t i = start; i < u.size(); i++) {
        if (i == dot) continue;
        uint32_t c = u[i];
        if (c == ' ' || (c == '.' && i != dot)) { lossy = true; continue; }
        char o;
        if (c >= 0x80 || strchr("+,;=[]", (int)c)) { o = '_'; lossy = true; }
        else {
            if (c >= 'a' && c <= 'z') { lower = true; c -= 0x20; }
            else if (c >= 'A' && c <= 'Z') upper = true;
            o = (char)c;
        }
        if (i < dot) body += o;
        else ext += o;
    }
    if (body.size() > 8 || ext.size() > 3) lossy = true;
    if (ext.size() > 3) ext.resize(3);
    if (body.empty()) { body = "_"; lossy = true; }
    std::string sfn;
    if (!lossy) {
        sfn = body + (ext.empty() ? "" : "." + ext);
        has_alt = lower;                             // lower case: case flags or a long name, the SFN is shown
        (void)upper;
        return sfn;
    }
    has_alt = true;
    for (int n = 1; n < 1000000; n++) {
        std::string tail = "~" + std::to_string(n);
        std::string b = body.substr(0, std::min(body.size(), 8 - tail.size())) + tail;
        sfn = b + (ext.empty() ? "" : "." + ext);
        if (!taken.count(sfn)) break;
    }
    return sfn;
}

static void put32(uint8_t* b, uint32_t v)
{
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16);
    b[3] = (uint8_t)(v >> 24);
}

static bool ci_equal(const char* a, const char* b)
{
    for (; *a && *b; a++, b++)
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return false;
    return *a == *b;
}

static std::string join(const std::string& dir, const std::string& name)
{
    return dir == "/" ? "/" + name : dir + "/" + name;
}

static std::string parent_of(const std::string& path)
{
    size_t s = path.rfind('/');
    return s == 0 ? std::string("/") : path.substr(0, s);
}

static std::string name_of(const std::string& path)
{
    return path.substr(path.rfind('/') + 1);
}

static fs::path u8p(const std::string& s)
{
    return fs::path(std::u8string(s.begin(), s.end()));
}

static std::string p8(const fs::path& p)
{
    std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

// FAT date and time of a host file time (local time)
static void fat_time(fs::file_time_type t, uint16_t& fdate, uint16_t& ftime)
{
    time_t tt = std::chrono::system_clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(std::chrono::clock_cast<std::chrono::system_clock>(t)));
    struct tm lt;
#ifdef _WIN32
    if (localtime_s(&lt, &tt) != 0) memset(&lt, 0, sizeof(lt));
#else
    if (!localtime_r(&tt, &lt)) memset(&lt, 0, sizeof(lt));
#endif
    int y = lt.tm_year + 1900;
    if (y < 1980) { fdate = (0 << 9) | (1 << 5) | 1; ftime = 0; return; }
    if (y > 2107) y = 2107;
    fdate = (uint16_t)(((y - 1980) << 9) | ((lt.tm_mon + 1) << 5) | lt.tm_mday);
    ftime = (uint16_t)((lt.tm_hour << 11) | (lt.tm_min << 5) | (lt.tm_sec / 2));
}

// ---------------------------------------------------------------- the card

bool DpiSd::init(const std::string& host_root, uint32_t mb, std::string& err)
{
    root = host_root;
    size_mb = mb ? mb : 1;
    std::error_code ec;
    fs::create_directories(u8p(root) / "DPI", ec);
    if (!fs::is_directory(u8p(root) / "DPI", ec)) {
        err = "DPI-1L: cannot create the SD card folder " + root;
        return false;
    }
    std::string ini = host(CONFIG_PATH);
    if (!file_exists(ini)) file_write_all(ini, default_ini, sizeof(default_ini) - 1);   // as create_default()
    power_on();
    return true;
}

void DpiSd::power_on(void)
{
    file.open = false;
    file.valid = file.dirty = false;
    dir_open = cfg_stream = false;
    dir_list.clear();
    cwd = "/";
}

std::string DpiSd::host(const std::string& path) const
{
    if (path == "/") return root;
    return p8(u8p(root) / u8p(path.substr(1)));
}

uint32_t DpiSd::cluster_bytes(void) const
{
    if (size_mb <= 128) return 2048;
    if (size_mb <= 32768) return 32768;
    return 131072;
}

uint32_t DpiSd::total_clusters(void) const
{
    uint64_t c = (uint64_t)size_mb * 1024 * 1024 / cluster_bytes();
    return (uint32_t)(c - c / 100);             // about 1 % for the FAT and the root
}

uint64_t DpiSd::used_clusters(const std::string& path)
{
    uint64_t n = 0;
    uint32_t cl = cluster_bytes();
    std::error_code ec;
    for (fs::recursive_directory_iterator it(u8p(host(path)), ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_directory(ec)) n++;
        else n += (it->file_size(ec) + cl - 1) / cl;
    }
    return n;
}

void DpiSd::fatfs(DpiSdResult& res, uint8_t fr)
{
    if ((fr == DFR_NO_FILE || fr == DFR_NO_PATH) && ambiguous) fr = DFR_INVALID_NAME;   // several fit
    res.ff_res = fr;
    if (fr != DFR_OK) res.err_code = REPO_ERR_FATFS;
}

// entries of a directory as FatFs lists them (without "." and ".."), short names made in this order
bool DpiSd::list(const std::string& path, std::vector<DpiEntry>& out)
{
    out.clear();
    std::error_code ec;
    fs::directory_iterator it(u8p(host(path)), ec), end;
    if (ec) return false;
    for (; !ec && it != end; it.increment(ec)) {
        DpiEntry e;
        e.name = p8(it->path().filename());
        if (e.name.empty() || !valid_name(e.name)) continue;   // nothing FatFs could hold
        e.dir = it->is_directory(ec);
        e.size = e.dir ? 0 : (uint32_t)std::min<uint64_t>(it->file_size(ec), 0xFFFFFFFFu);
        fat_time(it->last_write_time(ec), e.fdate, e.ftime);
        e.attr = e.dir ? 0x10 : 0x20;
        fs::perms pm = it->status(ec).permissions();
        if ((pm & fs::perms::owner_write) == fs::perms::none) e.attr |= 0x01;
        out.push_back(e);
    }
    std::sort(out.begin(), out.end(), [](const DpiEntry& a, const DpiEntry& b) {
        return upper_name(a.name) < upper_name(b.name);
    });
    std::set<std::string> taken;
    for (DpiEntry& e : out) {
        bool alt;
        std::string s = make_sfn(e.name, taken, alt);
        taken.insert(s);
        e.sfn = alt ? s : std::string();
    }
    return true;
}

void DpiSd::fix_entry_size(DpiEntry& e, const std::string& path)
{
    if (file.open && !e.dir && ci_equal(file.path.c_str(), path.c_str())) e.size = file.entry_size;
}

// Follow a FatFs path: DFR_OK (e and real filled; the root is a directory with an empty name), DFR_NO_FILE (the
// last name is missing), DFR_NO_PATH (a directory on the way is missing or a file), DFR_INVALID_NAME.
int DpiSd::walk(const std::string& path, DpiEntry* e, std::string* real)
{
    std::string cur = "/";
    if (e) *e = DpiEntry();
    if (e) e->dir = true;
    size_t p = 1;
    while (path != "/" && p <= path.size()) {
        size_t q = path.find('/', p);
        if (q == std::string::npos) q = path.size();
        std::string comp = strip_name(path.substr(p, q - p));
        p = q + 1;
        if (!valid_name(comp)) return DFR_INVALID_NAME;
        bool last = q >= path.size();
        std::vector<DpiEntry> ents;
        if (!list(cur, ents)) return DFR_NO_PATH;
        std::u32string want = upper_name(comp);
        std::string upper_sfn;
        for (char c : comp) upper_sfn += (char)toupper((unsigned char)c);
        const DpiEntry* hit = nullptr;
        for (const DpiEntry& x : ents)
            if (upper_name(x.name) == want || (!x.sfn.empty() && x.sfn == upper_sfn)) { hit = &x; break; }
        if (!hit) return last ? DFR_NO_FILE : DFR_NO_PATH;
        cur = join(cur, hit->name);
        if (last) {
            if (e) {
                *e = *hit;
                fix_entry_size(*e, cur);
            }
            break;
        }
        if (!hit->dir) return DFR_NO_PATH;
    }
    if (real) *real = cur;
    return DFR_OK;
}

// The real name of one component in `dir` (common/repo_fs.c look_up): the exact name first (FatFs, any case or the
// short name), then an entry whose KOI-8cs2 form equals `koi` without regard to case (also cut to 31 bytes), at
// last for a plain ASCII name one that equals it also without the diacritics, if a single entry fits.
bool DpiSd::look_up(const std::string& dir, const std::string& utf8, const uint8_t* koi, std::string& real)
{
    std::string r;
    if (walk(join(dir, utf8), nullptr, &r) == DFR_OK) {
        real = name_of(r);
        return true;
    }
    std::vector<DpiEntry> ents;
    if (!list(dir, ents)) return false;
    size_t klen = strlen((const char*)koi);
    uint8_t k[256];
    for (const DpiEntry& x : ents) {
        size_t n = utf8_to_koi8(x.name.c_str(), k, sizeof(k));
        if ((n == klen && koi8_ncasecmp(k, koi, n) == 0) ||
            (n > NAME_FIELD - 1 && klen == NAME_FIELD - 1 && koi8_ncasecmp(k, koi, NAME_FIELD - 1) == 0)) {
            real = x.name;
            return true;
        }
    }
    for (size_t i = 0; i < klen; i++)
        if (koi[i] >= 0x80) return false;      // a name with marks is meant as it is
    int fits = 0;
    for (const DpiEntry& x : ents) {
        size_t n = utf8_to_koi8(x.name.c_str(), k, sizeof(k));
        if ((n == klen && koi8_nplaincmp(k, koi, n) == 0) ||
            (n > NAME_FIELD - 1 && klen == NAME_FIELD - 1 && koi8_nplaincmp(k, koi, NAME_FIELD - 1) == 0)) {
            if (!fits++) real = x.name;
        }
    }
    if (fits > 1) ambiguous = true;
    return fits == 1;
}

// KOI-8cs2 path from the Z80 -> absolute FatFs path with the real names ('.' and '..' resolved, a component that
// does not exist kept as given)
bool DpiSd::resolve(const uint8_t* koi_path, std::string& out)
{
    ambiguous = false;
    const uint8_t* p = koi_path;
    if (p[0] == '0' && p[1] == ':') p += 2;     // "0:" drive prefix
    out = (*p == '/' || *p == '\\') ? std::string("/") : cwd;
    while (*p) {
        while (*p == '/' || *p == '\\') p++;
        if (!*p) break;
        uint8_t comp[256];
        size_t n = 0;
        while (*p && *p != '/' && *p != '\\' && n < sizeof(comp) - 1) comp[n++] = *p++;
        comp[n] = 0;
        if (!strcmp((char*)comp, ".")) continue;
        if (!strcmp((char*)comp, "..")) {
            out = parent_of(out);              // at the root ".." stays at the root
            if (out.empty()) out = "/";
            continue;
        }
        char utf8[768];
        koi8_to_utf8(comp, utf8, sizeof(utf8));
        std::string real;
        if (!look_up(out, utf8, comp, real)) real = utf8;
        if (out.size() + 1 + real.size() + 1 > 512) return false;
        out = join(out, real);
    }
    return true;
}

static bool is_secret(const std::string& path)
{
    return ci_equal(path.c_str(), SECRET_PATH) || ci_equal(path.c_str(), OLD_SECRET_PATH);
}

static bool is_protected(const std::string& path)
{
    return is_secret(path) || ci_equal(path.c_str(), DPI_DIR);
}

// 55-byte FILINFO record (docs/unicard.md 6)
static uint16_t record(const DpiEntry& f, uint8_t* b)
{
    memset(b, 0, 55);
    put32(b, f.size);
    b[4] = (uint8_t)f.fdate;
    b[5] = (uint8_t)(f.fdate >> 8);
    b[6] = (uint8_t)f.ftime;
    b[7] = (uint8_t)(f.ftime >> 8);
    b[8] = f.attr;
    utf8_to_koi8(f.sfn.c_str(), b + 9, 13);
    uint8_t name[256];
    size_t n = utf8_to_koi8(f.name.c_str(), name, sizeof(name));
    if (n > NAME_FIELD - 1) n = NAME_FIELD - 1;
    memcpy(b + 23, name, n);
    b[22] = (uint8_t)n;
    return 55;
}

// FILELIST text record: name['/'] 0Dh size 0Dh
static uint16_t text_record(const DpiEntry& f, uint8_t* b)
{
    size_t n = utf8_to_koi8(f.name.c_str(), b, 200);
    if (f.dir) b[n++] = '/';
    b[n++] = 0x0D;
    n += (size_t)snprintf((char*)b + n, 20, "%lu", f.dir ? 0ul : (unsigned long)f.size);
    b[n++] = 0x0D;
    return (uint16_t)n;
}

// ---- the open file ----

uint8_t DpiSd::flush_page(void)
{
    if (!file.open || !file.dirty) return DFR_OK;
    file.dirty = false;
    std::string h = host(file.path);
    uint64_t old = 0;
    int64_t mt;
    file_stat(h, old, mt);
    uint64_t end = (uint64_t)file.page_pos + file.page_len;
    uint32_t len = file.page_len;
    uint8_t fr = DFR_OK;
    if (end > old) {                            // the file grows: the card may be full
        uint32_t cl = cluster_bytes();
        uint64_t have = (old + cl - 1) / cl, need = (end + cl - 1) / cl;
        uint64_t used = used_clusters("/"), total = total_clusters();
        uint64_t free_cl = used < total ? total - used : 0;
        if (need - have > free_cl) {
            uint64_t cap = (have + free_cl) * cl;
            len = cap > file.page_pos ? (uint32_t)std::min<uint64_t>(len, cap - file.page_pos) : 0;
            fr = DFR_DENIED;                    // f_write wrote less than asked: card full
        }
    }
    FILE* f = file_open(h, "r+b");
    if (!f) return DFR_DISK_ERR;
    if (len && (fseek(f, (long)file.page_pos, SEEK_SET) != 0 || fwrite(file.page, 1, len, f) != len)) fr = DFR_DISK_ERR;
    fclose(f);
    return fr;
}

// the page with `pos`: bytes of the file, zeros after its end (a write there extends the file)
uint8_t DpiSd::load_page(uint32_t pos)
{
    uint8_t fr = flush_page();
    if (fr != DFR_OK) return fr;
    file.valid = false;
    file.page_pos = pos & ~(DPI_PAGE - 1u);
    uint64_t size = 0;
    int64_t mt;
    std::string h = host(file.path);
    if (!file_stat(h, size, mt)) return DFR_DISK_ERR;
    size_t br = 0;
    if (file.page_pos < size) {
        FILE* f = file_open(h, "rb");
        if (!f) return DFR_DISK_ERR;
        if (fseek(f, (long)file.page_pos, SEEK_SET) == 0) br = fread(file.page, 1, DPI_PAGE, f);
        fclose(f);
    }
    memset(file.page + br, 0, DPI_PAGE - br);
    file.page_len = (uint32_t)br;
    file.valid = true;
    return DFR_OK;
}

uint8_t DpiSd::close_file(void)
{
    if (!file.open) return DFR_OK;
    uint8_t fr = flush_page();
    file.open = false;
    file.valid = false;
    file.dirty = false;
    return fr;
}

void DpiSd::close_dir(void)
{
    dir_open = false;
    dir_list.clear();
    cfg_stream = false;
}

void DpiSd::dir_next(uint8_t dir_cmd, DpiSdResult& res)
{
    while (dir_open) {
        if (dir_index >= dir_list.size()) {     // end of the directory: closed, no error
            close_dir();
            return;
        }
        const DpiEntry& e = dir_list[dir_index++];
        if (dir_is_dpi && ci_equal(e.name.c_str(), "SECRET.INI")) continue;
        if (dir_is_root && ci_equal(e.name.c_str(), "DPI.INI")) continue;
        res.rec_len = dir_cmd == 0x42 ? text_record(e, res.data) : record(e, res.data);
        return;
    }
}

uint8_t DpiSd::do_open(uint8_t mode, const std::string& path)
{
    uint8_t fa = mode & 0x1F;
    if (fa & 0x02) fa |= 0x01;                  // pages are read before they are written
    if (path == "/") return DFR_INVALID_NAME;
    DpiEntry e;
    std::string real;
    int w = walk(path, &e, &real);
    if (w != DFR_OK && w != DFR_NO_FILE) return (uint8_t)w;
    bool create = (fa & (0x04 | 0x08 | 0x10)) != 0;
    std::string h;
    if (create) {
        if (w == DFR_NO_FILE) {                 // a new file in the directory that is there
            std::string dir;
            if (walk(parent_of(path), nullptr, &dir) != DFR_OK) return DFR_NO_PATH;
            std::string name = strip_name(name_of(path));
            if (!valid_name(name)) return DFR_INVALID_NAME;
            real = join(dir, name);
            FILE* f = file_open(host(real), "wb");
            if (!f) return DFR_DENIED;
            fclose(f);
            e = DpiEntry();
        } else {
            if (e.attr & (0x01 | 0x10)) return DFR_DENIED;
            if (fa & 0x04) return DFR_EXIST;
            if (fa & 0x08) {                    // CREATE_ALWAYS: emptied
                FILE* f = file_open(host(real), "wb");
                if (!f) return DFR_DENIED;
                fclose(f);
                e.size = 0;
            }
        }
    } else {
        if (w != DFR_OK) return (uint8_t)w;
        if (e.dir) return DFR_NO_FILE;
        if ((fa & 0x02) && (e.attr & 0x01)) return DFR_DENIED;
    }
    uint64_t size = 0;
    int64_t mt;
    file_stat(host(real), size, mt);
    file.open = true;
    file.mode = mode;
    file.path = real;
    file.size = (uint32_t)size;
    file.entry_size = (uint32_t)size;
    file.pos = 0;
    file.valid = false;
    file.dirty = false;
    file.page_len = 0;
    return DFR_OK;
}

uint8_t DpiSd::do_unlink(const std::string& path)
{
    if (is_secret(path)) return DFR_NO_FILE;    // a secret file does not exist for the Z80
    if (is_protected(path)) return DFR_DENIED;
    if (path == "/") return DFR_INVALID_NAME;
    DpiEntry e;
    std::string real;
    int w = walk(path, &e, &real);
    if (w != DFR_OK) return (uint8_t)w;
    if (file.open && ci_equal(file.path.c_str(), real.c_str())) return DFR_LOCKED;
    if (e.attr & 0x01) return DFR_DENIED;
    std::error_code ec;
    if (e.dir) {
        if (ci_equal(cwd.c_str(), real.c_str())) return DFR_DENIED;
        if (!fs::is_empty(u8p(host(real)), ec)) return DFR_DENIED;
    }
    if (!fs::remove(u8p(host(real)), ec)) return DFR_DENIED;
    return DFR_OK;
}

uint8_t DpiSd::do_rename(const std::string& from, const std::string& to)
{
    if (is_secret(from)) return DFR_NO_FILE;
    if (is_protected(from) || is_secret(to)) return DFR_DENIED;
    if (from == "/") return DFR_INVALID_NAME;
    std::string old_real, new_real;
    int w = walk(from, nullptr, &old_real);
    if (w != DFR_OK) return (uint8_t)w;
    if (file.open && ci_equal(file.path.c_str(), old_real.c_str())) return DFR_LOCKED;
    w = walk(to, nullptr, &new_real);
    if (w == DFR_OK && !ci_equal(new_real.c_str(), old_real.c_str())) return DFR_EXIST;
    if (w != DFR_OK && w != DFR_NO_FILE) return (uint8_t)w;
    std::string dir;
    if (walk(parent_of(to), nullptr, &dir) != DFR_OK) return DFR_NO_PATH;
    std::string name = strip_name(name_of(to));
    if (!valid_name(name)) return DFR_INVALID_NAME;
    std::error_code ec;
    fs::rename(u8p(host(old_real)), u8p(host(join(dir, name))), ec);
    return ec ? DFR_DENIED : DFR_OK;
}

uint8_t DpiSd::do_mkdir(const std::string& path)
{
    int w = walk(path, nullptr, nullptr);
    if (w == DFR_OK) return DFR_EXIST;
    if (w != DFR_NO_FILE) return (uint8_t)w;
    std::string dir;
    if (walk(parent_of(path), nullptr, &dir) != DFR_OK) return DFR_NO_PATH;
    std::string name = strip_name(name_of(path));
    if (!valid_name(name)) return DFR_INVALID_NAME;
    if (used_clusters("/") + 1 > total_clusters()) return DFR_DENIED;
    std::error_code ec;
    if (!fs::create_directory(u8p(host(join(dir, name))), ec)) return DFR_DENIED;
    return DFR_OK;
}

// ---- /DPI/DPI.INI (common/ini.c) ----

static bool section_line(const char* p, const char* section)
{
    p += strspn(p, " \t");
    if (*p != '[') return false;
    const char* end = strchr(p, ']');
    size_t sl = strlen(section);
    if (!end || (size_t)(end - p - 1) != sl) return false;
    for (size_t i = 0; i < sl; i++)
        if (toupper((unsigned char)p[1 + i]) != toupper((unsigned char)section[i])) return false;
    return true;
}

static bool key_line(const char* p, const char* key)
{
    p += strspn(p, " \t");
    size_t kl = strlen(key);
    for (size_t i = 0; i < kl; i++)
        if (toupper((unsigned char)p[i]) != toupper((unsigned char)key[i])) return false;
    p += kl;
    p += strspn(p, " \t");
    return *p == '=';
}

static void strip_comment(char* v)
{
    for (char* p = v; *p; p++) {
        if ((*p == ';' || *p == '#') && (p == v || p[-1] == ' ' || p[-1] == '\t')) {
            *p = '\0';
            break;
        }
    }
    size_t n = strlen(v);
    while (n && (v[n - 1] == ' ' || v[n - 1] == '\t')) v[--n] = '\0';
}

// the lines of a file as f_gets with a buffer of 160 gives them (a longer line comes in parts)
static std::vector<std::string> ini_lines(const std::string& text)
{
    std::vector<std::string> out;
    size_t p = 0;
    while (p < text.size()) {
        size_t nl = text.find('\n', p);
        size_t end = nl == std::string::npos ? text.size() : nl + 1;
        if (end - p > 159) end = p + 159;
        out.push_back(text.substr(p, end - p));
        p = end;
    }
    return out;
}

void DpiSd::cfg_next(DpiSdResult& res)
{
    std::string text;
    cfg_stream = false;
    res.rec_len = 0;
    if (!file_read_text(host(CONFIG_PATH), text)) return;
    unsigned n = cfg_index++;
    bool in_section = false;
    for (std::string l : ini_lines(text)) {
        char line[160];
        snprintf(line, sizeof(line), "%s", l.c_str());
        char* p = line + strspn(line, " \t");
        p[strcspn(p, "\r\n")] = '\0';
        if (*p == ';' || *p == '#' || *p == '\0') continue;
        if (*p == '[') {
            in_section = section_line(p, cfg_section.c_str());
            continue;
        }
        char* eq = strchr(p, '=');
        if (!in_section || !eq || n--) continue;
        char* k_end = eq;
        while (k_end > p && (k_end[-1] == ' ' || k_end[-1] == '\t')) k_end--;
        *k_end = '\0';
        char* v = eq + 1 + strspn(eq + 1, " \t");
        strip_comment(v);
        memset(res.data, 0, 80);
        memcpy(res.data, p, std::min<size_t>(strlen(p), 15));
        memcpy(res.data + 16, v, std::min<size_t>(strlen(v), 63));
        cfg_stream = true;
        res.rec_len = 80;
        return;
    }
}

// SETCONFIG (common/config.c repo_config_set and ini.c ini_set): an empty value deletes the key
uint8_t DpiSd::config_set(const char* section, const char* key, const char* value)
{
    if (!*section || !*key || strpbrk(section, "[]\r\n") || strpbrk(key, "=;#[\r\n") || strpbrk(value, "\r\n") ||
        key[0] == ' ' || strlen(section) > 32 || strlen(key) > 15 || strlen(value) > 63)
        return DFR_INVALID_NAME;
    std::string path = host(CONFIG_PATH), in;
    uint64_t sz = 0;
    int64_t mt;
    if (file_stat(path, sz, mt)) {
        if (sz > INI_MAX_SIZE) return DFR_DENIED;
        std::vector<uint8_t> b;
        if (!file_read_all(path, b)) return DFR_DISK_ERR;
        in.assign(b.begin(), b.end());
    }
    std::string out;
    size_t kl = strlen(key), vl = strlen(value);
    bool in_sec = false, done = false, seen = false;
    size_t held = 0;
    auto emit_kv = [&]() {
        if (vl) out += std::string(key) + "=" + value + "\r\n";
        done = true;
    };
    auto flush = [&]() {
        while (held) { out += "\r\n"; held--; }
    };
    size_t p = 0;
    while (p < in.size()) {
        size_t nl = in.find('\n', p);
        size_t len = nl == std::string::npos ? in.size() - p : nl - p + 1;
        std::string ln = in.substr(p, len);
        const char* t = ln.c_str() + strspn(ln.c_str(), " \t");
        if (*t == '[') {
            if (in_sec && !done) emit_kv();
            flush();
            in_sec = section_line(t, section);
            seen |= in_sec;
            out += ln;
        } else if (in_sec && (*t == '\r' || *t == '\n' || *t == '\0')) {
            held++;
        } else if (in_sec && !done && key_line(t, key)) {
            flush();
            const char* c = strchr(t, '=') + 1;
            const char* end = ln.c_str() + len;
            while (c < end && !((*c == ';' || *c == '#') && (c[-1] == ' ' || c[-1] == '\t'))) c++;
            if (vl && c < end) {               // an inline comment stays in its column
                size_t col = (size_t)(c - ln.c_str()), w = kl + 1 + vl;
                size_t clen = (size_t)(end - c);
                while (clen && (c[clen - 1] == '\r' || c[clen - 1] == '\n')) clen--;
                out += std::string(key) + "=" + value;
                do { out += " "; } while (++w < col);
                out.append(c, clen);
                out += "\r\n";
                done = true;
            } else {
                emit_kv();
            }
        } else {
            flush();
            out += ln;
        }
        p += len;
    }
    if (in_sec && !done) {
        if (!out.empty() && out.back() != '\n') out += "\r\n";
        emit_kv();
    }
    flush();
    if (!seen && vl) {
        if (!out.empty() && out.back() != '\n') out += "\r\n";
        if (!out.empty()) out += "\r\n";
        out += std::string("[") + section + "]\r\n";
        emit_kv();
    }
    if (out.size() > INI_MAX_SIZE + 256) return DFR_DENIED;
    if (!file_write_all(path, out.data(), out.size())) return DFR_DENIED;
    return DFR_OK;
}

// ---- the commands (repo_fs_execute) ----

void DpiSd::execute(uint8_t cmd, uint8_t dir_cmd, const uint8_t* param, DpiSdResult& res)
{
    res = DpiSdResult();
    std::string path, path2;
    const uint8_t* s1 = param;
    uint8_t fr;
    switch (cmd) {
    case 0x00:                                          // RESET
        close_dir();
        fr = close_file();
        cwd = "/";
        fatfs(res, fr);
        break;

    case 0x20: {                                        // GETFREE: total and free sectors
        uint32_t spc = cluster_bytes() / 512;
        uint64_t total = total_clusters(), used = used_clusters("/");
        put32(res.data, (uint32_t)(total * spc));
        put32(res.data + 4, (uint32_t)((used < total ? total - used : 0) * spc));
        res.out_len = 8;
        break;
    }

    case 0x21: {                                        // CHDIR
        if (!resolve(s1, path)) { res.err_code = REPO_ERR_BAD_PARAM; break; }
        DpiEntry e;
        std::string real;
        int w = walk(path, &e, &real);
        if (w == DFR_NO_FILE || (w == DFR_OK && !e.dir)) w = DFR_NO_PATH;
        if (w == DFR_OK) cwd = real;
        fatfs(res, (uint8_t)w);
        break;
    }

    case 0x22: {                                        // GETCWD: "/A/B" 0Dh
        size_t n = utf8_to_koi8(cwd.c_str(), res.data, 254);
        res.data[n++] = 0x0D;
        res.out_len = (uint16_t)n;
        break;
    }

    case 0x30: {                                        // STAT
        if (!resolve(s1, path)) { res.err_code = REPO_ERR_BAD_PARAM; break; }
        DpiEntry e;
        int w = is_secret(path) ? DFR_NO_FILE : path == "/" ? DFR_INVALID_NAME : walk(path, &e, nullptr);
        fatfs(res, (uint8_t)w);
        if (w == DFR_OK) res.out_len = record(e, res.data);
        break;
    }

    case 0x31:                                          // UNLINK
        if (!resolve(s1, path)) { res.err_code = REPO_ERR_BAD_PARAM; break; }
        fatfs(res, do_unlink(path));
        break;

    case 0x32:                                          // CHMOD and UTIME: FF_USE_CHMOD 0 on the DPI
    case 0x33:
        res.err_code = REPO_ERR_NOT_IMPLEMENTED;
        break;

    case 0x34: {                                        // RENAME: old, new
        const uint8_t* s2 = s1 + strlen((const char*)s1) + 1;
        if (!resolve(s1, path) || !resolve(s2, path2)) { res.err_code = REPO_ERR_BAD_PARAM; break; }
        fatfs(res, do_rename(path, path2));
        break;
    }

    case 0x40:                                          // MKDIR
        if (!resolve(s1, path)) { res.err_code = REPO_ERR_BAD_PARAM; break; }
        fatfs(res, do_mkdir(path));
        break;

    case 0x41:                                          // READDIR
    case 0x42: {                                        // FILELIST
        close_dir();
        close_file();                                   // as the Unicard: a directory closes the file
        if (!resolve(s1[0] ? s1 : (const uint8_t*)".", path)) { res.err_code = REPO_ERR_BAD_PARAM; break; }
        DpiEntry e;
        std::string real;
        int w = walk(path, &e, &real);
        if (w == DFR_NO_FILE || (w == DFR_OK && !e.dir)) w = DFR_NO_PATH;
        if (w == DFR_OK && !list(real, dir_list)) w = DFR_NO_PATH;
        fatfs(res, (uint8_t)w);
        if (w == DFR_OK) {
            dir_open = true;
            dir_path = real;
            dir_index = 0;
            dir_is_dpi = ci_equal(real.c_str(), DPI_DIR);
            dir_is_root = real == "/";
            dir_next(cmd, res);
        }
        break;
    }

    case 0x43:                                          // NEXT: skip the rest of the record
    case 0xF2:                                          // the record was read to its end
        if (cfg_stream) {
            cfg_next(res);
            break;
        }
        if (!dir_open) {
            if (cmd == 0x43) res.err_code = REPO_ERR_BAD_PARAM;
            break;
        }
        dir_next(dir_cmd, res);
        break;

    case 0x92:                                          // GETCONFIG: section -> records of 80 B
        close_dir();
        cfg_section.assign((const char*)s1, strnlen((const char*)s1, 32));
        cfg_index = 0;
        cfg_next(res);                                  // a missing section: no record, no error
        break;

    case 0x99: {                                        // SETCONFIG: section, key, value
        const char* key = (const char*)s1 + strlen((const char*)s1) + 1;
        const char* value = key + strlen(key) + 1;
        fr = config_set((const char*)s1, key, value);
        if (fr == DFR_INVALID_NAME) res.err_code = REPO_ERR_BAD_PARAM;
        else fatfs(res, fr);
        break;
    }

    case 0x50: {                                        // OPEN: mode, path
        close_dir();
        fr = close_file();                              // the previous file is written back first
        if (fr != DFR_OK) { fatfs(res, fr); break; }
        if (!resolve(param + 1, path)) { res.err_code = REPO_ERR_BAD_PARAM; break; }
        uint8_t mode = param[0] & (uint8_t)~0x20;       // 20h = Sharp ASCII conversion, not on the DPI
        if (is_secret(path)) {                          // reading "does not exist", writing denied
            fatfs(res, (mode & (0x02 | 0x04 | 0x08 | 0x10)) ? DFR_DENIED : DFR_NO_FILE);
            break;
        }
        fatfs(res, do_open(mode, path));
        break;
    }

    case 0x52: {                                        // TRUNC: at the position
        fr = flush_page();
        if (fr == DFR_OK && !(file.mode & 0x02)) fr = DFR_DENIED;
        if (fr == DFR_OK) {
            std::error_code ec;
            fs::resize_file(u8p(host(file.path)), file.pos, ec);   // shorter, or longer (f_lseek in write mode)
            if (ec) fr = DFR_DISK_ERR;
        }
        fatfs(res, fr);
        if (fr == DFR_OK) {
            file.size = file.pos;
            if (file.pos <= file.page_pos) file.valid = false;
            else if (file.page_len > file.pos - file.page_pos) file.page_len = file.pos - file.page_pos;
        }
        break;
    }

    case 0x53:                                          // SYNC
        fr = flush_page();
        if (fr == DFR_OK) file.entry_size = file.size;
        fatfs(res, fr);
        break;

    case 0x54:                                          // CLOSE (no file: OK)
        fatfs(res, close_file());
        break;

    case 0xB0: {                                        // SDCARD: the emulator has no USB to lend the card to
        uint8_t p = param[0];
        if (p == 1) {
            close_dir();
            fr = close_file();
            if (fr != DFR_OK) { fatfs(res, fr); break; }
            res.err_code = REPO_ERR_BUSY;               // as the DPI without a USB connection
            break;
        }
        if (p != 0 && p != 0xFF) { res.err_code = REPO_ERR_BAD_PARAM; break; }
        res.data[0] = 0;                                // the card is in the DPI
        res.out_len = 1;
        break;
    }

    case 0xF3:                                          // the page with pos
        fatfs(res, load_page(file.pos));
        break;

    default:
        res.err_code = REPO_ERR_NOT_IMPLEMENTED;
    }
}

void DpiSd::state(StateIO& io)
{
    io.v(cwd);
    io.v(file.open);
    io.v(file.mode);
    io.v(file.pos);
    io.v(file.size);
    io.v(file.valid);
    io.v(file.dirty);
    io.v(file.page_pos);
    io.v(file.page_len);
    io.arr(file.page, sizeof(file.page));
    io.v(file.path);
    io.v(file.entry_size);
    io.v(dir_open);
    io.v(dir_is_dpi);
    io.v(dir_is_root);
    io.v(dir_path);
    io.v(dir_index);
    io.v(cfg_stream);
    io.v(cfg_section);
    io.v(cfg_index);
    if (io.loading && dir_open && !list(dir_path, dir_list)) dir_open = false;   // the folder as it is now
}
