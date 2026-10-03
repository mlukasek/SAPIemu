// Serial channel over TCP: a client (like TeraTerm) connects to the DMR-1 TCP
// bridge, DCD follows the connection, MikroMon commands are sent over the
// network, raw and telnet mode.
#include <string.h>
#include <chrono>
#include <string>
#include <thread>
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "serial/bridge.h"
#include "test_util.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define sock_close closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define sock_close close
#endif

using nlohmann::json;

static sock_t connect_to(int port, bool v6 = false)
{
    if (v6) {
        sock_t s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in6 a;
        memset(&a, 0, sizeof(a));
        a.sin6_family = AF_INET6;
        a.sin6_port = htons((uint16_t)port);
        inet_pton(AF_INET6, "::1", &a.sin6_addr);
        if (s != (sock_t)-1 && connect(s, (sockaddr*)&a, sizeof(a)) == 0) return s;
        fprintf(stderr, "note: IPv6 loopback not available, using IPv4\n");
    }
    sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (connect(s, (sockaddr*)&a, sizeof(a)) != 0) return (sock_t)-1;
    return s;
}

static void send_str(sock_t s, const std::string& d)
{
    send(s, d.data(), (int)d.size(), 0);
}

static std::string recv_some(sock_t s)
{
    std::string out;
    for (;;) {
        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(s, &rd);
        struct timeval tv = { 0, 0 };
        if (select((int)s + 1, &rd, NULL, NULL, &tv) <= 0) break;
        char buf[1024];
        int r = recv(s, buf, sizeof(buf), 0);
        if (r <= 0) break;
        out.append(buf, (size_t)r);
    }
    return out;
}

// run emulation and give the bridge thread real time to move data
static void run(Machine& m, double seconds)
{
    for (double t = 0; t < seconds; t += 0.01) {
        m.run_seconds(0.01);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

static Machine* make(const char* proto)
{
    std::string text;
    file_read_text(data_path("machines/mikromon.sapi"), text);
    json cfg = json::parse(text, nullptr, false, true);
    for (json& c : cfg["cards"])
        if (c["type"] == "DMR-1") {
            c["baud"] = 38400;
            c["serial"] = { { "tcp", { { "port", 0 }, { "dcd_follows", true }, { "protocol", proto } } } };
        }
    Machine* m = new Machine();
    std::string err;
    if (!m->load_json(cfg, err) || !m->start_bridges(err)) {
        fprintf(stderr, "%s\n", err.c_str());
        delete m;
        return nullptr;
    }
    return m;
}

static void session(bool telnet)
{
    Machine* m = make(telnet ? "telnet" : "raw");
    CHECK(m != nullptr);
    if (!m) return;
    CHECK_EQ(m->bridges.size(), 1);
    int port = tcp_bridge_port(m->bridges[0].get());
    CHECK(port > 0);
    CHECK(!m->serials[0]->connected);           // no client yet: DCD inactive

    sock_t s = connect_to(port, telnet);   // telnet session via IPv6 ::1 (like "telnet localhost")
    CHECK(s != (sock_t)-1);
    for (int i = 0; i < 200 && !m->serials[0]->connected; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    CHECK(m->serials[0]->connected);            // client connected: DCD active

    if (telnet) {
        // the server offers character mode first: IAC WILL ECHO, IAC WILL SGA ...
        std::string neg;
        for (int i = 0; i < 100 && neg.size() < 6; i++) {
            neg += recv_some(s);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        CHECK(neg.compare(0, 6, "\xff\xfb\x01\xff\xfb\x03") == 0);
        // the client answers (IAC DO ECHO, IAC DO SGA) and asks for an option we
        // do not support (IAC WILL NAWS = 31), which must be refused with IAC DONT 31
        const char ans[] = { (char)255, (char)253, 1, (char)255, (char)253, 3, (char)255, (char)251, 31 };
        send_str(s, std::string(ans, sizeof(ans)));
        std::string refuse;
        for (int i = 0; i < 100 && refuse.find("\xff\xfe\x1f") == std::string::npos; i++) {
            refuse += recv_some(s);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        CHECK(refuse.find("\xff\xfe\x1f") != std::string::npos);
    }
    m->reset();                                 // MikroMon finds the UART (DCD active)
    CHECK(run_until_text(*m, "MikroMon 2kB V1.3", 2.0));
    run(*m, 0.2);
    recv_some(s);

    // USYNC makes MikroMon wait for the transmitter, then the help text arrives complete
    send_str(s, telnet ? "USYNC\r\n" : "USYNC\r");
    run(*m, 0.5);
    send_str(s, "?\r");
    std::string got;
    for (int i = 0; i < 100 && got.find("SCAN adr/port") == std::string::npos; i++) {
        run(*m, 0.05);
        got += recv_some(s);
    }
    CHECK(got.find("SCAN adr/port") != std::string::npos);
    if (got.find("SCAN adr/port") == std::string::npos) {
        fprintf(stderr, "received %d bytes: ", (int)got.size());
        for (unsigned char c : got) fprintf(stderr, c >= 32 && c < 127 ? "%c" : "<%02X>", c);
        fprintf(stderr, "\n%s", screen_text(*m).c_str());
    }
    CHECK(screen_contains(*m, "UART [port]"));  // the command came from the serial line
    if (telnet) CHECK((uint8_t)got[0] != 0xFF || got.find("\xff\xfb\x01") != std::string::npos);

    sock_close(s);
    for (int i = 0; i < 200 && m->serials[0]->connected; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    CHECK(!m->serials[0]->connected);           // client gone: DCD inactive again
    delete m;
}

int main(void)
{
    test_init();
    session(false);
    session(true);
    return TEST_RESULT();
}
