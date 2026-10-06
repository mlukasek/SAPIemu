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
