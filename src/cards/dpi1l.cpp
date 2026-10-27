#include "cards/dpi1l.h"
#include "core/log.h"
#include "core/relay.h"
#include "core/bus.h"
#include "core/state.h"
#include "core/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <chrono>
#include <vector>

using nlohmann::json;

// Repository API (SAPI_DPI-firmware common/repo.c, common/repo.h)
enum { PH_DONE, PH_PARAMRQ, PH_DOUTRQ, PH_PROGRESS };   // PROGRESS: NET CREATE/JOIN, status bit 6

enum {
    ST_PARAMRQ = 0x01, ST_DOUTRQ = 0x02, ST_DIR = 0x04, ST_FILE_READ = 0x08, ST_FILE_WRITE = 0x10, ST_EOF = 0x20,
    ST_IN_PROGRESS = 0x40, ST_ERROR = 0x80,
};
enum { ERR_NOT_IMPLEMENTED = 1, ERR_BAD_PARAM = 2, ERR_OVERFLOW = 3, ERR_NO_FILE = 4, ERR_FATFS = 6 };

// the version of the DPI firmware the card answers as (REV, REVD): 0.5.2 since 2026-10-27 as the real card (REVD
// 00 05 since 0.5.0 of 2026-10-25; 0.5.1 since 2026-10-26, 0.4.2 since 2026-10-22 evening, 0.4.1 that morning, 0.4.0 since 2026-10-20); the Repository API and NET
// are the same as in 0.2, the model below
static const uint8_t VERSION_MAJOR = 0, VERSION_MINOR = 5, VERSION_PATCH = 2;
static const uint8_t BOARD = 0x01;                           // REVD/INFO board byte: bit 0 = DPI-1L
static const uint8_t WIFI_CONNECTED = 3;                     // WIFISTATUS (MZPico numbering)
static const uint8_t FLASH_MB = 4;                           // INFO byte 2 (RP2350 board of DPI-1L)

// NET (common/netgame.c)
static const uint8_t S_NOLINK = 0, S_READY = 1, S_IN_ROOM = 2, S_RUNNING = 3, S_DESYNC = 4, S_DROPPED = 5,
                     S_SPECTATOR = 6;
static const uint8_t E_ROOM = 7, E_NOROOM = 8, E_NOLINK = 9, E_PARAM = 10, E_FULL = 11;   // 6 = build (from the relay)
static const int WINDOW = 128;
static const int MSGQ = 4;

// commands the SD card side (repo_fs.c) or NET runs, with their parameter formats (repo.c fs_cmds):
// B = byte, S = string ended by any byte < 20h, X = TCPSEND: WORD length (bit 15 = push), then the data
static const char* fs_format(uint8_t c)
{
    switch (c) {
    case 0x00: return "";                         // RESET: close everything, CWD = /
    case 0x20: return "";                         // GETFREE
    case 0x21: return "S";                        // CHDIR
    case 0x22: return "";                         // GETCWD
    case 0x30: return "S";                        // STAT
    case 0x31: return "S";                        // UNLINK
    case 0x32: return "BBS";                      // CHMOD
    case 0x33: return "BBBBBBS";                  // UTIME
    case 0x34: return "SS";                       // RENAME
    case 0x40: return "S";                        // MKDIR
    case 0x41: return "S";                        // READDIR
    case 0x42: return "S";                        // FILELIST
    case 0x43: return "";                         // NEXT
    case 0x50: return "BS";                       // OPEN: mode, path
    case 0x52: return "";                         // TRUNC
    case 0x53: return "";                         // SYNC
    case 0x54: return "";                         // CLOSE
    case 0x80: return "";                         // TCPSTATUS
    case 0x81: return "BBBB";                     // TCPIP
    case 0x82: return "BB";                       // TCPPORT
    case 0x83: return "";                         // TCPOPEN
    case 0x84: return "";                         // TCPCLOSE
    case 0x85: return "X";                        // TCPSEND
    case 0x86: return "BB";                       // TCPRECV: WORD max
    case 0x88: return "S";                        // DNSREQ
    case 0x89: return "";                         // DNSRES
    case 0x92: return "S";                        // GETCONFIG: section
    case 0x99: return "SSS";                      // SETCONFIG: section, key, value
    case 0xB0: return "B";                        // SDCARD
    case 0xA0: return "";                         // NET STATUS
    case 0xA1: return "BBBBBBBBBBBBBBBBBBBBBBB";  // CREATE: game W, build W, slots, bytes, settings length, 16 B
    case 0xA2: return "BBBBS";                    // JOIN: game W, build W, code
    case 0xA3: return "";                         // LEAVE
    case 0xA4: return "B";                        // READY
    case 0xA5: return "BBBBBB";                   // SEND: frame W, 4 input bytes
    case 0xA6: return "BB";                       // POLL: frame W
    case 0xA7: return "BBBB";                     // HASH: frame W, hash W
    case 0xA8: return "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";   // MSG: to, length, 32 bytes
    case 0xA9: return "";                         // RECV
    }
    return nullptr;
}

enum { CMD_STSR = 0x03, CMD_READDIR = 0x41, CMD_FILELIST = 0x42, CMD_NEXT = 0x43, CMD_OPEN = 0x50, CMD_SEEK = 0x51,
       CMD_TRUNC = 0x52, CMD_SYNC = 0x53, CMD_TELL = 0x55, CMD_SIZE = 0x56, CMD_GETCONFIG = 0x92 };
static const uint8_t REQ_DIRNEXT = 0xF2, REQ_PAGE = 0xF3;

static uint64_t ms_now(void)
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// ---- small helpers (netgame.c) ----

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int unhex(const char* h, uint8_t* out, int max)
{
    int n = 0;
    while (n < max && hexval(h[0]) >= 0 && hexval(h[1]) >= 0) {
        out[n++] = (uint8_t)(hexval(h[0]) * 16 + hexval(h[1]));
        h += 2;
    }
    return n;
}

static void tohex(const uint8_t* d, int n, char* out)
{
    static const char x[] = "0123456789abcdef";
    for (int i = 0; i < n; i++) {
        *out++ = x[d[i] >> 4];
        *out++ = x[d[i] & 15];
    }
    *out = 0;
}

// value of "key" in a JSON line, spaces allowed around ':' (relay.py writes them, Cloudflare does not)
static const char* json_val(const char* l, const char* key)
{
    char k[24];
    snprintf(k, sizeof(k), "\"%s\"", key);
    const char* p = strstr(l, k);
    if (!p) return nullptr;
    p += strlen(k);
    while (*p == ' ') p++;
    if (*p != ':') return nullptr;
    p++;
    while (*p == ' ') p++;
    return p;
}

static bool json_int(const char* l, const char* key, long* v)
{
    const char* p = json_val(l, key);
    if (!p || (*p != '-' && (*p < '0' || *p > '9'))) return false;
    *v = strtol(p, nullptr, 10);
    return true;
}

static bool json_str(const char* l, const char* key, char* out, size_t size)
{
    const char* p = json_val(l, key);
    if (!p || *p != '"') return false;
    p++;
    size_t n = 0;
    while (*p && *p != '"' && n + 1 < size) out[n++] = *p++;
    out[n] = 0;
    return true;
}

static bool json_true(const char* l, const char* key)
{
    const char* p = json_val(l, key);
    return p && strncmp(p, "true", 4) == 0;
}

// ---- the card ----

Dpi1l::~Dpi1l()
{
    link_close();   // no 'leave': the relay sees the link end (as switching the DPI off)
    if (tcp) tcp->close();
    flush();
}

bool Dpi1l::configure(const json& cfg, std::string& e)
{
    long long v;
    if (!cfg_number(cfg, "base", v, 0xB0, e)) return false;
    if (v < 0 || v > 0xFE) { e = "base must be 00h ... FEh (two ports)"; return false; }
    base = (uint8_t)v;
    json net = cfg.contains("net") && cfg["net"].is_object() ? cfg["net"] : json::object();
    std::string s;
    if (!cfg_string(net, "transport", s, "ws", e)) return false;
    if (s != "ws" && s != "tcp") { e = "net.transport must be ws or tcp"; return false; }
    relay_ws = s == "ws";
    if (!cfg_string(net, "host", relay_host, "api.mzpico.com", e)) return false;
    if (!cfg_number(net, "port", v, 80, e)) return false;
    if (v < 1 || v > 65535) { e = "net.port must be 1 ... 65535"; return false; }
    relay_port = (uint16_t)v;
    if (!cfg_string(net, "tcp_default", s, "", e)) return false;
    {                                   // TCPOPEN target until the Z80 sets one ([net] tcp_default of DPI.INI)
        unsigned a = 0, b = 0, c = 0, d = 0, pt = 0;
        memset(tcp_default_ip, 0, 4);
        tcp_default_port = 0;
        if (!s.empty()) {
            if (sscanf(s.c_str(), "%u.%u.%u.%u:%u", &a, &b, &c, &d, &pt) != 5 || a > 255 || b > 255 || c > 255 ||
                d > 255 || pt > 65535) {
                e = "net.tcp_default must be a.b.c.d:port";
                return false;
            }
            uint8_t ip[4] = { (uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d };
            memcpy(tcp_default_ip, ip, 4);
            tcp_default_port = (uint16_t)pt;
        }
    }
    json sdc = cfg.contains("sd") && cfg["sd"].is_object() ? cfg["sd"] : json::object();
    std::string folder;
    if (!cfg_string(sdc, "folder", folder, "work/dpi-1l/sdcard", e)) return false;
    if (!cfg_number(sdc, "sectors", v, 62309376, e)) return false;
    if (v < 64 || v > 0xFFFFFFFFll) { e = "sd.sectors must be 64 ... 4294967295"; return false; }
    if (!cfg_bool(sdc, "flush_on_exit", flush_on_exit, false, e)) return false;
    sd_ready = false;
    if (!probe) {                       // a folder under media/ is used through its working copy in work/
        std::string path;
        if (!media_path(folder, true, path, e)) return false;
        if (!sd.init(path, (uint32_t)v, e)) return false;
        sd_ready = true;
    }

    // power on: as repo_init and the NET variables of the firmware
    link_close();
    sd.power_on();
    ff_res = 0;
    rec_len = rec_pos = 0;
    dir_cmd = 0;
    rtc_offset = 0;
    boot_until = 0;
    if (tcp) tcp->close();
    tcp.reset();
    tstate = 0;
    memcpy(tcp_ip, tcp_default_ip, 4);
    tcp_port = tcp_default_port;
    dns.reset();
    dstate = 0;
    memset(dns_ip, 0, 4);
    phase = PH_DONE;
    cmd = 0;
    err = false;
    err_code = 0;
    sts_pos = 0;
    fmt = "";
    fmt_pos = 0;
    memset(buf, 0, sizeof(buf));
    len = 0;
    count = 0;
    out_pos = 0;
    gstate = S_NOLINK;
    slot = 0xFF;
    slots = nbytes = members = ready_mask = last_error = 0;
    settings_len = 0;
    memset(settings, 0, sizeof(settings));
    memset(code, 0, sizeof(code));
    seed = start_frame = mask = 0;
    started = false;
    wbase = 0;
    memset(have, 0, sizeof(have));
    memset(data, 0, sizeof(data));
    memset(msgq, 0, sizeof(msgq));
    mq_head = mq_n = 0;
    pend_cmd = 0;
    pend_since = 0;
    return true;
}

void Dpi1l::relay(const std::string& host, uint16_t port, bool websocket)
{
    net_reset();   // a room on the old relay is left (netgame_config)
    relay_host = host;
    relay_port = port;
    relay_ws = websocket;
}

// /RES of SAPI: the firmware leaves the NET room (firmware/main.c, dpi_bus_sapi_running falls) and a waiting
// CREATE/JOIN ends (status 00, no error); the rest of the Repository API state stays as it was
void Dpi1l::reset()
{
    poll();
    net_reset();
}

// ---- Repository API ----

void Dpi1l::fail(uint8_t code_)
{
    err = true;
    err_code = code_;
    phase = PH_DONE;
}

void Dpi1l::output(uint16_t n)
{
    phase = PH_DOUTRQ;
    out_pos = 0;
    count = (uint8_t)n;
    err = false;
}

void Dpi1l::want_params(const char* f)
{
    phase = PH_PARAMRQ;
    fmt = f;
    fmt_pos = 0;
    len = 0;
    uint8_t strings = 0;
    for (const char* p = f; *p; p++) strings += (*p == 'S');
    count = (uint8_t)(255 - strings);   // as the Unicard: 255 minus one byte per string terminator
}

void Dpi1l::command(uint8_t c)
{
    sts_pos = 0;
    if (c == CMD_STSR) return;          // STSR: only rewinds the status pointer
    if (phase == PH_PROGRESS) cancel_pending();   // a new command drops the pending CREATE/JOIN (as the Unicard)
    cmd = c;
    err = false;
    err_code = 0;
    phase = PH_DONE;
    len = 0;

    const char* f = fs_format(c);
    if (f) {
        if ((c == CMD_TRUNC || c == CMD_SYNC) && !sd.file.open) {
            fail(ERR_NO_FILE);
            return;
        }
        if (c == 0x00 || c == CMD_READDIR || c == CMD_FILELIST || c == CMD_OPEN || c == CMD_GETCONFIG)
            rec_len = rec_pos = 0;      // the old directory stream ends here
        if (c == CMD_READDIR || c == CMD_FILELIST || c == CMD_GETCONFIG) dir_cmd = c;
        if (c == CMD_NEXT && dir_cmd) cmd = dir_cmd;   // status byte 1 shows 41h/42h after NEXT, as the Unicard
        if (*f) want_params(f);
        else post(c);
        return;
    }
    struct tm t;
    switch (c) {
    case 0x01:                          // ASCII: names are always KOI-8cs2, contents never converted
    case 0x04:                          // STORNO
        break;
    case 0x05: {                        // REV
        int n = snprintf((char*)buf, sizeof(buf), "DPI-1L v%u.%u.%u", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
        buf[n++] = 0x0D;
        output((uint16_t)n);
        break;
    }
    case 0x06:                          // REVD
        buf[0] = VERSION_MAJOR;
        buf[1] = VERSION_MINOR;
        buf[2] = 'D';
        buf[3] = BOARD;
        output(4);
        break;
    case 0x07:                          // BOOT: /RTL pulled low for 100 ms (firmware/main.c rtl_pulse)
        if (bus) boot_until = bus->now + (uint64_t)bus->cpu_hz / 10 * (uint64_t)bus->cpu_step;
        break;
    case 0x11:                          // INTCALLER: no interrupt sources
        buf[0] = 0;
        output(1);
        break;
    case 0x60:                          // RTCSETD
    case 0x61:                          // RTCSETT
        want_params("BBB");
        break;
    case CMD_SEEK:
        want_params("BBBBB");
        break;
    case CMD_TELL:
    case CMD_SIZE: {
        if (!sd.file.open) { fail(ERR_NO_FILE); break; }
        uint32_t v = c == CMD_TELL ? sd.file.pos : sd.file.size;
        buf[0] = (uint8_t)v;
        buf[1] = (uint8_t)(v >> 8);
        buf[2] = (uint8_t)(v >> 16);
        buf[3] = (uint8_t)(v >> 24);
        output(4);
        break;
    }
    case 0x62:                          // RTCGETD
        rtc_get(t);
        buf[0] = (uint8_t)t.tm_mday;
        buf[1] = (uint8_t)(t.tm_mon + 1);
        buf[2] = (uint8_t)(t.tm_year - 80);
        output(3);
        break;
    case 0x63:                          // RTCGETT
        rtc_get(t);
        buf[0] = (uint8_t)t.tm_hour;
        buf[1] = (uint8_t)t.tm_min;
        buf[2] = (uint8_t)t.tm_sec;
        output(3);
        break;
    case 0x93:                          // WIFISTATUS: the emulator is always on the network
        buf[0] = WIFI_CONNECTED;
        output(1);
        break;
    case 0x95:                          // INFO: revision, board, flash MB, features 01h Wi-Fi + 08h NET
        memset(buf, 0, 16);
        buf[0] = 1;
        buf[1] = BOARD;
        buf[2] = FLASH_MB;
        buf[3] = 0x01 | 0x08;
        output(16);
        break;
    default:                            // SHASCII (Sharp only) and unknown commands
        fail(ERR_NOT_IMPLEMENTED);
    }
}

// ---- RTC: the clock of the PC, moved by RTCSETD/RTCSETT (the AON timer of the RP2350) ----

void Dpi1l::rtc_get(struct tm& t)
{
    time_t now = host_time_now() + (time_t)rtc_offset;
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
}

void Dpi1l::rtc_set(const struct tm& t)
{
    struct tm c = t;
    c.tm_isdst = -1;
    time_t want = mktime(&c);
    if (want != (time_t)-1) rtc_offset = (int64_t)want - (int64_t)host_time_now();
}

static bool valid_date(int d, int m, int y)
{
    static const uint8_t days[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m < 1 || m > 12 || d < 1 || d > days[m - 1]) return false;
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return !(m == 2 && d == 29 && !leap);
}

void Dpi1l::run_params(void)
{
    struct tm t;
    switch (cmd) {
    case CMD_SEEK: {                    // mode (0 from the start, 1 from the end, 2 forward, 3 back), DWORD
        if (!sd.file.open) { fail(ERR_NO_FILE); return; }
        uint32_t n = buf[1] | buf[2] << 8 | buf[3] << 16 | (uint32_t)buf[4] << 24;
        uint64_t from = buf[0] == 1 ? sd.file.size : buf[0] >= 2 ? sd.file.pos : 0;
        if (buf[0] > 3) { fail(ERR_BAD_PARAM); return; }
        if (buf[0] == 1 || buf[0] == 3) sd.file.pos = n > from ? 0 : (uint32_t)(from - n);   // before the start: 0
        else sd.file.pos = (uint32_t)(from + n);
        break;
    }
    case 0x60:                          // RTCSETD: D, M, Y-1980
        if (!valid_date(buf[0], buf[1], 1980 + buf[2])) { fail(ERR_BAD_PARAM); return; }
        rtc_get(t);
        t.tm_mday = buf[0];
        t.tm_mon = buf[1] - 1;
        t.tm_year = 80 + buf[2];
        rtc_set(t);
        break;
    case 0x61:                          // RTCSETT: H, M, S
        if (buf[0] > 23 || buf[1] > 59 || buf[2] > 59) { fail(ERR_BAD_PARAM); return; }
        rtc_get(t);
        t.tm_hour = buf[0];
        t.tm_min = buf[1];
        t.tm_sec = buf[2];
        rtc_set(t);
        break;
    default:
        fail(ERR_NOT_IMPLEMENTED);
    }
}

// ---- the data port ----

// one byte to the open file
void Dpi1l::file_putc(uint8_t v)
{
    DpiFile& f = sd.file;
    if (!(f.mode & 0x02)) {             // opened for reading only
        fail(ERR_NO_FILE);
        return;
    }
    if (!(f.valid && f.pos >= f.page_pos && f.pos < f.page_pos + DPI_PAGE)) {
        DpiSdResult r;
        sd.execute(REQ_PAGE, 0, buf, r);   // core 0 writes the old page back and loads the one with pos
        ff_res = r.ff_res;
        if (r.err_code) {
            fail(ERR_FATFS);
            return;
        }
    }
    uint32_t off = f.pos - f.page_pos;
    f.page[off] = v;
    if (off + 1 > f.page_len) f.page_len = off + 1;
    f.dirty = true;
    f.pos++;
    if (f.pos > f.size) f.size = f.pos;
    cmd = 0xF1;                         // INTPUTC, as the Unicard shows it in status byte 1
    err = false;
}

void Dpi1l::data_out(uint8_t v)
{
    sts_pos = 0;
    if (phase != PH_PARAMRQ) {
        if (sd.file.open) file_putc(v);
        else if (!err) fail(ERR_NO_FILE);   // nobody takes it; after refused parameters their error stays (f3e583b)
        return;
    }
    char f = fmt[fmt_pos];
    if (f == 'B') {
        buf[len++] = v;
        count--;
        fmt_pos++;
    } else if (f == 'X') {              // TCPSEND: WORD length (bit 15 = push now), then that many bytes
        buf[len++] = v;
        count--;
        if (len >= 2) {
            uint16_t n = (uint16_t)((buf[0] | buf[1] << 8) & 0x7FFF);
            if (n > 252) {              // the Unicard read past its buffer here (unicard.md Q9)
                fail(ERR_BAD_PARAM);
                return;
            }
            if (len >= 2u + n) fmt_pos++;
        }
    } else if (v >= 0x20) {             // 'S'
        buf[len++] = v;
        count--;
    } else {
        buf[len++] = 0;                 // any byte < 20h ends the string
        fmt_pos++;
    }
    err = false;
    if (count == 0) {                   // parameters do not fit
        fail(ERR_OVERFLOW);
        return;
    }
    if (fmt[fmt_pos] == 0) {            // all parameters are in: run the command
        phase = PH_DONE;
        if (fs_format(cmd)) post(cmd);
        else run_params();
    }
}

uint8_t Dpi1l::data_in(void)
{
    sts_pos = 0;
    if (phase == PH_DOUTRQ) {
        uint8_t v = buf[out_pos++];
        if (--count == 0) phase = PH_DONE;
        err = false;
        return v;
    }
    if (rec_pos < rec_len) {            // directory record
        uint8_t v = rec[rec_pos++];
        err = false;
        if (rec_pos == rec_len) {       // the whole record is read: the next one is loaded meanwhile
            rec_len = rec_pos = 0;
            DpiSdResult r;
            sd.execute(REQ_DIRNEXT, dir_cmd, buf, r);
            ff_res = r.ff_res;
            if (!sd.dir_active()) dir_cmd = 0;
            rec_len = r.err_code ? 0 : r.rec_len;
            rec_pos = 0;
            if (rec_len) memcpy(rec, r.data, rec_len);
            if (r.err_code) fail(r.err_code);
        }
        return v;
    }
    DpiFile& f = sd.file;
    if (f.open) {
        if (!(f.mode & 0x01)) {
            fail(ERR_NO_FILE);          // opened for writing only
            return 0x00;
        }
        cmd = 0xF0;                     // INTGETC
        err = false;
        if (f.pos >= f.size) return 0x00;   // at the end: 00, no error, the EOF bit tells it
        if (!(f.valid && f.pos >= f.page_pos && f.pos < f.page_pos + DPI_PAGE) || f.pos >= f.page_pos + f.page_len) {
            DpiSdResult r;
            sd.execute(REQ_PAGE, 0, buf, r);
            ff_res = r.ff_res;
            if (r.err_code) {
                fail(ERR_FATFS);
                return 0x00;
            }
            if (f.pos >= f.page_pos + f.page_len) return 0x00;   // a size not on the disk (not written yet)
        }
        return f.page[f.pos++ - f.page_pos];
    }
    fail(ERR_NO_FILE);                  // nothing to read
    return 0x00;
}

uint8_t Dpi1l::status(void)
{
    uint8_t v;
    switch (sts_pos) {
    case 0:
        v = sd.dir_active() ? ST_DIR : 0;
        if (sd.file.open) {
            if (sd.file.mode & 0x01) v |= ST_FILE_READ;
            if (sd.file.mode & 0x02) v |= ST_FILE_WRITE;
            if (sd.file.pos >= sd.file.size) v |= ST_EOF;
        }
        if (phase == PH_PARAMRQ) v |= ST_PARAMRQ;
        if (phase == PH_DOUTRQ) v |= ST_DOUTRQ;
        if (phase == PH_PROGRESS) v |= ST_IN_PROGRESS;
        if (err) v |= ST_ERROR;
        break;
    case 1:
        v = cmd;
        break;
    case 2:
        if (err) v = err_code;
        else if (phase == PH_PARAMRQ || phase == PH_DOUTRQ) v = count;
        else if (rec_len) v = (uint8_t)(rec_len - rec_pos);
        else v = 0;
        break;
    case 3:
        v = ff_res;                     // FatFs result
        break;
    default:
        return 0x00;                    // past the 4 bytes: 00 until the pointer is rewound
    }
    sts_pos++;
    return v;
}

void Dpi1l::complete(uint8_t e, const uint8_t* d, uint16_t n)
{
    if (phase != PH_PROGRESS) return;
    if (e) {
        fail(e);
    } else if (n) {
        memcpy(buf, d, n);
        output(n);
    } else {
        phase = PH_DONE;
    }
}

bool Dpi1l::io_read(uint8_t port, uint8_t& d)
{
    if (port != base && port != (uint8_t)(base + 1)) return false;
    poll();
    d = port == base ? status() : data_in();
    return true;
}

bool Dpi1l::io_write(uint8_t port, uint8_t d)
{
    if (port != base && port != (uint8_t)(base + 1)) return false;
    if (port == base && d != 0x03 && phase == PH_PROGRESS) cancel_pending();   // before a waiting 'room' is taken
    poll();
    if (port == base) command(d);
    else data_out(d);
    return true;
}

// ---- NET ----

void Dpi1l::window_reset(uint16_t from)
{
    wbase = from;
    memset(have, 0, sizeof(have));
}

void Dpi1l::store_input(uint16_t frame, uint8_t s, const uint8_t* d, uint8_t n)
{
    if (s >= 4 || (uint16_t)(frame - wbase) >= WINDOW || frame < wbase) return;
    uint8_t i = frame % WINDOW;
    memcpy(data[i][s], d, n > 4 ? 4 : n);
    have[i] |= (uint8_t)(1u << s);
}

uint16_t Dpi1l::full_mask(void)
{
    return mask ? mask : (uint16_t)((1u << slots) - 1u);
}

uint16_t Dpi1l::avail_frame(void)
{
    if (!started || !full_mask()) return 0xFFFF;   // no game (slots 0 would make every frame "complete")
    while (wbase != 0xFFFF && (have[wbase % WINDOW] & full_mask()) == full_mask()) {
        have[wbase % WINDOW] = 0;
        wbase++;
    }
    return wbase == 0 ? 0xFFFF : (uint16_t)(wbase - 1);
}

void Dpi1l::pend_finish(uint8_t e, const uint8_t* out, uint16_t n)
{
    if (!pend_cmd) return;
    pend_cmd = 0;
    complete(e, out, n);
}

void Dpi1l::link_close(void)
{
    if (link) {
        link->close();
        link.reset();
    }
}

void Dpi1l::link_start(const std::string& path, const std::string& first)
{
    link_close();
    link = RelayLink::start(relay_host, relay_port, relay_ws, path, first);
}

bool Dpi1l::send_line(const char* l)
{
    return link && link->send_line(l);
}

void Dpi1l::link_lost(void)
{
    link_close();
    if (gstate >= S_IN_ROOM || pend_cmd) last_error = E_NOLINK;   // not after 'error': its code stays
    if (gstate >= S_IN_ROOM) gstate = S_DROPPED;
    if (pend_cmd) pend_finish(E_NOLINK, nullptr, 0);
}

// LEAVE, a reset: 'leave' goes out before the link is closed
void Dpi1l::leave(void)
{
    if (link && link->is_open() && gstate >= S_IN_ROOM) send_line("{\"op\":\"leave\"}");
    link_close();
    gstate = S_NOLINK;
    slot = 0xFF;
    started = false;
    members = 0;
    ready_mask = 0;
    mask = 0;
    mq_n = 0;
}

void Dpi1l::net_reset(void)
{
    pend_cmd = 0;
    leave();
    last_error = 0;
    complete(0, nullptr, 0);            // a reset during CREATE/JOIN: the status leaves IN_PROGRESS (b72a8d0)
}

// The Z80 gave up waiting for CREATE/JOIN: its link is closed, a 'room' that comes later (or came and was not
// taken yet) must not put the card in a room nobody asked for (firmware b72a8d0)
void Dpi1l::cancel_pending(void)
{
    if (!pend_cmd) return;
    pend_cmd = 0;
    leave();
}

void Dpi1l::poll(void)
{
    if (link) {
        std::vector<RelayEvent> ev;
        std::shared_ptr<RelayLink> l = link;
        l->take(ev);
        for (size_t i = 0; i < ev.size() && link == l; i++) {
            if (ev[i].lost) link_lost();
            else handle_line(ev[i].line.c_str());
        }
    }
    if (pend_cmd && ms_now() - pend_since > pending_timeout_ms) {
        link_close();                   // no answer in 10 s, also with the link open
        last_error = E_NOLINK;
        pend_finish(E_NOLINK, nullptr, 0);
    }
}

void Dpi1l::handle_line(const char* l)
{
    char op[16];
    long v;
    if (!json_str(l, "op", op, sizeof(op))) return;
    if (!strcmp(op, "room")) {
        long sl = 0;
        json_int(l, "slot", &sl);
        if (json_int(l, "slots", &v) && v >= 1 && v <= 4) slots = (uint8_t)v;
        if (json_int(l, "bytes", &v) && v >= 1 && v <= 4) nbytes = (uint8_t)v;
        char hex[40] = "";
        memset(settings, 0, sizeof(settings));
        settings_len = 0;
        if (json_str(l, "settings", hex, sizeof(hex))) settings_len = (uint8_t)unhex(hex, settings, 16);
        char c[8] = "";
        if (json_str(l, "code", c, sizeof(c))) strncpy(code, c, sizeof(code) - 1);
        bool spect = sl < 0 || json_true(l, "spectator");
        slot = spect ? 0xFF : (uint8_t)sl;
        gstate = spect ? S_SPECTATOR : S_IN_ROOM;
        members = 1;
        ready_mask = 0;
        started = false;
        mask = 0;
        window_reset(0);
        if (pend_cmd == 0xA1) {
            uint8_t out[6] = { 0, 0, 0, 0, 0x0D, slot };
            memcpy(out, code, 4);
            pend_finish(0, out, 6);
        } else if (pend_cmd == 0xA2) {
            uint8_t out[20] = { slot, slots, nbytes, settings_len };
            memcpy(out + 4, settings, 16);
            pend_finish(0, out, 20);
        }
    } else if (!strcmp(op, "members")) {
        if (json_int(l, "count", &v)) members = (uint8_t)v;
        if (json_int(l, "ready", &v)) ready_mask = (uint8_t)v;
    } else if (!strcmp(op, "start")) {
        if (json_int(l, "seed", &v)) seed = (uint16_t)v;
        start_frame = json_int(l, "frame", &v) ? (uint16_t)v : 0;
        mask = json_int(l, "mask", &v) ? (uint16_t)v : 0;
        started = true;
        window_reset(start_frame);
        if (gstate != S_SPECTATOR) gstate = S_RUNNING;
    } else if (!strcmp(op, "input")) {
        long f, s;
        char hex[16];
        uint8_t d[4] = { 0 };
        if (json_int(l, "frame", &f) && json_int(l, "slot", &s) && json_str(l, "data", hex, sizeof(hex)))
            store_input((uint16_t)f, (uint8_t)s, d, (uint8_t)unhex(hex, d, 4));
    } else if (!strcmp(op, "desync")) {
        gstate = S_DESYNC;
    } else if (!strcmp(op, "dropped")) {
        gstate = S_DROPPED;
        started = false;
    } else if (!strcmp(op, "msg")) {
        long from = -1;
        char hex[70];
        json_int(l, "from", &from);
        if (mq_n < MSGQ && json_str(l, "data", hex, sizeof(hex))) {
            uint8_t i = (uint8_t)((mq_head + mq_n) % MSGQ);
            memset(&msgq[i], 0, sizeof(msgq[i]));
            msgq[i].from = from < 0 ? 0xFF : (uint8_t)from;
            msgq[i].len = (uint8_t)unhex(hex, msgq[i].d, 32);
            mq_n++;
        }
    } else if (!strcmp(op, "error")) {
        if (json_int(l, "code", &v)) last_error = (uint8_t)v;
        if (pend_cmd) pend_finish(last_error ? last_error : E_ROOM, nullptr, 0);
    }
}

// A command of the SD card side or NET with its parameters (post + repo_service of the firmware: the card runs it at
// once, the Z80 does not wait)
void Dpi1l::post(uint8_t c)
{
    DpiSdResult r;
    bool pending = false;
    if (c == 0x00) {                    // RESET: NET leaves its room, TCP closes, then the files (repo_fs_execute)
        net_reset();
        tcp_reset();
        if (sd_ready) sd.execute(0x00, dir_cmd, buf, r);
    } else if (c >= 0xA0 && c <= 0xA9) {
        run_net(r, pending);
    } else if (c >= 0x80 && c <= 0x89) {
        run_tcp(r);
    } else if (sd_ready) {
        sd.execute(c, dir_cmd, buf, r);
    } else {
        r.err_code = ERR_NOT_IMPLEMENTED;
    }

    ff_res = r.ff_res;
    if (!sd.dir_active()) dir_cmd = 0;
    bool dir_request = c == CMD_READDIR || c == CMD_FILELIST || c == CMD_NEXT || c == 0x00 || c == CMD_OPEN ||
                       c == CMD_GETCONFIG;
    if (dir_request) {                  // the directory stream is whatever the card says now (empty at the end)
        rec_len = r.err_code ? 0 : r.rec_len;
        rec_pos = 0;
        if (rec_len) memcpy(rec, r.data, rec_len);
    }
    if (pending) {
        phase = PH_PROGRESS;            // goes on in the background, complete() ends it
        err = false;
    } else if (r.err_code) {
        fail(r.err_code);
    } else if (r.out_len) {
        memcpy(buf, r.data, r.out_len);
        output(r.out_len);
    }
}

// RESET 00h: the TCP connection goes as well (as a NET room), DNS forgets its answer (f3e583b); address and port stay
void Dpi1l::tcp_reset(void)
{
    if (tcp) tcp->close();
    tcp.reset();
    tstate = 0;
    dns.reset();
    dstate = 0;
    memset(dns_ip, 0, 4);
}

// TCP and DNS (common/net.c repo_net_execute): one TCP client connection, a name looked up. RESET closes the
// connection, /RES of SAPI does not.
void Dpi1l::run_tcp(DpiSdResult& r)
{
    static const uint8_t T_IDLE = 0, T_CONNECTING = 1, T_CONNECTED = 2, T_FAILED = 3;
    static const uint8_t D_RESOLVING = 1, D_RESOLVED = 2, D_UNKNOWN = 3, D_BADARGS = 4;
    const uint8_t* p = buf;
    if (tcp && (tstate == T_CONNECTING || tstate == T_CONNECTED)) {
        int s = tcp->state();               // what the link thread found (connected, failed, closed by the other side)
        if (s != TcpLink::CONNECTING) tstate = (uint8_t)s;
    }
    switch (cmd) {
    case 0x80: {                            // TCPSTATUS: 10 B
        if (tstate == T_CONNECTING && ms_now() - connect_started > 10000) {
            if (tcp) tcp->close();
            tcp.reset();
            tstate = T_FAILED;              // nobody answered within 10 s
        }
        size_t n = tcp ? tcp->available() : 0;
        r.data[0] = tstate == T_CONNECTED ? 1 : 0;
        r.data[1] = tstate;
        r.data[2] = (uint8_t)(n > 0xFFFF ? 0xFF : n);
        r.data[3] = (uint8_t)(n > 0xFFFF ? 0xFF : n >> 8);
        memcpy(r.data + 4, tcp_ip, 4);
        r.data[8] = (uint8_t)tcp_port;
        r.data[9] = (uint8_t)(tcp_port >> 8);
        r.out_len = 10;
        break;
    }
    case 0x81:                              // TCPIP: 4 B, first octet first
        memcpy(tcp_ip, p, 4);
        break;
    case 0x82:                              // TCPPORT: WORD
        tcp_port = (uint16_t)(p[0] | p[1] << 8);
        break;
    case 0x83:                              // TCPOPEN: finishes in the background, TCPSTATUS shows it
        if (tcp) tcp->close();
        tcp = TcpLink::start(tcp_ip, tcp_port);
        tstate = T_CONNECTING;
        connect_started = ms_now();
        break;
    case 0x84:                              // TCPCLOSE (the bytes that came stay readable)
        if (tcp) tcp->close();
        tstate = T_IDLE;
        break;
    case 0x85: {                            // TCPSEND: WORD length (bit 15 = push now), data
        uint16_t n = (uint16_t)((p[0] | p[1] << 8) & 0x7FFF);
        if (tstate != T_CONNECTED || !tcp) break;   // not connected: dropped, as the Unicard
        if (!tcp->send(p + 2, n)) r.err_code = REPO_ERR_BUSY;
        break;
    }
    case 0x86: {                            // TCPRECV: WORD max -> WORD n, n bytes (n <= 253)
        size_t max = (size_t)(p[0] | p[1] << 8);
        size_t n = tcp ? tcp->take(r.data + 2, max < 253 ? max : 253) : 0;
        r.data[0] = (uint8_t)n;
        r.data[1] = (uint8_t)(n >> 8);
        r.out_len = (uint16_t)(2 + n);
        break;
    }
    case 0x88: {                            // DNSREQ: name
        if (!p[0]) {
            dstate = D_BADARGS;
            break;
        }
        dstate = D_RESOLVING;
        dns_started = ms_now();
        memset(dns_ip, 0, 4);               // a failed request must not show the IP of an older one (f3e583b)
        unsigned a, b, c, d;
        char tail;
        if (sscanf((const char*)p, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) == 4 && a < 256 && b < 256 && c < 256 &&
            d < 256) {                      // a number: at once, as lwIP
            uint8_t v[4] = { (uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d };
            memcpy(dns_ip, v, 4);
            dstate = D_RESOLVED;
            dns.reset();
        } else if (strlen((const char*)p) > 255) {
            dstate = D_BADARGS;
        } else {
            dns = DnsQuery::start((const char*)p);
        }
        break;
    }
    case 0x89:                              // DNSRES: state, IP (4 B)
        if (dstate == D_RESOLVING && dns && dns->done) {
            if (dns->ok) memcpy(dns_ip, dns->ip, 4);
            dstate = dns->ok ? D_RESOLVED : D_UNKNOWN;
            dns.reset();
        }
        if (dstate == D_RESOLVING && ms_now() - dns_started > 5000) {
            dstate = D_UNKNOWN;             // no answer: give up
            dns.reset();
        }
        r.data[0] = dstate;
        memcpy(r.data + 1, dns_ip, 4);
        r.out_len = 5;
        break;
    default:
        r.err_code = ERR_NOT_IMPLEMENTED;
    }
}

// NET A0h-A9h (common/netgame.c repo_netgame_execute)
void Dpi1l::run_net(DpiSdResult& r, bool& pending)
{
    const uint8_t* p = buf;
    uint8_t& res_err = r.err_code;
    uint8_t* o = r.data;
    uint16_t& on = r.out_len;
    char l[200];

    // a new command: the Z80 does not wait for an older CREATE/JOIN any more
    pend_cmd = 0;
    switch (cmd) {
    case 0xA0: {                        // STATUS
        uint16_t av = (gstate == S_RUNNING || gstate == S_SPECTATOR) ? avail_frame() : 0xFFFF;
        uint8_t fb = (av != 0xFFFF && av + 1 > start_frame) ? (uint8_t)(av + 1 - start_frame) : 0;
        uint8_t s[8] = { (uint8_t)(linked() ? (gstate == S_NOLINK ? S_READY : gstate) : S_NOLINK), slot, members,
                         ready_mask, 0, fb, mq_n, last_error };
        memcpy(o, s, 8);
        on = 8;
        break;
    }
    case 0xA1: {                        // CREATE
        unsigned game = p[0] | p[1] << 8, build = p[2] | p[3] << 8;
        if (p[4] < 1 || p[4] > 4 || p[5] < 1 || p[5] > 4) { res_err = E_PARAM; break; }
        if (!linked()) { last_error = E_NOLINK; res_err = E_NOLINK; break; }
        leave();
        slots = p[4];                   // kept: a relay that leaves them out of 'room' still works
        nbytes = p[5];
        uint8_t slen = p[6] > 16 ? 16 : p[6];
        char hex[40];
        tohex(p + 7, slen, hex);
        char path[96];
        snprintf(path, sizeof(path), "/net?game=%u&create=1", game);
        snprintf(l, sizeof(l), "{\"op\":\"create\",\"game\":%u,\"build\":%u,\"slots\":%u,\"bytes\":%u,\"settings\":\"%s\"}",
                 game, build, p[4], p[5], hex);
        pend_cmd = 0xA1;
        pend_since = ms_now();
        link_start(path, l);
        pending = true;
        break;
    }
    case 0xA2: {                        // JOIN
        unsigned game = p[0] | p[1] << 8, build = p[2] | p[3] << 8;
        char c[8];
        size_t n = 0;
        for (const uint8_t* s = p + 4; *s; s++) {
            if (n == 7 || !((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9'))) {
                n = 99;                 // 1-7 letters and digits (as the Unicard)
                break;
            }
            c[n++] = (char)((*s >= 'a' && *s <= 'z') ? *s - 32 : *s);
        }
        if (n == 0 || n > 7) { res_err = E_PARAM; break; }
        c[n] = 0;
        if (!linked()) { last_error = E_NOLINK; res_err = E_NOLINK; break; }
        leave();
        char path[96];
        snprintf(path, sizeof(path), "/net?game=%u&code=%s", game, c);
        snprintf(l, sizeof(l), "{\"op\":\"join\",\"game\":%u,\"build\":%u,\"code\":\"%s\"}", game, build, c);
        pend_cmd = 0xA2;
        pend_since = ms_now();
        link_start(path, l);
        pending = true;
        break;
    }
    case 0xA3:                          // LEAVE
        if (gstate < S_IN_ROOM) { res_err = E_NOROOM; break; }
        leave();
        break;
    case 0xA4: {                        // READY
        if (gstate != S_IN_ROOM && gstate != S_RUNNING) { res_err = E_NOROOM; break; }
        snprintf(l, sizeof(l), "{\"op\":\"ready\",\"ready\":%u}", p[0] ? 1 : 0);
        send_line(l);
        uint8_t s[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        if (started) {
            s[0] = (uint8_t)seed;
            s[1] = (uint8_t)(seed >> 8);
            s[2] = (uint8_t)start_frame;
            s[3] = (uint8_t)(start_frame >> 8);
        }
        memcpy(o, s, 4);
        on = 4;
        break;
    }
    case 0xA5: {                        // SEND: frame, 4 bytes (nbytes used)
        if (gstate != S_RUNNING) { res_err = E_NOROOM; break; }
        unsigned f = p[0] | p[1] << 8;
        char hex[12];
        tohex(p + 2, nbytes, hex);
        snprintf(l, sizeof(l), "{\"op\":\"input\",\"frame\":%u,\"data\":\"%s\"}", f, hex);
        if (!send_line(l)) { res_err = E_FULL; break; }
        store_input((uint16_t)f, slot, p + 2, nbytes);   // the relay does not send our own input back
        break;
    }
    case 0xA6: {                        // POLL: frame -> avail, slots x nbytes
        if (gstate != S_RUNNING && gstate != S_SPECTATOR) { res_err = E_NOROOM; break; }
        uint16_t f = (uint16_t)(p[0] | p[1] << 8), av = avail_frame();
        o[0] = (uint8_t)av;
        o[1] = (uint8_t)(av >> 8);
        on = 2;
        bool ok = av != 0xFFFF && f <= av && (uint16_t)(f + WINDOW) > wbase;
        for (uint8_t s = 0; s < slots; s++)
            for (uint8_t b = 0; b < nbytes; b++) o[on++] = ok ? data[f % WINDOW][s][b] : 0;
        break;
    }
    case 0xA7:                          // HASH
        if (gstate != S_RUNNING) { res_err = E_NOROOM; break; }
        snprintf(l, sizeof(l), "{\"op\":\"hash\",\"frame\":%u,\"hash\":%u}", (unsigned)(p[0] | p[1] << 8),
                 (unsigned)(p[2] | p[3] << 8));
        send_line(l);
        break;
    case 0xA8: {                        // MSG: to, length, 32 bytes
        if (gstate < S_IN_ROOM) { res_err = E_NOROOM; break; }
        char hex[70];
        tohex(p + 2, p[1] > 32 ? 32 : p[1], hex);
        snprintf(l, sizeof(l), "{\"op\":\"msg\",\"to\":%d,\"data\":\"%s\"}", p[0] == 0xFF ? -1 : p[0], hex);
        send_line(l);
        break;
    }
    case 0xA9:                          // RECV -> from, length, 32 bytes
        memset(o, 0, 34);
        o[0] = 0xFF;
        if (mq_n) {
            o[0] = msgq[mq_head].from;
            o[1] = msgq[mq_head].len;
            memcpy(o + 2, msgq[mq_head].d, 32);
            mq_head = (uint8_t)((mq_head + 1) % MSGQ);
            mq_n--;
        }
        on = 34;
        break;
    default:
        res_err = ERR_NOT_IMPLEMENTED;
    }
}

// ---- the rest of the card interface ----

// the parameter format of a command waiting for its parameters (fmt after loading a state)
static const char* params_format(uint8_t c)
{
    if (c == 0x60 || c == 0x61) return "BBB";
    if (c == CMD_SEEK) return "BBBBB";
    const char* f = fs_format(c);
    return f ? f : "";
}

void Dpi1l::state(StateIO& io)
{
    // The Repository API, the SD card side (the folder holds the files themselves, here the CWD, the open file with
    // its page not written yet, the directory stream) and the RTC. A link to the relay can not be saved: after
    // loading the card is out of the NET room, a waiting CREATE/JOIN ended.
    io.v(phase);
    io.v(cmd);
    io.v(err);
    io.v(err_code);
    io.v(sts_pos);
    io.v(fmt_pos);
    io.arr(buf, sizeof(buf));
    io.v(len);
    io.v(count);
    io.v(out_pos);
    io.v(ff_res);
    io.arr(rec, sizeof(rec));
    io.v(rec_len);
    io.v(rec_pos);
    io.v(dir_cmd);
    io.v(rtc_offset);
    io.v(boot_until);
    io.arr(tcp_ip, 4);
    io.v(tcp_port);
    io.v(tstate);
    io.v(dstate);
    io.arr(dns_ip, 4);
    sd.state(io);
    if (io.loading) {
        fmt = params_format(cmd);
        net_reset();
        // a TCP connection or a name being looked up can not be saved: closed / unknown after loading
        if (tcp) tcp->close();
        tcp.reset();
        if (tstate == 1) tstate = 3;
        if (tstate == 2) tstate = 4;
        dns.reset();
        if (dstate == 1) dstate = 3;
    }
}

bool Dpi1l::rtl_line()
{
    return bus && bus->now < boot_until;
}

// Switching off loses the page not written yet, as on the card; flush_on_exit (a deviation) writes it.
void Dpi1l::flush()
{
    if (flush_on_exit && sd_ready) sd.flush_page();
}

int Dpi1l::decode_io(uint8_t port, const char** what)
{
    if (port == base) {
        *what = "Repository API command/status";
        return DEC_READ | DEC_WRITE;
    }
    if (port == (uint8_t)(base + 1)) {
        *what = "Repository API data";
        return DEC_READ | DEC_WRITE;
    }
    return 0;
}

static const char* const k_state_names[] = { "nolink", "ready", "in room", "running", "desync", "dropped", "spectator" };

void Dpi1l::describe(json& out)
{
    char b[16];
    snprintf(b, sizeof(b), "%02Xh", base);
    out["base"] = b;
    out["relay"] = relay_host + ":" + std::to_string(relay_port);
    out["transport"] = relay_ws ? "ws" : "tcp";
    out["link_open"] = link && link->is_open();
    uint8_t s = linked() ? (gstate == S_NOLINK ? S_READY : gstate) : S_NOLINK;
    out["net_state"] = k_state_names[s <= S_SPECTATOR ? s : 0];
    out["waiting"] = pend_cmd != 0;
    if (gstate >= S_IN_ROOM) {
        out["code"] = code;
        out["slot"] = slot;
        out["slots"] = slots;
        out["bytes"] = nbytes;
        out["members"] = members;
        out["ready_mask"] = ready_mask;
    }
    out["last_error"] = last_error;
    if (sd_ready) {
        out["sd_folder"] = sd.root;
        out["sd_cwd"] = sd.cwd_path();
        if (sd.file.open) {
            out["sd_file"] = sd.file.path;
            out["sd_file_pos"] = sd.file.pos;
            out["sd_file_size"] = sd.file.size;
            out["sd_page_unwritten"] = sd.file.dirty;
        }
    }
}

std::string Dpi1l::live_status()
{
    char b[96];
    uint8_t s = linked() ? (gstate == S_NOLINK ? S_READY : gstate) : S_NOLINK;
    if (pend_cmd) snprintf(b, sizeof(b), "NET: %s ...", pend_cmd == 0xA1 ? "create" : "join");
    else if (gstate >= S_IN_ROOM)
        snprintf(b, sizeof(b), "NET: %s, room %s, slot %d, %u members", k_state_names[s <= S_SPECTATOR ? s : 0], code,
                 slot == 0xFF ? -1 : slot, members);
    else snprintf(b, sizeof(b), "NET: %s", k_state_names[s <= S_SPECTATOR ? s : 0]);
    return b;
}

bool Dpi1l::live_param(const char* path) const
{
    return !strcmp(path, "net.transport") || !strcmp(path, "net.host") || !strcmp(path, "net.port");
}

bool Dpi1l::set_live_param(const char* path, const json& v, std::string& e)
{
    if (!strcmp(path, "net.transport")) {
        if (!v.is_string() || (v != "ws" && v != "tcp")) { e = "net.transport must be ws or tcp"; return false; }
        relay(relay_host, relay_port, v == "ws");
        return true;
    }
    if (!strcmp(path, "net.host")) {
        if (!v.is_string()) { e = "net.host must be a text"; return false; }
        relay(v.get<std::string>(), relay_port, relay_ws);
        return true;
    }
    if (!strcmp(path, "net.port")) {
        if (!v.is_number_integer() || v.get<long long>() < 1 || v.get<long long>() > 65535) {
            e = "net.port must be 1 ... 65535";
            return false;
        }
        relay(relay_host, (uint16_t)v.get<long long>(), relay_ws);
        return true;
    }
    e = std::string(path) + " cannot change while running";
    return false;
}
