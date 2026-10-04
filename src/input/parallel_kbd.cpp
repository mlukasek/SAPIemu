#include "input/parallel_kbd.h"
#include "core/state.h"
#include "core/charset.h"
#include "core/log.h"

void ParallelKeyboard::reset()
{
    // The keyboard has its own power-on reset; /RES of the computer does not
    // clear it. Only the handshake state follows the cleared 3212 outputs.
    strobe_low = false;
    ack = false;
}

void ParallelKeyboard::press_clear(void)
{
    clr = true;
    clr_until = now + hold_cycles;
}

void ParallelKeyboard::advance(uint64_t t)
{
    beeper.advance(bus);
    now = t;
    if (clr && now >= clr_until) clr = false;
    if (!latched && strobe_low && now >= strobe_until) {
        strobe_low = false;   // end of the 1 ms pulse
        next_allowed = now + gap_cycles;
        if (!strict && !seen) queue.push_front(data);   // missed by the program: the same key again
    }
    if (strobe_low || queue.empty() || now < next_allowed) return;
    if (!strict && ack) return;   // previous character not finished by the program yet
    data = queue.front();
    queue.pop_front();
    strobe_low = true;
    seen = false;
    strobe_until = now + pulse_cycles;
    // latched: a program that keeps ACK high gets only a very short strobe
    if (latched && ack) {
        strobe_low = false;
        next_allowed = now + gap_cycles;
    }
}

uint8_t ParallelKeyboard::p0_in()
{
    if (strobe_low) seen = true;
    // only bit 0 (strobe) is connected, the other inputs float high
    return (uint8_t)(0xFE | (strobe_low ? 0 : 1));
}

uint8_t ParallelKeyboard::p1_in()
{
    uint8_t v = (uint8_t)~data;   // the data lines stay valid until the next key
    if (seven_bit) v = (uint8_t)((v & 0x7F) | (clr ? 0x00 : 0x80));   // /CLR active low
    return v;
}

void ParallelKeyboard::p0_out(uint8_t value)
{
    bool new_ack = (value & 0x01) != 0;
    bool new_beep = (value & 0x02) == 0;
    if (latched && new_ack && !ack && strobe_low) {
        // acknowledge: the strobe goes high again
        strobe_low = false;
        next_allowed = now + gap_cycles;
    }
    ack = new_ack;
    beeper.set(bus, new_beep);
    if (new_beep != beep) {
        beep = new_beep;
        LOGD("%s beeper %s (cycle %llu)", type_name(), beep ? "on" : "off", (unsigned long long)now);
    }
}

uint8_t koi8cs_keyboard_code(uint32_t cp, bool& ok)
{
    int c = sapi_code_from_unicode(cp);
    ok = c >= 0;
    if (!ok) return 0;
    // Czech keyboards (Consul 259.11 KOI-8 cs, EKL-1 in Czech mode) send C0-FF
    // with swapped case; ECMON and the CP/M drivers XOR 20h back.
    if (c >= 0xC0) c ^= 0x20;
    return (uint8_t)c;
}

// EKL-1 firmware tables (KEYB_REPAIR.a51, EXTEND / SHIFT EXTEND), WordStar style.
int wordstar_code(int key, int mods)
{
    bool shift = (mods & HM_SHIFT) != 0;
    switch (key) {
    case HK_ENTER: case HK_KP_ENTER: return 0x0D;
    case HK_BACKSPACE: return 0x08;
    case HK_TAB: return 0x09;
    case HK_ESCAPE: return 0x1B;
    case HK_UP: return shift ? 0x19 : 0x05;
    case HK_DOWN: return shift ? 0x17 : 0x18;
    case HK_LEFT: return shift ? 0x01 : 0x13;
    case HK_RIGHT: return shift ? 0x06 : 0x04;
    case HK_HOME: return shift ? 0x15 : 0x1E;
    case HK_END: return shift ? 0x0F : 0x11;
    case HK_PGUP: return shift ? -1 : 0x12;
    case HK_PGDN: return shift ? -1 : 0x03;
    case HK_INSERT: return shift ? -1 : 0x16;
    case HK_DELETE: return shift ? 0x1A : 0x0B;
    default: return -1;
    }
}

void ParallelKeyboard::state(StateIO& io)
{
    io.v(queue);
    io.v(data);
    io.v(strobe_low);
    io.v(strobe_until);
    io.v(ack);
    io.v(seen);
    io.v(beep);
    io.v(clr);
    io.v(clr_until);
    io.v(now);
    io.v(next_allowed);
    io.v(strict);
    beeper.state(io);
}
