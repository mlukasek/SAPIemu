#include "core/devices.h"

static const size_t SERIAL_QUEUE_MAX = 1 << 20;

void SerialChannel::to_host(uint8_t b)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (tx_q.size() < SERIAL_QUEUE_MAX) tx_q.push_back(b);
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
    for (size_t i = 0; i < n && rx_q.size() < SERIAL_QUEUE_MAX; i++) rx_q.push_back(data[i]);
}

size_t SerialChannel::host_receive(std::string& out, size_t max)
{
    std::lock_guard<std::mutex> lock(mtx);
    size_t n = 0;
    while (!tx_q.empty() && n < max) {
        out += (char)tx_q.front();
        tx_q.pop_front();
        n++;
    }
    return n;
}

size_t SerialChannel::host_pending_rx() const
{
    std::lock_guard<std::mutex> lock(mtx);
    return rx_q.size();
}
