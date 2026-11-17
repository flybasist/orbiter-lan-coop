// Socket wrapper: UDP, TCP connection and TCP listener (docs/PROTOCOL.md, section 3).
//
// Why: the network core must not know about Winsock and POSIX sockets. Platform
// differences (Windows - Winsock, Mac and Linux - POSIX sockets) are hidden in socket.cpp,
// and this header does not include system network headers.
// All sockets are non-blocking, there are no threads inside, and no function waits for the
// network: "no data" is a normal result (false or an empty answer), not an error.
// So the wrapper can be polled from Orbiter's main thread every frame - that is how
// the plugin works (decision 23 in docs/DECISIONS.md).
// IPv4 only; the address is given as digits ("192.168.1.10"), names (DNS) are not supported.
//
// The network must be started: startNetwork() before the first socket, stopNetwork() after the last.
// In an Orbiter plugin call them from clbkSimulationStart and clbkSimulationEnd, not from
// InitModule: Winsock must not be started while the DLL is loading (docs/ORBITER-INTERNALS.md,
// section 14).
// The classes are not thread-safe: one object - one thread.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "header.h"

namespace olc {

// Socket descriptor. On Windows it is SOCKET (UINT_PTR), on other systems int.
// Our own type, so that system headers are not needed here.
#ifdef _WIN32
using SocketHandle = uintptr_t;
constexpr SocketHandle kNoSocket = ~static_cast<uintptr_t>(0);  // INVALID_SOCKET
#else
using SocketHandle = int;
constexpr SocketHandle kNoSocket = -1;
#endif

// Starts the system's network layer. On Windows this is WSAStartup (version 2.2), on other
// systems it does nothing. Returns false if it could not be started.
bool startNetwork();

// Stops the network layer (WSACleanup on Windows). Call after closing the sockets.
void stopNetwork();

// IPv4 address and port.
struct Endpoint {
    uint32_t ip = 0;     // in normal order: 127.0.0.1 = 0x7F000001
    uint16_t port = 0;
};

// Parses an address like "192.168.1.10": exactly 4 decimal numbers 0...255 separated by
// dots, no spaces or other characters. On error returns false and leaves ip unchanged.
bool parseIpv4(const std::string& text, uint32_t& ip);

// Address and port for logs: "192.168.1.10:47800".
std::string endpointToString(const Endpoint& endpoint);

// The host needs it to recognize a client by the address of its UDP packet.
bool operator==(const Endpoint& a, const Endpoint& b);

// UDP socket: packets are sent and received whole, without a connection.
class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Opens a non-blocking socket on all addresses of the computer. port 0 - any
    // free port. Returns false on error. An already open socket is closed first.
    bool open(uint16_t port);
    void close();
    bool isOpen() const;

    // The actual port after open (with port 0 the system chooses it): the client sends it
    // to the host in UDP_READY. 0 if the socket is closed.
    uint16_t localPort() const;

    // Sends one packet. false on error (including when the system send buffer is
    // full: UDP does not wait, the packet is simply lost) or if the packet is larger than kMaxUdpPacketSize.
    bool sendTo(const Endpoint& to, const std::vector<uint8_t>& packet);

    // Takes the next received packet and the sender's address. false - no more packets
    // (or the socket is closed). Packets longer than kMaxUdpPacketSize are silently dropped.
    bool receiveFrom(std::vector<uint8_t>& packet, Endpoint& from);

private:
    SocketHandle handle_ = kNoSocket;
};

// TCP connection. Data flows as a stream, so TcpFrameReader assembles the messages.
class TcpConnection {
public:
    enum class State { Closed, Connecting, Connected };

    TcpConnection() = default;
    ~TcpConnection();
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    // Move is needed so that accept can hand over a connection and connections can live in a vector.
    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    // Starts connecting and returns at once. false - immediate error. Otherwise State
    // becomes Connected when the connection completes, or Closed if it fails.
    bool startConnect(const Endpoint& to);

    // Current state. For Connecting it checks the connection without waiting and, if
    // needed, moves to Connected or closes (Closed).
    State state();

    // Puts a ready frame (makeTcpFrame) into the queue and sends as much as it can.
    // A TCP send may go out partially: the rest stays in the queue and is sent by
    // later send and flush calls. While connecting (Connecting), frames only accumulate.
    // false - the connection is closed or broken (then it is closed).
    bool send(const std::vector<uint8_t>& frame);

    // Sends the rest of the queue. false - the connection is closed or broken.
    bool flush();

    // How many bytes are not yet sent.
    size_t pendingBytes() const;

    // Reads everything that has arrived (but no more than a few hundred KB per call, so that
    // a chatty peer does not hold the thread for long) and passes it to frames. If nothing
    // arrived - true. false - the peer closed the connection or an error occurred (the connection
    // is closed); bytes that arrived before closing are already in frames - take them.
    bool receive(TcpFrameReader& frames);

    void close();

    // The peer's address (for Closed - the last one we connected to).
    const Endpoint& remote() const;

private:
    friend class TcpListener;  // accept hands over a ready connection

    // Checks the connection in the Connecting state (without waiting).
    void updateConnecting();

    SocketHandle handle_ = kNoSocket;
    State state_ = State::Closed;
    Endpoint remote_;
    std::vector<uint8_t> outbox_;  // send queue
    size_t outboxSent_ = 0;        // how many queue bytes have already gone out
};

// TCP listener: accepts incoming connections.
class TcpListener {
public:
    TcpListener() = default;
    ~TcpListener();
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    // Opens a non-blocking listening socket on all addresses. port 0 - any free port.
    // Returns false on error. An already open listener is closed first.
    bool open(uint16_t port);
    void close();
    bool isOpen() const;

    // The actual port after open; 0 if the listener is closed.
    uint16_t localPort() const;

    // Accepts one pending connection without waiting. false - nobody is waiting.
    // The new connection is already Connected and also non-blocking.
    bool accept(TcpConnection& connection);

private:
    SocketHandle handle_ = kNoSocket;
};

}  // namespace olc
