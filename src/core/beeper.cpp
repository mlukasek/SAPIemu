#include "core/beeper.h"
#include "core/bus.h"
#include "core/state.h"

#include <math.h>

static const float SPEAKER_AMP = 7000.0f;
static const float BUZZER_AMP = 5000.0f;
static const float HP_A = 0.995f;   // DC blocker, about 40 Hz at 49716 Hz

void Beeper::advance(Bus* b)
{
    if (kind == NONE || !b || !b->audio_rate) return;
    double p = b->audio_pos(b->now);
    if (!started) {
        started = true;
        pos = p;
        index = (uint64_t)p;
        acc = 0;
        float x = kind == SPEAKER && on ? 1.0f : 0.0f;   // as if the line had been so for ever
        hp_x1 = x;
        hp_y1 = 0;
        return;
    }
    if (p <= pos) return;
    double level = kind == SPEAKER ? (on ? 1.0 : 0.0) : (on ? 1.0 : 0.0);
    while (p >= (double)(index + 1)) {
        acc += level * ((double)(index + 1) - pos);
        float x;
        if (kind == SPEAKER) {
            x = (float)acc;
        } else {   // the oscillator runs on its own, its output only while sounding
            phase += tone_hz / (double)b->audio_rate;
            phase -= floor(phase);
            x = (float)(acc * (phase < 0.5 ? 1.0 : -1.0));
        }
        float y = HP_A * (hp_y1 + x - hp_x1);
        hp_x1 = x;
        hp_y1 = y;
        float amp = kind == SPEAKER ? SPEAKER_AMP : BUZZER_AMP;
        int v = (int)lrintf(y * amp);
        if (v) b->audio_add(index, v);
        index++;
        pos = (double)index;
        acc = 0;
    }
    acc += level * (p - pos);
    pos = p;
}

void Beeper::set(Bus* b, bool level)
{
    advance(b);   // the old level up to now
    on = level;
}

void Beeper::state(StateIO& io)
{
    io.e(kind);
    io.v(tone_hz);
    io.v(on);
    io.v(started);
    io.v(pos);
    io.v(index);
    io.v(acc);
    io.v(phase);
    io.v(hp_x1);
    io.v(hp_y1);
}
