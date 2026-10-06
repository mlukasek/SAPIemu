#include "chips/tape_deck.h"
#include "core/log.h"
#include "core/util.h"
#include "core/wav.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "minimp3_ex.h"

// ---------------------------------------------------------------- files

static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static bool parse_wav(const std::vector<uint8_t>& d, std::vector<int16_t>& out, int& rate, std::string& err)
{
    if (d.size() < 12 || memcmp(d.data(), "RIFF", 4) != 0 || memcmp(d.data() + 8, "WAVE", 4) != 0) {
        err = "not a WAV file";
        return false;
    }
    int fmt = 0, channels = 0, bits = 0;
    const uint8_t* data = nullptr;
    size_t data_size = 0;
    size_t p = 12;
    while (p + 8 <= d.size()) {
        uint32_t sz = rd32(&d[p + 4]);
        const uint8_t* c = &d[p + 8];
        size_t avail = d.size() - (p + 8);
        if (sz > avail) sz = (uint32_t)avail;   // truncated file: take what is there
        if (memcmp(&d[p], "fmt ", 4) == 0 && sz >= 16) {
            fmt = rd16(c);
            channels = rd16(c + 2);
            rate = (int)rd32(c + 4);
            bits = rd16(c + 14);
            if (fmt == 0xFFFE && sz >= 26) fmt = rd16(c + 24);   // WAVE_FORMAT_EXTENSIBLE: sub format
        } else if (memcmp(&d[p], "data", 4) == 0) {
            data = c;
            data_size = sz;
        }
        p += 8 + sz + (sz & 1);
    }
    if (!data || channels < 1 || rate < 8000) {
        err = "WAV without audio data";
        return false;
    }
    if (!((fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (fmt == 3 && bits == 32))) {
        err = "WAV format not supported (PCM 8/16/24/32 bit or float)";
        return false;
    }
    int bps = bits / 8;
    size_t frames = data_size / ((size_t)bps * channels);
    out.resize(frames);
    for (size_t i = 0; i < frames; i++) {
        double sum = 0;
        for (int ch = 0; ch < channels; ch++) {
            const uint8_t* s = data + (i * channels + ch) * bps;
            double v;
            if (fmt == 3) {
                float f;
                memcpy(&f, s, 4);
                v = f;
            } else if (bits == 8) {
                v = (s[0] - 128) / 128.0;
            } else if (bits == 16) {
                v = (int16_t)rd16(s) / 32768.0;
            } else if (bits == 24) {
                int32_t x = (int32_t)((uint32_t)s[0] << 8 | (uint32_t)s[1] << 16 | (uint32_t)s[2] << 24) >> 8;
                v = x / 8388608.0;
            } else {
                v = (int32_t)rd32(s) / 2147483648.0;
            }
            sum += v;
        }
        double m = sum / channels * 32767.0;
        out[i] = (int16_t)(m > 32767 ? 32767 : m < -32768 ? -32768 : m);
    }
    return true;
}

static bool parse_mp3(const std::vector<uint8_t>& d, std::vector<int16_t>& out, int& rate, std::string& err)
{
    mp3dec_t dec;
    mp3dec_file_info_t info;
    memset(&info, 0, sizeof(info));
    if (mp3dec_load_buf(&dec, d.data(), d.size(), &info, nullptr, nullptr) != 0 || !info.buffer || !info.channels) {
        if (info.buffer) free(info.buffer);
        err = "cannot decode the MP3 file";
        return false;
    }
    rate = info.hz;
    size_t frames = info.samples / (size_t)info.channels;
    out.resize(frames);
    for (size_t i = 0; i < frames; i++) {
        int sum = 0;
        for (int ch = 0; ch < info.channels; ch++) sum += info.buffer[i * info.channels + ch];
        out[i] = (int16_t)(sum / info.channels);
    }
    free(info.buffer);
    return true;
}

static bool ends_with(const std::string& s, const char* ext)
{
    size_t n = strlen(ext);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)s[s.size() - n + i]) != ext[i]) return false;
    return true;
}

bool TapeDeck::load(const std::string& p, std::string& err)
{
    if (tape_is_program(p)) return load_program(p, TapeProgram(), err);
    std::vector<uint8_t> d;
    std::string full = data_path(p);
    if (!file_read_all(full, d)) {
        err = "cannot read " + full;
        return false;
    }
    std::vector<int16_t> s;
    int r = 0;
    bool mp3 = ends_with(full, ".mp3");
    if (!(mp3 ? parse_mp3(d, s, r, err) : parse_wav(d, s, r, err))) return false;
    samples.swap(s);
    rate = r;
    path = full;
    is_mp3 = mp3;
    generated = false;
    program_info.clear();
    write_protect = true;
    modified = false;
    mode = STOP;
    pos = 0;
    line_dirty = true;
    LOGI("tape %s: %.1f s, %d Hz", full.c_str(), length_seconds(), rate);
    return true;
}

bool TapeDeck::load_program(const std::string& p, const TapeProgram& prog, std::string& err)
{
    std::string full = data_path(p);
    std::vector<int16_t> s;
    int r = 0;
    std::string info;
    if (!tape_from_file(full, prog, s, r, info, err)) return false;
    samples.swap(s);
    rate = r;
    path = full;
    is_mp3 = false;
    generated = true;
    program = prog;
    program_info = info;
    write_protect = true;
    modified = false;
    mode = STOP;
    pos = 0;
    line_dirty = true;
    LOGI("tape %s: %s, %.1f s", full.c_str(), info.c_str(), length_seconds());
    return true;
}

bool TapeDeck::export_wav(const std::string& p, std::string& err)
{
    if (!loaded()) {
        err = "no cassette in the recorder";
        return false;
    }
    std::string full = data_path(p);
    make_dirs(path_dir(full));
    if (!wav_write_mono16(full, samples.data(), samples.size(), (uint32_t)rate)) {
        err = "cannot write " + full;
        return false;
    }
    LOGI("tape exported: %s (%.1f s)", full.c_str(), length_seconds());
    return true;
}

bool TapeDeck::create(const std::string& p, std::string& err)
{
    std::string full = data_path(p);
    if (ends_with(full, ".mp3")) {
        err = "a new tape is a WAV file";
        return false;
    }
    samples.clear();
    rate = 44100;
    path = full;
    is_mp3 = false;
    generated = false;
    program_info.clear();
    write_protect = false;
    modified = true;   // an empty tape is written even when nothing was recorded
    mode = STOP;
    pos = 0;
    line_dirty = true;
    return save(err);
}

bool TapeDeck::save(std::string& err)
{
    if (!loaded() || !modified || read_only()) return true;
    std::vector<uint8_t> f(44 + samples.size() * 2);
    auto w32 = [&](size_t o, uint32_t v) { f[o] = (uint8_t)v; f[o + 1] = (uint8_t)(v >> 8); f[o + 2] = (uint8_t)(v >> 16); f[o + 3] = (uint8_t)(v >> 24); };
    auto w16 = [&](size_t o, uint16_t v) { f[o] = (uint8_t)v; f[o + 1] = (uint8_t)(v >> 8); };
    memcpy(&f[0], "RIFF", 4);
    w32(4, (uint32_t)(f.size() - 8));
    memcpy(&f[8], "WAVEfmt ", 8);
    w32(16, 16);
    w16(20, 1);
    w16(22, 1);
    w32(24, (uint32_t)rate);
    w32(28, (uint32_t)rate * 2);
    w16(32, 2);
    w16(34, 16);
    memcpy(&f[36], "data", 4);
    w32(40, (uint32_t)(samples.size() * 2));
    for (size_t i = 0; i < samples.size(); i++) w16(44 + i * 2, (uint16_t)samples[i]);
    make_dirs(path_dir(path));
    if (!file_write_all(path, f.data(), f.size())) {
        err = "cannot write " + path;
        return false;
    }
    modified = false;
    LOGI("tape saved: %s (%.1f s)", path.c_str(), length_seconds());
    return true;
}

void TapeDeck::eject(void)
{
    std::string err;
    if (!save(err)) LOGW("%s", err.c_str());
    path.clear();
    samples.clear();
    line.clear();
    generated = false;
    program_info.clear();
    mode = STOP;
    pos = 0;
}

// ---------------------------------------------------------------- recording

void TapeDeck::write(double from, double to, int16_t value)
{
    size_t a = (size_t)(from < 0 ? 0 : from), b = (size_t)(to < 0 ? 0 : to);
    if (b <= a) return;
    if (samples.size() < b) samples.resize(b, 0);
    for (size_t i = a; i < b; i++) samples[i] = value;
    modified = true;
    line_dirty = true;
}

// ---------------------------------------------------------------- playback

// Demodulation of the tape (Manchester: sent bit XOR clock, 2400 bit/s).
// - Zero crossings of the signal (DC removed, Schmitt trigger at 25 % of the
//   envelope).
// - A burst of tone starts with the leader (constant 1: crossings every 208 us);
//   the first interval of about 416 us marks the first change of the data, the
//   start bit: there the bit grid and the polarity of the recording are known.
// - Every bit cell is then decided on its own by comparing the signal in its
//   first and second half (integrate and dump), and the grid follows the
//   crossings in the middle of the cells (always present) like a PLL. A glitch
//   spoils one cell at most (MP3 copies of tapes have them), unlike a decoder
//   that only toggles the data on long intervals.
// - Silence (no crossings for 1.5 cells) ends the burst: the line is idle (1).
void TapeDeck::demodulate(void)
{
    line.clear();
    line_dirty = false;
    line.push_back(Edge{ 0, 1 });
    size_t n = samples.size();
    if (n == 0) return;
    const double T = rate / 2400.0;   // samples per bit
    const double us = 1e6 / rate;

    std::vector<int16_t> hp(n);       // DC removed signal
    std::vector<double> cr;           // crossings (sample positions)
    double dc = 0, env = 0;
    double dc_k = 1.0 - exp(-1.0 / (rate * 0.005));
    double env_decay = exp(-1.0 / (rate * 0.02));
    int state = 0;
    for (size_t i = 0; i < n; i++) {
        double v = samples[i] - dc;
        dc += (samples[i] - dc) * dc_k;
        hp[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        double a = fabs(v);
        env = a > env ? a : env * env_decay;
        double th = env * 0.25;
        if (th < 300) th = 300;   // noise floor
        int ns = state;
        if (v > th) ns = 1;
        else if (v < -th) ns = -1;
        if (ns != state && ns != 0) {
            if (state != 0) cr.push_back((double)i);
            state = ns;
        }
    }
    auto integ = [&](double a, double b) {
        size_t i0 = a <= 0 ? 0 : (size_t)ceil(a), i1 = b <= 0 ? 0 : (size_t)ceil(b);
        if (i1 > n) i1 = n;
        long long s = 0;
        for (size_t i = i0; i < i1; i++) s += hp[i];
        return s;
    };
    auto first_at = [&](double t) {   // index of the first crossing at or after t
        return (size_t)(std::lower_bound(cr.begin(), cr.end(), t) - cr.begin());
    };

    size_t k = 0;
    while (k + 1 < cr.size()) {
        // the first long interval of the burst: the start of the first start bit
        size_t j = k;
        while (j + 1 < cr.size() && (cr[j + 1] - cr[j]) * us <= 312) j++;
        if (j + 1 >= cr.size()) break;
        if ((cr[j + 1] - cr[j]) * us > 1000) {   // a gap before any data: the next burst
            k = j + 1;
            continue;
        }
        double b = (cr[j] + cr[j + 1]) * 0.5;   // cell boundary, the line goes 1 -> 0
        // polarity of the recording: the start bit cell holds a 0
        int pol = integ(b, b + T * 0.5) - integ(b + T * 0.5, b + T) < 0 ? 1 : -1;
        int level = 1;
        double period = T;
        for (;;) {
            long long dd = (integ(b, b + period * 0.5) - integ(b + period * 0.5, b + period)) * pol;
            int bit = dd > 0 ? 1 : 0;
            if (bit != level) {
                line.push_back(Edge{ b, bit });
                level = bit;
            }
            // follow the crossing in the middle of the cell
            double mid = b + period * 0.5;
            size_t ci = first_at(mid - period * 0.25);
            if (ci < cr.size() && fabs(cr[ci] - mid) < period * 0.25) {
                double e = cr[ci] - mid;
                b += e * 0.3;
                period += e * 0.02;
                if (period < T * 0.9) period = T * 0.9;
                if (period > T * 1.1) period = T * 1.1;
            }
            double next = b + period;
            size_t c2 = first_at(next);
            if (c2 >= cr.size() || cr[c2] - next > period * 1.5) {   // silence: end of the burst
                if (level != 1) line.push_back(Edge{ next, 1 });
                k = c2;
                break;
            }
            b = next;
        }
    }
}

int TapeDeck::line_level(double p)
{
    if (line_dirty) demodulate();
    if (line.empty()) return 1;
    size_t lo = 0, hi = line.size();
    while (hi - lo > 1) {
        size_t mid = (lo + hi) / 2;
        if (line[mid].at <= p) lo = mid;
        else hi = mid;
    }
    return line[lo].at <= p ? line[lo].level : 1;
}

double TapeDeck::next_start(double from, double to)
{
    if (line_dirty) demodulate();
    size_t lo = 0, hi = line.size();
    while (lo < hi) {   // first edge at or after `from`
        size_t mid = (lo + hi) / 2;
        if (line[mid].at < from) lo = mid + 1;
        else hi = mid;
    }
    for (size_t i = lo; i < line.size() && line[i].at < to; i++)
        if (line[i].level == 0) return line[i].at;
    return -1;
}
