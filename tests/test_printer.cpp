// Printer (chips/printer.h): the text view leaves out ESC/P sequences, prints over at CR, breaks
// pages at FF and maps the upper half by its character set. ECMON prints LST: on X3 of
// JPR-1ZI (^P with DIR); with LST: = UL1: (IOBYTE D5h) the same goes through DPP-1 channel 1
// in 8255 mode 1 (/OBF, /ACK), docs/tiskarna.md.
#include "chips/printer.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

#include <stdio.h>
#include <string.h>

using nlohmann::json;

static void feed(Printer& p, const char* s, size_t n)
{
    for (size_t i = 0; i < n; i++) p.take((uint8_t)s[i], 0, 2000000);
}

static void text_view(void)
{
    Printer p;
    std::string err;
    CHECK(p.configure(json{ { "output", "" }, { "charset", "kamenicky" } }, err));
    static const char s[] = "\x1b@\x1bx\x01HELLO\r_____\r\n"   // ESC @, ESC x 1, overprint by underline
                            "\x1b*\x00\x03\x00\xff\xff\xff" "A\x87\r\n"   // 3 columns of 8-dot graphics, Kamenicky c-caron
                            "\x0cNEXT";
    feed(p, s, sizeof(s) - 1);
    CHECK(p.lines.size() == 3);
    if (p.lines.size() == 3) {
        CHECK(p.lines[0] == "HELLO");
        CHECK(p.lines[1] == "A\xc4\x8d");   // A, c with caron
        CHECK(p.lines[2] == "\f");
    }
    CHECK(p.current_line() == "NEXT");
    CHECK(p.bytes == sizeof(s) - 1);
    CHECK(p.busy(100) && !p.ack_due(100));   // 100 us at 2 MHz = 200 cycles
    CHECK(!p.busy(200) && p.ack_due(200) && !p.ack_due(300));
}

static json machine(const char* jpr_out, const char* dpp_out)
{
    std::string t;
    CHECK(file_read_text(data_path("machines/cpm_zrmd_dpp1.sapi"), t));
    json cfg = json::parse(t, nullptr, false, true);
    for (json& c : cfg["cards"]) {
        std::string ty = c.value("type", std::string());
        if (ty == "ZRMD-1") {
            c["mode"] = "readonly";
            c["working_copy"] = false;
        }
        if (ty == "DMR-1") c["battery"] = false;
        if (ty == "DMR-1" && c.contains("serial")) c["serial"].erase("tcp");
        if (c.contains("pmd32sd")) {
            c["pmd32sd"]["working_copy"] = false;
            c["pmd32sd"]["card_locked"] = true;
        }
        if (ty == "JPR-1ZI") c["printer"] = { { "output", jpr_out } };
        if (ty == "DPP-1") c["printer"] = { { "output", dpp_out } };
    }
    return cfg;
}

static Printer* printer_of(Machine& m, const char* type)
{
    for (auto& c : m.cards)
        if (c->type == type) return c->printer();
    return nullptr;
}

static void ecmon(bool dpp)
{
    printf("--- ECMON, LST: on %s\n", dpp ? "DPP-1 (UL1:)" : "JPR-1ZI X3");
    const char* jo = "work/test/prn_jpr.prn";
    const char* dout = "work/test/prn_dpp.prn";
    remove(data_path(jo).c_str());
    remove(data_path(dout).c_str());
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(jo, dout), err));
    m.type_text("1");   // ECMON variant 1 (RPD8, no floppy controller needed)
    CHECK(run_until_text(m, "PMD32-SD OK", 30.0));
    CHECK(run_until_text(m, "\nA>", 10.0));
    m.run_seconds(0.3);
    if (dpp) m.bus.mem_write(3, 0xD5);   // IOBYTE: LST: = UL1: (-> PUN: = DPP-1 channel 1)
    m.type_text("\x10" "DIR E:\r");      // ^P: the console also to LST:
    m.run_seconds(8.0);
    m.type_text("\x10");
    m.run_seconds(0.5);
    Printer* pj = printer_of(m, "JPR-1ZI");
    Printer* pd = printer_of(m, "DPP-1");
    CHECK(pj && pd);
    if (!pj || !pd) return;
    Printer* on = dpp ? pd : pj;
    Printer* off = dpp ? pj : pd;
    std::string text = on->text(100);
    printf("%s\n", text.c_str());
    CHECK(text.find("FORMFDC .MAC") != std::string::npos);
    CHECK(off->text(100).find("FORMFDC") == std::string::npos);
    m.flush();
    std::vector<uint8_t> raw;
    CHECK(file_read_all(on->path(), raw) && raw.size() > 500);
    if (!dpp) CHECK(!raw.empty() && raw[0] == 0x00);   // CINIT: OUT P1,3 ends the /STROBE held since the reset
}

int main(void)
{
    test_init();
    text_view();
    battery_files_enable(true);   // the printers write their files (work/test/)
    ecmon(false);
    ecmon(true);
    battery_files_enable(false);
    return TEST_RESULT();
}
