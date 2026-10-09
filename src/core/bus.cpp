#include "core/bus.h"
#include "core/state.h"
#include "core/card.h"
#include "core/devices.h"
#include "core/plant.h"
#include "core/log.h"

#include <stdio.h>
#include <string.h>

// An undriven data bus reads FFh (pull-ups / TTL inputs). To be verified on HW.
static const uint8_t OPEN_BUS = 0xFF;

Bus::Bus()
{
    memset(mem_watch, 0, sizeof(mem_watch));
    memset(io_watch, 0, sizeof(io_watch));
}

static void report_conflict(Bus* bus, const char* what, unsigned addr, Card* a, Card* b)
{
    bus->conflicts++;
    if (bus->warn_conflicts && bus->conflicts <= 20)
        LOGW("bus conflict on %s %04X: %s and %s both respond", what, addr, a->name.c_str(), b->name.c_str());
}

void Bus::watch_hit(const char* what, unsigned addr, uint8_t value)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "%s %04Xh = %02Xh (PC %04Xh)", what, addr, value, pc);
    break_reason = buf;
    break_request = true;
}

void Bus::log_io(uint8_t port, uint8_t value, bool write)
{
    IoLogEntry e = { now, pc, port, value, write };
    if (io_log.size() < IO_LOG_SIZE) {
        io_log.push_back(e);
    } else {
        io_log[io_log_next] = e;
        io_log_next = (io_log_next + 1) % IO_LOG_SIZE;
    }
}

void Bus::io_log_copy(std::vector<IoLogEntry>& out, size_t max) const
{
    out.clear();
    size_t n = io_log.size();
    size_t start = (n < IO_LOG_SIZE) ? 0 : io_log_next;
    size_t skip = n > max ? n - max : 0;
    for (size_t i = skip; i < n; i++) out.push_back(io_log[(start + i) % n]);
}

void Bus::set_mem_watch(uint16_t addr, int flags)
{
    mem_watch[addr] = (uint8_t)flags;
    any_mem_watch = false;
    for (int i = 0; i < 65536 && !any_mem_watch; i++) any_mem_watch = mem_watch[i] != 0;
}

uint8_t Bus::mem_read(uint16_t addr)
{
    uint8_t result = OPEN_BUS;
    Card* driver = nullptr;
    for (Card* c : cards) {
        uint8_t d;
        if (c->mem_read(addr, d)) {
            if (driver) { report_conflict(this, "memory read", addr, driver, c); result &= d; }
            else { driver = c; result = d; }
        }
    }
    if (any_mem_watch && (mem_watch[addr] & WATCH_READ)) watch_hit("read", addr, result);
    return result;
}

void Bus::mem_write(uint16_t addr, uint8_t data)
{
    for (Card* c : cards)
        c->mem_write(addr, data);
    if (any_mem_watch && (mem_watch[addr] & WATCH_WRITE)) watch_hit("write", addr, data);
    if (addr <= 2) cpm_poll();   // JP WBOOT written: the BIOS jump table is known before the first SELDSK
}

uint8_t Bus::io_read(uint8_t port)
{
    uint8_t result = OPEN_BUS;
    Card* driver = nullptr;
    for (Card* c : cards) {
        uint8_t d;
        c->advance(now);
        if (c->io_read(port, d)) {
            if (driver) { report_conflict(this, "I/O read", port, driver, c); result &= d; }
            else { driver = c; result = d; }
        }
    }
    if (io_log_enabled) log_io(port, result, false);
    if (io_watch[port] & WATCH_READ) watch_hit("IN", port, result);
    return result;
}

void Bus::io_write(uint8_t port, uint8_t data)
{
    for (Card* c : cards) {
        c->advance(now);
        c->io_write(port, data);
    }
    update_map();
    if (io_log_enabled) log_io(port, data, true);
    if (io_watch[port] & WATCH_WRITE) watch_hit("OUT", port, data);
}

uint8_t Bus::inta(void)
{
    for (Card* c : cards) {
        uint8_t d;
        if (c->inta(d)) {
            inta_card = c;
            return d;
        }
    }
    inta_card = nullptr;
    return OPEN_BUS;   // RST 38h in IM0
}

void Bus::power_on_fill(uint8_t* p, size_t n, uint8_t value)
{
    if (!power_on_random) {
        memset(p, value, n);
        return;
    }
    uint32_t x = power_on_rng ? power_on_rng : 1;
    for (size_t i = 0; i < n; i++) {   // xorshift32
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        p[i] = (uint8_t)(x >> 24);
    }
    power_on_rng = x;
}

void Card::power_on_fill(uint8_t* p, size_t n, uint8_t value)
{
    if (bus) bus->power_on_fill(p, n, value);
    else memset(p, value, n);
}

Card* Bus::mem_card(uint16_t addr)
{
    for (Card* c : cards) {
        uint8_t d;
        if (c->mem_peek(addr, d)) return c;
    }
    return nullptr;
}

void Bus::int_ack(uint16_t pc, int im, uint8_t i, uint8_t data)
{
    IntLogEntry e;
    e.time = now;
    e.pc = pc;
    e.im = (uint8_t)im;
    e.i = i;
    e.data = data;
    e.vector = 0;
    e.handler = 0x0038;
    if (im == 2) {
        e.vector = (uint16_t)(i << 8 | data);
        e.handler = (uint16_t)(mem_peek(e.vector) | mem_peek((uint16_t)(e.vector + 1)) << 8);
        Card* t = mem_card(e.vector);
        if (t) e.table = t->type;
        if (t && dynamic_cast<DisplayDevice*>(t) && int_warned.find(t->type) == std::string::npos) {
            int_warned += t->type + " ";
            LOGW("IM 2 interrupt at %04Xh: the vector %04Xh (I = %02Xh, bus %02Xh%s%s) is read from the video "
                 "memory of %s, handler %04Xh", pc, e.vector, i, data, inta_card ? " from " : ", nobody answered /INTA",
                 inta_card ? inta_card->type.c_str() : "", t->type.c_str(), e.handler);
        }
    } else if (im == 0) {
        e.handler = (data & 0xC7) == 0xC7 ? (uint16_t)(data & 0x38) : 0;   // RST n, else another opcode
    }
    if (!int_log_enabled) return;
    for (Card* c : cards)
        if (c->irq_lines() & IRQ_INT0) e.from += (e.from.empty() ? "" : ", ") + c->type;
    if (int_log.size() < INT_LOG_SIZE) int_log.push_back(e);
    else int_log[int_log_next] = e;
    int_log_next = (int_log_next + 1) % INT_LOG_SIZE;
}

void Bus::int_log_copy(std::vector<IntLogEntry>& out, size_t max) const
{
    out.clear();
    size_t n = int_log.size();
    size_t start = (n < INT_LOG_SIZE) ? 0 : int_log_next;
    size_t skip = n > max ? n - max : 0;
    for (size_t i = skip; i < n; i++) out.push_back(int_log[(start + i) % n]);
}

uint8_t Bus::mem_peek(uint16_t addr)
{
    for (Card* c : cards) {
        uint8_t d;
        if (c->mem_peek(addr, d)) return d;
    }
    return OPEN_BUS;
}

void Bus::cpm_poll(void)
{
    // CP/M page zero: JP WBOOT (BIOS base + 3); SELDSK is entry 9 of the BIOS jump table
    cpm_seldsk = 0;
    if (mem_peek(0x0000) != 0xC3) return;
    uint16_t wboot = (uint16_t)(mem_peek(0x0001) | (mem_peek(0x0002) << 8));
    uint16_t seldsk = (uint16_t)(wboot - 3 + 27);
    if (wboot >= 0x0103 && mem_peek(seldsk) == 0xC3 && mem_peek(wboot) == 0xC3) cpm_seldsk = seldsk;
}

void Bus::reset(void)
{
    map = 0;
    cpm_seldsk = 0;
    cpm_drive = -1;
    for (Card* c : cards) c->reset();
    update_map();
    ecmon_reset_check();
}

// ECMON (JPR-1Z/ZI/V, ZRMD-1, IDE-1) after a reset: with its signature AA 55 00 still in RAM it boots the
// variant in 003Bh without the menu, and does not check it is 0-3. A program that used 003Bh then boots
// something else and hangs (seen with the Prince of Persia port). The signature is at `CHECK EQU $+2` after
// the code, so it is found by the code that tests it: LDA CHECK, CPI 0AAH, JNZ, LDA CHECK+1, CPI 55H.
void Bus::ecmon_reset_check(void)
{
    for (uint32_t a = 0xF000; a + 13 <= 0xFFFF; a++) {
        if (mem_peek((uint16_t)a) != 0x3A || mem_peek((uint16_t)(a + 3)) != 0xFE || mem_peek((uint16_t)(a + 4)) != 0xAA ||
            mem_peek((uint16_t)(a + 5)) != 0xC2 || mem_peek((uint16_t)(a + 8)) != 0x3A || mem_peek((uint16_t)(a + 11)) != 0xFE ||
            mem_peek((uint16_t)(a + 12)) != 0x55)
            continue;
        uint16_t check = (uint16_t)(mem_peek((uint16_t)(a + 1)) | mem_peek((uint16_t)(a + 2)) << 8);
        if (mem_peek((uint16_t)(a + 9)) != (uint8_t)(check + 1)) continue;
        uint8_t variant = mem_peek(0x003B);
        if (mem_peek(check) == 0xAA && mem_peek((uint16_t)(check + 1)) == 0x55 && mem_peek((uint16_t)(check + 2)) == 0x00 &&
            variant > 3)
            LOGW("ECMON: after this reset it boots variant %u from 003Bh without the menu, only 0-3 exist (did a program "
                 "use 003Bh?). Hold a key 0-3 during the start for the menu, or switch the machine off and on.", variant);
        return;
    }
}

static const uint64_t AUDIO_MASTER = 14318182;   // the samples follow the MPH-1V oscillator
static const uint64_t AUDIO_DIV = 288;

uint64_t Bus::audio_index(uint64_t t) const
{
    uint64_t q = t / cpu_hz, r = t % cpu_hz;
    return (q * AUDIO_MASTER + r * AUDIO_MASTER / cpu_hz) / AUDIO_DIV;
}

double Bus::audio_pos(uint64_t t) const
{
    uint64_t q = t / cpu_hz, r = t % cpu_hz;
    return ((double)q * AUDIO_MASTER + (double)r * AUDIO_MASTER / cpu_hz) / AUDIO_DIV;
}

void Bus::audio_add(uint64_t index, int value)
{
    if (index < mix_base) {   // too late: already mixed out
        audio_lost++;
        return;
    }
    size_t off = (size_t)(index - mix_base);
    if (off > (size_t)audio_rate * 4) {   // nonsense (or audio_collect not called for 4 s)
        audio_lost++;
        return;
    }
    if (off >= mix.size()) mix.resize(off + 1, 0);
    mix[off] += value;
}

void Bus::audio_collect(void)
{
    advance_all();
    uint64_t end = audio_index(now);
    end = end > 2 ? end - 2 : 0;   // a source may still add to the last two samples
    if (end <= mix_base) return;
    size_t n = (size_t)(end - mix_base);
    std::vector<int16_t> out(n);
    for (size_t i = 0; i < n; i++) {
        int32_t v = i < mix.size() ? mix[i] : 0;
        out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    if (n >= mix.size()) mix.clear();
    else mix.erase(mix.begin(), mix.begin() + (ptrdiff_t)n);
    mix_base = end;
    audio_put(out.data(), n);
}

void Bus::audio_restart(void)
{
    mix.clear();
    mix_base = audio_index(now);
    audio.clear();
}

void Bus::audio_record(bool on)
{
    advance_all();   // the sound up to now belongs to the previous state
    if (on) {
        audio_rec.clear();
        audio_lost = 0;
    }
    audio_recording = on;
}

void Bus::audio_put(const int16_t* s, size_t n)
{
    if (audio_recording && audio_rec.size() < (size_t)audio_rate * 3600)   // at most an hour
        audio_rec.insert(audio_rec.end(), s, s + n);
    if (!audio_cap) return;
    if (audio.size() + n > audio_cap) {
        size_t drop = audio.size() + n - audio_cap;
        if (drop >= audio.size()) audio.clear();
        else audio.erase(audio.begin(), audio.begin() + (ptrdiff_t)drop);
    }
    audio.insert(audio.end(), s, s + n);
}

void Bus::advance_all(void)
{
    for (Card* c : cards) c->advance(now);
    if (plant) plant->advance(now);
}

bool Bus::rtl(void)
{
    for (Card* c : cards)
        if (c->rtl_line()) return true;
    return false;
}

int Bus::irq_lines(void)
{
    int irq = 0;
    for (Card* c : cards) irq |= c->irq_lines();
    return irq;
}

void Bus::update_map(void)
{
    int m = 0;
    for (Card* c : cards) m |= c->map_lines();
    map = m;
}

void Bus::state(StateIO& io)
{
    io.v(now);
    io.e(map);
    io.e(wait);
    io.v(conflicts);
    io.v(pc);
}
