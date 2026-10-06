#pragma once
// Cassette tape recorder for the DSM-1: the tape is audio (WAV or MP3, any rate,
// mixed to mono), the recorder plays, records, stops and rewinds like a real one;
// the motor runs only while PLAY or RECORD is pressed and the computer closes the
// remote relay. The DSM-1 card does the modulation and the UART framing; the deck
// keeps the samples, the position and the demodulated line (see docs/desky/DSM-1.md).
// A program (Intel HEX, binary) becomes a tape computed when it is inserted
// (core/tapegen, docs/software/kazeta.md).
#include <stdint.h>
#include <string>
#include <vector>

#include "core/tapegen.h"

class TapeDeck {
public:
    enum Mode { STOP = 0, PLAY, RECORD };

    bool load(const std::string& path, std::string& err);     // .wav, .mp3; .hex as MIKOS
    // A program file made into a tape (read only, never written back).
    bool load_program(const std::string& path, const TapeProgram& p, std::string& err);
    // The tape as it is now into a WAV file (any tape; the tape itself stays).
    bool export_wav(const std::string& path, std::string& err);
    bool create(const std::string& path, std::string& err);   // new blank tape (WAV, 44.1 kHz)
    bool save(std::string& err);                              // write the WAV if recorded on
    void eject(void);

    bool loaded(void) const { return !path.empty(); }
    double length_seconds(void) const { return rate ? (double)samples.size() / rate : 0.0; }
    double position_seconds(void) const { return rate ? pos / rate : 0.0; }

    // Demodulated serial line (2400 bit/s Manchester): level at a sample position.
    int line_level(double sample_pos);
    // First falling edge (start of a start bit) at or after `from`, -1 if none before `to`.
    double next_start(double from, double to);
    // Recording: write `level` (already modulated) into samples [from, to).
    void write(double from, double to, int16_t value);

    std::string path;            // file the tape lives in
    bool is_mp3 = false;
    bool generated = false;      // made from a program file
    TapeProgram program;         // how (generated tapes)
    std::string program_info;    // range and size on the tape (generated tapes)
    bool read_only(void) const { return is_mp3 || generated; }
    bool write_protect = true;   // the tab of the cassette; MP3 and program tapes are always protected
    bool modified = false;
    Mode mode = STOP;
    int rate = 44100;
    double pos = 0.0;            // in samples
    std::vector<int16_t> samples;

private:
    void demodulate(void);
    struct Edge {
        double at;   // sample position
        int level;   // line level from here on
    };
    std::vector<Edge> line;
    bool line_dirty = true;
};
