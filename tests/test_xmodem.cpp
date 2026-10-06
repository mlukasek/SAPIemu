// Integration test: CP/M program DOPC10.COM sends a file with XMODEM (checksum)
// over DMR-1 to the host. The console stays on the screen (DCD inactive) while
// CTS is active - DOPC uses RTS/CTS handshake and waits for CTS.
// The other way: the XMODEM sender of the emulator (core/xmodem, the "Send file XMODEM"
// of the terminal window) sends a text file to DOCPM10.COM, TYPE shows it.
#include <stdio.h>
#include "core/headless.h"
#include "core/xmodem.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

using nlohmann::json;

int main(void)
{
    test_init();
    std::string text;
    CHECK(file_read_text(data_path("machines/cpm_ide.sapi"), text));
    json cfg = json::parse(text, nullptr, false, true);
    std::string img = path_join(data_root(), "work/tests/xmodem_ide.img");
    make_dirs(path_dir(img));
    remove(img.c_str());
    for (json& c : cfg["cards"]) {
        if (c["type"] == "ZRMD-1") { c["mode"] = "nosave"; c["working_copy"] = false; }   // A: writable, not saved
        if (c["type"] == "DMR-1") { c["connected"] = false; c["cts"] = true; c.erase("serial"); }
        if (c["type"] == "IDE-1") {
            c["ide"]["image"] = img;
            c["ide"]["create_mb"] = 16;
            c["pmd32sd"]["working_copy"] = false;
            c["pmd32sd"]["card_locked"] = true;
        }
    }
    Machine* m = new Machine();
    std::string err;
    CHECK(m->load_json(cfg, err));
    if (!err.empty()) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    SerialChannel* ch = m->serials[0];

    CHECK(run_until_text(*m, "Zadej 0-3", 3.0));
    m->type_text("3");
    CHECK(run_until_text(*m, "A>", 15.0));
    m->type_text("DOPC10 TD.MAC\r");
    CHECK(run_until_text(*m, "Soubor otevren", 10.0));

    // XMODEM receiver (checksum): NAK starts the transfer
    uint64_t pos = ch->host_end();
    std::string buf;
    const uint8_t nak = 0x15, ack = 0x06;
    ch->host_send(&nak, 1);
    int packets = 0, bad = 0, dups = 0;
    bool eot = false;
    for (int i = 0; i < 3000 && !eot; i++) {   // up to 30 s emulated
        m->run_seconds(0.01);
        ch->host_read(pos, buf, 4096);
        while (!buf.empty()) {
            uint8_t first = (uint8_t)buf[0];
            if (first == 0x04) { ch->host_send(&ack, 1); eot = true; buf.erase(0, 1); break; }
            if (first != 0x01) { buf.erase(0, 1); continue; }   // noise before a packet
            if (buf.size() < 132) break;
            uint8_t sum = 0;
            for (int k = 3; k < 131; k++) sum = (uint8_t)(sum + (uint8_t)buf[k]);
            bool ok = sum == (uint8_t)buf[131] && (uint8_t)buf[1] == (uint8_t)~(uint8_t)buf[2];
            uint8_t seq = (uint8_t)buf[1];
            if (ok && seq == (uint8_t)(packets + 1)) packets++;
            else if (ok && seq == (uint8_t)packets) {   // repeated packet: ACK and ignore (XMODEM)
                dups++;
                fprintf(stderr, "repeated packet %02X\n", seq);
            }
            else ok = false;
            if (!ok) {
                bad++;
                fprintf(stderr, "bad packet: seq %02X/%02X sum %02X/%02X expected seq %02X\n", seq,
                        (uint8_t)buf[2], sum, (uint8_t)buf[131], packets + 1);
            }
            ch->host_send(ok ? &ack : &nak, 1);
            buf.erase(0, 132);
        }
    }
    printf("packets %d, bad %d, EOT %d\n", packets, bad, eot ? 1 : 0);
    CHECK(eot);
    CHECK(packets > 10);
    CHECK_EQ(bad, 0);
    // A repeated first packet is normal XMODEM start-up behaviour; later repeats would
    // mean lost ACKs (the received character must have its line delay).
    CHECK(dups <= 1);
    CHECK(run_until_text(*m, "Soubor odeslan", 5.0));

    // PC -> SAPI: 14 lines of text (about 900 B, 8 packets) to DOCPM10.COM
    std::string file;
    for (int i = 1; i <= 14; i++) {
        char line[80];
        snprintf(line, sizeof(line), "RADEK %02d: PRILIS ZLUTOUCKY KUN UPEL DABELSKE ODY 0123456789\r\n", i);
        file += line;
    }
    m->run_seconds(1.0);
    m->type_text("DOCPM10 X.TXT\r");
    CHECK(run_until_text(*m, "PRIJEM SOUBORU", 10.0));
    XmodemSender x;
    x.start(std::vector<uint8_t>(file.begin(), file.end()));
    uint64_t rpos = ch->host_end();
    for (int i = 0; i < 6000 && x.active(); i++) {   // up to 60 s emulated
        m->run_seconds(0.01);
        std::string in;
        ch->host_read(rpos, in, 4096);
        std::string out = x.feed(in);
        if (!out.empty()) ch->host_send((const uint8_t*)out.data(), out.size());
    }
    printf("XMODEM send: state %d, %zu packets, %s, retries %d %s\n", x.state, x.packets, x.crc ? "CRC" : "checksum",
           x.retries, x.error.c_str());
    CHECK_EQ(x.state, XmodemSender::DONE);
    m->run_seconds(2.0);
    m->type_text("TYPE X.TXT\r");
    CHECK(run_until_text(*m, "RADEK 14: PRILIS", 10.0));
    printf("%s\n", screen_text(*m).c_str());

    delete m;
    remove(img.c_str());
    return TEST_RESULT();
}
