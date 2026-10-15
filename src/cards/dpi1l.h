#pragma once
// DPI-1L (Martin Lukasek, RP2350 card on the SAPI bus), only its network part: the Repository API (Unicard
// compatible) on base/base+1 with the NET commands A0h-A9h for network games through a relay (BomberNet).
// The emulation of other cards the real DPI does on the bus (DPP-1, IDE-1, RAM disk...) is not here: SAPIemu
// has those cards itself. Files on the SD card (20h-56h), RTC (60h-63h) and TCP/DNS (80h-89h) answer ERROR 1.
//
// The model is the verified DPI firmware (SAPI_DPI-firmware, common/repo.c and common/netgame.c, API v0.2); what
// the Z80 sees has to match it byte for byte. docs/desky/DPI-1L.md.
//   base     OUT command, IN status (4 bytes, then 00)
//   base + 1 OUT parameter byte, IN output byte
// The real card holds the Z80 in WAIT until an access is done; here every access is done at once. Only CREATE
// and JOIN take time: they go on in the background (status bit 6) while a thread talks to the relay.
#include <stdint.h>
#include <memory>
#include <string>
#include "core/card.h"

class RelayLink;

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
    void run_net(void);                   // a NET command (or RESET) with its parameters
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
