#pragma once
// DPI-1L (Martin Lukasek, RP2350 card on the SAPI bus), its Repository API (Unicard compatible) on base/base+1:
// files and directories of the SD card over a folder of the PC (cards/dpi1l_fs), the configuration in
// /DPI/DPI.INI, RTC, BOOT, one TCP connection and DNS (80h-89h) and the NET commands A0h-A9h for network games
// through a relay (BomberNet). The emulation of other cards the real DPI does on the bus (DPP-1, IDE-1, RAM
// disk...) is not here: SAPIemu has those cards itself.
//
// The model is the verified DPI firmware (SAPI_DPI-firmware, common/repo.c, repo_fs.c and netgame.c, API v0.2, the
// same in v0.4.0, which REV and REVD report since 2026-10-20);
// what the Z80 sees has to match it byte for byte. docs/desky/DPI-1L.md.
//   base     OUT command, IN status (4 bytes, then 00)
//   base + 1 OUT parameter byte, IN output byte
// The real card holds the Z80 in WAIT until an access is done; here every access is done at once. Only CREATE
// and JOIN take time: they go on in the background (status bit 6) while a thread talks to the relay.
#include <stdint.h>
#include <memory>
#include <string>
#include "cards/dpi1l_fs.h"
#include "core/card.h"

class RelayLink;
class TcpLink;
class DnsQuery;

class Dpi1l : public Card {
public:
    Dpi1l() : Card("DPI-1L") {}
    ~Dpi1l() override;

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void reset() override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    void describe(nlohmann::json& out) override;
    void state(StateIO& io) override;
    std::string live_status() override;
    int decode_io(uint8_t port, const char** what) override;
    bool live_param(const char* path) const override;
    bool set_live_param(const char* path, const nlohmann::json& v, std::string& err) override;
    bool rtl_line() override;
    void flush() override;

    // the relay the NET commands use (a room on the old one is left)
    void relay(const std::string& host, uint16_t port, bool websocket);
    // CREATE/JOIN give up after this time of the PC clock (the firmware: 10 s; tests shorten it)
    uint64_t pending_timeout_ms = 10000;

private:
    // ---- Repository API (repo.c) ----
    void command(uint8_t c);
    void data_out(uint8_t v);
    uint8_t data_in(void);
    uint8_t status(void);
    void fail(uint8_t code);
    void output(uint16_t n);
    void want_params(const char* fmt);
    void post(uint8_t c);                 // a command with its parameters that the SD card side or NET runs
    void run_net(DpiSdResult& res, bool& pending);   // NET A0h-A9h (repo_netgame_execute)
    void run_params(void);                // SEEK, RTC with their parameters
    void run_tcp(DpiSdResult& res);       // TCP and DNS 80h-89h (common/net.c)
    void tcp_reset(void);                 // RESET 00h: close the connection, forget the DNS answer
    void file_putc(uint8_t v);
    void rtc_get(struct tm& t);
    void rtc_set(const struct tm& t);
    void complete(uint8_t err, const uint8_t* d, uint16_t n);   // the end of CREATE/JOIN (repo_complete)

    uint8_t base = 0xB0;
    uint8_t phase = 0;                    // PH_*
    uint8_t cmd = 0;
    bool err = false;
    uint8_t err_code = 0;
    uint8_t sts_pos = 0;                  // next status byte 0..3, 4 = past the end
    const char* fmt = "";                 // parameter format of the running command
    uint8_t fmt_pos = 0;
    uint8_t buf[256];                     // parameters, then the output
    uint16_t len = 0;                     // parameter bytes stored
    uint8_t count = 0;                    // PARAMRQ: free space left; DOUTRQ: output bytes left
    uint8_t out_pos = 0;
    uint8_t ff_res = 0;                   // FatFs result of the last command of the SD card side
    uint8_t rec[256];                     // directory or configuration record being streamed
    uint16_t rec_len = 0, rec_pos = 0;
    uint8_t dir_cmd = 0;                  // READDIR or FILELIST of the open directory (GETCONFIG too)

    // ---- SD card, RTC, BOOT ----
    DpiSd sd;
    bool sd_ready = false;                // the folder is there (not when only probing)
    bool flush_on_exit = false;           // deviation: the page not written yet survives switching off
    int64_t rtc_offset = 0;               // seconds the RTC is ahead of the PC clock (RTCSETD, RTCSETT)
    uint64_t boot_until = 0;              // BOOT: /RTL low until this bus time

    // ---- TCP and DNS (net.c) ----
    std::shared_ptr<TcpLink> tcp;
    uint8_t tstate = 0;                   // TCPSTATUS byte 1: 0 idle, 1 connecting, 2 connected, 3 failed, 4 closed
    uint8_t tcp_ip[4] = { 0, 0, 0, 0 }, tcp_default_ip[4] = { 0, 0, 0, 0 };
    uint16_t tcp_port = 0, tcp_default_port = 0;
    uint64_t connect_started = 0;         // ms of the PC clock
    std::shared_ptr<DnsQuery> dns;
    uint8_t dstate = 0;                   // DNSRES: 0 idle, 1 resolving, 2 resolved, 3 unknown host, 4 bad args
    uint8_t dns_ip[4] = { 0, 0, 0, 0 };
    uint64_t dns_started = 0;

    // ---- NET (netgame.c) ----
    void poll(void);                      // relay events, the 10 s limit of CREATE/JOIN (netgame_poll)
    void handle_line(const char* l);
    void link_lost(void);
    void link_close(void);
    void link_start(const std::string& path, const std::string& first);
    bool send_line(const char* l);
    void leave(void);
    void net_reset(void);                 // RESET 00h, /RES: leave the room (netgame_reset)
    void cancel_pending(void);            // a new command while CREATE/JOIN waits
    void window_reset(uint16_t from);
    void store_input(uint16_t frame, uint8_t s, const uint8_t* d, uint8_t n);
    uint16_t full_mask(void);
    uint16_t avail_frame(void);
    void pend_finish(uint8_t e, const uint8_t* out, uint16_t n);
    bool linked(void) const { return !relay_host.empty(); }

    std::string relay_host = "api.mzpico.com";
    uint16_t relay_port = 80;
    bool relay_ws = true;
    std::shared_ptr<RelayLink> link;

    uint8_t gstate = 0;                   // S_*: S_NOLINK stands for "nothing yet" too
    uint8_t slot = 0xFF, slots = 0, nbytes = 0, members = 0, ready_mask = 0, last_error = 0;
    uint8_t settings_len = 0, settings[16];
    char code[8];
    uint16_t seed = 0, start_frame = 0, mask = 0;
    bool started = false;
    uint16_t wbase = 0;                   // first frame not yet complete
    uint8_t have[128];                    // bit per slot: input present
    uint8_t data[128][4][4];
    struct Msg { uint8_t from, len, d[32]; } msgq[4];
    uint8_t mq_head = 0, mq_n = 0;
    uint8_t pend_cmd = 0;                 // 0, A1h or A2h
    uint64_t pend_since = 0;              // ms of the PC clock
};
