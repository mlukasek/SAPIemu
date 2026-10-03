#include "serial/bridge.h"
#include "core/devices.h"
#include "core/log.h"

#include <stdio.h>
#include <string.h>
#include <atomic>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET sock_t;
#define SOCK_INVALID INVALID_SOCKET
#define sock_close closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define sock_close ::close
#endif

using nlohmann::json;

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

static bool sock_wait_readable(sock_t s, int ms)
{
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(s, &rd);
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
    return select((int)s + 1, &rd, NULL, NULL, &tv) > 0;
}

// ---------------------------------------------------------------- TCP

// Telnet commands
enum { T_IAC = 255, T_DONT = 254, T_DO = 253, T_WONT = 252, T_WILL = 251, T_SB = 250, T_SE = 240 };
enum { OPT_BINARY = 0, OPT_ECHO = 1, OPT_SGA = 3 };

class TcpBridge : public SerialBridge {
public:
    ~TcpBridge() override { stop(); }

    bool start(SerialChannel* ch, const std::string& b, int p, bool dcd, std::string& err)
    {
        net_init();
        channel = ch;
        bind_addr = b.empty() ? "127.0.0.1" : b;
        dcd_follows = dcd;
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == SOCK_INVALID) { err = "cannot create socket"; return false; }
        int yes = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
        sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_port = htons((uint16_t)p);
        if (inet_pton(AF_INET, bind_addr.c_str(), &a.sin_addr) != 1) {
            err = "invalid bind address " + bind_addr;
            sock_close(listener);
            return false;
        }
        if (::bind(listener, (sockaddr*)&a, sizeof(a)) != 0 || listen(listener, 1) != 0) {
            err = "cannot listen on " + bind_addr + ":" + std::to_string(p) + " (port in use?)";
            sock_close(listener);
            listener = SOCK_INVALID;
            return false;
        }
        socklen_t len = sizeof(a);
        getsockname(listener, (sockaddr*)&a, &len);
        port = ntohs(a.sin_port);
        if (dcd_follows) channel->connected = false;
        running = true;
        th = std::thread([this]() { loop(); });
        LOGI("%s: TCP server %s:%d", channel->name.c_str(), bind_addr.c_str(), port);
        return true;
    }

    void stop(void) override
    {
        running = false;
        if (th.joinable()) th.join();
        if (listener != SOCK_INVALID) sock_close(listener);
        listener = SOCK_INVALID;
    }

    std::string describe(void) const override
    {
        std::string s = "TCP " + bind_addr + ":" + std::to_string(port);
        if (client) s += telnet ? " (telnet client connected)" : " (client connected)";
        return s;
    }
    bool client_connected(void) const override { return client; }
    const char* kind(void) const override { return "tcp"; }
    int port = 0;

private:
    void send_all(sock_t s, const uint8_t* d, size_t n)
    {
        while (n > 0) {
            int r = send(s, (const char*)d, (int)n, 0);
            if (r <= 0) return;
            d += r;
            n -= (size_t)r;
        }
    }

    void telnet_start(sock_t s)
    {
        // server echoes (SAPI does), suppress go ahead, 8-bit binary both ways
        const uint8_t neg[] = { T_IAC, T_WILL, OPT_ECHO, T_IAC, T_WILL, OPT_SGA, T_IAC, T_DO, OPT_SGA,
                                T_IAC, T_WILL, OPT_BINARY, T_IAC, T_DO, OPT_BINARY };
        send_all(s, neg, sizeof(neg));
    }

    // Filter telnet commands from client data; returns payload bytes.
    std::string filter_in(const uint8_t* d, int n)
    {
        std::string out;
        for (int i = 0; i < n; i++) {
            uint8_t c = d[i];
            switch (tstate) {
            case 0:
                if (c == T_IAC && (telnet || first_byte)) {
                    if (!telnet) { telnet = true; telnet_start(cs); }
                    tstate = 1;
                } else if (telnet && after_cr && (c == 0 || c == '\n')) {
                    // telnet NVT end of line: CR NUL / CR LF -> CR
                } else {
                    out += (char)c;
                }
                after_cr = (c == '\r');
                break;
            case 1:   // after IAC
                if (c == T_IAC) { out += (char)0xFF; tstate = 0; }
                else if (c >= T_WILL && c <= T_DONT) tstate = 2;
                else if (c == T_SB) tstate = 3;
                else tstate = 0;
                break;
            case 2: tstate = 0; break;                        // option byte
            case 3: if (c == T_IAC) tstate = 4; break;         // subnegotiation
            case 4: tstate = (c == T_SE) ? 0 : 3; break;
            }
            first_byte = false;
        }
        return out;
    }

    void loop(void)
    {
        while (running) {
            if (!sock_wait_readable(listener, 50)) continue;
            cs = accept(listener, NULL, NULL);
            if (cs == SOCK_INVALID) continue;
            int one = 1;
            setsockopt(cs, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
            client = true;
            telnet = false;
            first_byte = true;
            tstate = 0;
            after_cr = false;
            pos = channel->host_end();   // do not replay what was sent before the connection
            if (dcd_follows) channel->connected = true;
            LOGI("%s: TCP client connected", channel->name.c_str());
            while (running) {
                // SAPI -> client
                std::string out;
                channel->host_read(pos, out, 4096);
                if (!out.empty()) {
                    std::string esc;
                    for (char c : out) {
                        esc += c;
                        if (telnet && (uint8_t)c == 0xFF) esc += c;   // IAC IAC
                    }
                    send_all(cs, (const uint8_t*)esc.data(), esc.size());
                }
                // client -> SAPI
                if (sock_wait_readable(cs, 5)) {
                    uint8_t buf[1024];
                    int r = recv(cs, (char*)buf, sizeof(buf), 0);
                    if (r <= 0) break;
                    std::string in = filter_in(buf, r);
                    if (!in.empty()) channel->host_send((const uint8_t*)in.data(), in.size());
                }
            }
            sock_close(cs);
            cs = SOCK_INVALID;
            client = false;
            if (dcd_follows) channel->connected = false;
            LOGI("%s: TCP client disconnected", channel->name.c_str());
        }
    }

    std::string bind_addr;
    bool dcd_follows = true;
    sock_t listener = SOCK_INVALID, cs = SOCK_INVALID;
    std::thread th;
    std::atomic<bool> running{ false };
    std::atomic<bool> client{ false };
    std::atomic<bool> telnet{ false };
    bool first_byte = true, after_cr = false;
    int tstate = 0;
    uint64_t pos = 0;
};

SerialBridge* tcp_bridge_start(SerialChannel* ch, const std::string& bind, int port, bool dcd_follows,
                               std::string& err)
{
    TcpBridge* b = new TcpBridge();
    if (!b->start(ch, bind, port, dcd_follows, err)) {
        delete b;
        return nullptr;
    }
    return b;
}

int tcp_bridge_port(SerialBridge* b)
{
    TcpBridge* t = dynamic_cast<TcpBridge*>(b);
    return t ? t->port : 0;
}

// ---------------------------------------------------------------- COM port

class ComBridge : public SerialBridge {
public:
    ~ComBridge() override { stop(); }

    bool start(SerialChannel* ch, const std::string& p, int b, std::string& err)
    {
        channel = ch;
        port_name = p;
        baud = b;
#ifdef _WIN32
        std::string dev = "\\\\.\\" + p;
        h = CreateFileA(dev.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) { err = "cannot open " + p; return false; }
        DCB dcb;
        memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);
        GetCommState(h, &dcb);
        dcb.BaudRate = (DWORD)baud;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fBinary = TRUE;
        dcb.fParity = FALSE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        dcb.fOutX = dcb.fInX = FALSE;
        if (!SetCommState(h, &dcb)) { err = "cannot configure " + p; CloseHandle(h); h = INVALID_HANDLE_VALUE; return false; }
        COMMTIMEOUTS to;
        memset(&to, 0, sizeof(to));
        to.ReadIntervalTimeout = MAXDWORD;
        to.ReadTotalTimeoutMultiplier = MAXDWORD;
        to.ReadTotalTimeoutConstant = 10;   // ReadFile returns after 10 ms or with data
        to.WriteTotalTimeoutConstant = 1000;
        SetCommTimeouts(h, &to);
#else
        fd = ::open(p.c_str(), O_RDWR | O_NOCTTY);
        if (fd < 0) { err = "cannot open " + p; return false; }
        struct termios t;
        tcgetattr(fd, &t);
        cfmakeraw(&t);
        speed_t sp = B9600;
        switch (baud) {
        case 300: sp = B300; break; case 600: sp = B600; break; case 1200: sp = B1200; break;
        case 2400: sp = B2400; break; case 4800: sp = B4800; break; case 19200: sp = B19200; break;
        case 38400: sp = B38400; break; case 57600: sp = B57600; break; case 115200: sp = B115200; break;
        default: sp = B9600; break;
        }
        cfsetispeed(&t, sp);
        cfsetospeed(&t, sp);
        t.c_cflag |= CLOCAL | CREAD;
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 1;   // 100 ms
        tcsetattr(fd, TCSANOW, &t);
#endif
        pos = channel->host_end();
        running = true;
        th = std::thread([this]() { loop(); });
        LOGI("%s: host serial port %s, %d Bd", channel->name.c_str(), p.c_str(), baud);
        return true;
    }

    void stop(void) override
    {
        running = false;
        if (th.joinable()) th.join();
#ifdef _WIN32
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        h = INVALID_HANDLE_VALUE;
#else
        if (fd >= 0) ::close(fd);
        fd = -1;
#endif
    }

    std::string describe(void) const override { return port_name + " " + std::to_string(baud) + " Bd"; }
    bool client_connected(void) const override { return true; }
    const char* kind(void) const override { return "com"; }

private:
    void loop(void)
    {
        while (running) {
            std::string out;
            channel->host_read(pos, out, 4096);
            uint8_t buf[512];
#ifdef _WIN32
            DWORD n = 0;
            if (!out.empty()) WriteFile(h, out.data(), (DWORD)out.size(), &n, NULL);
            if (ReadFile(h, buf, sizeof(buf), &n, NULL) && n > 0) channel->host_send(buf, n);
#else
            if (!out.empty()) { ssize_t w = ::write(fd, out.data(), out.size()); (void)w; }
            ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n > 0) channel->host_send(buf, (size_t)n);
#endif
        }
    }

    std::string port_name;
    int baud = 9600;
#ifdef _WIN32
    HANDLE h = INVALID_HANDLE_VALUE;
#else
    int fd = -1;
#endif
    std::thread th;
    std::atomic<bool> running{ false };
    uint64_t pos = 0;
};

SerialBridge* com_bridge_start(SerialChannel* ch, const std::string& port, int baud, std::string& err)
{
    ComBridge* b = new ComBridge();
    if (!b->start(ch, port, baud, err)) {
        delete b;
        return nullptr;
    }
    return b;
}

// ---------------------------------------------------------------- configuration

bool serial_bridges_from_config(SerialChannel* ch, const json& cfg, int default_baud,
                                std::vector<SerialBridge*>& out, std::string& err)
{
    if (!cfg.is_object()) return true;
    if (cfg.contains("tcp") && !cfg["tcp"].is_null()) {
        const json& t = cfg["tcp"];
        int port = t.value("port", 2323);
        std::string bind = t.value("bind", std::string("127.0.0.1"));
        bool dcd = t.value("dcd_follows", true);
        SerialBridge* b = tcp_bridge_start(ch, bind, port, dcd, err);
        if (!b) {
            // e.g. a second emulator instance uses the port: run without it
            LOGW("%s: %s", ch->name.c_str(), err.c_str());
            err.clear();
        } else {
            out.push_back(b);
        }
    }
    if (cfg.contains("com") && !cfg["com"].is_null()) {
        const json& c = cfg["com"];
        std::string port = c.value("port", std::string());
        int baud = c.value("baud", default_baud);
        if (port.empty()) { err = "serial.com.port missing"; return false; }
        SerialBridge* b = com_bridge_start(ch, port, baud, err);
        if (!b) {
            // a missing host port should not prevent the machine from starting
            LOGW("%s: %s", ch->name.c_str(), err.c_str());
            err.clear();
        } else {
            out.push_back(b);
        }
    }
    return true;
}
