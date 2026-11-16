// Сетевая часть клиента: подключение к хосту и приём состояния мира.
//
// Что делает (docs/PROTOCOL.md, раздел 3):
// - подключается по TCP и шлёт HELLO;
// - принимает WELCOME (или REJECT), BODIES, VESSELS, OWNERSHIP и заполняет таблицу сессии;
// - после VESSELS отвечает CLASSES_MISSING (какие классы есть, решает плагин через
//   classExists), открывает UDP-сокет и шлёт UDP_READY;
// - принимает WORLD_STATE: проверяет номер сессии и порядок пакетов, раскладывает
//   состояния по буферам интерполяции кораблей (InterpolationBuffer) и подводит часы
//   показа (ClientClock);
// - принимает FUEL и LANDED и отдаёт их плагину (takeFuel, takeLanded);
// - раз в секунду шлёт PING и считает задержку туда-обратно по PONG.
//
// Orbiter здесь не нужен: плагин спрашивает состояние корабля на нужный момент
// (sampleVessel) и сам навязывает его кораблю. Поэтому ClientLink проверяется тестами
// на loopback вместе с HostLink.
//
// Все функции ничего не ждут: poll вызывается каждый кадр (и по таймеру на паузе) из
// главного потока Orbiter (решение 23). Сообщения для Orbiter.log копятся и забираются
// takeLog - строки на английском.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "client_clock.h"
#include "header.h"
#include "interpolation.h"
#include "messages.h"
#include "session_table.h"
#include "socket.h"

namespace olc {

// Сколько секунд клиент ждёт подключения и WELCOME.
constexpr double kConnectTimeout = 5.0;

// Как часто клиент шлёт PING, секунды.
constexpr double kPingInterval = 1.0;

// Настройки клиента.
struct ClientConfig {
    Endpoint host;                // адрес и порт хоста
    std::string player;           // имя игрока для HELLO
    uint32_t orbiterVersion = 0;  // oapiGetOrbiterVersion, уходит в HELLO
    std::string pluginVersion;    // версия плагина, уходит в HELLO
    std::string system;           // своя планетная система; другая у хоста - отключение
    double displayDelay = kDefaultDisplayDelay;
    // Есть ли у клиента класс корабля. Классы, для которых вернёт false, уходят хосту в
    // CLASSES_MISSING. Если не задано - считается, что есть все.
    std::function<bool(const std::string& className)> classExists;
};

// Статистика для лога.
struct ClientStats {
    uint64_t worldStatePackets = 0;  // принятые WORLD_STATE
    uint64_t droppedPackets = 0;     // чужая сессия, старый seq, битые
    uint64_t starvedSamples = 0;     // sampleVessel, когда буфер кончился
};

class ClientLink {
public:
    enum class State {
        Idle,        // start ещё не вызывали
        Connecting,  // идёт TCP-подключение и рукопожатие
        Ready,       // UDP_READY отправлен, идёт обмен состояниями
        Closed,      // отключились; причина - closeReason
    };

    ClientLink() = default;
    ~ClientLink();
    ClientLink(const ClientLink&) = delete;
    ClientLink& operator=(const ClientLink&) = delete;

    // Начинает подключение. false - сразу ошибка (причина - в closeReason и takeLog).
    // Сеть уже должна быть запущена (startNetwork). now - реальное время, секунды.
    bool start(const ClientConfig& config, double now);

    // Шлёт хосту BYE с причиной и закрывает сокеты.
    void stop(const std::string& reason);

    // Принимает и отправляет всё, что нужно. now - реальное время (oapiGetSysTime).
    void poll(double now);

    State state() const { return state_; }
    const std::string& closeReason() const { return closeReason_; }

    // ----- Что прислал хост -----

    const SessionTable& table() const { return table_; }
    const std::vector<BodyEntry>& bodies() const { return bodies_; }
    uint8_t clientId() const { return clientId_; }
    uint8_t stateRateHz() const { return stateRateHz_; }

    // Пришёл ли хоть один WORLD_STATE. До этого время и состояния не известны.
    bool hasWorld() const { return clock_.started(); }

    // Сим-время хоста, которое показывать сейчас (ClientClock).
    double displayTime(double now) { return clock_.displayTime(now); }

    // Ускорение и пауза хоста из последнего WORLD_STATE.
    double hostWarp() const { return hostWarp_; }
    bool hostPaused() const { return hostPaused_; }

    // MJD, соответствующий сим-времени хоста simTime (по последнему WORLD_STATE:
    // mjd = mjd_пакета + (simTime − sim_time_пакета) / 86400).
    double hostMjdAt(double simTime) const;

    // Состояние корабля netId на сим-время хоста simTime (InterpolationBuffer::sample).
    // false - по этому кораблю ещё не было ни одного пакета.
    bool sampleVessel(uint16_t netId, double simTime, VesselState& result, bool& starved);

    // Забирает топливо из FUEL, пришедших с прошлого вызова (по последнему на корабль).
    std::vector<VesselFuel> takeFuel();

    // Забирает LANDED, пришедшие с прошлого вызова, по порядку прихода.
    std::vector<Landed> takeLanded();

    // Последняя задержка туда-обратно, секунды; меньше 0 - ещё не измерена.
    double rtt() const { return rtt_; }

    const ClientStats& stats() const { return stats_; }

    // Забирает накопленные строки для лога.
    std::vector<std::string> takeLog();

private:
    // Разбирает TCP-сообщения хоста. false - соединение надо закрыть (причина в closeReason_).
    bool handleFrames(double now);
    // Разбор отдельных сообщений. При ошибке или отказе вызывают close.
    void handleWelcome(Reader& reader);
    void handleReject(Reader& reader);
    void handleVessels(Reader& reader, double now);
    void handleVesselAdded(Reader& reader);
    void handleVesselRemoved(Reader& reader);
    // Кладёт TCP-сообщение в очередь и отправляет сколько получится.
    void sendMessage(TcpType type, const Writer& body);
    void handleUdp(double now);
    void sendPing(double now);
    void close(const std::string& reason);
    void log(const std::string& line);

    ClientConfig config_;
    State state_ = State::Idle;
    std::string closeReason_;
    double startedAt_ = 0.0;

    TcpConnection tcp_;
    TcpFrameReader frames_;
    UdpSocket udp_;
    bool welcomed_ = false;
    uint32_t session_ = 0;
    uint8_t clientId_ = 0;
    uint8_t stateRateHz_ = 0;

    SessionTable table_;
    std::vector<BodyEntry> bodies_;

    ClientClock clock_;
    std::map<uint16_t, InterpolationBuffer> buffers_;  // по net_id
    bool haveSeq_ = false;
    uint32_t lastSeq_ = 0;  // последний принятый seq WORLD_STATE и FUEL
    double hostWarp_ = 1.0;
    bool hostPaused_ = false;
    double lastPacketSimTime_ = 0.0;
    double lastPacketMjd_ = 0.0;

    std::map<uint16_t, VesselFuel> fuel_;  // по net_id, ещё не забранное
    std::vector<Landed> landed_;

    uint32_t pingSeq_ = 0;
    double nextPing_ = 0.0;
    double rtt_ = -1.0;

    ClientStats stats_;
    std::vector<std::string> log_;
};

}  // namespace olc
