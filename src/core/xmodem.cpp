#include "core/xmodem.h"

static const uint8_t SOH = 0x01, EOT = 0x04, ACK = 0x06, NAK = 0x15, CAN = 0x18, SUB = 0x1A;

uint16_t xmodem_crc16(const uint8_t* p, size_t n)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < n; i++) {
        crc ^= (uint16_t)(p[i] << 8);
        for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (uint16_t)(crc << 1 ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

void XmodemSender::start(const std::vector<uint8_t>& file)
{
    data = file;
    packets = (data.size() + 127) / 128;
    packet = 0;
    retries = 0;
    crc = false;
    error.clear();
    state = WAIT_START;
}

std::string XmodemSender::packet_bytes(void) const
{
    std::string p;
    uint8_t seq = (uint8_t)(packet + 1);
    p += (char)SOH;
    p += (char)seq;
    p += (char)(uint8_t)~seq;
    uint8_t block[128];
    for (size_t i = 0; i < 128; i++) {
        size_t at = packet * 128 + i;
        block[i] = at < data.size() ? data[at] : SUB;
    }
    p.append((const char*)block, 128);
    if (crc) {
        uint16_t c = xmodem_crc16(block, 128);
        p += (char)(c >> 8);
        p += (char)(c & 0xFF);
    } else {
        uint8_t sum = 0;
        for (uint8_t b : block) sum = (uint8_t)(sum + b);
        p += (char)sum;
    }
    return p;
}

std::string XmodemSender::feed(const std::string& received)
{
    std::string out;
    for (char ch : received) {
        uint8_t c = (uint8_t)ch;
        if (c == CAN && active()) {
            state = FAILED;
            error = "cancelled by the receiver";
            return out;
        }
        switch (state) {
        case WAIT_START:
            if (c != NAK && c != 'C') break;   // the receiver's text before the transfer
            crc = c == 'C';
            if (packets == 0) {
                state = WAIT_EOT_ACK;
                out += (char)EOT;
            } else {
                state = WAIT_ACK;
                out += packet_bytes();
            }
            break;
        case WAIT_ACK:
            if (c == ACK) {
                retries = 0;
                if (++packet >= packets) {
                    state = WAIT_EOT_ACK;
                    out += (char)EOT;
                } else {
                    out += packet_bytes();
                }
            } else if (c == NAK || c == 'C') {
                if (++retries > MAX_RETRIES) {
                    state = FAILED;
                    error = "too many retries";
                    return out;
                }
                out += packet_bytes();
            }
            break;
        case WAIT_EOT_ACK:
            if (c == ACK) state = DONE;
            else if (c == NAK) {
                if (++retries > MAX_RETRIES) {
                    state = FAILED;
                    error = "no ACK after EOT";
                    return out;
                }
                out += (char)EOT;
            }
            break;
        default:
            break;
        }
    }
    return out;
}

std::string XmodemSender::resend(void)
{
    if (state != WAIT_ACK && state != WAIT_EOT_ACK) return std::string();
    if (++retries > MAX_RETRIES) {
        state = FAILED;
        error = "no answer from the receiver";
        return std::string();
    }
    return state == WAIT_ACK ? packet_bytes() : std::string(1, (char)EOT);
}

std::string XmodemSender::cancel(void)
{
    bool was = active();
    state = FAILED;
    error = "cancelled";
    return was ? std::string("\x18\x18", 2) : std::string();
}

std::string XmodemReceiver::start(void)
{
    data.clear();
    buf.clear();
    packets = 0;
    retries = 0;
    bad = 0;
    error.clear();
    state = WAIT_FIRST;
    return std::string(1, (char)NAK);
}

std::string XmodemReceiver::feed(const std::string& received)
{
    std::string out;
    if (!active()) return out;
    buf += received;
    while (!buf.empty() && active()) {
        uint8_t c = (uint8_t)buf[0];
        if (c == EOT) {
            buf.erase(0, 1);
            out += (char)ACK;
            state = DONE;
            break;
        }
        if (c == CAN) {
            if (buf.size() < 2) break;   // CAN CAN: wait for the second one
            if ((uint8_t)buf[1] == CAN) {
                state = FAILED;
                error = "cancelled by the sender";
                break;
            }
            buf.erase(0, 1);
            continue;
        }
        if (c != SOH && c != 0x02) {   // noise between packets (text of the sender program)
            buf.erase(0, 1);
            continue;
        }
        size_t len = c == SOH ? 128 : 1024;
        if (buf.size() < len + 4) break;   // the rest of the packet comes later
        uint8_t seq = (uint8_t)buf[1], inv = (uint8_t)buf[2];
        uint8_t sum = 0;
        for (size_t i = 0; i < len; i++) sum = (uint8_t)(sum + (uint8_t)buf[3 + i]);
        bool ok = (uint8_t)~seq == inv && sum == (uint8_t)buf[3 + len];
        uint8_t expect = (uint8_t)(packets + 1);
        if (ok && seq == expect) {
            data.insert(data.end(), buf.begin() + 3, buf.begin() + 3 + (long)len);
            packets++;
            retries = 0;
            state = RECEIVING;
            out += (char)ACK;
        } else if (ok && seq == (uint8_t)packets) {
            out += (char)ACK;   // the previous one again (our ACK was lost): drop it
        } else {
            bad++;
            if (++retries > MAX_RETRIES) {
                state = FAILED;
                error = "too many bad packets";
                out += std::string("\x18\x18", 2);
                break;
            }
            out += (char)NAK;
        }
        buf.erase(0, len + 4);
    }
    return out;
}

std::string XmodemReceiver::timeout(void)
{
    if (!active()) return std::string();
    if (state == RECEIVING && ++retries > MAX_RETRIES) {
        state = FAILED;
        error = "the sender stopped";
        return std::string("\x18\x18", 2);
    }
    buf.clear();   // a half packet will not be finished
    return std::string(1, (char)NAK);
}

std::string XmodemReceiver::cancel(void)
{
    bool was = active();
    state = FAILED;
    error = "cancelled";
    return was ? std::string("\x18\x18", 2) : std::string();
}

std::vector<uint8_t> XmodemReceiver::file(bool strip_sub) const
{
    std::vector<uint8_t> f = data;
    if (strip_sub)
        while (!f.empty() && f.back() == SUB) f.pop_back();
    return f;
}
