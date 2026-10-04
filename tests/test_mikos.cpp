// MIKOS V4.0F on ZPS-2 (JPR-1, REM-1, RAM-1/48K, AND-1, DSM-1, ANK-1): the
// monitor starts, a memory block goes to a new tape (.K_A, .K_S) and back
// (.K_L), and real tapes (TOOL 80, BASIC EX) load and run when available.
#include <string.h>
#include "cards/dsm1.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

static const char* REAL_TAPES = "H:/SAPI-1/SAPI_DOKU_2024-01-03/Kazetopáskové vybavení AUDIO WAV/";

static Dsm1* find_dsm(Machine& m)
{
    for (auto& c : m.cards)
        if (Dsm1* d = dynamic_cast<Dsm1*>(c.get())) return d;
    return nullptr;
}

// MIKOS reads a key only some time after printing a question
static bool answer(Machine& m, const char* prompt, const char* keys, double timeout_s)
{
    bool ok = run_until_text(m, prompt, timeout_s);
    m.run_seconds(0.5);
    m.type_text(keys);
    return ok;
}

static bool boot(Machine& m, Dsm1*& dsm)
{
    std::string err;
    CHECK(m.load_file("machines/zps/ZPS-2.sapi", err));
    dsm = find_dsm(m);
    CHECK(dsm != nullptr);
    if (!dsm) return false;
    CHECK(run_until_text(m, "MIKOS-1 V4.0F +", 2.0));
    CHECK(run_until_text(m, "SAPI-1 V4.0\n.", 1.0));
    m.run_seconds(0.5);
    return true;
}

// .K_L with any name: header found, confirmed, all blocks read back to the prompt
static int load_tape(Machine& m, Dsm1* dsm, const char* name, int blocks)
{
    m.type_text("KL");
    CHECK(answer(m, "NAME\n:", "*\r", 2.0));
    CHECK(answer(m, "PLAY  DONE?", "Y", 2.0));
    std::string q = std::string(name) + " --(Y/N)?";
    CHECK(answer(m, q.c_str(), "Y", 120.0));
    m.run_seconds(1.0);
    for (int i = 0; i < 300 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(run_until_text(m, "?Y\n\n.", 10.0));
    std::string s = screen_text(m);
    char status[8];
    snprintf(status, sizeof(status), "\" %02X", blocks);
    if (blocks >= 0) CHECK(s.find(status) != std::string::npos);   // the status line shows the last block
    CHECK(s.find("ERROR") == std::string::npos);
    return (int)s.size();
}

static void save_load(void)
{
    Machine m;
    Dsm1* dsm;
    if (!boot(m, dsm)) return;
    for (int i = 0; i < 0x300; i++) m.bus.mem_write((uint16_t)(0x5000 + i), (uint8_t)(i * 7 + 3));

    TapeDeck* t = dsm->tape();
    std::string err;
    CHECK(t->create("work/test/mikos_test.wav", err));
    t->mode = TapeDeck::RECORD;
    m.type_text("KA");
    CHECK(answer(m, "RECORD  DONE?", "Y", 2.0));
    CHECK(answer(m, "NEW  NAME\n:", "DATA\r", 2.0));
    CHECK(run_until_text(m, ":DATA\n\n\n.", 10.0));   // the header is on the tape
    m.run_seconds(0.5);
    m.type_text("KS");
    CHECK(answer(m, ".K_S\n=", "5000 52FF\r", 2.0));
    m.run_seconds(1.0);
    for (int i = 0; i < 120 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(!dsm->motor_on());
    m.run_seconds(0.5);
    printf("%s\n", screen_text(m).c_str());
    CHECK(t->length_seconds() > 3.0);
    CHECK(t->save(err));

    for (int i = 0; i < 0x300; i++) m.bus.mem_write((uint16_t)(0x5000 + i), 0);
    t->mode = TapeDeck::STOP;
    CHECK(t->load("work/test/mikos_test.wav", err));
    t->mode = TapeDeck::PLAY;
    m.type_text("\r");
    m.run_seconds(0.5);
    load_tape(m, dsm, "DATA", -1);
    printf("%s\n", screen_text(m).c_str());
    int bad = 0;
    for (int i = 0; i < 0x300; i++)
        if (m.bus.mem_peek((uint16_t)(0x5000 + i)) != (uint8_t)(i * 7 + 3)) bad++;
    CHECK_EQ(bad, 0);
}

static void real_tape(const char* file, const char* name, int blocks, const char* banner,
                      const char* input = nullptr, const char* expect = nullptr)
{
    std::string path = std::string(REAL_TAPES) + file;
    if (!file_exists(path)) {
        printf("%s not available, skipped\n", file);
        return;
    }
    Machine m;
    Dsm1* dsm;
    if (!boot(m, dsm)) return;
    std::string err;
    TapeDeck* t = dsm->tape();
    CHECK(t->load(path, err));
    t->mode = TapeDeck::PLAY;
    load_tape(m, dsm, name, blocks);
    m.type_text("J");   // checksum and start
    CHECK(run_until_text(m, banner, 3.0));
    if (input) {
        m.run_seconds(0.5);
        m.type_text(input);
        CHECK(run_until_text(m, expect, 5.0));
    }
    printf("%s\n", screen_text(m).c_str());
}

// Monitor V5.0F + MIKOS V5.0 with the Consul 262.3 keyboard: dialogs in Czech
// (NAZEV:, --(A/N)?), the motor starts without asking, "@NAME" writes where the
// tape is, a V4 tape (BASIC EX) loads and runs.
static bool boot_v5(Machine& m, Dsm1*& dsm, const char* file = "machines/zps/ZPS-2_MIKOS_V5.sapi")
{
    std::string err;
    CHECK(m.load_file(file, err));
    dsm = find_dsm(m);
    CHECK(dsm != nullptr);
    if (!dsm) return false;
    CHECK(run_until_text(m, "MIKOS V5.0 (C) TESLA DIZ 1987", 2.0));
    CHECK(run_until_text(m, "MONITOR V5.0\n.", 1.0));
    m.run_seconds(0.5);
    return true;
}

static void v5_save_load(void)
{
    Machine m;
    Dsm1* dsm;
    if (!boot_v5(m, dsm)) return;
    m.type_text("D0 7\r");
    CHECK(run_until_text(m, "0000 C3 8B 01 55 50 C3 06 F0", 2.0));   // the Consul keyboard works
    for (int i = 0; i < 0x200; i++) m.bus.mem_write((uint16_t)(0x5000 + i), (uint8_t)(i * 5 + 1));

    TapeDeck* t = dsm->tape();
    std::string err;
    CHECK(t->create("work/test/mikos5_test.wav", err));
    t->mode = TapeDeck::RECORD;
    m.run_seconds(0.5);
    m.type_text("KS");
    CHECK(answer(m, "NAZEV:", "@DATA\r", 2.0));
    CHECK(answer(m, "@DATA\n\n=", "5000 51FF\r", 10.0));
    m.run_seconds(1.0);
    for (int i = 0; i < 60 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(run_until_text(m, "=5000 =51FF\n\n.", 5.0));
    CHECK(t->save(err));

    for (int i = 0; i < 0x200; i++) m.bus.mem_write((uint16_t)(0x5000 + i), 0);
    t->mode = TapeDeck::STOP;
    CHECK(t->load("work/test/mikos5_test.wav", err));
    t->mode = TapeDeck::PLAY;
    m.type_text("KL");
    CHECK(answer(m, "K:L\n\n NAZEV:", "DATA\r", 2.0));
    m.run_seconds(1.0);
    for (int i = 0; i < 60 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(run_until_text(m, ":DATA\n\nDATA\n\n.", 5.0));
    printf("%s\n", screen_text(m).c_str());
    int bad = 0;
    for (int i = 0; i < 0x200; i++)
        if (m.bus.mem_peek((uint16_t)(0x5000 + i)) != (uint8_t)(i * 5 + 1)) bad++;
    CHECK_EQ(bad, 0);
}

static void v5_real_tape(void)
{
    std::string path = std::string(REAL_TAPES) + "BASIC EX V4.0.mp3";
    if (!file_exists(path)) {
        printf("BASIC EX V4.0.mp3 not available, skipped\n");
        return;
    }
    Machine m;
    Dsm1* dsm;
    if (!boot_v5(m, dsm)) return;
    std::string err;
    TapeDeck* t = dsm->tape();
    CHECK(t->load(path, err));
    t->mode = TapeDeck::PLAY;
    m.type_text("KL");
    CHECK(answer(m, "NAZEV:", "*\r", 2.0));
    CHECK(answer(m, "BASIC --(A/N)?", "A", 60.0));
    m.run_seconds(1.0);
    for (int i = 0; i < 300 && dsm->motor_on(); i++) m.run_seconds(0.5);
    CHECK(!dsm->motor_on());
    m.run_seconds(1.0);   // back at the prompt
    CHECK(screen_contains(m, "*\"BASIC      \" 01 1E"));
    m.type_text("J");
    CHECK(run_until_text(m, "READY", 3.0));
    m.run_seconds(0.5);
    m.type_text("PRINT 12*34+5\r");
    CHECK(run_until_text(m, " 413", 5.0));
    printf("%s\n", screen_text(m).c_str());
}

// BASIC EXP V5.0 and V5.0/G (Intel HEX from sapi.cz, from 4400H) under MIKOS V5;
// /G draws into the DGD-1 at C000H: line 0 = y 239, bit 7 = the left dot.
static bool load_basic_exp(Machine& m, Dsm1*& dsm, const char* hex, const char* banner,
                           const char* file = "machines/zps/ZPS-2_MIKOS_V5.sapi")
{
    if (!boot_v5(m, dsm, file)) return false;
    std::string text, err;
    CHECK(file_read_text(data_path(hex), text));
    int n = 0;
    CHECK(m.load_ihex_text(text, err, &n));
    CHECK(n > 9000);
    m.type_text("B");
    bool ok = run_until_text(m, banner, 3.0);
    CHECK(ok);
    CHECK(run_until_text(m, "READY\n:", 2.0));
    m.run_seconds(0.5);
    return ok;
}

static void type_lines(Machine& m, const char* const* lines)
{
    for (int i = 0; lines[i]; i++) {
        m.type_text(lines[i]);
        m.run_seconds(0.6);
    }
}

static void basic_exp(void)
{
    Machine m;
    Dsm1* dsm;
    if (!load_basic_exp(m, dsm, "media/mikos/BASIC_EXP_V5.0.hex", "* BASIC - EXP V5.0 *")) return;
    static const char* const prog[] = {
        "10 I=0\r", "20 REPEAT\r", "30 I=I+1:PRINT I;\r", "40 UNTIL I=5\r", "50 PRINT\r",
        "60 WHILE I>2 DO I=I-1:PRINT I;:WEND\r", "RUN\r", nullptr
    };
    type_lines(m, prog);
    CHECK(run_until_text(m, " 1  2  3  4  5\n 4  3  2", 5.0));
    printf("%s\n", screen_text(m).c_str());
}

// DGD-1 (monitor phosphor) or DGD-1V (VGA, colour jumpers S1, amber in the machine).
static void basic_exp_g(const char* file, const char* card)
{
    Machine m;
    Dsm1* dsm;
    if (!load_basic_exp(m, dsm, "media/mikos/BASIC_EXP_V5.0G.hex", "* BASIC - EXP V5.0/G *", file)) return;
    bool vga = strcmp(card, "DGD-1V") == 0;
    DisplayDevice* g = m.find_display(card);
    CHECK(g != nullptr && g != m.display);
    CHECK(m.display && m.display->text_display());
    m.bus.mem_write(0xC000, 0x55);   // CLSG clears the whole 16 KB
    m.type_text("CLSG:COLOR 2,255:MOVE 0,0:PLOT 10,0:MOVE 0,0:PLOT 0,5:MOVE 319,239:PLOT 316,239\r");
    m.run_seconds(3.0);   // the command line wraps on the 40 columns
    CHECK_EQ(m.bus.mem_peek(0xFBC0), 0xFF);   // y 0, x 0..7
    CHECK_EQ(m.bus.mem_peek(0xFBC1), 0xE0);   // x 8..10
    CHECK_EQ(m.bus.mem_peek(0xFA80), 0x80);   // y 5, x 0
    CHECK_EQ(m.bus.mem_peek(0xC027), 0x0F);   // y 239, x 316..319
    CHECK_EQ(m.bus.mem_peek(0xC000), 0x00);
    // the ink (monitor phosphor / jumpers S1): changed while running, written into the machine description
    std::string err;
    CHECK(g->ink_param() != nullptr && !strcmp(g->ink_param(), vga ? "color" : "monitor"));
    CHECK(g->ink() == (vga ? "amber" : "white"));
    CHECK(m.set_display_ink(g, "green", err));
    CHECK(!m.set_display_ink(g, "purple", err) && g->ink() == "green");
    bool in_config = false;
    for (const nlohmann::json& cc : m.config["cards"])
        if (cc.value("type", std::string()) == card) in_config = cc.value(g->ink_param(), std::string()) == "green";
    CHECK(in_config);
    uint32_t green = 0;
    CHECK(vga ? vga_ink("green", green) : monitor_phosphor("green", green));
    std::vector<uint32_t> px((size_t)g->screen_width() * g->screen_height());
    g->render(px.data(), 0xFF000000u);
    CHECK_EQ(g->screen_width(), 320);
    CHECK(px[239 * 320 + 10] == green && px[239 * 320 + 11] == 0xFF000000u);
    CHECK(px[0 * 320 + 319] == green);
    // the program keeps running with the graphics memory full
    m.type_text("CLSG 255:PRINT 6*7\r");
    CHECK(run_until_text(m, "\n 42", 5.0));
    CHECK_EQ(m.bus.mem_peek(0xD234), 0xFF);
}

// DGD-1V S2: base by /A15 /A14, MAP conditions as on CGA-1V.
static void dgd1v_s2(void)
{
    std::unique_ptr<Card> c(card_create("DGD-1V"));
    CHECK(c != nullptr);
    if (!c) return;
    std::string err;
    const char* what = nullptr;
    // the manual's second example: 0000h, only with MAP2 = H (like the PROM of DGD-1)
    CHECK(c->configure(nlohmann::json::parse(R"({ "base": "0000h", "map2": "h" })"), err));
    CHECK(c->decode_mem(0x0000, MAP_2, &what) != 0);
    CHECK(c->decode_mem(0x3FFF, MAP_2 | MAP_1, &what) != 0);
    CHECK(c->decode_mem(0x0000, 0, &what) == 0);
    CHECK(c->decode_mem(0x4000, MAP_2, &what) == 0);
    CHECK(c->configure(nlohmann::json::parse(R"({ "base": "8000h", "map1": "l", "map2": "h" })"), err));
    CHECK(c->decode_mem(0x8000, MAP_2, &what) != 0);
    CHECK(c->decode_mem(0x8000, MAP_1 | MAP_2, &what) == 0);
    CHECK(c->configure(nlohmann::json::parse(R"({ "map1": "never" })"), err));
    CHECK(c->decode_mem(0xC000, 0, &what) == 0);
    CHECK(!c->configure(nlohmann::json::parse(R"({ "base": "2000h" })"), err));
}

int main(void)
{
    test_init();
    save_load();
    real_tape("tool_80.wav", "TOOL 80", 0x1E, "PEDIT V2.5");
    real_tape("BASIC EX V4.0.mp3", "BASIC", 0x1E, "READY", "PRINT 12*34+5\r", " 413");
    v5_save_load();
    v5_real_tape();
    basic_exp();
    basic_exp_g("machines/zps/ZPS-2_MIKOS_V5.sapi", "DGD-1");
    basic_exp_g("machines/zps/ZPS-2_MIKOS_V5_DGD-1V.sapi", "DGD-1V");
    dgd1v_s2();
    return TEST_RESULT();
}
