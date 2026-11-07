#include "socket.h"

#include <algorithm>
#include <utility>

#include "protocol.h"

// Системные заголовки подключаются только здесь: socket.h их не тянет.
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

// Всё платформенное - в этом безымянном пространстве имён, чтобы классы ниже читались
// без #ifdef.
namespace {

#ifdef _WIN32
using AddrLength = int;
#else
using AddrLength = socklen_t;
#endif

// Размер адреса IPv4 в виде, который ждут системные функции.
constexpr AddrLength kAddressSize = sizeof(sockaddr_in);

// Сколько байтов TCP-соединение читает за один вызов receive (см. socket.h).
constexpr size_t kMaxReceivePerCall = 256 * 1024;

// Сколько байтов отдаётся системе за один вызов send.
constexpr size_t kMaxSendChunk = 1024 * 1024;

// ----- Ошибки -----

int lastError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

// "Данных пока нет" (при чтении) или "буфер отправки полон" (при записи).
bool isWouldBlock(int error) {
#ifdef _WIN32
    return error == WSAEWOULDBLOCK;
#else
    return error == EWOULDBLOCK || error == EAGAIN;
#endif
}

// Вызов прервал сигнал или система: его надо просто повторить.
bool isInterrupted(int error) {
#ifdef _WIN32
    return error == WSAEINTR;
#else
    return error == EINTR;
#endif
}

// connect на неблокирующем сокете не успел закончить, подключение идёт в фоне.
// POSIX: EINPROGRESS (а если connect прервал сигнал, подключение тоже продолжается).
// Windows: WSAEWOULDBLOCK.
bool isConnectInProgress(int error) {
#ifdef _WIN32
    return error == WSAEWOULDBLOCK;
#else
    return error == EINPROGRESS || error == EINTR;
#endif
}

// Ошибки приёма UDP, после которых надо просто читать дальше. Бывают только на Windows:
// - WSAEMSGSIZE: пакет не поместился в буфер (слишком длинный), он отбрасывается;
// - WSAECONNRESET: наш прошлый пакет ушёл на закрытый порт и пришёл ответ ICMP. Это
//   не ошибка приёма, но Windows сообщает о ней при чтении. Дополнительно сообщение
//   отключено в UdpSocket::open (SIO_UDP_CONNRESET), а здесь - на случай, если не вышло.
bool isIgnorableUdpReceiveError(int error) {
#ifdef _WIN32
    return error == WSAEMSGSIZE || error == WSAECONNRESET;
#else
    (void)error;
    return false;
#endif
}

// ----- Сокет -----

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

// Включает опцию сокета со значением "да" (1).
bool setOptionOn(SocketHandle handle, int level, int name) {
    int value = 1;
    return setsockopt(handle, level, name, reinterpret_cast<const char*>(&value),
                      static_cast<int>(sizeof(value))) == 0;
}

// Настройка TCP-сокета: неблокирующий, без задержки отправки и без SIGPIPE.
// TCP_NODELAY: сообщения мелкие, и нажатие клавиши не должно ждать, пока система
// накопит больше данных.
// SIGPIPE: запись в закрытое соединение иначе убьёт процесс (Orbiter или тест).
// На Linux это флаг MSG_NOSIGNAL в send (kSendFlags), на macOS - опция сокета
// SO_NOSIGPIPE, на Windows такого сигнала нет.
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

// ----- Адреса -----

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

// Привязывает сокет к порту на всех адресах компьютера (port 0 - любой свободный).
bool bindAnyAddress(SocketHandle handle, uint16_t port) {
    Endpoint any;
    any.ip = 0;  // INADDR_ANY
    any.port = port;
    sockaddr_in address = toSockaddr(any);
    return bind(handle, reinterpret_cast<const sockaddr*>(&address), kAddressSize) == 0;
}

// Настоящий порт сокета (getsockname). 0 при ошибке.
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

// ----- Проверка подключения -----

enum class ConnectProgress { Waiting, Done, Failed };

// Смотрит без ожидания (select с нулевым таймаутом), закончился ли connect.
// Успех - сокет готов к записи, а SO_ERROR равен 0. Неудача: на POSIX сокет тоже
// становится готов к записи, но SO_ERROR хранит ошибку; на Windows неудача приходит
// в наборе исключений, поэтому смотрим и его.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)  // макросы FD_SET/FD_ZERO: условие while (0) постоянное
#endif
ConnectProgress checkConnect(SocketHandle handle) {
#ifndef _WIN32
    if (handle >= FD_SETSIZE) {
        return ConnectProgress::Failed;  // select не умеет работать с таким номером
    }
#endif
    fd_set writeSet;
    fd_set errorSet;
    FD_ZERO(&writeSet);
    FD_ZERO(&errorSet);
    FD_SET(handle, &writeSet);
    FD_SET(handle, &errorSet);
    timeval timeout = {};  // 0 секунд: не ждать
    // Первый параметр select на Windows не используется.
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

// ----- Сеть и адреса -----

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
        // Одно число: от 1 до 3 цифр, не больше 255.
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
        i++;  // точка; после неё обязано быть ещё число
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
    // Без этого Windows сообщает о каждом ответе ICMP "порт закрыт" ошибкой
    // WSAECONNRESET при чтении, хотя приём тут ни при чём.
    // SIO_UDP_CONNRESET есть не во всех версиях заголовков, поэтому задан вручную.
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
    BOOL newBehavior = FALSE;
    DWORD returned = 0;
    WSAIoctl(handle, SIO_UDP_CONNRESET, &newBehavior, sizeof(newBehavior), nullptr, 0,
             &returned, nullptr, nullptr);  // не вышло - не страшно, см. receiveFrom
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

    // Буфер на один байт больше предела: так видно, что пакет слишком длинный
    // (система обрезает его до размера буфера).
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
            return false;  // пакетов нет (EWOULDBLOCK) или настоящая ошибка
        }
        if (static_cast<size_t>(received) > kMaxUdpPacketSize) {
            continue;  // слишком длинный пакет: отбрасываем и читаем следующий
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
    // У источника ничего не остаётся: его деструктор не закроет наш сокет.
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
        state_ = State::Connected;  // бывает, что соединение готово сразу
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
        return true;  // подключение ещё идёт: кадры ждут в очереди
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
                return true;  // буфер системы полон: остаток уйдёт при следующем flush
            }
            close();
            return false;
        }
        outboxSent_ += static_cast<size_t>(sent);
    }

    // Всё ушло: очередь можно очистить.
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
        return true;  // ещё не подключились: данных быть не может
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
                return true;  // всё прочитано
            }
            close();
            return false;
        }
        if (received == 0) {
            close();  // собеседник закрыл соединение
            return false;
        }
        frames.append(buffer, static_cast<size_t>(received));
        total += static_cast<size_t>(received);
    }
    return true;  // за вызов прочитано достаточно, остальное - в следующий раз
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
    // Позволяет сразу открыть тот же порт после перезапуска хоста, пока старые
    // соединения ещё в состоянии TIME_WAIT. На Windows SO_REUSEADDR значит другое:
    // разрешает второму сокету занять уже занятый порт. Там он не нужен и вреден.
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
            return false;  // никого нет (EWOULDBLOCK) или ошибка: попробуем в следующий раз
        }
    }

    // Принятый сокет не всегда наследует настройки слушающего, поэтому задаём их заново.
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
