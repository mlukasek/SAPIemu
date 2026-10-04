#include "core/devices.h"

static const size_t SERIAL_HISTORY_MAX = 1 << 20;
static const size_t SERIAL_RX_MAX = 1 << 20;

bool monitor_phosphor(const std::string& name, uint32_t& argb)
{
    struct P { const char* name; uint32_t argb; };
    static const P phosphors[] = {
        { "white", 0xFFE8E8E8u }, { "green", 0xFF40FF70u }, { "amber", 0xFFFFB020u },
    };
    for (const P& p : phosphors)
        if (name == p.name) { argb = p.argb; return true; }
    return false;
}

// red, green (2 bits: GRH 680R, GRL 1k5), blue
bool vga_ink(const std::string& name, uint32_t& argb)
{
    struct C { const char* name; uint32_t argb; };
    static const C colors[] = {
        { "white", 0xFFFFFFFFu }, { "green", 0xFF00FF00u }, { "amber", 0xFFFFAA00u }, { "red", 0xFFFF0000u },
        { "blue", 0xFF0000FFu }, { "cyan", 0xFF00FFFFu }, { "yellow", 0xFFFFFF00u }, { "magenta", 0xFFFF00FFu },
    };
    for (const C& c : colors)
        if (name == c.name) { argb = c.argb; return true; }
    return false;
}

bool MonitorInk::configure(const nlohmann::json& cfg, std::string& err)
{
    std::string v;
    if (!cfg_string(cfg, "monitor", v, "white", err)) return false;
    return set(v, err);
}

bool MonitorInk::set(const std::string& value, std::string& err)
{
    uint32_t c;
    if (!monitor_phosphor(value, c)) { err = "monitor must be white, green or amber"; return false; }
    name = value;
    argb = c;
    return true;
}

void SerialChannel::to_host(uint8_t b)
{
    std::lock_guard<std::mutex> lock(mtx);
    history.push_back(b);
    if (history.size() > SERIAL_HISTORY_MAX) {
        history.pop_front();
        base++;
    }
}

bool SerialChannel::from_host(uint8_t& b)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (rx_q.empty()) return false;
    b = rx_q.front();
    rx_q.pop_front();
    return true;
}

void SerialChannel::host_send(const uint8_t* data, size_t n)
{
    std::lock_guard<std::mutex> lock(mtx);
    for (size_t i = 0; i < n && rx_q.size() < SERIAL_RX_MAX; i++) rx_q.push_back(data[i]);
}

size_t SerialChannel::host_read(uint64_t& pos, std::string& out, size_t max)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (pos < base) pos = base;   // older data was dropped
    uint64_t end = base + history.size();
    size_t n = 0;
    while (pos < end && n < max) {
        out += (char)history[(size_t)(pos - base)];
        pos++;
        n++;
    }
    return n;
}

size_t SerialChannel::host_receive(std::string& out, size_t max)
{
    uint64_t pos;
    {
        std::lock_guard<std::mutex> lock(mtx);
        pos = default_pos;
    }
    size_t n = host_read(pos, out, max);
    std::lock_guard<std::mutex> lock(mtx);
    default_pos = pos;
    return n;
}

uint64_t SerialChannel::host_end(void) const
{
    std::lock_guard<std::mutex> lock(mtx);
    return base + history.size();
}

size_t SerialChannel::host_pending_rx() const
{
    std::lock_guard<std::mutex> lock(mtx);
    return rx_q.size();
}
