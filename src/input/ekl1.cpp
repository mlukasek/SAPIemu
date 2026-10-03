#include "input/ekl1.h"
#include "core/log.h"

void Ekl1::reset()
{
    // The converter has its own power-on reset; /RES of the computer does not
    // clear it. Only the handshake state follows the cleared 3212 outputs.
    strobe_low = false;
    ack = false;
}

void Ekl1::advance(uint64_t t)
{
    now = t;
    if (strobe_low || queue.empty() || now < next_allowed) return;
    if (!strict && ack) return;   // previous character not finished by the program yet
    data = queue.front();
    queue.pop_front();
    strobe_low = true;
    // A program that keeps ACK high gets only a very short strobe (real firmware).
    if (ack) {
        strobe_low = false;
        next_allowed = now + gap_cycles;
    }
}

uint8_t Ekl1::p0_in()
{
    // only bit 0 (STROBE) is connected, the other inputs float high
    return (uint8_t)(0xFE | (strobe_low ? 0 : 1));
}

uint8_t Ekl1::p1_in()
{
    return (uint8_t)~data;
}

void Ekl1::p0_out(uint8_t value)
{
    bool new_ack = (value & 0x01) != 0;
    bool new_beep = (value & 0x02) == 0;
    if (new_ack && !ack && strobe_low) {
        // firmware loop "JNB ACK,$" ends, STROBE goes high again
        strobe_low = false;
        next_allowed = now + gap_cycles;
    }
    ack = new_ack;
    if (new_beep != beep) {
        beep = new_beep;
        LOGD("EKL-1 beeper %s", beep ? "on" : "off");
    }
}
