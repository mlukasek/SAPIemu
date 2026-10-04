#pragma once
// ANK-1 - membrane keyboard of the original SAPI-1, a raw 5x8 matrix scanned by
// software (appendix VII of the manual, see docs/desky/klavesnice.md):
//   P0-OUT D4..D0  rows, 0 = active (several rows may be active at once)
//   P0-OUT D7 ERR LED, D6 beeper, D5 SHIFT LED (1 = on)
//   P0-IN  D7..D0  columns, pressed key on an active row = 0
//   P1-IN  D7 INT key, D6 T key (0 = pressed), D5..D0 = 1
// Text typed on the PC is turned into key presses (SHIFT + key for the shifted
// legends). A key stays pressed at least press_cycles and until the program has
// scanned its row; then it stays released the same way, so nothing is lost while
// the program is busy (MIKROBASIC beeps ~65 ms after every key).
// The shifted legends differ between the programs reading the matrix:
// MIKROBASIC (default) and MIKOS V4.
#include <deque>
#include "core/devices.h"

class Ank1 : public KeyboardDevice {
public:
    const char* type_name() const override { return software == SW_MIKOS ? "ANK-1 (MIKOS)" : "ANK-1"; }
    void reset() override {}
    void advance(uint64_t now) override;
    uint8_t p0_in() override;
    uint8_t p1_in() override;
    void p0_out(uint8_t value) override;
    void type_char(uint32_t cp) override;
    enum { SW_MIKROBASIC = 0, SW_MIKOS = 1 };
    int software = SW_MIKROBASIC;
    bool host_key(int key, int mods) override;
    size_t pending() const override { return queue.size() + (cur_key >= 0 ? 1 : 0); }
    void clear() override { queue.clear(); }
    std::string status() const override;
    void state(StateIO& io) override;

    uint64_t press_cycles = 80000;     // 40 ms at 2 MHz
    uint64_t release_cycles = 80000;
    uint64_t hold_cycles = 200000;     // INT / T switch

    // matrix position of a key: row 0..4 = P0-OUT D4..D0, column = P0-IN bit
    static const int SHIFT_KEY = 7;    // row 0, column 0 (index = row * 8 + 7 - column)

private:
    struct Press { int key; bool shift; };
    void press(int key, bool shift) { queue.push_back(Press{ key, shift }); }
    bool key_down(int key) const;

    std::deque<Press> queue;
    int cur_key = -1;          // key being held (-1 none)
    bool scanned = false;      // the program read the key's row in the current phase
    bool cur_shift = false;
    bool releasing = false;
    uint64_t phase_end = 0;
    uint64_t now = 0;
    uint8_t out = 0;           // P0-OUT latch
    bool int_key = false, t_key = false;
    uint64_t int_until = 0, t_until = 0;
};
