#include "core/headless.h"
#include "core/machine.h"
#include "core/png.h"
#include "core/util.h"

#include <stdio.h>
#include <stdlib.h>

std::string screen_text(Machine& m)
{
    if (!m.display) return std::string();
    std::string out;
    for (const std::string& row : m.display->text_rows()) {
        std::string r = row;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        out += r;
        out += '\n';
    }
    return out;
}

bool screen_contains(Machine& m, const std::string& text)
{
    return screen_text(m).find(text) != std::string::npos;
}

bool run_until_text(Machine& m, const std::string& text, double timeout_s)
{
    const double slice = 0.01;
    for (double t = 0; t < timeout_s; t += slice) {
        if (screen_contains(m, text)) return true;
        m.run_seconds(slice);
        if (m.stopped_on_breakpoint()) return false;
    }
    return screen_contains(m, text);
}

static void print_regs(Machine& m)
{
    CpuRegs r;
    m.cpu->get_regs(r);
    printf("PC=%04X SP=%04X AF=%04X BC=%04X DE=%04X HL=%04X IX=%04X IY=%04X I=%02X IM=%d IFF1=%d%s\n",
           r.pc, r.sp, r.af, r.bc, r.de, r.hl, r.ix, r.iy, r.i, r.im, r.iff1 ? 1 : 0,
           r.halted ? " HALT" : "");
}

int headless_run(Machine& m, const std::vector<std::string>& actions)
{
    double timeout_s = 5.0;
    std::string shot_display;   // --display CARD: the output --screenshot takes
    for (size_t i = 0; i < actions.size(); i++) {
        const std::string& a = actions[i];
        bool has_arg = i + 1 < actions.size();
        if (a == "--run-ms" && has_arg) {
            m.run_seconds(atof(actions[++i].c_str()) / 1000.0);
            if (m.stopped_on_breakpoint()) printf("breakpoint hit\n");
        } else if (a == "--keyboard" && has_arg) {
            std::string err;
            if (!m.set_keyboard(actions[++i], err)) {
                fprintf(stderr, "%s\n", err.c_str());
                return 1;
            }
        } else if (a == "--type" && has_arg) {
            m.type_text(unescape_text(actions[++i]));
        } else if (a == "--timeout-ms" && has_arg) {
            timeout_s = atof(actions[++i].c_str()) / 1000.0;
        } else if (a == "--wait-text" && has_arg) {
            const std::string& t = actions[++i];
            if (!run_until_text(m, t, timeout_s)) {
                fprintf(stderr, "timeout waiting for text '%s'\n", t.c_str());
                printf("%s", screen_text(m).c_str());
                return 2;
            }
        } else if (a == "--print-screen") {
            printf("%s", screen_text(m).c_str());
        } else if (a == "--serial-send" && has_arg) {
            std::string s = unescape_text(actions[++i]);
            if (!m.serials.empty()) m.serials[0]->host_send((const uint8_t*)s.data(), s.size());
        } else if (a == "--print-serial") {
            std::string s;
            if (!m.serials.empty()) m.serials[0]->host_receive(s, 1 << 20);
            printf("%s", s.c_str());
        } else if (a == "--load-hex" && has_arg) {
            std::string text, err;
            const std::string& path = actions[++i];
            if (!file_read_text(data_path(path), text)) { fprintf(stderr, "cannot read %s\n", path.c_str()); return 1; }
            if (!m.load_ihex_text(text, err)) { fprintf(stderr, "%s: %s\n", path.c_str(), err.c_str()); return 1; }
        } else if (a == "--break" && has_arg) {
            long long addr;
            std::string v = actions[++i];
            if (!parse_number(v + (v.find_first_of("hHxX$") == std::string::npos ? "h" : ""), addr)) {
                fprintf(stderr, "invalid address '%s'\n", v.c_str());
                return 1;
            }
            m.set_breakpoint((uint16_t)addr, true);
        } else if (a == "--print-regs") {
            print_regs(m);
        } else if (a == "--disasm" && i + 2 < actions.size()) {
            long long addr, count;
            std::string v = actions[++i];
            if (!parse_number(v + (v.find_first_of("hHxX$") == std::string::npos ? "h" : ""), addr) ||
                !parse_number(actions[++i], count)) {
                fprintf(stderr, "usage: --disasm ADDR COUNT\n");
                return 1;
            }
            uint16_t pc = (uint16_t)addr;
            for (long long n = 0; n < count; n++) {
                DisasmResult d = m.disassemble(pc);
                printf("%04X  ", pc);
                for (int k = 0; k < 4; k++) {
                    if (k < d.length) printf("%02X ", m.peek((uint16_t)(pc + k)));
                    else printf("   ");
                }
                printf(" %s\n", d.text.c_str());
                pc = (uint16_t)(pc + d.length);
            }
        } else if (a == "--display" && has_arg) {
            shot_display = actions[++i];
        } else if (a == "--screenshot" && has_arg) {
            const std::string& path = actions[++i];
            DisplayDevice* d = m.find_display(shot_display);
            if (!d) { fprintf(stderr, "no display card %s\n", shot_display.c_str()); return 1; }
            int w = d->screen_width(), h = d->screen_height();
            std::vector<uint32_t> px((size_t)w * h);
            d->render(px.data(), 0xFF101010u);
            if (!png_write_argb(path, px.data(), w, h, 2, 2)) {
                fprintf(stderr, "cannot write %s\n", path.c_str());
                return 1;
            }
        } else {
            fprintf(stderr, "unknown or incomplete headless action '%s'\n", a.c_str());
            return 1;
        }
    }
    return 0;
}
