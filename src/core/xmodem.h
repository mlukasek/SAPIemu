#pragma once
// XMODEM sender on the host side of a serial channel (sending a file to SAPI, e.g. DOCPM10.COM):
// 128 byte packets SOH seq ~seq data[128] check, the last one padded with 1Ah; the receiver starts
// with NAK (8-bit checksum) or 'C' (CRC-16 CCITT); ACK = next packet, NAK = the same again (10
// retries), EOT at the end, CAN cancels. The caller feeds what the receiver sent and transmits
// what the sender returns; a timeout (no answer) is the caller's (resend()).
#include <stdint.h>
#include <string>
#include <vector>

struct XmodemSender {
    enum State { IDLE, WAIT_START, WAIT_ACK, WAIT_EOT_ACK, DONE, FAILED };
    static const int MAX_RETRIES = 10;

    void start(const std::vector<uint8_t>& file);
    std::string feed(const std::string& received);   // bytes to send now
    std::string resend(void);                        // no answer for a while: the packet (or EOT) again
    std::string cancel(void);                        // CAN CAN, state FAILED
    bool active(void) const { return state == WAIT_START || state == WAIT_ACK || state == WAIT_EOT_ACK; }

    int state = IDLE;
    bool crc = false;          // the receiver asked for CRC-16
    size_t packet = 0;         // index of the packet being sent
    size_t packets = 0;        // all packets
    int retries = 0;
    std::string error;

private:
    std::string packet_bytes(void) const;
    std::vector<uint8_t> data;
};

uint16_t xmodem_crc16(const uint8_t* p, size_t n);
