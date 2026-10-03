// DMR-1 serial part (MHB1012): register layout, character timing, overrun of the
// holding register when the program does not wait for TBRE.
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

int main(void)
{
    data_root_init();
    Machine m;
    std::string err;
    json cfg;
    cfg["cards"] = json::array();
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    cfg["cards"].push_back({ { "slot", 2 }, { "type", "DMR-1" }, { "uart_base", "14h" }, { "baud", 9600 } });
    CHECK(m.load_json(cfg, err));
    CHECK_EQ(m.serials.size(), 1);
    SerialChannel* ch = m.serials[0];
    Bus& b = m.bus;

    // modem status: DCD active (bit 5 = 0) -> MikroMon detects the channel
    CHECK_EQ(b.io_read(0x14) & 0x20, 0);
    CHECK_EQ(b.io_read(0x10), 0xFF);              // no card at 10h
    b.io_write(0x15, 0x0D);                       // 8N1
    CHECK_EQ(b.io_read(0x15) & 0xC0, 0x40);       // TBRE=1, DR=0

    // transmit one character: 10 bits at 9600 Bd = 1.04 ms = 2083 cycles at 2 MHz
    b.io_write(0x16, 'A');
    b.now += 10; b.advance_all();
    CHECK_EQ(b.io_read(0x15) & 0x40, 0x40);       // moved to the shift register
    std::string rx;
    ch->host_receive(rx, 10);
    CHECK(rx.empty());
    b.now += 2100; b.advance_all();
    ch->host_receive(rx, 10);
    CHECK(rx == "A");

    // writing faster than the line overwrites the holding register (AY-5-1013 behaviour)
    rx.clear();
    b.io_write(0x16, '1'); b.now += 10; b.advance_all();   // -> shift register
    b.io_write(0x16, '2');                                 // holding register
    b.io_write(0x16, '3');                                 // overwrites '2'
    for (int i = 0; i < 10; i++) { b.now += 1000; b.advance_all(); }
    ch->host_receive(rx, 10);
    CHECK(rx == "13");

    // receive: a character is on the line for one character time before DR
    const uint8_t in[2] = { 'x', 'y' };
    ch->host_send(in, 2);
    b.now += 10; b.advance_all();
    CHECK_EQ(b.io_read(0x15) & 0x80, 0);          // still being received
    b.now += 2100; b.advance_all();
    CHECK_EQ(b.io_read(0x15) & 0x80, 0x80);       // DR
    CHECK_EQ(b.io_read(0x16), 'x');
    CHECK_EQ(b.io_read(0x15) & 0x80, 0);
    b.now += 10; b.advance_all();                 // pacing: 'y' starts after 'x' was read
    CHECK_EQ(b.io_read(0x15) & 0x80, 0);
    b.now += 2100; b.advance_all();
    CHECK_EQ(b.io_read(0x16), 'y');

    // without pacing the host sends at line speed: an unread character is overrun
    ch->pacing = false;
    const uint8_t in2[2] = { 'a', 'b' };
    ch->host_send(in2, 2);
    for (int i = 0; i < 10; i++) { b.now += 1000; b.advance_all(); }
    CHECK_EQ(b.io_read(0x14) & 0x02, 0x02);       // OE
    CHECK_EQ(b.io_read(0x16), 'b');
    return TEST_RESULT();
}
