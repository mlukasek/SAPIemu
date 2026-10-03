#include "core/bus.h"
#include "core/card.h"
#include "core/log.h"

// An undriven data bus reads FFh (pull-ups / TTL inputs). To be verified on HW.
static const uint8_t OPEN_BUS = 0xFF;

static void report_conflict(Bus* bus, const char* what, unsigned addr, Card* a, Card* b)
{
    bus->conflicts++;
    if (bus->warn_conflicts && bus->conflicts <= 20)
        LOGW("bus conflict on %s %04X: %s and %s both respond", what, addr, a->name.c_str(), b->name.c_str());
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
    return result;
}

void Bus::mem_write(uint16_t addr, uint8_t data)
{
    for (Card* c : cards)
        c->mem_write(addr, data);
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
    return result;
}

void Bus::io_write(uint8_t port, uint8_t data)
{
    for (Card* c : cards) {
        c->advance(now);
        c->io_write(port, data);
    }
    update_map();
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
