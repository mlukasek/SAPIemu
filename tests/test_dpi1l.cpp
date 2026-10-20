// DPI-1L, the network part: the Repository API on B0h/B1h and NET A0h-A9h against a small fake relay in the test
// (TCP lines and WebSocket on 127.0.0.1). The records without a room and the game of two players are those taken
// from the real DPI-1L (SAPI_DPI-firmware docs/sapiemu_dpi.md section 6), written here the same way:
// command and parameters -> status (STSR + 4 bytes, read again while bit 6 is set) | output.
#include "cards/dpi1l.h"
#include "core/machine.h"
#include "core/state.h"
#include "test_util.h"

#include <string.h>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define SOCK_INVALID INVALID_SOCKET
#define sock_close closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define sock_close ::close
#endif

using nlohmann::json;

static const uint8_t B = 0xB0;

static void sleep_ms(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

static bool readable(sock_t s, int ms)
{
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(s, &rd);
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
    return select((int)s + 1, &rd, NULL, NULL, &tv) > 0;
}

// The relay side: one client at a time, JSON lines over TCP or WebSocket frames (the server does not mask).
struct FakeRelay {
    sock_t lst = SOCK_INVALID, cl = SOCK_INVALID;
    int port = 0;
    bool ws = false;
    std::string buf;            // bytes from the client not used yet
    std::string request;        // WebSocket: the HTTP request of the last client

    bool listen_on(void)
    {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        lst = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = 0;
        if (bind(lst, (struct sockaddr*)&a, sizeof(a)) != 0 || listen(lst, 4) != 0) return false;
        socklen_t n = sizeof(a);
        getsockname(lst, (struct sockaddr*)&a, &n);
        port = ntohs(a.sin_port);
        return true;
    }
    ~FakeRelay()
    {
        drop();
        if (lst != SOCK_INVALID) sock_close(lst);
    }
    void drop(void)
    {
        if (cl != SOCK_INVALID) sock_close(cl);
        cl = SOCK_INVALID;
        buf.clear();
    }
    bool fill(int ms)
    {
        if (cl == SOCK_INVALID || !readable(cl, ms)) return false;
        char b[1024];
        int n = recv(cl, b, sizeof(b), 0);
        if (n <= 0) return false;
        buf.append(b, (size_t)n);
        return true;
    }
    bool accept_client(int ms = 5000)
    {
        drop();
        if (!readable(lst, ms)) return false;
        cl = accept(lst, NULL, NULL);
        if (cl == SOCK_INVALID) return false;
        if (!ws) return true;
        while (buf.find("\r\n\r\n") == std::string::npos)
            if (!fill(3000)) return false;
        size_t e = buf.find("\r\n\r\n") + 4;
        request = buf.substr(0, e);
        buf.erase(0, e);
        const char* ok = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                         "Sec-WebSocket-Accept: x\r\n\r\n";
        return ::send(cl, ok, (int)strlen(ok), 0) > 0;
    }
    // one frame from the client (masked), false on a timeout
    bool frame(uint8_t& op, std::string& pay, int ms)
    {
        for (;;) {
            if (buf.size() >= 2) {
                size_t l = (uint8_t)buf[1] & 0x7F, p = 2;
                if (l == 126 && buf.size() >= 4) { l = (uint8_t)buf[2] << 8 | (uint8_t)buf[3]; p = 4; }
                bool masked = ((uint8_t)buf[1] & 0x80) != 0;
                CHECK(masked);
                if ((l < 126 || p == 4) && buf.size() >= p + 4 + l) {
                    op = (uint8_t)buf[0] & 0x0F;
                    pay.clear();
                    for (size_t i = 0; i < l; i++) pay += (char)(buf[p + 4 + i] ^ buf[p + (i & 3)]);
                    buf.erase(0, p + 4 + l);
                    return true;
                }
            }
            if (!fill(ms)) return false;
        }
    }
    std::string line(int ms = 3000)
    {
        if (ws) {
            uint8_t op;
            std::string pay;
            while (frame(op, pay, ms))
                if (op == 1) return pay;
            return std::string();
        }
        for (;;) {
            size_t e = buf.find('\n');
            if (e != std::string::npos) {
                std::string l = buf.substr(0, e);
                buf.erase(0, e + 1);
                return l;
            }
            if (!fill(ms)) return std::string();
        }
    }
    void send_frame(uint8_t op, const std::string& pay)
    {
        std::string f;
        f += (char)(0x80 | op);
        f += (char)pay.size();
        f += pay;
        ::send(cl, f.data(), (int)f.size(), 0);
    }
    void send(const std::string& l)
    {
        if (ws) {
            send_frame(1, l);
        } else {
            std::string t = l + "\n";
            ::send(cl, t.data(), (int)t.size(), 0);
        }
        sleep_ms(60);   // the link thread of the card has it in its queue
    }
    // the client closed the link (nothing else may come)
    bool closed(int ms = 4000)
    {
        if (cl == SOCK_INVALID || !readable(cl, ms)) return false;
        char b[64];
        return recv(cl, b, sizeof(b), 0) <= 0;
    }
};

static std::vector<uint8_t> hex(const std::string& s)
{
    std::vector<uint8_t> v;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == ' ' || s[i] == '|') { i++; continue; }
        v.push_back((uint8_t)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
        i += 2;
    }
    return v;
}

static std::string zeros(int n)
{
    std::string s;
    for (int i = 0; i < n; i++) s += " 00";
    return s;
}

static std::string text(const char* t, bool end = true)   // a string parameter
{
    std::string s;
    char b[4];
    for (const char* p = t; *p; p++) {
        snprintf(b, sizeof(b), " %02X", (uint8_t)*p);
        s += b;
    }
    return end ? s + " 0D" : s;
}

static std::string dump(const std::vector<uint8_t>& v, size_t status)
{
    std::string s;
    char b[4];
    for (size_t i = 0; i < v.size(); i++) {
        if (i == status) s += " |";
        snprintf(b, sizeof(b), " %02X", v[i]);
        s += b;
    }
    return s;
}

// Command (and parameters), then the status as the DPI console reads it (STSR + 4 bytes, again while bit 6 is
// set, up to `wait_ms`), then the output if DOUTRQ. Checked against `want` ("st st st st | output").
static void rec(Machine& m, const std::string& cmd, const std::string& want, int line, int wait_ms = 15000)
{
    std::vector<uint8_t> c = hex(cmd);
    m.bus.io_write(B, c[0]);
    for (size_t i = 1; i < c.size(); i++) m.bus.io_write(B + 1, c[i]);
    std::vector<uint8_t> got;
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    for (;;) {
        got.clear();
        m.bus.io_write(B, 0x03);
        for (int i = 0; i < 4; i++) got.push_back(m.bus.io_read(B));
        if (!(got[0] & 0x40)) break;
        if (std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(wait_ms)) break;
        sleep_ms(2);
    }
    if (got[0] & 0x02) {
        int n = got[2];
        for (int i = 0; i < n; i++) got.push_back(m.bus.io_read(B + 1));
    }
    std::vector<uint8_t> w = hex(want);
    if (got != w) {
        fprintf(stderr, "test_dpi1l.cpp:%d: %s ->%s, want%s\n", line, cmd.c_str(), dump(got, 4).c_str(),
                dump(w, 4).c_str());
        g_test_failures++;
    }
}
#define REC(m, c, w) rec(m, c, w, __LINE__)
#define REC0(m, c, w) rec(m, c, w, __LINE__, 0)   // the status at once, bit 6 or not

static void expect_line(FakeRelay& r, const char* want, int line)
{
    std::string l = r.line();
    if (l != want) {
        fprintf(stderr, "test_dpi1l.cpp:%d: relay got '%s', want '%s'\n", line, l.c_str(), want);
        g_test_failures++;
    }
}
#define LINE(r, w) expect_line(r, w, __LINE__)

static json machine(int port, const char* transport)
{
    json cfg = { { "cards", json::array() } };
    cfg["cards"].push_back({ { "slot", 1 }, { "type", "JPR-1ZI" } });
    cfg["cards"].push_back({ { "slot", 2 },
                             { "type", "DPI-1L" },
                             { "net", { { "transport", transport }, { "host", "127.0.0.1" }, { "port", port } } },
                             { "sd", { { "folder", "work/test/dpi1l_net_sd" } } } });
    return cfg;
}

static Dpi1l* card(Machine& m)
{
    return (Dpi1l*)m.find_card("DPI-1L");
}

static const char* CREATE = "A1 4E 42 01 00 02 02 02 01 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00";
static const char* CREATE_LINE = "{\"op\":\"create\",\"game\":16974,\"build\":1,\"slots\":2,\"bytes\":2,\"settings\":\"0102\"}";

// the records without a room (section 6)
static void test_no_room(FakeRelay& r)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(r.port, "tcp"), err));
    REC(m, "00", "00 00 00 00");
    REC(m, "06", "02 06 04 00 | 00 04 44 01");
    REC(m, "05", "02 05 0E 00 | 44 50 49 2D 31 4C 20 76 30 2E 34 2E 30 0D");   // "DPI-1L v0.4.0" as the card
    REC(m, "95", "02 95 10 00 | 01 01 04 09" + zeros(12));
    REC(m, "93", "02 93 01 00 | 03");
    REC(m, "77", "80 77 01 00");
    {   // STSR after an error: it does not clear it
        std::vector<uint8_t> s;
        m.bus.io_write(B, 0x03);
        for (int i = 0; i < 4; i++) s.push_back(m.bus.io_read(B));
        CHECK(s == hex("80 77 01 00"));
        CHECK_EQ(m.bus.io_read(B), 0x00);   // past the 4 bytes: 00
    }
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 00");
    REC(m, "A6 00 00", "80 A6 08 00");
    REC(m, "A9", "02 A9 22 00 | FF" + zeros(33));
    REC(m, "A3", "80 A3 08 00");
    REC(m, "A4 01", "80 A4 08 00");
    REC(m, "A5 00 00 01 02 03 04", "80 A5 08 00");
    REC(m, "A1 4E 42 04 06 00 01" + zeros(17), "80 A1 0A 00");   // slots 0 = PARAM
    REC(m, "A2 4E 42 04 06" + text("A-B"), "80 A2 0A 00");        // a code with '-' = PARAM

    // the rest of the protocol (repo.c): ASCII, STORNO, INTCALLER, SHASCII, data with nothing to read or write,
    // the parameter phase in the status
    REC(m, "01", "00 01 00 00");
    REC(m, "04", "00 04 00 00");
    REC(m, "11", "02 11 01 00 | 00");
    REC(m, "02", "80 02 01 00");
    m.bus.io_write(B, 0x01);
    CHECK_EQ(m.bus.io_read(B + 1), 0x00);   // nothing to read: ERROR 4
    REC(m, "03", "80 01 04 00");
    m.bus.io_write(B + 1, 0x55);            // a byte nobody wants: ERROR 4
    REC(m, "03", "80 01 04 00");
    REC(m, "A2 4E 42", "01 A2 FC 00");      // JOIN waits for parameters: 255 - 1 string - 2 bytes
    m.bus.io_write(B, 0x04);                // STORNO
    REC(m, "03", "00 04 00 00");
    REC(m, "60 1F 02 2E", "80 60 02 00");   // RTCSETD 31. 2.: BAD_PARAM (files, RTC: test_dpi1l_files)
    REC(m, "87", "80 87 01 00");            // between TCP and DNS: unknown

    // the detection of BomberNet (netdev_card.c): REVD status 02 06 04, INFO without ERROR with >= 16 bytes, bit 08h
    m.bus.io_write(B, 0x06);
    m.bus.io_write(B, 0x03);
    CHECK_EQ(m.bus.io_read(B), 0x02);
    CHECK_EQ(m.bus.io_read(B), 0x06);
    CHECK_EQ(m.bus.io_read(B), 0x04);
}

// CREATE and a game of two players (section 6), the relay answers as relay.py (spaces after ':')
static void test_game(FakeRelay& r)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(r.port, r.ws ? "ws" : "tcp"), err));
    REC(m, "00", "00 00 00 00");

    // CREATE: status bit 6 while the relay has not answered
    std::vector<uint8_t> c = hex(CREATE);
    m.bus.io_write(B, c[0]);
    for (size_t i = 1; i < c.size(); i++) m.bus.io_write(B + 1, c[i]);
    CHECK(r.accept_client());
    if (r.ws) CHECK(r.request.find("GET /net?game=16974&create=1 HTTP/1.1\r\nHost: 127.0.0.1\r\n") == 0);
    LINE(r, CREATE_LINE);
    REC0(m, "03", "40 A1 00 00");
    if (r.ws) {   // a ping on the way: pong with the same content
        r.send_frame(9, "abc");
        uint8_t op = 0;
        std::string pay;
        CHECK(r.frame(op, pay, 3000));
        CHECK_EQ(op, 10);
        CHECK(pay == "abc");
    }
    r.send("{\"op\": \"room\", \"code\": \"SYTN\", \"slot\": 0, \"slots\": 2, \"bytes\": 2, \"settings\": \"0102\"}");
    r.send("{\"op\": \"members\", \"count\": 1, \"ready\": 0}");
    REC(m, "03", "02 A1 06 00 | 53 59 54 4E 0D 00");
    REC(m, "A0", "02 A0 08 00 | 02 00 01 00 00 00 00 00");
    r.send("{\"op\": \"members\", \"count\": 2, \"ready\": 0}");
    r.send("{\"op\": \"members\", \"count\": 2, \"ready\": 2}");
    REC(m, "A0", "02 A0 08 00 | 02 00 02 02 00 00 00 00");
    REC(m, "A4 01", "02 A4 04 00 | FF FF FF FF");
    LINE(r, "{\"op\":\"ready\",\"ready\":1}");
    r.send("{\"op\": \"members\", \"count\": 2, \"ready\": 3}");
    r.send("{\"op\": \"start\", \"seed\": 63527, \"frame\": 0, \"mask\": 3}");
    r.send("{\"op\": \"input\", \"frame\": 0, \"slot\": 1, \"data\": \"1010\"}");
    r.send("{\"op\": \"input\", \"frame\": 1, \"slot\": 1, \"data\": \"1111\"}");
    r.send("{\"op\": \"msg\", \"from\": 1, \"data\": \"4849\"}");
    REC(m, "A0", "02 A0 08 00 | 03 00 02 03 00 00 01 00");
    REC(m, "A4 01", "02 A4 04 00 | 27 F8 00 00");
    LINE(r, "{\"op\":\"ready\",\"ready\":1}");
    REC(m, "A6 00 00", "02 A6 06 00 | FF FF 00 00 00 00");
    REC(m, "A5 00 00 0A 0B 00 00", "00 A5 00 00");
    LINE(r, "{\"op\":\"input\",\"frame\":0,\"data\":\"0a0b\"}");
    REC(m, "A6 00 00", "02 A6 06 00 | 00 00 0A 0B 10 10");
    REC(m, "A5 01 00 1A 1B 00 00", "00 A5 00 00");
    LINE(r, "{\"op\":\"input\",\"frame\":1,\"data\":\"1a1b\"}");
    REC(m, "A6 01 00", "02 A6 06 00 | 01 00 1A 1B 11 11");
    REC(m, "A6 05 00", "02 A6 06 00 | 01 00 00 00 00 00");
    REC(m, "A0", "02 A0 08 00 | 03 00 02 03 00 02 01 00");
    REC(m, "A9", "02 A9 22 00 | 01 02 48 49" + zeros(30));
    REC(m, "A9", "02 A9 22 00 | FF" + zeros(33));
    REC(m, "A7 10 00 34 12", "00 A7 00 00");
    LINE(r, "{\"op\":\"hash\",\"frame\":16,\"hash\":4660}");
    REC(m, "A8 FF 02 41 42" + zeros(30), "00 A8 00 00");
    LINE(r, "{\"op\":\"msg\",\"to\":-1,\"data\":\"4142\"}");
    REC(m, "A3", "00 A3 00 00");
    LINE(r, "{\"op\":\"leave\"}");   // 'leave' comes before the end of the link
    CHECK(r.closed());
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 00");
}

// CREATE answered: the room with a given slot (members 1)
static void enter_room(Machine& m, FakeRelay& r)
{
    std::vector<uint8_t> c = hex(CREATE);
    m.bus.io_write(B, c[0]);
    for (size_t i = 1; i < c.size(); i++) m.bus.io_write(B + 1, c[i]);
    CHECK(r.accept_client());
    LINE(r, CREATE_LINE);
    r.send("{\"op\":\"room\",\"code\":\"ABCD\",\"slot\":0,\"slots\":2,\"bytes\":2,\"settings\":\"0102\"}");
    REC(m, "03", "02 A1 06 00 | 41 42 43 44 0D 00");
}

static void test_join(FakeRelay& r)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(r.port, "tcp"), err));

    // a spectator (the room runs or is full): slot FFh, state 6, POLL allowed
    REC0(m, "A2 4E 42 01 00" + text("sytn"), "40 A2 00 00");
    CHECK(r.accept_client());
    LINE(r, "{\"op\":\"join\",\"game\":16974,\"build\":1,\"code\":\"SYTN\"}");
    r.send("{\"op\":\"room\",\"code\":\"SYTN\",\"slot\":-1,\"slots\":2,\"bytes\":2,\"settings\":\"0102\",\"spectator\":true}");
    REC(m, "03", "02 A2 14 00 | FF 02 02 02 01 02" + zeros(14));
    REC(m, "A0", "02 A0 08 00 | 06 FF 01 00 00 00 00 00");
    r.send("{\"op\":\"start\",\"seed\":5,\"frame\":0,\"mask\":3}");
    r.send("{\"op\":\"input\",\"frame\":0,\"slot\":0,\"data\":\"0102\"}");
    r.send("{\"op\":\"input\",\"frame\":0,\"slot\":1,\"data\":\"0304\"}");
    REC(m, "A6 00 00", "02 A6 06 00 | 00 00 01 02 03 04");
    REC(m, "A0", "02 A0 08 00 | 06 FF 01 00 00 01 00 00");
    REC(m, "A5 01 00 00 00 00 00", "80 A5 08 00");   // a spectator sends nothing

    // a player: slot 1, a code of 7 characters, settings of other length
    REC0(m, "A2 4E 42 01 00" + text("ab12CD9"), "40 A2 00 00");
    LINE(r, "{\"op\":\"leave\"}");   // the old room first
    CHECK(r.closed());
    CHECK(r.accept_client());
    LINE(r, "{\"op\":\"join\",\"game\":16974,\"build\":1,\"code\":\"AB12CD9\"}");
    r.send("{\"op\":\"room\",\"code\":\"AB12CD9\",\"slot\":1,\"slots\":3,\"bytes\":1,\"settings\":\"aabbcc\"}");
    REC(m, "03", "02 A2 14 00 | 01 03 01 03 AA BB CC" + zeros(13));
    REC(m, "A0", "02 A0 08 00 | 02 01 01 00 00 00 00 00");
    REC(m, "A3", "00 A3 00 00");
    LINE(r, "{\"op\":\"leave\"}");
    CHECK(r.closed());

    // errors of the relay: their code; the relay closes the link then, the code stays
    REC0(m, "A2 4E 42 02 00" + text("SYTN"), "40 A2 00 00");
    CHECK(r.accept_client());
    LINE(r, "{\"op\":\"join\",\"game\":16974,\"build\":2,\"code\":\"SYTN\"}");
    r.send("{\"op\":\"error\",\"code\":6,\"text\":\"build mismatch\"}");
    r.drop();
    REC(m, "03", "80 A2 06 00");
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 06");
    REC0(m, "A2 4E 42 01 00" + text("ZZZZ"), "40 A2 00 00");
    CHECK(r.accept_client());
    LINE(r, "{\"op\":\"join\",\"game\":16974,\"build\":1,\"code\":\"ZZZZ\"}");
    r.send("{\"op\":\"error\",\"code\":7,\"text\":\"no such room\"}");
    r.drop();
    REC(m, "03", "80 A2 07 00");
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 07");
}

static void test_link(FakeRelay& r)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(r.port, "tcp"), err));
    Dpi1l* d = card(m);
    CHECK(d != nullptr);
    if (!d) return;

    // no answer: error 9 after the limit, also with the link open
    d->pending_timeout_ms = 400;
    std::vector<uint8_t> c = hex(CREATE);
    m.bus.io_write(B, c[0]);
    for (size_t i = 1; i < c.size(); i++) m.bus.io_write(B + 1, c[i]);
    CHECK(r.accept_client());
    LINE(r, CREATE_LINE);
    REC(m, "03", "80 A1 09 00");
    CHECK(r.closed());
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 09");
    d->pending_timeout_ms = 10000;

    // the link lost in the game: DROPPED, error 9
    enter_room(m, r);
    r.send("{\"op\":\"start\",\"seed\":1,\"frame\":0,\"mask\":3}");
    REC(m, "A0", "02 A0 08 00 | 03 00 01 00 00 00 00 09");
    r.drop();
    sleep_ms(200);
    REC(m, "A0", "02 A0 08 00 | 05 00 01 00 00 00 00 09");
    REC(m, "A5 00 00 01 02 00 00", "80 A5 08 00");
    REC(m, "A3", "00 A3 00 00");   // DROPPED is in the room still: LEAVE is allowed
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 09");

    // the relay drops a player, desync
    enter_room(m, r);
    r.send("{\"op\":\"start\",\"seed\":1,\"frame\":0,\"mask\":3}");
    r.send("{\"op\":\"desync\",\"frame\":3}");
    REC(m, "A0", "02 A0 08 00 | 04 00 01 00 00 00 00 09");   // the last error stays until a reset
    r.send("{\"op\":\"dropped\",\"slot\":1}");
    REC(m, "A0", "02 A0 08 00 | 05 00 01 00 00 00 00 09");

    // RESET 00h leaves the room: 'leave', then the end of the link
    REC(m, "00", "00 00 00 00");
    LINE(r, "{\"op\":\"leave\"}");
    CHECK(r.closed());
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 00");

    // /RES of SAPI the same
    enter_room(m, r);
    m.reset();
    LINE(r, "{\"op\":\"leave\"}");
    CHECK(r.closed());
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 00");

    // /RES while CREATE waits: the status leaves IN_PROGRESS (00, no error), the link is closed (firmware b72a8d0)
    m.bus.io_write(B, c[0]);
    for (size_t i = 1; i < c.size(); i++) m.bus.io_write(B + 1, c[i]);
    CHECK(r.accept_client());
    LINE(r, CREATE_LINE);
    REC0(m, "03", "40 A1 00 00");
    m.reset();
    CHECK(r.closed());
    REC(m, "03", "00 A1 00 00");
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 00");

    // a new command while CREATE waits: the result is not wanted any more and the link is closed, a 'room'
    // that comes later does not put the card in a room (firmware b72a8d0)
    m.bus.io_write(B, c[0]);
    for (size_t i = 1; i < c.size(); i++) m.bus.io_write(B + 1, c[i]);
    CHECK(r.accept_client());
    LINE(r, CREATE_LINE);
    REC(m, "05", "02 05 0E 00 | 44 50 49 2D 31 4C 20 76 30 2E 34 2E 30 0D");   // "DPI-1L v0.4.0" as the card
    CHECK(r.closed());
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 00");

    // the same when the 'room' came but the Z80 did not read the status before the new command
    m.bus.io_write(B, c[0]);
    for (size_t i = 1; i < c.size(); i++) m.bus.io_write(B + 1, c[i]);
    CHECK(r.accept_client());
    LINE(r, CREATE_LINE);
    r.send("{\"op\":\"room\",\"code\":\"ABCD\",\"slot\":0,\"slots\":2,\"bytes\":2,\"settings\":\"0102\"}");
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 00");
    CHECK(r.closed());

    // JOIN with a code over 7 characters: PARAM
    REC(m, "A2 4E 42 01 00" + text("ABCDEFGH"), "80 A2 0A 00");

    // a saved state can not hold the link: after loading, as after RESET (the room is left)
    enter_room(m, r);
    std::vector<uint8_t> st;
    CHECK(m.save_state(st, "", err));
    CHECK(m.load_state(st, err));
    LINE(r, "{\"op\":\"leave\"}");
    CHECK(r.closed());
    REC(m, "03", "00 A1 00 00");            // the Repository API as it was saved
    REC(m, "A0", "02 A0 08 00 | 01 FF 00 00 00 00 00 00");

    // nobody listens: error 9
    FakeRelay none;
    CHECK(none.listen_on());
    int dead = none.port;
    sock_close(none.lst);
    none.lst = SOCK_INVALID;
    d->relay("127.0.0.1", (uint16_t)dead, false);
    REC(m, CREATE, "80 A1 09 00");

    // no relay address: STATUS says NOLINK, CREATE fails at once
    d->relay("", 80, true);
    REC(m, "A0", "02 A0 08 00 | 00 FF 00 00 00 00 00 00");
    REC(m, CREATE, "80 A1 09 00");
}

// status read again until byte `i` of the output equals `v` (the link thread works meanwhile)
static std::vector<uint8_t> wait_out(Machine& m, uint8_t c, int i, uint8_t v, int ms = 12000)
{
    std::vector<uint8_t> got;
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    do {
        got.clear();
        m.bus.io_write(B, c);
        m.bus.io_write(B, 0x03);
        for (int k = 0; k < 4; k++) got.push_back(m.bus.io_read(B));
        if (got[0] & 0x02)
            for (int k = got[2]; k > 0; k--) got.push_back(m.bus.io_read(B + 1));
        if ((int)got.size() > 4 + i && got[4 + i] == v) break;
        sleep_ms(5);
    } while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(ms));
    return got;
}

static std::string ip_port(int port)
{
    char b[32];
    snprintf(b, sizeof(b), "7F 00 00 01 %02X %02X", port & 0xFF, port >> 8);
    return b;
}

// TCP and DNS (common/net.c)
static void test_tcp(FakeRelay& r)
{
    Machine m;
    std::string err;
    CHECK(m.load_json(machine(r.port, "tcp"), err));
    REC(m, "80", "02 80 0A 00 | 00 00 00 00 00 00 00 00 00 00");
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "82 %02X %02X", r.port & 0xFF, r.port >> 8);
    REC(m, "81 7F 00 00 01", "00 81 00 00");
    REC(m, cmd, "00 82 00 00");
    REC(m, "80", "02 80 0A 00 | 00 00 00 00 " + ip_port(r.port));
    REC(m, "83", "00 83 00 00");
    CHECK(r.accept_client());
    std::vector<uint8_t> st = wait_out(m, 0x80, 1, 2);
    CHECK(st == hex("02 80 0A 00 | 01 02 00 00 " + ip_port(r.port)));
    REC(m, "85 05 80 48 45 4C 4C 4F", "00 85 00 00");          // TCPSEND "HELLO", push
    CHECK(r.fill(3000) && r.buf == "HELLO");
    ::send(r.cl, "WORLD!", 6, 0);
    st = wait_out(m, 0x80, 2, 6);
    CHECK(st == hex("02 80 0A 00 | 01 02 06 00 " + ip_port(r.port)));
    REC(m, "86 04 00", "02 86 06 00 | 04 00 57 4F 52 4C");       // TCPRECV max 4
    REC(m, "86 2C 01", "02 86 04 00 | 02 00 44 21");             // max 300: what is there
    REC(m, "86 10 00", "02 86 02 00 | 00 00");
    std::vector<uint8_t> c = hex("85 FD 80");                    // 253 bytes: BAD_PARAM after the length,
    m.bus.io_write(B, c[0]);                                     // the data after it keep that error (f3e583b)
    m.bus.io_write(B + 1, c[1]);
    m.bus.io_write(B + 1, c[2]);
    for (int i = 0; i < 253; i++) m.bus.io_write(B + 1, 0x41);
    REC(m, "03", "80 85 02 00");
    r.drop();                                                    // the other side closes
    st = wait_out(m, 0x80, 1, 4);
    CHECK(st == hex("02 80 0A 00 | 00 04 00 00 " + ip_port(r.port)));
    REC(m, "85 02 80 41 42", "00 85 00 00");                     // not connected: dropped
    REC(m, "84", "00 84 00 00");
    REC(m, "80", "02 80 0A 00 | 00 00 00 00 " + ip_port(r.port));

    // a port nobody listens on: failed
    FakeRelay none;
    CHECK(none.listen_on());
    int dead = none.port;
    sock_close(none.lst);
    none.lst = SOCK_INVALID;
    snprintf(cmd, sizeof(cmd), "82 %02X %02X", dead & 0xFF, dead >> 8);
    REC(m, cmd, "00 82 00 00");
    REC(m, "83", "00 83 00 00");
    st = wait_out(m, 0x80, 1, 3);
    CHECK(st.size() == 14 && st[4] == 0 && st[5] == 3);

    // RESET closes the connection (f3e583b), address and port stay
    snprintf(cmd, sizeof(cmd), "82 %02X %02X", r.port & 0xFF, r.port >> 8);
    REC(m, cmd, "00 82 00 00");
    REC(m, "83", "00 83 00 00");
    CHECK(r.accept_client());
    st = wait_out(m, 0x80, 1, 2);
    CHECK(st == hex("02 80 0A 00 | 01 02 00 00 " + ip_port(r.port)));
    REC(m, "00", "00 00 00 00");
    REC(m, "80", "02 80 0A 00 | 00 00 00 00 " + ip_port(r.port));
    CHECK(r.closed());

    // DNS
    REC(m, "89", "02 89 05 00 | 00 00 00 00 00");
    REC(m, "88 31 32 37 2E 30 2E 30 2E 31 00", "00 88 00 00");  // a number: at once
    REC(m, "89", "02 89 05 00 | 02 7F 00 00 01");
    REC(m, "88 6C 6F 63 61 6C 68 6F 73 74 00", "00 88 00 00");  // localhost
    st = wait_out(m, 0x89, 0, 2, 6000);
    CHECK(st == hex("02 89 05 00 | 02 7F 00 00 01"));
    REC(m, "88 6E 65 6E 69 2E 69 6E 76 61 6C 69 64 00", "00 88 00 00");   // neni.invalid
    st = wait_out(m, 0x89, 0, 3, 8000);
    CHECK(st == hex("02 89 05 00 | 03 00 00 00 00"));            // no old IP (f3e583b)
    REC(m, "88 6C 6F 63 61 6C 68 6F 73 74 00", "00 88 00 00");
    st = wait_out(m, 0x89, 0, 2, 6000);
    REC(m, "88 00", "00 88 00 00");                              // no name
    REC(m, "89", "02 89 05 00 | 04 7F 00 00 01");
    REC(m, "00", "00 00 00 00");                                 // RESET forgets the answer
    REC(m, "89", "02 89 05 00 | 00 00 00 00 00");
}

int main(void)
{
    test_init();
    FakeRelay tcp;
    CHECK(tcp.listen_on());
    test_no_room(tcp);
    test_game(tcp);
    test_join(tcp);
    test_link(tcp);
    test_tcp(tcp);
    FakeRelay ws;
    ws.ws = true;
    CHECK(ws.listen_on());
    test_game(ws);
    return TEST_RESULT();
}
