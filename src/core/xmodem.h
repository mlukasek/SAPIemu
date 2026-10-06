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

// XMODEM receiver on the host side (a file from SAPI, e.g. DOPC10.COM): asks with NAK (8-bit
// checksum) until the first packet comes, then ACK for a good packet (a repeated one is ACKed and
// dropped), NAK for a bad one, ACK for EOT; 1 KB packets (STX) are taken too. XMODEM carries no
// file name and pads the last packet with 1Ah (strip_sub() cuts them off, CP/M text ends with 1Ah).
struct XmodemReceiver {
    enum State { IDLE, WAIT_FIRST, RECEIVING, DONE, FAILED };
    static const int MAX_RETRIES = 10;

    std::string start(void);                          // returns the first NAK
    std::string feed(const std::string& received);    // ACK / NAK to send
    std::string timeout(void);                        // no data for a while: NAK again
    std::string cancel(void);                         // CAN CAN, state FAILED
    bool active(void) const { return state == WAIT_FIRST || state == RECEIVING; }
    std::vector<uint8_t> file(bool strip_sub) const;

    int state = IDLE;
    size_t packets = 0;        // good packets received
    int retries = 0;
    int bad = 0;               // packets with an error (sent again by the sender)
    std::string error;

private:
    std::vector<uint8_t> data;
    std::string buf;           // bytes of a packet not complete yet
};

uint16_t xmodem_crc16(const uint8_t* p, size_t n);
