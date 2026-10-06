// Integration test: CP/M program DOPC10.COM sends a file with XMODEM (checksum)
// over DMR-1 to the XMODEM receiver of the emulator (core/xmodem). The console stays on the screen (DCD inactive) while
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

    // the XMODEM receiver of the emulator (core/xmodem, "Receive file XMODEM" of the terminal window)
    uint64_t pos = ch->host_end();
    XmodemReceiver r;
    std::string first = r.start();
    ch->host_send((const uint8_t*)first.data(), first.size());
    for (int i = 0; i < 3000 && r.active(); i++) {   // up to 30 s emulated
        m->run_seconds(0.01);
        std::string in;
        ch->host_read(pos, in, 4096);
        std::string out = r.feed(in);
        if (!out.empty()) ch->host_send((const uint8_t*)out.data(), out.size());
    }
    std::vector<uint8_t> td = r.file(true);
    size_t printable = 0;
    for (uint8_t c : td) printable += (c >= 0x20 && c < 0x7F) || c == '\r' || c == '\n' || c == '\t';
    printf("XMODEM receive: state %d, %zu packets, %d bad, %zu B (%zu printable) %s\n", r.state, r.packets, r.bad,
           td.size(), printable, r.error.c_str());
    CHECK_EQ(r.state, XmodemReceiver::DONE);
    CHECK(r.packets > 10);
    CHECK_EQ(r.bad, 0);
    CHECK(!td.empty() && printable == td.size());   // TD.MAC is source text, the 1Ah padding cut off
    CHECK(r.file(false).size() == r.packets * 128);
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
