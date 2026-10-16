#include "core/relay.h"
#include "core/log.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET sock_t;
#define SOCK_INVALID INVALID_SOCKET
#define sock_close closesocket
#define SHUT_WR SD_SEND
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define sock_close ::close
#endif

static void net_init(void)
{
#ifdef _WIN32
    static bool done = false;
    if (!done) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        done = true;
    }
#endif
}

static void set_blocking(sock_t s, bool on)
{
#ifdef _WIN32
    u_long nb = on ? 0 : 1;
    ioctlsocket(s, FIONBIO, &nb);
#else
    int f = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, on ? (f & ~O_NONBLOCK) : (f | O_NONBLOCK));
#endif
}

// 0 = timeout, 1 = ready, -1 = error
static int wait_sock(sock_t s, bool write, int ms)
{
    fd_set set, ex;
    FD_ZERO(&set);
    FD_ZERO(&ex);
    FD_SET(s, &set);
    FD_SET(s, &ex);
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
    int r = select((int)s + 1, write ? NULL : &set, write ? &set : NULL, &ex, &tv);
    if (r < 0) return -1;
    if (r == 0) return 0;
    return FD_ISSET(s, &set) ? 1 : -1;
}

typedef std::chrono::steady_clock clk;

static long long ms_since(clk::time_point t)
{
    return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(clk::now() - t).count();
}

std::shared_ptr<RelayLink> RelayLink::start(const std::string& host, uint16_t port, bool websocket,
                                            const std::string& path, const std::string& first)
{
    net_init();
    std::shared_ptr<RelayLink> l = std::make_shared<RelayLink>();
    l->host = host;
    l->port = port;
    l->ws = websocket;
    l->path = path;
    l->first = first;
    std::random_device rd;
    l->rng = rd() | 1;
    std::thread([l]() { l->run(); }).detach();   // the thread keeps the link alive until it ends
    return l;
}

RelayLink::~RelayLink()
{
}

bool RelayLink::is_open(void)
{
    std::lock_guard<std::mutex> g(wlock);
    return open;
}

void RelayLink::close(void)
{
    {
        std::lock_guard<std::mutex> g(wlock);
        open = false;
    }
    stop = true;
}

void RelayLink::take(std::vector<RelayEvent>& out)
{
    std::lock_guard<std::mutex> g(qlock);
    while (!queue.empty()) {
        out.push_back(queue.front());
        queue.pop_front();
    }
}

void RelayLink::push(bool lost, const std::string& l)
{
    if (stopping()) return;   // nobody listens any more
    std::lock_guard<std::mutex> g(qlock);
    RelayEvent e;
    e.lost = lost;
    e.line = l;
    queue.push_back(e);
}

bool RelayLink::send_raw(const void* d, size_t n)
{
    const char* p = (const char*)d;
    while (n) {
        int r = ::send((sock_t)sock, p, (int)n, 0);
        if (r <= 0) {
            open = false;     // the reading side sees the end too
            return false;
        }
        p += r;
        n -= (size_t)r;
    }
    return true;
}

// a frame from the client: masked (RFC 6455)
bool RelayLink::send_frame(uint8_t op, const uint8_t* d, size_t n)
{
    std::vector<uint8_t> f;
    f.push_back((uint8_t)(0x80 | op));
    if (n < 126) {
        f.push_back((uint8_t)(0x80 | n));
    } else {
        f.push_back(0x80 | 126);
        f.push_back((uint8_t)(n >> 8));
        f.push_back((uint8_t)n);
    }
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    uint8_t mk[4] = { (uint8_t)rng, (uint8_t)(rng >> 8), (uint8_t)(rng >> 16), (uint8_t)(rng >> 24) };
    f.insert(f.end(), mk, mk + 4);
    for (size_t i = 0; i < n; i++) f.push_back(d[i] ^ mk[i & 3]);
    return send_raw(f.data(), f.size());
}

bool RelayLink::send_line(const std::string& l)
{
    std::lock_guard<std::mutex> g(wlock);
    if (!open) return false;
    if (ws) return send_frame(1, (const uint8_t*)l.data(), l.size());
    std::string t = l + "\n";
    return send_raw(t.data(), t.size());
}

bool RelayLink::connect_host(void)
{
    char ps[8];
    snprintf(ps, sizeof(ps), "%u", port);
    struct addrinfo hints, *res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(host.c_str(), ps, &hints, &res) != 0 || !res) {
        LOGW("DPI-1L: relay %s not found", host.c_str());
        return false;
    }
    sock_t s = SOCK_INVALID;
    for (struct addrinfo* a = res; a && !stopping(); a = a->ai_next) {
        s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == SOCK_INVALID) continue;
        set_blocking(s, false);
        ::connect(s, a->ai_addr, (int)a->ai_addrlen);
        clk::time_point t0 = clk::now();
        int r = 0;
        while (r == 0 && !stopping() && ms_since(t0) < 5000) r = wait_sock(s, true, 100);
        int e = 0;
        socklen_t el = sizeof(e);
        if (r == 1 && getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&e, &el) == 0 && e == 0) break;
        sock_close(s);
        s = SOCK_INVALID;
    }
    freeaddrinfo(res);
    if (s == SOCK_INVALID) {
        if (!stopping()) LOGW("DPI-1L: cannot connect to the relay %s:%u", host.c_str(), port);
        return false;
    }
    set_blocking(s, true);
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
    sock = (intptr_t)s;
    return true;
}

// WebSocket upgrade; `rest` = bytes after the HTTP headers (the first frames). Sec-WebSocket-Accept is not
// checked (as the DPI firmware and MZPico).
bool RelayLink::handshake(std::string& rest)
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint8_t key[18] = { 0 };
    for (int i = 0; i < 16; i++) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        key[i] = (uint8_t)rng;
    }
    char k[25];
    int o = 0;
    for (int i = 0; i < 18; i += 3) {
        uint32_t t = (uint32_t)key[i] << 16 | key[i + 1] << 8 | key[i + 2];
        k[o++] = b64[t >> 18];
        k[o++] = b64[(t >> 12) & 63];
        k[o++] = b64[(t >> 6) & 63];
        k[o++] = b64[t & 63];
    }
    k[22] = '=';
    k[23] = '=';
    k[24] = 0;
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + host +
                      "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + k +
                      "\r\nSec-WebSocket-Version: 13\r\nUser-Agent: SAPIemu DPI-1L\r\n\r\n";
    {
        std::lock_guard<std::mutex> g(wlock);
        if (!send_raw(req.data(), req.size())) return false;
    }
    std::string got;
    clk::time_point t0 = clk::now();
    while (!stopping() && ms_since(t0) < 10000) {
        int r = wait_sock((sock_t)sock, false, 100);
        if (r < 0) return false;
        if (r == 0) continue;
        char b[1024];
        int n = recv((sock_t)sock, b, sizeof(b), 0);
        if (n <= 0) return false;
        got.append(b, (size_t)n);
        size_t e = got.find("\r\n\r\n");
        if (e != std::string::npos) {
            if (got.compare(0, 12, "HTTP/1.1 101") != 0) {
                LOGW("DPI-1L: the relay refused the WebSocket: %s", got.substr(0, got.find('\r')).c_str());
                return false;
            }
            rest = got.substr(e + 4);
            return true;
        }
        if (got.size() > 8192) return false;
    }
    return false;
}

void RelayLink::line_byte(uint8_t c)
{
    if (c == '\n' || c == '\r') {
        if (!line.empty()) push(false, line);
        line.clear();
    } else if (line.size() < 4096) {
        line += (char)c;
    }
}

// a frame from the server (normally not masked): text = one JSON message, ping -> pong, close -> lost
void RelayLink::ws_byte(uint8_t c)
{
    if (pay_left == 0 && hdr_n < hdr_need) {
        hdr[hdr_n++] = c;
        if (hdr_n == 2) {
            uint8_t l7 = hdr[1] & 0x7F;
            hdr_need = 2 + (l7 == 126 ? 2 : l7 == 127 ? 8 : 0) + ((hdr[1] & 0x80) ? 4 : 0);
        }
        if (hdr_n < hdr_need) return;
        opcode = hdr[0] & 0x0F;
        masked = (hdr[1] & 0x80) != 0;
        uint8_t l7 = hdr[1] & 0x7F;
        int p = 2;
        if (l7 < 126) {
            pay_left = l7;
        } else if (l7 == 126) {
            pay_left = (uint64_t)hdr[2] << 8 | hdr[3];
            p = 4;
        } else {
            pay_left = 0;
            for (int i = 0; i < 8; i++) pay_left = pay_left << 8 | hdr[2 + i];
            p = 10;
        }
        if (masked) memcpy(mask, hdr + p, 4);
        bool fin = (hdr[0] & 0x80) != 0;
        hdr_n = 0;
        hdr_need = 2;
        pay_pos = 0;
        if (opcode == 9) ping.clear();
        if (pay_left == 0) {
            if (opcode == 9) send_frame_locked_pong();
            else if (opcode == 8) dead = true;
            else if ((opcode == 0 || opcode == 1) && fin) line_byte('\n');
        }
        last_fin = fin;
        return;
    }
    if (masked) c ^= mask[pay_pos & 3];
    pay_pos++;
    pay_left--;
    if (opcode == 1 || opcode == 0) {
        line_byte(c);
        if (pay_left == 0 && last_fin) line_byte('\n');   // the end of a text message ends the JSON line
    } else if (opcode == 9) {
        if (ping.size() < 125) ping.push_back(c);
        if (pay_left == 0) send_frame_locked_pong();
    } else if (opcode == 8 && pay_left == 0) {
        dead = true;
    }
}

void RelayLink::send_frame_locked_pong(void)
{
    std::lock_guard<std::mutex> g(wlock);
    if (open) send_frame(10, ping.data(), ping.size());
    ping.clear();
}

void RelayLink::feed(const uint8_t* d, size_t n)
{
    for (size_t i = 0; i < n && !dead; i++) {
        if (ws) ws_byte(d[i]);
        else line_byte(d[i]);
    }
}

void RelayLink::run(void)
{
    bool ok = connect_host();
    std::string rest;
    if (ok && ws) ok = handshake(rest);
    if (ok && !stopping()) {
        {
            std::lock_guard<std::mutex> g(wlock);
            open = true;
        }
        send_line(first);
        feed((const uint8_t*)rest.data(), rest.size());
        while (!stopping() && !dead) {
            int r = wait_sock((sock_t)sock, false, 100);
            if (r == 0) continue;
            uint8_t b[2048];
            int n = r < 0 ? -1 : recv((sock_t)sock, (char*)b, sizeof(b), 0);
            if (n <= 0) break;
            feed(b, (size_t)n);
        }
    }
    {
        std::lock_guard<std::mutex> g(wlock);
        open = false;
    }
    if (sock != -1) {
        if (stopping()) {
            // closed by us: FIN after what was sent ('leave' first), read until the relay closes too
            shutdown((sock_t)sock, SHUT_WR);
            clk::time_point t0 = clk::now();
            while (ms_since(t0) < 2000) {
                int r = wait_sock((sock_t)sock, false, 100);
                if (r == 0) continue;
                char b[512];
                if (r < 0 || recv((sock_t)sock, b, sizeof(b), 0) <= 0) break;
            }
        }
        std::lock_guard<std::mutex> g(wlock);
        sock_close((sock_t)sock);
        sock = -1;
    }
    push(true, std::string());   // nothing when we closed it ourselves (push() skips it then)
}

// ---------------------------------------------------------------- TCP client of the Repository API (80h-86h)

static const size_t TCP_RX_SIZE = 16384;   // more than a TCP window: the window does the flow control

std::shared_ptr<TcpLink> TcpLink::start(const uint8_t a[4], uint16_t p)
{
    net_init();
    std::shared_ptr<TcpLink> l = std::make_shared<TcpLink>();
    memcpy(l->ip, a, 4);
    l->port = p;
    l->rx.resize(TCP_RX_SIZE);
    std::thread([l]() { l->run(); }).detach();
    return l;
}

void TcpLink::run(void)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    memcpy(&sa.sin_addr, ip, 4);
    sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == SOCK_INVALID) {
        st = FAILED;
        return;
    }
    set_blocking(s, false);
    ::connect(s, (struct sockaddr*)&sa, sizeof(sa));
    int r = 0;
    while (r == 0 && !stop) r = wait_sock(s, true, 100);   // the card gives up after 10 s (TCPSTATUS)
    int e = 0;
    socklen_t el = sizeof(e);
    if (stop || r != 1 || getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&e, &el) != 0 || e != 0) {
        sock_close(s);
        if (!stop) st = FAILED;
        return;
    }
    set_blocking(s, true);
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));   // small Z80 packets go out at once
    {
        std::lock_guard<std::mutex> g(lock);
        sock = (intptr_t)s;
    }
    st = CONNECTED;
    while (!stop) {
        size_t room;
        {
            std::lock_guard<std::mutex> g(lock);
            room = TCP_RX_SIZE - (head - tail);
        }
        if (!room) {                            // full: wait for the Z80 (TCPRECV)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        int w = wait_sock(s, false, 100);
        if (w == 0) continue;
        uint8_t b[2048];
        int n = w < 0 ? -1 : recv(s, (char*)b, (int)std::min(room, sizeof(b)), 0);
        if (n <= 0) {                           // the other side closed: what came is still readable
            if (!stop) st = CLOSED;
            break;
        }
        std::lock_guard<std::mutex> g(lock);
        for (int i = 0; i < n; i++) rx[(head++) % TCP_RX_SIZE] = b[i];
    }
    if (stop) shutdown(s, SHUT_WR);
    std::lock_guard<std::mutex> g(lock);
    sock_close(s);
    sock = -1;
}

bool TcpLink::send(const uint8_t* d, size_t n)
{
    std::lock_guard<std::mutex> g(lock);
    if (sock == -1 || st != CONNECTED) return false;
    while (n) {
        int r = ::send((sock_t)sock, (const char*)d, (int)n, 0);
        if (r <= 0) return false;
        d += r;
        n -= (size_t)r;
    }
    return true;
}

size_t TcpLink::available(void)
{
    std::lock_guard<std::mutex> g(lock);
    return head - tail;
}

size_t TcpLink::take(uint8_t* out, size_t max)
{
    std::lock_guard<std::mutex> g(lock);
    size_t n = std::min(max, head - tail);
    for (size_t i = 0; i < n; i++) out[i] = rx[(tail++) % TCP_RX_SIZE];
    return n;
}

void TcpLink::close(void)
{
    stop = true;
}

// ---------------------------------------------------------------- DNS (88h, 89h)

std::shared_ptr<DnsQuery> DnsQuery::start(const std::string& name)
{
    net_init();
    std::shared_ptr<DnsQuery> q = std::make_shared<DnsQuery>();
    std::thread([q, name]() {
        struct addrinfo hints, *res = nullptr;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(name.c_str(), nullptr, &hints, &res) == 0 && res) {
            memcpy(q->ip, &((struct sockaddr_in*)res->ai_addr)->sin_addr, 4);
            q->ok = true;
        }
        if (res) freeaddrinfo(res);
        q->done = true;
    }).detach();
    return q;
}
