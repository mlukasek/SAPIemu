#include "cards/drv1a.h"
#include "core/bus.h"
#include "core/plant.h"
#include "core/state.h"

#include <stdio.h>

using nlohmann::json;

bool Drv1a::configure(const json& cfg, std::string& err)
{
    long long v;
    if (!cfg_number(cfg, "port", v, 0x77, err)) return false;
    if (v < 0x40 || v > 0x77 || (v & 0x08)) { err = "port must be 40h-47h, 50h-57h, 60h-67h or 70h-77h (jumpers)"; return false; }
    port = (uint8_t)v;
    std::string load;
    if (!cfg_string(cfg, "load", load, "none", err)) return false;
    if (load != "none" && load != "plant") { err = "load must be none or plant (the heaters of the furnaces)"; return false; }
    to_plant = load == "plant";
    return true;
}

void Drv1a::set(uint8_t v)
{
    latch = v;
    if (to_plant && bus && bus->plant) bus->plant->relays(latch, bus->now);
}

void Drv1a::reset()
{
    set(0);   // /RES clears the 3212
}

bool Drv1a::io_write(uint8_t p, uint8_t data)
{
    if (p != port) return false;
    set(data);
    return true;
}

int Drv1a::decode_io(uint8_t p, const char** what)
{
    if (p != port) return 0;
    *what = "relays";
    return DEC_WRITE;
}

void Drv1a::describe(json& out)
{
    char b[16];
    snprintf(b, sizeof(b), "%02Xh", port);
    out["port"] = b;
    snprintf(b, sizeof(b), "%02Xh", latch);
    out["contacts"] = b;
    out["load"] = to_plant ? "plant" : "none";
}

std::string Drv1a::live_status()
{
    char b[16];
    for (int i = 0; i < 8; i++) b[i] = (latch >> i) & 1 ? '1' + i : '-';
    b[8] = 0;
    return std::string("S1-S8: ") + b;
}

void Drv1a::state(StateIO& io)
{
    io.v(latch);
}
