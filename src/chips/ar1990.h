#pragma once
// The plotter of Amaterske radio 1990 (V. Julius OK1IVJ, "Souradnicovy zapisovac rizeny mikroprocesorem",
// docs/desky/AR1990.md): a JPR-1 with its own firmware (EPROM 0000-0FFF, RAM 2000-23FF) as the controller
// of a Minigraf 0507, commands of the Sharp MZ-1P16 from a host computer through a Centronics-like
// cable. This is the rest of the device, on the connectors K2 and K3 of the JPR-1:
//
// - P0-OUT (MOTR, 2400h): the Minigraf cable as it is (X1 X2 X4 Y1 Y2 Y4, D7 pen; "primo slucitelny se
//   vstupem plotteru MINIGRAF 0507").
// - P0-IN (RDSI, 2400h): D0 right stop, D1 left stop (not connected with the Minigraf: H), D2 switch
//   COLOR (L = colours asked for), D3 STOP, D4 TEST, D5 READY (buttons, L = pressed), D6 RDP (H = data
//   valid), D7 switch ASCII/MZ800 (L = MZ800: Sharp codes converted).
// - P1-IN (DSHI, 2800h): the data from the host.
// - P1-OUT (RDSO, 2800h): D2 RDA (data taken), D3 STA, D4 step 0.125/0.25 mm (for the decoder of
//   an own mechanism; the Minigraf ignores it), D5 speaker, D6 LED BUFF, D7 LED READY.
// - P2-IN (RDSI1, 2C00h): D0 button JMP 1000H (L), D5 switch A4/ROLE (L = roll), D6 LF (L = LF+CR),
//   D7 CR (L = CR+LF).
//
// The host (the window, MCP, a test) queues bytes; each is put on DSHI with RDP high, dropped when the
// firmware answers RDA, the next one comes when RDA is low again.
#include <deque>
#include "core/devices.h"
#include "chips/minigraf.h"

class Ar1990Plotter : public KeyboardDevice {
public:
    explicit Ar1990Plotter(uint32_t cpu_hz);
    const char* type_name() const override { return "AR 1990"; }
    void reset() override;
    void advance(uint64_t now) override;
    uint8_t p0_in() override;
    uint8_t p1_in() override;
    uint8_t p2_in() override;
    void p0_out(uint8_t value) override;
    void p1_out(uint8_t value) override;
    void type_char(uint32_t cp) override;   // typed text goes to the host queue
    size_t pending() const override { return host.size(); }
    void clear() override { host.clear(); }
    std::string status() const override;
    int beeper_kind() const override { return Beeper::SPEAKER; }
    void state(StateIO& io) override;
    Minigraf* minigraf() override { return &mg; }

    // the window, MCP
    enum { BTN_STOP = 0, BTN_TEST, BTN_READY, BTN_JMP1000, BUTTONS };
    void press(int button);               // held 150 ms
    void send(const uint8_t* data, size_t len);
    bool led_ready(void) const { return (out1 & 0x80) != 0; }
    bool led_buff(void) const { return (out1 & 0x40) != 0; }
    bool rda(void) const { return (out1 & 0x04) != 0; }
    // switches (the panel and the back of the plotter)
    bool color = false;                   // COLOR: the colour commands stop and beep for a pen change
    bool ascii = true;                    // ASCII / MZ800
    bool a4 = false;                      // A4 / ROLE: a new page on a new sheet or just a few lines
    bool cr_only = true, lf_only = true;  // CR / CR+LF, LF / LF+CR
    uint64_t sent = 0;                    // bytes taken by the firmware

private:
    uint64_t now(void) const;
    Minigraf mg;
    uint32_t hz;
    std::deque<uint8_t> host;
    bool rdp = false;                     // a byte waits on DSHI
    uint8_t data = 0xFF;
    uint8_t out1 = 0;                     // P1-OUT
    uint64_t held[BUTTONS] = { 0, 0, 0, 0 };   // pressed until this bus time
};
