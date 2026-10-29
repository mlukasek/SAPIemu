#pragma once
// Bridges between an emulated serial channel and the host: a TCP server (raw or
// telnet, auto-detected; TeraTerm can connect directly) and a host COM port.
// Each bridge runs its own thread; SerialChannel is thread safe.
#include <string>
#include <vector>
#include "json.hpp"

class SerialChannel;

class SerialBridge {
public:
    virtual ~SerialBridge() {}
    virtual void stop(void) = 0;
    virtual std::string describe(void) const = 0;   // human readable status
    virtual bool client_connected(void) const = 0;
    virtual const char* kind(void) const = 0;        // "tcp" or "com"
    SerialChannel* channel = nullptr;
};

// TCP protocols: telnet = the server negotiates character mode right after the
// connection (TeraTerm "Telnet", Windows telnet, PuTTY Telnet); raw = bytes as
// they are (TeraTerm "Other", PuTTY Raw, netcat); auto = raw until the client
// starts with a telnet command.
enum TcpProtocol { TCP_TELNET = 0, TCP_RAW = 1, TCP_AUTO = 2 };
int tcp_protocol_from_name(const std::string& name);   // -1 if unknown

// TCP server on bind:port (port 0 = any free port, see tcp_bridge_port()).
// "127.0.0.1"/"localhost" listens on IPv4 and IPv6 loopback, "0.0.0.0" on all
// interfaces. dcd_follows: the channel's "terminal connected" follows the
// client connection.
SerialBridge* tcp_bridge_start(SerialChannel* ch, const std::string& bind, int port, bool dcd_follows,
                               int protocol, std::string& err);
int tcp_bridge_port(SerialBridge* b);

// Host serial port ("COM5" on Windows, "/dev/ttyUSB0" on Linux), 8N1.
SerialBridge* com_bridge_start(SerialChannel* ch, const std::string& port, int baud, std::string& err);

// Create the bridges described by a card's "serial" configuration object:
//   { "tcp": { "port": 2323, "bind": "127.0.0.1", "dcd_follows": true },
//     "com": { "port": "COM5", "baud": 9600 } }
bool serial_bridges_from_config(SerialChannel* ch, const nlohmann::json& cfg, int default_baud,
                                std::vector<SerialBridge*>& out, std::string& err);
// The errors of that configuration without opening anything (a port in use or a missing COM port only warn when
// the bridges start: the machine runs without them).
bool serial_bridges_check(const nlohmann::json& cfg, std::string& err);
