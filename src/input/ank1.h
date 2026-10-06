#pragma once
// ANK-1 - membrane keyboard of the original SAPI-1, a raw 5x8 matrix scanned by
// software (appendix VII of the manual, see docs/desky/klavesnice.md):
//   P0-OUT D4..D0  rows, 0 = active (several rows may be active at once)
//   P0-OUT D7 ERR LED, D6 beeper, D5 SHIFT LED (1 = on)
//   P0-IN  D7..D0  columns, pressed key on an active row = 0
//   P1-IN  D7 INT key, D6 T key (0 = pressed), D5..D0 = 1
// Keys held on the PC are held on the matrix (hold_key, release_key; the game mode
// holds keys by position). Text typed through the queue (type_char: paste, MCP) is
// turned into taps of the same kind. A typed key stays pressed at least press_cycles
// and until the program has scanned its row; then it stays released the same way, so
// nothing is lost while the program is busy (MIKROBASIC beeps ~65 ms after every key).
// For the shifted legends SHIFT goes down before the key and up after it.
// The shifted legends differ between the programs reading the matrix:
// MIKROBASIC (default) and MIKOS V4.
// ANK-1M2 (Libor Lasota, 2024): the same matrix, LEDs, beeper and INT / T keys, plus an Atari
// joystick connector on P1-IN D5..D0 (input/joyport.h: a joystick or a parallel mouse).
#include <deque>
#include <vector>
#include "core/devices.h"
#include "input/joyport.h"

class Ank1 : public KeyboardDevice {
public:
    ~Ank1() override;
    const char* type_name() const override
    {
        if (m2) return "ANK-1M2";
        return software == SW_MIKOS ? "ANK-1 (MIKOS)" : "ANK-1";
    }
    void reset() override {}
    void advance(uint64_t now) override;
    uint8_t p0_in() override;
    uint8_t p1_in() override;
    void p0_out(uint8_t value) override;
    void type_char(uint32_t cp) override;
    enum { SW_MIKROBASIC = 0, SW_MIKOS = 1 };
    int software = SW_MIKROBASIC;
    bool host_key(int key, int mods) override;
    bool held_keys() const override { return true; }
    bool hold_key(int id, uint32_t cp, int hk, bool raw) override;
    void release_key(int id) override;
    void release_all() override;
    size_t pending() const override;
    void clear() override { queue.clear(); }
    std::string status() const override;
    void state(StateIO& io) override;
    int beeper_kind() const override { return Beeper::SPEAKER; }   // P0-OUT D6 drives it
    int joyport() const override { return m2 ? joy_dev : -1; }
    bool set_joyport(int dev) override;

    bool m2 = false;                   // ANK-1M2: the joystick connector
    int joy_dev = JOY_DEV_JOYSTICK;    // what is plugged into it
    uint64_t mouse_period = 2000;      // CPU cycles per quadrature step (cpu_hz / QuadMouse::RATE)

    uint64_t press_cycles = 80000;     // 40 ms at 2 MHz
    uint64_t release_cycles = 80000;
    uint64_t hold_cycles = 200000;     // INT / T switch
    uint64_t min_hold = 40000;         // a key held on the PC: at least this long (20 ms at 2 MHz)

    // matrix position of a key: row 0..4 = P0-OUT D4..D0, column = P0-IN bit
    static const int SHIFT_KEY = 7;    // row 0, column 0 (index = row * 8 + 7 - column)

private:
    struct Press { int key; bool shift; };
    void press(int key, bool shift) { queue.push_back(Press{ key, shift }); }
    bool key_down(int key) const;
    bool char_key(uint32_t cp, int& key, bool& shift) const;   // a typed character on the matrix
    bool special_key(int hk, int& key, bool& shift) const;     // Enter, Backspace... on the matrix

    // Keys held on the PC. With SHIFT (a symbol) the SHIFT key goes down first and the key only
    // after the program has seen SHIFT; on release the key goes up first and SHIFT after the
    // program has seen that, so no scan finds the key without its SHIFT. Every key stays down
    // until the program has read it, a short tap is not lost.
    // Typed keys (not raw) go down one after another, like the queue of type_char: down at least
    // press_cycles, then up at least release_cycles and until the program has read the rows
    // without them (MIKROBASIC takes the next key only after a scan without a key, it would lose
    // the overlapping keys of fast typing). Keys of the game mode (raw) go down together, at
    // least min_hold.
    struct Held {
        int id;            // PC key
        int key;           // matrix key
        bool shift;        // with SHIFT
        bool raw;          // the game mode
        bool started;      // down on the matrix (typed keys wait for their turn)
        bool up;           // the PC key was released
        bool shift_seen;   // the program has read the SHIFT row with SHIFT down
        bool seen;         // ... the key's row with the key down
        bool key_up;       // the key is up (SHIFT still down until seen_up)
        bool seen_up;      // the program has read the key's row with the key up
        bool shift_up;     // SHIFT is up too
        bool free_key;     // after that: the key's row read
        bool free_shift;   // ... the SHIFT row read
        uint64_t since;    // start of the current phase
    };
    std::vector<Held> held;
    bool held_active(const Held& h) const { return h.started && !h.key_up && (!h.shift || h.shift_seen); }
    void held_advance(void);
    void held_start(void);
    void held_scan(uint8_t rows);

    std::deque<Press> queue;   // typed text waiting: a tap when no typed key is held
    uint64_t now = 0;
    uint8_t out = 0;           // P0-OUT latch
    mutable uint8_t lit = 0;   // P0-OUT bits set since status() was read last (LEDs)
    bool int_key = false, t_key = false;
    uint64_t int_until = 0, t_until = 0;
    QuadMouse mouse;                   // ANK-1M2 with a mouse plugged in
    uint64_t mouse_next = 0;           // time of its next step
    void update_mouse_flag(void);
};
