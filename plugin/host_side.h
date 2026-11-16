// Роль «хост»: снимает состояние кораблей и рассылает его клиентам.
//
// Каждый кадр (clbkPreStep) и по таймеру на паузе вызывается tick:
// - принять подключения и сообщения клиентов (HostLink::poll);
// - с частотой StateRate снять состояния всех кораблей (GetStatusEx) и разослать
//   WORLD_STATE; на паузе - не чаще kPausedStateRate раз в секунду;
// - раз в секунду разослать FUEL;
// - корабль встал на поверхность - сразу LANDED; стоящие корабли повторяются в LANDED
//   раз в kLandedRepeat секунд, чтобы о них узнали и клиенты, подключившиеся позже.
// Корабли и тела берутся один раз при старте сессии (этап 1). Корабли, появившиеся во
// время сессии, на этапе 1 не передаются - только запись в лог (VESSEL_ADDED - этап 2).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Orbitersdk.h"

#include "config.h"
#include "host_link.h"
#include "session_table.h"
#include "trace.h"

namespace olcplugin {

class HostSide {
public:
    explicit HostSide(const Config& config);

    // Начало сессии: таблицы кораблей и тел, запуск сети. false - не запустился (в логе).
    bool start();

    // Конец сессии: BYE клиентам, закрыть сеть. Корабли уже удалены - не трогаем.
    void stop();

    // now - oapiGetSysTime; paused - Orbiter на паузе (вызов из таймера).
    void tick(double now, bool paused);

    // Корабль удалён: больше не трогаем его handle.
    void onDeleteVessel(OBJHANDLE vessel);

    // Корабль создан во время сессии.
    void onNewVessel(OBJHANDLE vessel);

private:
    // Корабль хоста: номер в таблице и handle в Orbiter.
    struct Ship {
        uint16_t netId = 0;
        OBJHANDLE handle = nullptr;  // nullptr - корабль удалён
        bool landed = false;         // стоял на поверхности при последнем снятии
    };

    void buildBodies();
    void buildShips();
    // Номер тела по handle; false - тела нет в списке.
    bool bodyId(OBJHANDLE body, uint16_t& id) const;
    void sendWorld(bool paused);
    void sendFuel();
    void repeatLanded();
    void logStats();
    void flushLog();

    Config config_;
    olc::SessionTable table_;
    olc::HostLink link_;
    std::vector<OBJHANDLE> bodies_;  // индекс - net_id тела
    std::vector<Ship> ships_;
    Trace trace_;
    OBJHANDLE traceHandle_ = nullptr;

    double nextState_ = 0.0;
    double nextFuel_ = 0.0;
    double nextLanded_ = 0.0;
    double nextStats_ = 0.0;
    uint64_t worldStatesSent_ = 0;
};

}  // namespace olcplugin
