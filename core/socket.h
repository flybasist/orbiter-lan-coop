// Обёртка над сокетами: UDP, TCP-соединение и TCP-слушатель (docs/PROTOCOL.md, раздел 3).
//
// Зачем: сетевое ядро не должно знать про Winsock и POSIX-сокеты. Платформенные
// различия (Windows - Winsock, Mac и Linux - сокеты POSIX) спрятаны в socket.cpp,
// а этот заголовок не подключает системные сетевые заголовки.
// Все сокеты неблокирующие, потоков внутри нет, и ни одна функция не ждёт сеть:
// "нет данных" - это обычный результат (false или пустой ответ), а не ошибка.
// Поэтому обёртку можно опрашивать и из главного потока каждый кадр, и из отдельного
// сетевого потока - какой из вариантов выбрать, решает вопрос В4 в docs/QUESTIONS.md.
// Только IPv4; адрес задаётся цифрами ("192.168.1.10"), имена (DNS) не поддерживаются.
//
// Сеть надо запустить: startNetwork() до первого сокета, stopNetwork() после последнего.
// В плагине Orbiter вызывайте их из clbkSimulationStart и clbkSimulationEnd, а не из
// InitModule: при загрузке DLL запускать Winsock нельзя (docs/ORBITER-INTERNALS.md,
// раздел 14).
// Классы не потокобезопасны: один объект - один поток.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "header.h"

namespace olc {

// Дескриптор сокета. На Windows это SOCKET (UINT_PTR), в остальных системах int.
// Тип свой, чтобы не подключать системные заголовки.
#ifdef _WIN32
using SocketHandle = uintptr_t;
constexpr SocketHandle kNoSocket = ~static_cast<uintptr_t>(0);  // INVALID_SOCKET
#else
using SocketHandle = int;
constexpr SocketHandle kNoSocket = -1;
#endif

// Запускает сетевую часть системы. На Windows это WSAStartup (версия 2.2), в остальных
// системах ничего не делает. Возвращает false, если запустить не удалось.
bool startNetwork();

// Останавливает сетевую часть (на Windows WSACleanup). Вызывать после закрытия сокетов.
void stopNetwork();

// Адрес IPv4 и порт.
struct Endpoint {
    uint32_t ip = 0;     // в обычном порядке: 127.0.0.1 = 0x7F000001
    uint16_t port = 0;
};

// Разбирает адрес вида "192.168.1.10": ровно 4 десятичных числа 0...255 через точку,
// без пробелов и других символов. При ошибке возвращает false, ip не меняется.
bool parseIpv4(const std::string& text, uint32_t& ip);

// Адрес и порт для логов: "192.168.1.10:47800".
std::string endpointToString(const Endpoint& endpoint);

// Нужен хосту, чтобы узнать клиента по адресу его UDP-пакета.
bool operator==(const Endpoint& a, const Endpoint& b);

// UDP-сокет: пакеты отправляются и принимаются целиком, без соединения.
class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Открывает неблокирующий сокет на всех адресах компьютера. port 0 - любой
    // свободный порт. Возвращает false при ошибке. Уже открытый сокет сначала закрывается.
    bool open(uint16_t port);
    void close();
    bool isOpen() const;

    // Настоящий порт после open (с port 0 его выбирает система): клиент шлёт его
    // хосту в UDP_READY. 0, если сокет закрыт.
    uint16_t localPort() const;

    // Отправляет один пакет. false при ошибке (в том числе когда буфер отправки системы
    // занят: UDP не ждёт, пакет просто теряется) или если пакет больше kMaxUdpPacketSize.
    bool sendTo(const Endpoint& to, const std::vector<uint8_t>& packet);

    // Достаёт следующий принятый пакет и адрес отправителя. false - пакетов больше нет
    // (или сокет закрыт). Пакеты длиннее kMaxUdpPacketSize молча отбрасываются.
    bool receiveFrom(std::vector<uint8_t>& packet, Endpoint& from);

private:
    SocketHandle handle_ = kNoSocket;
};

// TCP-соединение. Данные идут потоком, поэтому сообщения собирает TcpFrameReader.
class TcpConnection {
public:
    enum class State { Closed, Connecting, Connected };

    TcpConnection() = default;
    ~TcpConnection();
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    // Перемещение нужно, чтобы accept отдавал соединение и чтобы хранить их в vector.
    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    // Начинает подключение и сразу возвращается. false - сразу ошибка. Иначе State
    // станет Connected, когда подключение завершится, или Closed, если не удастся.
    bool startConnect(const Endpoint& to);

    // Текущее состояние. Для Connecting проверяет подключение без ожидания и при
    // необходимости переходит в Connected или закрывается (Closed).
    State state();

    // Ставит готовый кадр (makeTcpFrame) в очередь и отправляет сколько получится.
    // TCP-отправка может уйти частично: остаток хранится в очереди и досылается
    // следующими send и flush. Пока идёт подключение (Connecting), кадры только копятся.
    // false - соединение закрыто или разорвано (тогда оно закрывается).
    bool send(const std::vector<uint8_t>& frame);

    // Досылает очередь. false - соединение закрыто или разорвано.
    bool flush();

    // Сколько байтов ещё не отправлено.
    size_t pendingBytes() const;

    // Читает всё, что пришло (но не больше нескольких сотен КБ за вызов, чтобы
    // болтливый собеседник не занял поток надолго), и отдаёт в frames. Если ничего не
    // пришло - true. false - собеседник закрыл соединение или ошибка (соединение
    // закрыто); байты, пришедшие до закрытия, уже лежат в frames - заберите их.
    bool receive(TcpFrameReader& frames);

    void close();

    // Адрес собеседника (для Closed - последний, к которому подключались).
    const Endpoint& remote() const;

private:
    friend class TcpListener;  // accept отдаёт готовое соединение

    // Проверяет подключение в состоянии Connecting (без ожидания).
    void updateConnecting();

    SocketHandle handle_ = kNoSocket;
    State state_ = State::Closed;
    Endpoint remote_;
    std::vector<uint8_t> outbox_;  // очередь на отправку
    size_t outboxSent_ = 0;        // сколько байтов очереди уже ушло
};

// TCP-слушатель: принимает входящие подключения.
class TcpListener {
public:
    TcpListener() = default;
    ~TcpListener();
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    // Открывает неблокирующий слушающий сокет на всех адресах. port 0 - любой свободный.
    // Возвращает false при ошибке. Уже открытый слушатель сначала закрывается.
    bool open(uint16_t port);
    void close();
    bool isOpen() const;

    // Настоящий порт после open; 0, если слушатель закрыт.
    uint16_t localPort() const;

    // Принимает одно ожидающее подключение без ожидания. false - никого нет.
    // Новое соединение уже в состоянии Connected и тоже неблокирующее.
    bool accept(TcpConnection& connection);

private:
    SocketHandle handle_ = kNoSocket;
};

}  // namespace olc
