#pragma once
// A link to a game relay for the NET commands of DPI-1L (docs/desky/DPI-1L.md): TCP with JSON lines (BomberNet
// relay.py, port 8766) or a WebSocket (ws://api.mzpico.com/net, one text message = one JSON line).
//
// Each link has its own thread: name lookup, connect, the WebSocket handshake, the first line (create or join),
// then it reads. What comes from the relay waits in a queue the emulation thread takes (RelayLink::take), so the
// emulation stays single threaded and no line is lost. Lines are sent from the emulation thread. close() sends
// FIN after everything sent so far ('leave' reaches the relay before the link ends) and lets the thread end by
// itself; the thread holds its own reference, nobody waits for it.
#include <stdint.h>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct RelayEvent {
    bool lost;          // the link is gone (could not connect, refused, closed by the relay); else `line`
    std::string line;   // one JSON message
};

class RelayLink {
public:
    // Open a link to host:port. websocket: `path` is the request path with its query; `first` is sent as soon as
    // the link is open.
    static std::shared_ptr<RelayLink> start(const std::string& host, uint16_t port, bool websocket,
                                            const std::string& path, const std::string& first);
    ~RelayLink();

    bool is_open(void);
    // One JSON line to the relay; false when the link is not open or the send failed.
    bool send_line(const std::string& l);
    // End the link: FIN after what was sent, the thread goes on until the relay closes (at most 2 s).
    void close(void);
    // Move the events that came since the last call to `out` (appended).
    void take(std::vector<RelayEvent>& out);

    RelayLink() {}

private:
    void run(void);
    bool connect_host(void);
    bool handshake(std::string& rest);
    void feed(const uint8_t* d, size_t n);   // bytes from the relay
    void ws_byte(uint8_t c);
    void line_byte(uint8_t c);
    bool send_raw(const void* d, size_t n);  // with `wlock` held
    bool send_frame(uint8_t opcode, const uint8_t* d, size_t n);   // with `wlock` held
    void send_frame_locked_pong(void);
    void push(bool lost, const std::string& l);
    bool stopping(void) { return stop.load(); }

    std::string host, path, first;
    uint16_t port = 0;
    bool ws = true;
    intptr_t sock = -1;          // the socket (SOCKET on Windows), owned by the thread
    std::mutex wlock;            // writes (emulation thread and the pong of the link thread), `open`
    bool open = false;
    std::atomic<bool> stop{ false };
    std::mutex qlock;
    std::deque<RelayEvent> queue;
    uint32_t rng = 1;            // masks of the frames sent and the handshake key
    // incoming
    std::string line;
    uint8_t hdr[14];
    int hdr_n = 0, hdr_need = 2;
    uint64_t pay_left = 0;
    uint8_t opcode = 0, mask[4];
    bool masked = false, last_fin = true;
    bool dead = false;           // a close frame came
    uint64_t pay_pos = 0;
    std::vector<uint8_t> ping;
};
