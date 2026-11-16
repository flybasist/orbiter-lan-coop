// Роль «клиент»: показывает мир хоста.
//
// Каждый кадр (clbkPreStep) и по таймеру на паузе вызывается tick:
// - принять сообщения хоста (ClientLink::poll);
// - пауза и ускорение - как у хоста;
// - сим-время клиента держится рядом со временем показа: при расхождении больше
//   TimeSyncThreshold - прыжок oapiSetSimMJD (не чаще раза в kTimeJumpPause секунд);
// - только вне паузы: топливо из FUEL и состояния кораблей на момент показа - через
//   DefSetStateEx в clbkPreStep (ORBITER-INTERNALS.md, раздел 3). Корабль, который на момент
//   показа стоит на поверхности, ставится туда один раз по месту из LANDED.
// Корабли клиента сопоставляются с кораблями хоста по имени: на этапе 1 оба запускают один
// и тот же сценарий (docs/ARCHITECTURE.md, раздел 2).
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Orbitersdk.h"

#include "client_link.h"
#include "config.h"
#include "trace.h"

namespace olcplugin {

class ClientSide {
public:
    explicit ClientSide(const Config& config);

    // Начало сессии: подключение к хосту. false - сразу не удалось (в логе).
    bool start();

    // Конец сессии: BYE хосту. Корабли уже удалены - не трогаем.
    void stop();

    // now - oapiGetSysTime; paused - Orbiter на паузе (вызов из таймера).
    void tick(double now, bool paused);

    void onDeleteVessel(OBJHANDLE vessel);

private:
    void followTime(double now, double displayTime);
    void applyFuel();
    void collectLanded();
    void putOnSurface(VESSEL* vessel, OBJHANDLE body, const olc::Landed& place);
    void applyStates(double displayTime);
    // Корабль клиента для net_id хоста: ищется по имени и запоминается. nullptr - нет такого.
    OBJHANDLE vesselHandle(uint16_t netId);
    OBJHANDLE bodyHandle(uint16_t netId);
    void logStats();
    void flushLog();

    Config config_;
    olc::ClientLink link_;
    std::map<uint16_t, OBJHANDLE> vessels_;  // net_id → корабль клиента (nullptr - нет)
    std::map<uint16_t, OBJHANDLE> bodies_;   // net_id тела → тело клиента
    std::map<uint16_t, olc::Landed> landedAt_;  // net_id → последнее место посадки из LANDED

    Trace trace_;
    uint16_t traceNetId_ = 0;
    bool traceResolved_ = false;

    double requestedWarp_ = 0.0;   // какое ускорение плагин сам задал последним
    double nextWarpCheck_ = 0.0;
    double lastPauseRequest_ = -1.0;
    double lastTimeJump_ = -1.0e9;
    int timeJumps_ = 0;

    // Статистика расхождения сим-времени с хостом за интервал логов, секунды.
    double timeErrorMax_ = 0.0;
    double timeErrorSum_ = 0.0;
    int timeErrorCount_ = 0;
    double nextStats_ = 0.0;
};

}  // namespace olcplugin
