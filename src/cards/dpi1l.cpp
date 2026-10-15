#include "cards/dpi1l.h"
#include "core/log.h"
#include "core/relay.h"
#include "core/state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <vector>

using nlohmann::json;

// Repository API (SAPI_DPI-firmware common/repo.c, common/repo.h)
enum { PH_DONE, PH_PARAMRQ, PH_DOUTRQ, PH_PROGRESS };   // PROGRESS: NET CREATE/JOIN, status bit 6

enum {
    ST_PARAMRQ = 0x01, ST_DOUTRQ = 0x02, ST_IN_PROGRESS = 0x40, ST_ERROR = 0x80,
};
enum { ERR_NOT_IMPLEMENTED = 1, ERR_OVERFLOW = 3, ERR_NO_FILE = 4 };

static const uint8_t VERSION_MAJOR = 0, VERSION_MINOR = 2;   // API of the DPI firmware the card answers as
static const uint8_t BOARD = 0x01;                           // REVD/INFO board byte: bit 0 = DPI-1L
static const uint8_t WIFI_CONNECTED = 3;                     // WIFISTATUS (MZPico numbering)
static const uint8_t FLASH_MB = 4;                           // INFO byte 2 (RP2350 board of DPI-1L)

// NET (common/netgame.c)
static const uint8_t S_NOLINK = 0, S_READY = 1, S_IN_ROOM = 2, S_RUNNING = 3, S_DESYNC = 4, S_DROPPED = 5,
                     S_SPECTATOR = 6;
static const uint8_t E_ROOM = 7, E_NOROOM = 8, E_NOLINK = 9, E_PARAM = 10, E_FULL = 11;   // 6 = build (from the relay)
static const int WINDOW = 128;
static const int MSGQ = 4;

// parameter formats of the commands the card runs with parameters (repo.c fs_cmds: the NET part and RESET)
static const char* net_format(uint8_t c)
{
    switch (c) {
    case 0x00: return "";                         // RESET
    case 0xA0: return "";                         // STATUS
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

    // power on: as repo_init and the NET variables of the firmware
    link_close();
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

// /RES of SAPI: the firmware leaves the NET room (firmware/main.c, dpi_bus_sapi_running falls); the Repository
// API state stays as it was (the next command sets it)
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
    if (c == 0x03) return;              // STSR: only rewinds the status pointer
    if (phase == PH_PROGRESS) pend_cmd = 0;   // a new command drops the pending CREATE/JOIN (as the Unicard)
    cmd = c;
    err = false;
    err_code = 0;
    phase = PH_DONE;
    len = 0;

    const char* f = net_format(c);
    if (f) {
        if (*f) want_params(f);
        else run_net();
        return;
    }
    switch (c) {
    case 0x01:                          // ASCII
    case 0x04:                          // STORNO
        break;
    case 0x05: {                        // REV
        int n = snprintf((char*)buf, sizeof(buf), "DPI-1L v%u.%u", VERSION_MAJOR, VERSION_MINOR);
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
    case 0x11:                          // INTCALLER: no interrupt sources
        buf[0] = 0;
        output(1);
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
    default:                            // SHASCII (Sharp only), files, RTC, TCP/DNS, BOOT: not here
        fail(ERR_NOT_IMPLEMENTED);
    }
}

void Dpi1l::data_out(uint8_t v)
{
    sts_pos = 0;
    if (phase != PH_PARAMRQ) {          // no file can be open here
        fail(ERR_NO_FILE);
        return;
    }
    if (fmt[fmt_pos] == 'B') {
        buf[len++] = v;
        count--;
        fmt_pos++;
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
        run_net();
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
    fail(ERR_NO_FILE);                  // nothing to read
    return 0x00;
}

uint8_t Dpi1l::status(void)
{
    uint8_t v;
    switch (sts_pos) {
    case 0:
        v = 0;
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
        else v = 0;
        break;
    case 3:
        v = 0;                          // FatFs result
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

// a command with its parameters (repo_fs_execute: RESET, repo_netgame_execute: A0h-A9h)
void Dpi1l::run_net(void)
{
    const uint8_t* p = buf;
    uint8_t res_err = 0;
    bool pending = false;
    uint8_t o[64];
    uint16_t on = 0;
    char l[200];

    if (cmd == 0x00) {                  // RESET: leave a NET room (as MZPico; nothing else without files)
        net_reset();
        return;
    }
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
        for (const uint8_t* s = p + 4; *s && n < 7; s++) {
            if (!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9'))) {
                n = 99;                 // only letters and digits (as the Unicard)
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

    // as repo_service
    if (pending) {
        phase = PH_PROGRESS;            // goes on in the background, complete() ends it
        err = false;
    } else if (res_err) {
        fail(res_err);
    } else if (on) {
        memcpy(buf, o, on);
        output(on);
    }
}

// ---- the rest of the card interface ----

void Dpi1l::state(StateIO& io)
{
    // a link to the relay can not be saved: after loading the card is as after RESET 00h (no room, 00 00 00 00)
    if (io.loading) {
        net_reset();
        phase = PH_DONE;
        cmd = 0;
        err = false;
        err_code = 0;
        sts_pos = 0;
        len = 0;
        count = 0;
    }
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
