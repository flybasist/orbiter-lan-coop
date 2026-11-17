// Сетевая часть хоста: подключение клиентов и рассылка состояния мира.
//
// Что делает (docs/PROTOCOL.md, раздел 3):
// - слушает TCP и UDP на одном порту;
// - ведёт рукопожатие: HELLO → WELCOME (или REJECT) → BODIES → VESSELS → OWNERSHIP;
//   затем ждёт от клиента CLASSES_MISSING и UDP_READY;
// - отвечает PONG на PING и по PING узнаёт настоящий UDP-адрес клиента (клиент за NAT);
// - рассылает WORLD_STATE и FUEL клиентам, у которых уже известен UDP-порт; состояние,
//   которое не помещается в один пакет (kMaxUdpPacketSize), делит на несколько пакетов
//   с одним и тем же sim_time;
// - рассылает TCP-сообщения (LANDED) всем клиентам, прошедшим рукопожатие;
// - ушедшего клиента (BYE, обрыв, нет HELLO за kHelloTimeout) удаляет из таблицы сессии.
//
// Orbiter здесь не нужен: корабли и тела описывает плагин через SessionTable и
// setBodies, а состояние мира он собирает сам и передаёт в sendWorldState. Поэтому
// HostLink проверяется тестами на loopback вместе с ClientLink.
//
// Все функции ничего не ждут: poll вызывается каждый кадр (и по таймеру на паузе) из
// главного потока Orbiter (решение 23). Сообщения для Orbiter.log копятся и забираются
// takeLog - строки на английском.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "header.h"
#include "messages.h"
#include "session_table.h"
#include "socket.h"

namespace olc {

// Сколько секунд хост ждёт HELLO после подключения клиента.
constexpr double kHelloTimeout = 5.0;

// Настройки хоста.
struct HostConfig {
    uint16_t port = 0;            // TCP и UDP; 0 - любой свободный (для тестов)
    uint32_t session = 0;         // номер сессии, пишется в каждый UDP-пакет
    uint32_t orbiterVersion = 0;  // oapiGetOrbiterVersion; другая версия у клиента - REJECT
    std::string system;           // имя планетной системы, уходит в WELCOME
    uint8_t stateRateHz = 30;     // частота WORLD_STATE, уходит в WELCOME
};

// Статистика для лога.
struct HostStats {
    uint64_t udpPacketsSent = 0;
    uint64_t udpBadPackets = 0;   // чужие и битые входящие UDP-пакеты
};

class HostLink {
public:
    // table - таблица сессии плагина: игроки, корабли и владельцы. HostLink добавляет и
    // удаляет в ней игроков, корабли заводит плагин. Таблица должна жить дольше HostLink.
    explicit HostLink(SessionTable& table);
    ~HostLink();
    HostLink(const HostLink&) = delete;
    HostLink& operator=(const HostLink&) = delete;

    // Открывает TCP-слушатель и UDP-сокет на config.port. false - порт занят или ошибка
    // (причина - в takeLog). Сеть уже должна быть запущена (startNetwork).
    bool start(const HostConfig& config);

    // Шлёт всем клиентам BYE с причиной и закрывает сокеты. Повторный вызов ничего не делает.
    void stop(const std::string& reason);

    // Порт после start (с port 0 его выбирает система); 0, если хост не запущен.
    uint16_t port() const;

    // Небесные тела для BODIES. Задаётся до подключения клиентов.
    void setBodies(const std::vector<BodyEntry>& bodies);

    // Принимает подключения и сообщения, ведёт рукопожатие, отвечает на PING, досылает
    // TCP-очереди. now - реальное время в секундах (oapiGetSysTime), нужно для kHelloTimeout.
    void poll(double now);

    // Рассылает WORLD_STATE клиентам с известным UDP-портом. Кораблей может быть сколько
    // угодно: они делятся на пакеты не больше kMaxUdpPacketSize.
    void sendWorldState(const WorldState& state);

    // Рассылает FUEL так же, делением на пакеты.
    void sendFuel(const Fuel& fuel);

    // Рассылает LANDED по TCP всем клиентам, прошедшим рукопожатие.
    void sendLanded(const Landed& landed);

    // Сколько клиентов прошло рукопожатие (получили WELCOME).
    int clientCount() const;

    // Сколько из них прислали UDP_READY и получают WORLD_STATE.
    int readyClientCount() const;

    const HostStats& stats() const { return stats_; }

    // Забирает накопленные строки для лога.
    std::vector<std::string> takeLog();

private:
    // Один подключённый клиент.
    struct Client {
        TcpConnection tcp;
        TcpFrameReader frames;
        double connectedAt = 0.0;  // реальное время подключения
        bool welcomed = false;     // получил WELCOME
        uint8_t clientId = 0;      // номер из таблицы, когда welcomed
        std::string player;
        bool udpReady = false;     // прислал UDP_READY
        Endpoint udp;              // куда слать UDP
        bool udpFromPing = false;  // адрес взят из PING, а не из UDP_READY (см. handleUdp)
        std::string leaveReason;   // почему отключаем; заполняется, когда handleFrames вернул false
    };

    // Разбирает TCP-сообщения клиента. false - клиента надо отключить.
    bool handleFrames(Client& client);
    bool handleHello(Client& client, Reader& reader);
    // Отправляет клиенту BODIES, VESSELS и OWNERSHIP. false - не удалось собрать сообщение.
    bool sendWorldLists(Client& client);
    // Кладёт TCP-сообщение в очередь клиента.
    bool sendMessage(Client& client, TcpType type, const Writer& body);
    // Отправляет REJECT и запоминает причину отключения. Всегда возвращает false:
    // handleHello возвращает его как "клиента надо отключить".
    bool reject(Client& client, RejectReason reason, const std::string& text);
    // Отправляет UDP-пакет всем клиентам с udpReady.
    void sendUdpToReady(uint8_t type, const std::vector<uint8_t>& body);
    void handleUdp();
    // Запоминает адрес, с которого пришёл PING клиента clientId: туда и шлём ему UDP.
    void learnUdpAddress(uint8_t clientId, const Endpoint& from);
    void dropClient(size_t index, const std::string& reason);
    void log(const std::string& line);

    SessionTable& table_;
    HostConfig config_;
    bool running_ = false;
    TcpListener listener_;
    UdpSocket udp_;
    std::vector<Client> clients_;
    std::vector<BodyEntry> bodies_;
    uint32_t seq_ = 0;  // порядковый номер исходящих UDP-пакетов
    HostStats stats_;
    std::vector<std::string> log_;
};

}  // namespace olc
