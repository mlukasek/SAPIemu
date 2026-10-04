#include "core/bus.h"
#include "core/card.h"
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
        if (c->inta(d)) return d;
    }
    return OPEN_BUS;   // RST 38h in IM0
}

uint8_t Bus::mem_peek(uint16_t addr)
{
    for (Card* c : cards) {
        uint8_t d;
        if (c->mem_peek(addr, d)) return d;
    }
    return OPEN_BUS;
}

void Bus::reset(void)
{
    map = 0;
    for (Card* c : cards) c->reset();
    update_map();
}

void Bus::audio_put(const int16_t* s, size_t n)
{
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
