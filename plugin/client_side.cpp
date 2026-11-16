#include "client_side.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "protocol.h"

namespace olcplugin {

// std::max в скобках: windows.h (через Orbitersdk.h) определяет макрос max.

namespace {

// Не прыгать во времени чаще, чем раз в столько секунд: прыжок тяжёлый
// (ORBITER-INTERNALS.md, раздел 4), а сразу после него расхождение ещё не устоялось.
constexpr double kTimeJumpPause = 2.0;

// Как часто проверять, не изменил ли игрок ускорение сам, секунды.
constexpr double kWarpCheckInterval = 1.0;

// Не повторять запрос паузы чаще: oapiSetPause срабатывает только в следующем кадре.
constexpr double kPauseRequestPause = 0.5;

constexpr double kStatsInterval = 10.0;

constexpr double kSecondsPerDay = 86400.0;

VECTOR3 toVector3(const olc::Vec3& v) {
    return _V(v.x, v.y, v.z);
}

std::string objectName(OBJHANDLE object) {
    char name[256] = {};
    oapiGetObjectName(object, name, sizeof(name));
    return name;
}

}  // namespace

ClientSide::ClientSide(const Config& config) : config_(config) {}

bool ClientSide::start() {
    olc::ClientConfig clientConfig;
    if (!olc::parseIpv4(config_.hostAddress, clientConfig.host.ip)) {
        oapiWriteLogV("OLC client: HostAddress '%s' is not an IPv4 address",
                      config_.hostAddress.c_str());
        return false;
    }
    clientConfig.host.port = static_cast<uint16_t>(config_.port);
    clientConfig.player = config_.player;
    clientConfig.orbiterVersion = static_cast<uint32_t>(oapiGetOrbiterVersion());
    clientConfig.pluginVersion = "0.1-stage1";
    // Как у хоста: имя звезды - тела с номером 0.
    clientConfig.system = oapiGetGbodyCount() > 0 ? objectName(oapiGetGbodyByIndex(0)) : "";
    clientConfig.displayDelay = config_.displayDelay;
    // Класс есть, если на клиенте есть его файл конфигурации.
    clientConfig.classExists = [](const std::string& className) {
        std::string path = "Vessels\\" + className + ".cfg";
        FILEHANDLE file = oapiOpenFile(path.c_str(), FILE_IN_ZEROONFAIL, CONFIG);
        if (!file) {
            return false;
        }
        oapiCloseFile(file, FILE_IN_ZEROONFAIL);
        return true;
    };

    const bool started = link_.start(clientConfig, oapiGetSysTime());
    flushLog();
    return started;
}

void ClientSide::stop() {
    link_.stop("client session ended");
    flushLog();
    trace_.close();
}

void ClientSide::tick(double now, bool paused) {
    link_.poll(now);

    // Подключение и отключение пишет в лог сам ClientLink.
    if (link_.state() == olc::ClientLink::State::Ready && link_.hasWorld()) {
        const double displayTime = link_.displayTime(now);
        followTime(now, displayTime);
        if (!paused) {
            applyFuel();
            collectLanded();
            applyStates(displayTime);
        }
        if (now >= nextStats_) {
            if (nextStats_ > 0.0) {
                logStats();
            }
            nextStats_ = now + kStatsInterval;
        }
    }
    flushLog();
}

void ClientSide::followTime(double now, double displayTime) {
    // Пауза - как у хоста.
    const bool hostPaused = link_.hostPaused();
    if (oapiGetPause() != hostPaused && now - lastPauseRequest_ > kPauseRequestPause) {
        oapiSetPause(hostPaused);
        lastPauseRequest_ = now;
        oapiWriteLogV("OLC client: following host pause %s", hostPaused ? "on" : "off");
    }

    // Ускорение - как у хоста. Повторяем запрос, только если хост сменил ускорение или
    // игрок поменял его сам (проверка раз в kWarpCheckInterval).
    const double hostWarp = link_.hostWarp();
    if (hostWarp != requestedWarp_ ||
        (now >= nextWarpCheck_ && std::fabs(oapiGetTimeAcceleration() - hostWarp) > 1e-6)) {
        oapiSetTimeAcceleration(hostWarp);
        if (hostWarp != requestedWarp_) {
            oapiWriteLogV("OLC client: following host time acceleration x%g", hostWarp);
        }
        requestedWarp_ = hostWarp;
        nextWarpCheck_ = now + kWarpCheckInterval;
    }

    // Сим-время: весь мир клиента отстаёт от хоста на задержку показа (PROTOCOL.md,
    // раздел 8), поэтому цель - MJD момента показа.
    const double targetMjd = link_.hostMjdAt(displayTime);
    const double error = (oapiGetSimMJD() - targetMjd) * kSecondsPerDay;
    const double limit = config_.timeSyncThreshold * (std::max)(1.0, hostWarp);
    if (std::fabs(error) > limit && now - lastTimeJump_ > kTimeJumpPause) {
        oapiSetSimMJD(targetMjd);
        lastTimeJump_ = now;
        timeJumps_++;
        oapiWriteLogV("OLC client: sim time jump, error was %.3f s", error);
    } else {
        timeErrorMax_ = (std::max)(timeErrorMax_, std::fabs(error));
        timeErrorSum_ += std::fabs(error);
        timeErrorCount_++;
    }
}

void ClientSide::applyFuel() {
    // Топливо - раньше уровней двигателей: двигатель с пустым баком ядро обнуляет
    // (ORBITER-INTERNALS.md, раздел 17).
    for (const olc::VesselFuel& fuel : link_.takeFuel()) {
        OBJHANDLE handle = vesselHandle(fuel.netId);
        if (!handle) {
            continue;
        }
        VESSEL* vessel = oapiGetVesselInterface(handle);
        vessel->SetEmptyMass(fuel.emptyMass);
        const DWORD tanks = vessel->GetPropellantCount();
        for (size_t i = 0; i < fuel.tankLevels.size() && i < tanks; i++) {
            PROPELLANT_HANDLE tank = vessel->GetPropellantHandleByIndex(static_cast<DWORD>(i));
            const double maxMass = vessel->GetPropellantMaxMass(tank);
            vessel->SetPropellantMass(tank, maxMass * static_cast<double>(fuel.tankLevels[i]));
        }
    }
}

void ClientSide::collectLanded() {
    // Место посадки запоминается, а применяется в applyStates, когда момент показа дойдёт до
    // посадки: LANDED приходит сразу, а клиент показывает мир с задержкой. Если поставить
    // корабль раньше, следующий же кадр снова навяжет ему полёт из буфера.
    for (const olc::Landed& message : link_.takeLanded()) {
        landedAt_[message.netId] = message;
    }
}

void ClientSide::putOnSurface(VESSEL* vessel, OBJHANDLE body, const olc::Landed& place) {
    VESSELSTATUS2 status;
    std::memset(&status, 0, sizeof(status));
    status.version = 2;
    status.rbody = body;
    status.status = 1;
    status.surf_lng = place.longitude;
    status.surf_lat = place.latitude;
    status.surf_hdg = place.heading;
    // arot.x > 4: «расширенной» ориентации нет, ядро поставит корабль на опоры само
    // (Vesselstatus.cpp, SetState2, ветка status 1).
    status.arot.x = 10.0;
    vessel->DefSetStateEx(&status);
    oapiWriteLogV("OLC client: vessel '%s' landed", vessel->GetName());
}

void ClientSide::applyStates(double displayTime) {
    // Трасса: корабль ищется один раз, когда таблица уже пришла.
    if (!traceResolved_ && !config_.traceVessel.empty()) {
        traceResolved_ = true;
        const olc::VesselRecord* record = link_.table().findByName(config_.traceVessel);
        if (record && trace_.open(config_.traceFile)) {
            traceNetId_ = record->netId;
            oapiWriteLogV("OLC client: tracing '%s' to %s", config_.traceVessel.c_str(),
                          config_.traceFile.c_str());
        } else {
            oapiWriteLogV("OLC client: cannot trace '%s'", config_.traceVessel.c_str());
        }
    }

    for (const olc::VesselEntry& entry : link_.table().vesselEntries()) {
        olc::VesselState state;
        bool starved = false;
        if (!link_.sampleVessel(entry.netId, displayTime, state, starved)) {
            continue;  // по этому кораблю ещё не было пакетов
        }
        OBJHANDLE handle = vesselHandle(entry.netId);
        OBJHANDLE body = bodyHandle(state.refBody);
        if (!handle || !body) {
            continue;
        }
        VESSEL* vessel = oapiGetVesselInterface(handle);

        if (state.status == static_cast<uint8_t>(olc::VesselStatus::Flying)) {
            VESSELSTATUS2 status;
            std::memset(&status, 0, sizeof(status));
            status.version = 2;
            status.rbody = body;
            status.status = 0;
            status.rpos = toVector3(state.rpos);
            status.rvel = toVector3(state.rvel);
            status.arot = toVector3(state.arot);
            status.vrot = toVector3(state.vrot);
            vessel->DefSetStateEx(&status);

            if (traceResolved_ && trace_.isOpen() && entry.netId == traceNetId_) {
                trace_.write(link_.hostMjdAt(displayTime), objectName(body).c_str(),
                             state.rpos.x, state.rpos.y, state.rpos.z, starved);
            }
        } else if (!(vessel->GetFlightStatus() & 1)) {
            // Стоит на поверхности у хоста, а у клиента ещё нет: ставим один раз
            // (ORBITER-INTERNALS.md, раздел 6). Дальше стоящий корабль ядро держит само.
            auto place = landedAt_.find(entry.netId);
            if (place != landedAt_.end()) {
                putOnSurface(vessel, body, place->second);
            }
        }

        // Уровни двигателей: для выхлопа, а заодно физика клиента между пакетами
        // двигает корабль так же, как хост.
        const DWORD thrusters = vessel->GetThrusterCount();
        for (size_t i = 0; i < state.thrusters.size() && i < thrusters; i++) {
            THRUSTER_HANDLE thruster = vessel->GetThrusterHandleByIndex(static_cast<DWORD>(i));
            vessel->SetThrusterLevel(thruster, state.thrusters[i] / 255.0);
        }
    }
}

OBJHANDLE ClientSide::vesselHandle(uint16_t netId) {
    auto found = vessels_.find(netId);
    if (found != vessels_.end()) {
        return found->second;
    }
    OBJHANDLE handle = nullptr;
    const olc::VesselRecord* record = link_.table().findById(netId);
    if (record) {
        std::string name = record->name;
        handle = oapiGetVesselByName(&name[0]);
        if (!handle) {
            oapiWriteLogV("OLC client: host vessel '%s' not found here, skipped", name.c_str());
        }
    }
    vessels_[netId] = handle;
    return handle;
}

OBJHANDLE ClientSide::bodyHandle(uint16_t netId) {
    auto found = bodies_.find(netId);
    if (found != bodies_.end()) {
        return found->second;
    }
    OBJHANDLE handle = nullptr;
    for (const olc::BodyEntry& body : link_.bodies()) {
        if (body.netId == netId) {
            std::string name = body.name;
            handle = oapiGetGbodyByName(&name[0]);
            if (!handle) {
                oapiWriteLogV("OLC client: host body '%s' not found here", name.c_str());
            }
            break;
        }
    }
    bodies_[netId] = handle;
    return handle;
}

void ClientSide::onDeleteVessel(OBJHANDLE vessel) {
    for (auto& entry : vessels_) {
        if (entry.second == vessel) {
            entry.second = nullptr;
        }
    }
}

void ClientSide::logStats() {
    const olc::ClientStats& stats = link_.stats();
    const double average = timeErrorCount_ > 0 ? timeErrorSum_ / static_cast<double>(timeErrorCount_) : 0.0;
    oapiWriteLogV("OLC client: rtt %.1f ms, world states %llu, dropped %llu, starved %llu, "
                  "sim time error avg %.3f s max %.3f s, time jumps %d",
                  link_.rtt() * 1000.0,
                  static_cast<unsigned long long>(stats.worldStatePackets),
                  static_cast<unsigned long long>(stats.droppedPackets),
                  static_cast<unsigned long long>(stats.starvedSamples), average,
                  timeErrorMax_, timeJumps_);
    timeErrorMax_ = 0.0;
    timeErrorSum_ = 0.0;
    timeErrorCount_ = 0;
}

void ClientSide::flushLog() {
    for (const std::string& line : link_.takeLog()) {
        oapiWriteLogV("%s", line.c_str());
    }
}

}  // namespace olcplugin
