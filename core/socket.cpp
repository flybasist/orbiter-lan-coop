#include "socket.h"

#include <algorithm>
#include <utility>

#include "protocol.h"

// System headers are included only here: socket.h does not pull them in.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace olc {

// Everything platform-specific is in this unnamed namespace, so that the classes below read
// without #ifdef.
namespace {

#ifdef _WIN32
using AddrLength = int;
#else
using AddrLength = socklen_t;
#endif

// IPv4 address size in the form the system functions expect.
constexpr AddrLength kAddressSize = sizeof(sockaddr_in);

// How many bytes a TCP connection reads per receive call (see socket.h).
constexpr size_t kMaxReceivePerCall = 256 * 1024;

// How many bytes are handed to the system per send call.
constexpr size_t kMaxSendChunk = 1024 * 1024;

// ----- Errors -----

int lastError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

// "No data yet" (when reading) or "send buffer is full" (when writing).
bool isWouldBlock(int error) {
#ifdef _WIN32
    return error == WSAEWOULDBLOCK;
#else
    return error == EWOULDBLOCK || error == EAGAIN;
#endif
}

// The call was interrupted by a signal or the system: just repeat it.
bool isInterrupted(int error) {
#ifdef _WIN32
    return error == WSAEINTR;
#else
    return error == EINTR;
#endif
}

// connect on a non-blocking socket did not finish, the connection continues in the background.
// POSIX: EINPROGRESS (and if a signal interrupted connect, the connection continues too).
// Windows: WSAEWOULDBLOCK.
bool isConnectInProgress(int error) {
#ifdef _WIN32
    return error == WSAEWOULDBLOCK;
#else
    return error == EINPROGRESS || error == EINTR;
#endif
}

// UDP receive errors after which we simply keep reading. They happen only on Windows:
// - WSAEMSGSIZE: the packet did not fit the buffer (too long), it is dropped;
// - WSAECONNRESET: our previous packet went to a closed port and an ICMP reply came back. This
//   is not a receive error, but Windows reports it on read. The report is also
//   turned off in UdpSocket::open (SIO_UDP_CONNRESET); this is here in case that failed.
bool isIgnorableUdpReceiveError(int error) {
#ifdef _WIN32
    return error == WSAEMSGSIZE || error == WSAECONNRESET;
#else
    (void)error;
    return false;
#endif
}

// ----- Socket -----

void closeHandle(SocketHandle handle) {
#ifdef _WIN32
    closesocket(handle);
#else
    ::close(handle);
#endif
}

bool setNonBlocking(SocketHandle handle) {
#ifdef _WIN32
    u_long enable = 1;
    return ioctlsocket(handle, FIONBIO, &enable) == 0;
#else
    int flags = fcntl(handle, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(handle, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

// Turns on a socket option with the value "yes" (1).
bool setOptionOn(SocketHandle handle, int level, int name) {
    int value = 1;
    return setsockopt(handle, level, name, reinterpret_cast<const char*>(&value),
                      static_cast<int>(sizeof(value))) == 0;
}

// TCP socket setup: non-blocking, no send delay and no SIGPIPE.
// TCP_NODELAY: messages are small, and a key press must not wait until the system
// collects more data.
// SIGPIPE: otherwise a write into a closed connection kills the process (Orbiter or a test).
// On Linux this is the MSG_NOSIGNAL flag in send (kSendFlags), on macOS the socket option
// SO_NOSIGPIPE, Windows has no such signal.
bool prepareTcpSocket(SocketHandle handle) {
    if (!setNonBlocking(handle)) {
        return false;
    }
    if (!setOptionOn(handle, IPPROTO_TCP, TCP_NODELAY)) {
        return false;
    }
#ifdef SO_NOSIGPIPE
    if (!setOptionOn(handle, SOL_SOCKET, SO_NOSIGPIPE)) {
        return false;
    }
#endif
    return true;
}

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

// ----- Addresses -----

sockaddr_in toSockaddr(const Endpoint& endpoint) {
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(endpoint.ip);
    address.sin_port = htons(endpoint.port);
    return address;
}

Endpoint fromSockaddr(const sockaddr_in& address) {
    Endpoint endpoint;
    endpoint.ip = static_cast<uint32_t>(ntohl(address.sin_addr.s_addr));
    endpoint.port = static_cast<uint16_t>(ntohs(address.sin_port));
    return endpoint;
}

// Binds the socket to a port on all addresses of the computer (port 0 - any free one).
bool bindAnyAddress(SocketHandle handle, uint16_t port) {
    Endpoint any;
    any.ip = 0;  // INADDR_ANY
    any.port = port;
    sockaddr_in address = toSockaddr(any);
    return bind(handle, reinterpret_cast<const sockaddr*>(&address), kAddressSize) == 0;
}

// The actual port of the socket (getsockname). 0 on error.
uint16_t localPortOf(SocketHandle handle) {
    if (handle == kNoSocket) {
        return 0;
    }
    sockaddr_in address = {};
    AddrLength length = sizeof(address);
    if (getsockname(handle, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return 0;
    }
    return fromSockaddr(address).port;
}

// ----- Connection check -----

enum class ConnectProgress { Waiting, Done, Failed };

// Checks without waiting (select with zero timeout) whether connect has finished.
// Success - the socket is writable and SO_ERROR is 0. Failure: on POSIX the socket also
// becomes writable but SO_ERROR holds the error; on Windows failure arrives
// in the exception set, so we check it too.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)  // FD_SET/FD_ZERO macros: the while (0) condition is constant
#endif
ConnectProgress checkConnect(SocketHandle handle) {
#ifndef _WIN32
    if (handle >= FD_SETSIZE) {
        return ConnectProgress::Failed;  // select cannot handle such a descriptor number
    }
#endif
    fd_set writeSet;
    fd_set errorSet;
    FD_ZERO(&writeSet);
    FD_ZERO(&errorSet);
    FD_SET(handle, &writeSet);
    FD_SET(handle, &errorSet);
    timeval timeout = {};  // 0 seconds: do not wait
    // The first select parameter is not used on Windows.
    int ready = select(static_cast<int>(handle) + 1, nullptr, &writeSet, &errorSet, &timeout);
    if (ready < 0) {
        return isInterrupted(lastError()) ? ConnectProgress::Waiting : ConnectProgress::Failed;
    }
    if (ready == 0) {
        return ConnectProgress::Waiting;
    }

    int socketError = 0;
    AddrLength length = sizeof(socketError);
    if (getsockopt(handle, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError),
                   &length) != 0) {
        return ConnectProgress::Failed;
    }
    if (socketError != 0) {
        return ConnectProgress::Failed;
    }
    return FD_ISSET(handle, &writeSet) ? ConnectProgress::Done : ConnectProgress::Failed;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace

// ----- Network and addresses -----

bool startNetwork() {
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return true;
#endif
}

void stopNetwork() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool parseIpv4(const std::string& text, uint32_t& ip) {
    uint32_t result = 0;
    int parts = 0;
    size_t i = 0;
    while (true) {
        // One number: 1 to 3 digits, at most 255.
        uint32_t value = 0;
        int digits = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            value = value * 10 + static_cast<uint32_t>(text[i] - '0');
            digits++;
            i++;
            if (digits > 3) {
                return false;
            }
        }
        if (digits == 0 || value > 255) {
            return false;
        }
        result = (result << 8) | value;
        parts++;

        if (i == text.size()) {
            break;
        }
        if (text[i] != '.') {
            return false;
        }
        i++;  // dot; another number must follow it
    }
    if (parts != 4) {
        return false;
    }
    ip = result;
    return true;
}

std::string endpointToString(const Endpoint& endpoint) {
    return std::to_string((endpoint.ip >> 24) & 0xFF) + "." +
           std::to_string((endpoint.ip >> 16) & 0xFF) + "." +
           std::to_string((endpoint.ip >> 8) & 0xFF) + "." + std::to_string(endpoint.ip & 0xFF) +
           ":" + std::to_string(endpoint.port);
}

bool operator==(const Endpoint& a, const Endpoint& b) {
    return a.ip == b.ip && a.port == b.port;
}

// ----- UdpSocket -----

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::open(uint16_t port) {
    close();

    SocketHandle handle = static_cast<SocketHandle>(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (handle == kNoSocket) {
        return false;
    }
    if (!setNonBlocking(handle)) {
        closeHandle(handle);
        return false;
    }

#ifdef _WIN32
    // Without this, Windows reports every ICMP "port closed" reply as a
    // WSAECONNRESET error on read, although receiving has nothing to do with it.
    // SIO_UDP_CONNRESET is not in every header version, so it is defined by hand.
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
    BOOL newBehavior = FALSE;
    DWORD returned = 0;
    WSAIoctl(handle, SIO_UDP_CONNRESET, &newBehavior, sizeof(newBehavior), nullptr, 0,
             &returned, nullptr, nullptr);  // failure is not a problem, see receiveFrom
#endif

    if (!bindAnyAddress(handle, port)) {
        closeHandle(handle);
        return false;
    }
    handle_ = handle;
    return true;
}

void UdpSocket::close() {
    if (handle_ != kNoSocket) {
        closeHandle(handle_);
        handle_ = kNoSocket;
    }
}

bool UdpSocket::isOpen() const {
    return handle_ != kNoSocket;
}

uint16_t UdpSocket::localPort() const {
    return localPortOf(handle_);
}

bool UdpSocket::sendTo(const Endpoint& to, const std::vector<uint8_t>& packet) {
    if (handle_ == kNoSocket || packet.size() > kMaxUdpPacketSize) {
        return false;
    }
    sockaddr_in address = toSockaddr(to);
    while (true) {
        auto sent = sendto(handle_, reinterpret_cast<const char*>(packet.data()),
                           static_cast<int>(packet.size()), 0,
                           reinterpret_cast<const sockaddr*>(&address), kAddressSize);
        if (sent >= 0) {
            return static_cast<size_t>(sent) == packet.size();
        }
        if (!isInterrupted(lastError())) {
            return false;
        }
    }
}

bool UdpSocket::receiveFrom(std::vector<uint8_t>& packet, Endpoint& from) {
    if (handle_ == kNoSocket) {
        return false;
    }

    // The buffer is one byte larger than the limit: that way a too long packet is visible
    // (the system truncates it to the buffer size).
    uint8_t buffer[kMaxUdpPacketSize + 1];
    while (true) {
        sockaddr_in address = {};
        AddrLength length = sizeof(address);
        auto received = recvfrom(handle_, reinterpret_cast<char*>(buffer),
                                 static_cast<int>(sizeof(buffer)), 0,
                                 reinterpret_cast<sockaddr*>(&address), &length);
        if (received < 0) {
            int error = lastError();
            if (isInterrupted(error) || isIgnorableUdpReceiveError(error)) {
                continue;
            }
            return false;  // no packets (EWOULDBLOCK) or a real error
        }
        if (static_cast<size_t>(received) > kMaxUdpPacketSize) {
            continue;  // packet too long: drop it and read the next one
        }
        packet.assign(buffer, buffer + received);
        from = fromSockaddr(address);
        return true;
    }
}

// ----- TcpConnection -----

TcpConnection::~TcpConnection() {
    close();
}

TcpConnection::TcpConnection(TcpConnection&& other) noexcept
    : handle_(other.handle_),
      state_(other.state_),
      remote_(other.remote_),
      outbox_(std::move(other.outbox_)),
      outboxSent_(other.outboxSent_) {
    // Nothing is left in the source: its destructor will not close our socket.
    other.handle_ = kNoSocket;
    other.state_ = State::Closed;
    other.outbox_.clear();
    other.outboxSent_ = 0;
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        state_ = other.state_;
        remote_ = other.remote_;
        outbox_ = std::move(other.outbox_);
        outboxSent_ = other.outboxSent_;

        other.handle_ = kNoSocket;
        other.state_ = State::Closed;
        other.outbox_.clear();
        other.outboxSent_ = 0;
    }
    return *this;
}

bool TcpConnection::startConnect(const Endpoint& to) {
    close();

    SocketHandle handle = static_cast<SocketHandle>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (handle == kNoSocket) {
        return false;
    }
    if (!prepareTcpSocket(handle)) {
        closeHandle(handle);
        return false;
    }

    sockaddr_in address = toSockaddr(to);
    int result = connect(handle, reinterpret_cast<const sockaddr*>(&address), kAddressSize);
    if (result == 0) {
        state_ = State::Connected;  // sometimes the connection is ready at once
    } else if (isConnectInProgress(lastError())) {
        state_ = State::Connecting;
    } else {
        closeHandle(handle);
        return false;
    }
    handle_ = handle;
    remote_ = to;
    return true;
}

void TcpConnection::updateConnecting() {
    ConnectProgress progress = checkConnect(handle_);
    if (progress == ConnectProgress::Done) {
        state_ = State::Connected;
    } else if (progress == ConnectProgress::Failed) {
        close();
    }
}

TcpConnection::State TcpConnection::state() {
    if (state_ == State::Connecting) {
        updateConnecting();
    }
    return state_;
}

bool TcpConnection::send(const std::vector<uint8_t>& frame) {
    if (state_ == State::Closed) {
        return false;
    }
    outbox_.insert(outbox_.end(), frame.begin(), frame.end());
    return flush();
}

bool TcpConnection::flush() {
    if (state_ == State::Connecting) {
        updateConnecting();
    }
    if (state_ == State::Closed) {
        return false;
    }
    if (state_ == State::Connecting) {
        return true;  // still connecting: frames wait in the queue
    }

    while (outboxSent_ < outbox_.size()) {
        size_t chunk = std::min(outbox_.size() - outboxSent_, kMaxSendChunk);
        auto sent = ::send(handle_, reinterpret_cast<const char*>(outbox_.data() + outboxSent_),
                           static_cast<int>(chunk), kSendFlags);
        if (sent < 0) {
            int error = lastError();
            if (isInterrupted(error)) {
                continue;
            }
            if (isWouldBlock(error)) {
                return true;  // the system buffer is full: the rest goes out on the next flush
            }
            close();
            return false;
        }
        outboxSent_ += static_cast<size_t>(sent);
    }

    // Everything went out: the queue can be cleared.
    outbox_.clear();
    outboxSent_ = 0;
    return true;
}

size_t TcpConnection::pendingBytes() const {
    return outbox_.size() - outboxSent_;
}

bool TcpConnection::receive(TcpFrameReader& frames) {
    if (state_ == State::Connecting) {
        updateConnecting();
    }
    if (state_ == State::Closed) {
        return false;
    }
    if (state_ == State::Connecting) {
        return true;  // not connected yet: there can be no data
    }

    uint8_t buffer[16 * 1024];
    size_t total = 0;
    while (total < kMaxReceivePerCall) {
        auto received =
            recv(handle_, reinterpret_cast<char*>(buffer), static_cast<int>(sizeof(buffer)), 0);
        if (received < 0) {
            int error = lastError();
            if (isInterrupted(error)) {
                continue;
            }
            if (isWouldBlock(error)) {
                return true;  // everything has been read
            }
            close();
            return false;
        }
        if (received == 0) {
            close();  // the peer closed the connection
            return false;
        }
        frames.append(buffer, static_cast<size_t>(received));
        total += static_cast<size_t>(received);
    }
    return true;  // enough read for one call, the rest next time
}

void TcpConnection::close() {
    if (handle_ != kNoSocket) {
        closeHandle(handle_);
        handle_ = kNoSocket;
    }
    state_ = State::Closed;
    outbox_.clear();
    outboxSent_ = 0;
}

const Endpoint& TcpConnection::remote() const {
    return remote_;
}

// ----- TcpListener -----

TcpListener::~TcpListener() {
    close();
}

bool TcpListener::open(uint16_t port) {
    close();

    SocketHandle handle = static_cast<SocketHandle>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (handle == kNoSocket) {
        return false;
    }
    if (!setNonBlocking(handle)) {
        closeHandle(handle);
        return false;
    }

#ifndef _WIN32
    // Lets the host reopen the same port right after a restart while old
    // connections are still in TIME_WAIT. On Windows SO_REUSEADDR means something else:
    // it lets a second socket take a port that is already in use. There it is not needed and harmful.
    if (!setOptionOn(handle, SOL_SOCKET, SO_REUSEADDR)) {
        closeHandle(handle);
        return false;
    }
#endif

    if (!bindAnyAddress(handle, port) || listen(handle, SOMAXCONN) != 0) {
        closeHandle(handle);
        return false;
    }
    handle_ = handle;
    return true;
}

void TcpListener::close() {
    if (handle_ != kNoSocket) {
        closeHandle(handle_);
        handle_ = kNoSocket;
    }
}

bool TcpListener::isOpen() const {
    return handle_ != kNoSocket;
}

uint16_t TcpListener::localPort() const {
    return localPortOf(handle_);
}

bool TcpListener::accept(TcpConnection& connection) {
    if (handle_ == kNoSocket) {
        return false;
    }

    sockaddr_in address = {};
    SocketHandle accepted = kNoSocket;
    while (true) {
        AddrLength length = sizeof(address);
        accepted = static_cast<SocketHandle>(
            ::accept(handle_, reinterpret_cast<sockaddr*>(&address), &length));
        if (accepted != kNoSocket) {
            break;
        }
        if (!isInterrupted(lastError())) {
            return false;  // nobody is waiting (EWOULDBLOCK) or an error: try next time
        }
    }

    // An accepted socket does not always inherit the listener's settings, so set them again.
    if (!prepareTcpSocket(accepted)) {
        closeHandle(accepted);
        return false;
    }

    connection.close();
    connection.handle_ = accepted;
    connection.state_ = TcpConnection::State::Connected;
    connection.remote_ = fromSockaddr(address);
    return true;
}

}  // namespace olc
