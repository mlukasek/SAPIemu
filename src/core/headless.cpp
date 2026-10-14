#include "core/headless.h"
#include "cards/dsm1.h"
#include "core/machine.h"
#include "core/png.h"
#include "core/tapegen.h"
#include "core/util.h"
#include "core/wav.h"

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

// The sound recorded since --audio-wav, written to its file.
static bool write_wav(Machine& m, const std::string& path)
{
    m.bus.audio_record(false);
    if (!wav_write_mono16(path, m.bus.audio_rec.data(), m.bus.audio_rec.size(), m.bus.audio_rate)) {
        fprintf(stderr, "cannot write %s\n", path.c_str());
        return false;
    }
    printf("%s: %.1f s of sound\n", path.c_str(), (double)m.bus.audio_rec.size() / (double)m.bus.audio_rate);
    m.bus.audio_rec.clear();
    return true;
}

static bool hex_arg(const std::string& v, long long& out)
{
    return parse_number(v + (v.find_first_of("hHxX$") == std::string::npos ? "h" : ""), out) && out >= 0 && out <= 0xFFFF;
}

static Dsm1* first_dsm1(Machine& m)
{
    for (auto& c : m.cards)
        if (Dsm1* d = dynamic_cast<Dsm1*>(c.get()))
            if (d->has_tape()) return d;
    return nullptr;
}

static int run_actions(Machine& m, const std::vector<std::string>& actions, std::string& wav)
{
    double timeout_s = 5.0;
    std::string shot_display;   // --display CARD: the output --screenshot takes
    TapeProgram tape_prog;      // --tape-format, --tape-name, --tape-addr, --tape-entry
    for (size_t i = 0; i < actions.size(); i++) {
        const std::string& a = actions[i];
        bool has_arg = i + 1 < actions.size();
        if (a == "--tape-format" && has_arg) {
            if (!tape_format_parse(actions[++i], tape_prog.format)) {
                fprintf(stderr, "--tape-format: mikos or mikrobasic\n");
                return 1;
            }
        } else if (a == "--tape-name" && has_arg) {
            tape_prog.name = actions[++i];
        } else if ((a == "--tape-addr" || a == "--tape-entry") && has_arg) {
            long long v;
            if (!hex_arg(actions[++i], v)) {
                fprintf(stderr, "invalid address '%s'\n", actions[i].c_str());
                return 1;
            }
            (a == "--tape-addr" ? tape_prog.address : tape_prog.entry) = v;
        } else if (a == "--make-tape" && i + 2 < actions.size()) {   // program -> WAV, no recorder needed
            std::string in = actions[++i], out = actions[++i], info, err;
            std::vector<int16_t> s;
            int rate;
            if (!tape_from_file(data_path(in), tape_prog, s, rate, info, err) ||
                !wav_write_mono16(out, s.data(), s.size(), (uint32_t)rate)) {
                fprintf(stderr, "--make-tape %s: %s\n", in.c_str(), err.empty() ? "cannot write the WAV" : err.c_str());
                return 1;
            }
            printf("%s: %s, %.1f s\n", out.c_str(), info.c_str(), (double)s.size() / rate);
        } else if ((a == "--tape" || a == "--tape-export") && has_arg) {
            Dsm1* d = first_dsm1(m);
            if (!d) { fprintf(stderr, "no DSM-1 with a cassette recorder in the machine\n"); return 1; }
            TapeDeck* t = d->tape();
            std::string err, path = actions[++i];
            if (a == "--tape-export") {
                if (!t->export_wav(path, err)) { fprintf(stderr, "--tape-export: %s\n", err.c_str()); return 1; }
                continue;
            }
            t->eject();
            bool ok = tape_is_program(path) ? t->load_program(path, tape_prog, err) : t->load(path, err);
            if (!ok) { fprintf(stderr, "--tape %s: %s\n", path.c_str(), err.c_str()); return 1; }
            if (t->generated) printf("%s\n", t->program_info.c_str());
            t->mode = TapeDeck::PLAY;   // PLAY pressed; the tape moves when the program starts the motor
        } else if (a == "--run-ms" && has_arg) {
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
        } else if (a == "--joystick" && has_arg) {   // [PORT:]up,fire1,... held; "-" = released
            std::string spec = actions[++i];
            int port = 0;
            if (spec.size() > 1 && (spec[0] == '0' || spec[0] == '1') && spec[1] == ':') {
                port = spec[0] - '0';
                spec = spec.substr(2);
            }
            static const char* names[8] = { "down", "up", "right", "left", "fire1", "fire2", "button3", "button4" };
            uint8_t st = 0;
            size_t p = 0;
            while (spec != "-" && p < spec.size()) {
                size_t q = spec.find(',', p);
                std::string n = spec.substr(p, q == std::string::npos ? std::string::npos : q - p);
                int bit = -1;
                for (int k = 0; k < 8; k++)
                    if (n == names[k]) bit = k;
                if (bit < 0) { fprintf(stderr, "unknown joystick input '%s'\n", n.c_str()); return 1; }
                st |= (uint8_t)(1 << bit);
                if (q == std::string::npos) break;
                p = q + 1;
            }
            m.bus.joystick[port] = st;
        } else if (a == "--print-serial") {
            std::string s;
            if (!m.serials.empty()) m.serials[0]->host_receive(s, 1 << 20);
            printf("%s", s.c_str());
        } else if (a == "--load-hex" && has_arg) {
            std::string text, err;
            const std::string& path = actions[++i];
            if (!file_read_text(data_path(path), text)) { fprintf(stderr, "cannot read %s\n", path.c_str()); return 1; }
            if (!m.load_ihex_text(text, err)) { fprintf(stderr, "%s: %s\n", path.c_str(), err.c_str()); return 1; }
        } else if (a == "--save-state" && has_arg) {
            std::vector<uint8_t> d;
            std::string err, path = actions[++i];
            if (!m.save_state(d, m.config_path.empty() ? std::string() : data_relative(m.config_path), err) ||
                !file_write_all(path, d.data(), d.size())) {
                fprintf(stderr, "--save-state %s: %s\n", path.c_str(), err.empty() ? "cannot write" : err.c_str());
                return 1;
            }
        } else if (a == "--load-state" && has_arg) {
            std::vector<uint8_t> d;
            std::string err, path = actions[++i];
            if (!file_read_all(path, d)) { fprintf(stderr, "cannot read %s\n", path.c_str()); return 1; }
            if (!m.load_state(d, err, true)) { fprintf(stderr, "--load-state %s: %s\n", path.c_str(), err.c_str()); return 1; }
        } else if (a == "--load-bin" && i + 2 < actions.size()) {
            std::string err, path = actions[++i], v = actions[++i];
            long long addr;
            if (!parse_number(v + (v.find_first_of("hHxX$") == std::string::npos ? "h" : ""), addr) || addr < 0 || addr > 0xFFFF) {
                fprintf(stderr, "invalid address '%s'\n", v.c_str());
                return 1;
            }
            if (!m.load_binary(path, (uint16_t)addr, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
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
        } else if (a == "--audio-wav" && has_arg) {
            if (!wav.empty() && !write_wav(m, wav)) return 1;
            wav = actions[++i];
            if (!m.bus.audio_rate) { fprintf(stderr, "no sound card in the machine\n"); return 1; }
            m.bus.audio_record(true);
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

int headless_run(Machine& m, const std::vector<std::string>& actions)
{
    std::string wav;
    int rc = run_actions(m, actions, wav);
    if (!wav.empty() && !write_wav(m, wav) && rc == 0) rc = 1;
    return rc;
}
