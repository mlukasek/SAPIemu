// DSP-1: ports (SA1, SA2), the two 8251 at 9600 Bd, the interrupt mask, and the XMODEM programs of
// J. Biba (media/dsp-1: DOPCP sends a file to the PC, DOCPMP takes one from it) under CP/M.
#include <string.h>
#include "core/devices.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "core/xmodem.h"
#include "test_util.h"

using nlohmann::json;

static json machine(const json& dsp)
{
    json cfg = { { "cards", json::array() } };
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    json c = dsp;
    c["slot"] = 2;
    c["type"] = "DSP-1";
    cfg["cards"].push_back(c);
    return cfg;
}

static void run_s(Machine& m, double s)   // the bus time in 50 us steps (the 8251s move bits on)
{
    uint64_t end = m.bus.now + (uint64_t)(s * m.bus.cpu_hz);
    while (m.bus.now < end) {
        m.bus.now += 100;
        m.bus.advance_all();
    }
}

static void test_ports(void)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(json::object()), err));
    SerialChannel* x2 = m.bus.cards[1]->serial_channel(0);
    SerialChannel* x3 = m.bus.cards[1]->serial_channel(1);
    CHECK(x2 && x3);
    if (!x2 || !x3) return;
    CHECK(x2->name == "DSP-1 X2 #48");
    CHECK(x3->name == "DSP-1 X3 #4A");
    // channel 2 at 48h/49h: mode 4Eh (x16, 8 bits, 1 stop), command 15h, as DOPCP
    for (uint8_t p : { 0x49, 0x4B }) {
        m.bus.io_write(p, 0);
        m.bus.io_write(p, 0);
        m.bus.io_write(p, 0);
        m.bus.io_write(p, 0x40);
        m.bus.io_write(p, 0x4E);
        m.bus.io_write(p, 0x15);
    }
    CHECK_EQ(m.bus.io_read(0x49) & 0x05, 0x05);   // TxRDY, TxEMPTY
    m.bus.io_write(0x48, 'A');
    m.bus.io_write(0x4A, 'B');
    run_s(m, 0.002);                               // 10 bits at 9600 Bd = 1.04 ms
    std::string s;
    x2->host_receive(s, 16);
    CHECK(s == "A");
    s.clear();
    x3->host_receive(s, 16);
    CHECK(s == "B");
    x2->host_send((const uint8_t*)"z", 1);
    run_s(m, 0.002);
    CHECK(m.bus.io_read(0x49) & 0x02);              // RxRDY
    CHECK_EQ(m.bus.io_read(0x48), 'z');
    // the mask register at 4Ch = 4Dh, write only; 4Eh, 4Fh nothing
    CHECK_EQ(m.bus.io_read(0x4C), 0xFF);
    CHECK_EQ(m.bus.irq_lines(), 0);
    m.bus.io_write(0x4D, 0x02);                     // TxRDY2: the transmitter is empty
    CHECK_EQ(m.bus.irq_lines(), IRQ_INT0);
    m.bus.io_write(0x4C, 0x01);                     // RxRDY2
    CHECK_EQ(m.bus.irq_lines(), 0);
    x2->host_send((const uint8_t*)"y", 1);
    run_s(m, 0.002);
    CHECK_EQ(m.bus.irq_lines(), IRQ_INT0);
    m.bus.io_read(0x48);
    CHECK_EQ(m.bus.irq_lines(), 0);
    m.bus.reset();                                  // /RES clears the mask
    x2->host_send((const uint8_t*)"x", 1);
    CHECK_EQ(m.bus.irq_lines(), 0);
    // other switches: channel 2 at 30h, channel 1 off, mask at 34h; 4800 Bd
    Machine o;
    CHECK(o.load_json(machine({ { "high", 3 }, { "ch2", "X0" }, { "ch1", "none" }, { "int", "X4" },
                                { "x2", { { "rate", 1 } } } }), err));
    CHECK_EQ(o.bus.io_read(0x48), 0xFF);            // nothing at 4xh
    o.bus.io_write(0x31, 0x4E);   // after /RES the first control write is the mode
    o.bus.io_write(0x31, 0x15);
    o.bus.io_write(0x30, 'C');
    run_s(o, 0.0015);                               // 10 bits at 4800 Bd = 2.08 ms
    s.clear();
    o.bus.cards[1]->serial_channel(0)->host_receive(s, 16);
    CHECK(s.empty());
    run_s(o, 0.001);
    o.bus.cards[1]->serial_channel(0)->host_receive(s, 16);
    CHECK(s == "C");
}

// CP/M from the ZRMD-1 RAM disk (ECMON option 1) with DSP-1 as J. Biba had it (48h, 4Ah, 4Ch)
static Machine* cpm_machine(void)
{
    std::string text, err;
    CHECK(file_read_text(data_path("machines/cpm_zrmd.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    if (cfg.is_discarded()) return nullptr;
    for (json& c : cfg["cards"])
        if (c["type"] == "ZRMD-1") {
            c["mode"] = "nosave";
            c["working_copy"] = false;
        }
    cfg["cards"].push_back({ { "slot", 5 }, { "type", "DSP-1" } });
    Machine* m = new Machine();
    CHECK(m->load_json(cfg, err));
    CHECK(run_until_text(*m, "Zadej 0-3", 3.0));
    m->type_text("1");
    CHECK(run_until_text(*m, "A>", 15.0));
    return m;
}

// a CP/M program from media/dsp-1 saved on the RAM disk
static bool put_program(Machine& m, const char* file, const char* name)
{
    std::vector<uint8_t> b;
    if (!file_read_all(data_path(std::string("media/dsp-1/") + file), b)) return false;
    for (size_t i = 0; i < b.size(); i++) m.poke((uint16_t)(0x100 + i), b[i]);
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "SAVE %d %s\r", (int)((b.size() + 255) / 256), name);
    m.type_text(cmd);
    cmd[strlen(cmd) - 1] = 0;
    return run_until_text(m, std::string("A>") + cmd + "\nA>", 10.0);   // the new prompt after it
}

static void test_xmodem(void)
{
    Machine* m = cpm_machine();
    if (!m) return;
    CHECK(put_program(*m, "dopcp.com", "DOPCP.COM"));
    CHECK(put_program(*m, "docpmp.com", "DOCPMP.COM"));
    SerialChannel* x2 = m->bus.cards[4]->serial_channel(0);
    CHECK(x2 != nullptr);
    if (!x2) { delete m; return; }

    // SAPI -> PC: DOPCP sends DOCPMP.COM itself
    m->type_text("DOPCP DOCPMP.COM\r");
    XmodemReceiver rx;
    rx.start();   // DOPCP does not wait for the first NAK: it sends the first packet right away
    for (int i = 0; i < 3000 && rx.state != XmodemReceiver::DONE && rx.state != XmodemReceiver::FAILED; i++) {
        m->run_seconds(0.01);
        std::string in;
        x2->host_receive(in, 4096);
        if (!in.empty()) {
            std::string out = rx.feed(in);
            if (!out.empty()) x2->host_send((const uint8_t*)out.data(), out.size());
        }
    }
    CHECK_EQ(rx.state, XmodemReceiver::DONE);
    std::vector<uint8_t> want;
    CHECK(file_read_all(data_path("media/dsp-1/docpmp.com"), want));
    std::vector<uint8_t> got = rx.file(false);
    CHECK_EQ(got.size(), 1280);   // SAVE 5 = 10 records
    got.resize(want.size());
    CHECK(got == want);
    m->run_seconds(2.0);

    // PC -> SAPI: DOCPMP takes a text file, then DOPCP sends it back
    std::vector<uint8_t> text;
    for (int i = 0; i < 300; i++) text.push_back((uint8_t)('A' + i % 26));
    text.push_back(0x1A);
    m->type_text("DOCPMP TEST.TXT\r");
    CHECK(run_until_text(*m, "DOCPMP", 2.0));
    XmodemSender tx;
    tx.start(text);
    int quiet = 0;
    for (int i = 0; i < 3000 && tx.active(); i++) {
        m->run_seconds(0.01);
        std::string in;
        x2->host_receive(in, 4096);
        std::string out = in.empty() ? std::string() : tx.feed(in);
        if (in.empty() && ++quiet > 400) { out = tx.resend(); quiet = 0; }
        if (!in.empty()) quiet = 0;
        if (!out.empty()) x2->host_send((const uint8_t*)out.data(), out.size());
    }
    CHECK_EQ(tx.state, XmodemSender::DONE);
    m->run_seconds(3.0);
    m->type_text("DOPCP TEST.TXT\r");
    XmodemReceiver back;
    back.start();
    for (int i = 0; i < 3000 && back.state != XmodemReceiver::DONE && back.state != XmodemReceiver::FAILED; i++) {
        m->run_seconds(0.01);
        std::string in;
        x2->host_receive(in, 4096);
        if (!in.empty()) {
            std::string out = back.feed(in);
            if (!out.empty()) x2->host_send((const uint8_t*)out.data(), out.size());
        }
    }
    CHECK_EQ(back.state, XmodemReceiver::DONE);
    std::vector<uint8_t> round = back.file(true);
    text.pop_back();
    CHECK(round == text);
    if (g_test_failures) printf("%s", screen_text(*m).c_str());
    delete m;
}

int main(void)
{
    test_init();
    test_ports();
    test_xmodem();
    return TEST_RESULT();
}
