#pragma once
// DSM-1 - serial interface with cassette tape recorder (see docs/desky/DSM-1.md).
//
// - MHB1012 UART, registers as on the DMR-1: base+0 modem status / control,
//   base+1 UART status / control, base+2 data, base+3 UART reset. Base 10h, 14h,
//   18h or 1Ch (jumpers 16-12..16-15), terminal baud rate by jumper (600 Bd from
//   the factory), 12.288 MHz crystal.
// - Modem control: b7 IER, b6 IET, b5 relay (recorder motor), b4 KAZ (the UART
//   works with the recorder at a fixed 2400 Bd instead of the terminal), b3 RD
//   (1 = read, 0 = record), b2 BRK, b1 DTR, b0 RTS.
// - Cassette: Manchester, sent bit XOR clock, 2400 bit/s; reading by the time
//   between zero crossings (208 / 416 us).
#include <memory>
#include <vector>
#include "chips/mhb1012.h"
#include "chips/tape_deck.h"
#include "core/devices.h"

class Dsm1 : public Card {
public:
    // "DSM-1", or "DSM-1V" for one channel of the DSM-1V: no cassette, baud
    // rate 230400 / (256 - N) from the 40103 divider (900 Bd to 115200 Bd)
    explicit Dsm1(const char* type = "DSM-1");

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    int irq_lines() override;
    SerialChannel* serial_channel(int index) override { return index == 0 ? serial.get() : nullptr; }
    void flush() override;
    void describe(nlohmann::json& out) override;
    int decode_io(uint8_t port, const char** what) override;

    TapeDeck* tape(void) { return &deck; }
    // the cassette in the recorder: tape
    const char* media_param(int u) override { return u == 0 ? "tape" : nullptr; }
    bool media_change(int u, const std::string& image, std::string& err) override;
    void media_discard(int u) override { if (u == 0) deck.modified = false; }
    bool has_tape(void) const { return !no_tape; }
    uint8_t base_port(void) const { return base; }
    bool motor_on(void) const;
    // DSM-1V: a memory read on the bus (/MR) ends the UART reset
    void bus_mem_read(void) { mr_held = false; }

private:
    bool cassette(void) const { return !no_tape && (modem_ctrl & 0x10) != 0; }
    bool no_tape = false;
    void set_mode(uint8_t ctrl);
    void tape_run(uint64_t now);
    void tape_receive(double p0, double p1);
    void tape_record(double p0, double p1, uint64_t t0, uint64_t t1);

    uint8_t base = 0x10;
    uint32_t baud = 600;           // terminal
    bool int_enabled = true;
    bool connected_default = true, cts_default = true;
    bool rts_flow = false;
    uint8_t modem_ctrl = 0;
    // DSM-1V: MR of the UART is a latch (74LS10) set by /RES and by a write to base+3 and cleared by the
    // next /MR on the bus (the JPR-1V does not give /MR for its ROM), measured on the real board 2026-10-11.
    // While set, data writes and received characters are lost and the status reads 7Fh. Not in the saved
    // state (it lasts until the first RAM read); a loaded state has it released.
    bool mr_held = false;
    Mhb1012 uart;
    std::unique_ptr<SerialChannel> serial;

    // cassette
    TapeDeck deck;
    uint64_t tape_time = 0;        // bus time the tape position belongs to
    double rx_scan = 0;            // tape position where the next start bit is looked for
    bool moving = false;           // the motor ran at the last tape_run
    double runup = 0;              // samples left of the motor run-up (recorded as silence)
    std::vector<Mhb1012::TxChar> tx_chars;   // characters sent to the recorder
};

// DSM-1V (Libor Lasota, docs/desky/DSM-1V.md): two DSM-1 channels without the
// cassette (6402 UARTs), each with its own base (10h/14h/18h/1Ch or disabled)
// and baud rate, both interrupts wired-OR to /INT0.
class Dsm1v : public Card {
public:
    Dsm1v() : Card("DSM-1V"), ch{ Dsm1("DSM-1V"), Dsm1("DSM-1V") } {}

    bool configure(const nlohmann::json& cfg, std::string& err) override;
    void state(StateIO& io) override;
    void reset() override;
    void advance(uint64_t now) override;
    bool io_read(uint8_t port, uint8_t& data) override;
    bool io_write(uint8_t port, uint8_t data) override;
    // not a memory card: a read on the bus only ends the MR of both UARTs (/IMR)
    bool mem_read(uint16_t addr, uint8_t& data) override;
    int irq_lines() override;
    SerialChannel* serial_channel(int index) override;
    void flush() override;
    void describe(nlohmann::json& out) override;
    int decode_io(uint8_t port, const char** what) override;

private:
    Dsm1 ch[2];
    bool enabled[2] = { true, true };
};

