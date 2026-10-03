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
    Dsm1();

    bool configure(const nlohmann::json& cfg, std::string& err) override;
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
    uint8_t base_port(void) const { return base; }
    bool motor_on(void) const;

private:
    bool cassette(void) const { return (modem_ctrl & 0x10) != 0; }
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
    Mhb1012 uart;
    std::unique_ptr<SerialChannel> serial;

    // cassette
    TapeDeck deck;
    uint64_t tape_time = 0;        // bus time the tape position belongs to
    double rx_scan = 0;            // tape position where the next start bit is looked for
    std::vector<Mhb1012::TxChar> tx_chars;   // characters sent to the recorder
};
