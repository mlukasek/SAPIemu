#include "input/joyport.h"

static const char* const k_names[4] = { "none", "joystick", "mouse_atari", "mouse_amiga" };

bool joy_device_parse(const nlohmann::json& v, int& out, std::string& err, const char* key)
{
    std::string s = v.is_string() ? v.get<std::string>() : std::string("?");
    for (int i = 0; i < 4; i++)
        if (s == k_names[i]) {
            out = i;
            return true;
        }
    err = std::string(key) + " must be none, joystick, mouse_atari or mouse_amiga";
    return false;
}

const char* joy_device_name(int dev)
{
    return dev >= 0 && dev < 4 ? k_names[dev] : "none";
}

void QuadMouse::step(Bus* bus)
{
    const int cap = RATE / 4;   // at most 250 ms behind the host, then it is dropped
    int& dx = bus->mouse_dx;
    int& dy = bus->mouse_dy;
    if (dx > cap) dx = cap;
    if (dx < -cap) dx = -cap;
    if (dy > cap) dy = cap;
    if (dy < -cap) dy = -cap;
    if (dx > 0) { mx = (uint8_t)((mx + 1) & 3); dx--; }
    else if (dx < 0) { mx = (uint8_t)((mx + 3) & 3); dx++; }
    if (dy > 0) { my = (uint8_t)((my + 1) & 3); dy--; }
    else if (dy < 0) { my = (uint8_t)((my + 3) & 3); dy++; }
}

uint8_t joy_port_read(int dev, int host_joy, const QuadMouse& mouse, const Bus* bus)
{
    switch (dev) {
    case JOY_DEV_JOYSTICK:
        return (uint8_t)(~bus->joystick[host_joy & 1] | 0xC0);
    case JOY_DEV_MOUSE_ATARI:
    case JOY_DEV_MOUSE_AMIGA: {
        // quadrature: phase 0-3 -> (A, B) = 00, 10, 11, 01; A leads B moving right / down
        static const uint8_t qa[4] = { 0, 1, 1, 0 }, qb[4] = { 0, 0, 1, 1 };
        uint8_t v = 0xFF;
        if (bus->mouse_buttons & MOUSE_LEFT) v &= (uint8_t)~0x10;    // pin 6
        if (bus->mouse_buttons & MOUSE_RIGHT) v &= (uint8_t)~0x20;   // pin 9
        v &= 0xF0;
        uint8_t xa = qa[mouse.mx], xb = qb[mouse.mx], ya = qa[mouse.my], yb = qb[mouse.my];
        if (dev == JOY_DEV_MOUSE_ATARI)   // pin 2 XA, pin 1 XB, pin 4 YB, pin 3 YA
            v |= (uint8_t)(xa | xb << 1 | yb << 2 | ya << 3);
        else                              // pin 2 H, pin 1 V, pin 4 HQ, pin 3 VQ
            v |= (uint8_t)(xa | ya << 1 | xb << 2 | yb << 3);
        return v;
    }
    default:
        return 0xFF;   // nothing plugged in: the pull-ups
    }
}
