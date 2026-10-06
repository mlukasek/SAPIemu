#pragma once
// Printer with a Centronics interface (EPSON LQ-1070, Robotron K6313...): a byte is taken with
// /STROBE, BUSY then stays high for busy_us, its end is the /ACK pulse. The bytes go unchanged
// to a file (ESC/P sequences kept); a text view of the printed paper (control characters and
// ESC/P sequences left out, the upper half by a character set) is kept for the GUI and MCP.
// Connected on the JPR cards (X3) and on DPP-1 (8255 mode 1), docs/tiskarna.md.
#include "json.hpp"

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

class StateIO;

struct Printer {
    enum Charset { CS_KOI8CS, CS_KAMENICKY, CS_CP852, CS_ASCII };

    std::string output;           // file for the raw bytes (as configured, relative to the data root)
    uint32_t busy_us = 100;       // BUSY after each byte; 0 = at once (a deviation)
    int charset = CS_KOI8CS;

    ~Printer();
    // the "printer" object of a card description
    bool configure(const nlohmann::json& p, std::string& err);

    // Centronics side (bus cycles, hz = their rate)
    void take(uint8_t b, uint64_t now, uint32_t hz);
    bool busy(uint64_t now) const { return now < busy_until; }
    bool ack_due(uint64_t now);   // once after a byte, when BUSY has ended (the /ACK pulse)
    void flush(void);
    void state(StateIO& io);

    // the printed paper
    std::vector<std::string> lines;   // finished lines (UTF-8); "\f" = a page break
    std::string current_line(void) const;
    uint64_t bytes = 0;           // bytes received since the start
    uint64_t generation = 0;      // changes with every printed character
    void new_sheet(void);         // a clean paper in the view (the file goes on)
    std::string text(size_t max_lines) const;   // the last lines, page breaks as form feeds
    std::string path(void) const; // the output file in use

private:
    uint64_t busy_until = 0;
    bool ack_pending = false;
    FILE* f = nullptr;
    bool open_failed = false;
    std::vector<uint32_t> cur;    // the line being printed (code points), col = print position
    size_t col = 0;
    int esc = 0;                  // ESC/P parser: 0 text, 1 after ESC, 2 skip `skip` bytes, 3 skip to NUL,
    uint32_t skip = 0;            // 4-6 header of a graphics command
    uint8_t esc_cmd = 0, esc_m = 0, esc_lo = 0;
    void put(uint8_t b);
    void glyph(uint32_t cp);
    void end_line(void);
};
