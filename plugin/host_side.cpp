#include "host_side.h"

#include <cmath>
#include <cstring>
#include <random>

#include "protocol.h"

namespace olcplugin {

namespace {

// Частота WORLD_STATE на паузе: состояние не меняется, нужен только флаг паузы.
constexpr double kPausedStateRate = 5.0;

// Как часто повторять LANDED для стоящих кораблей, секунды.
constexpr double kLandedRepeat = 2.0;

// Как часто писать статистику в лог, секунды.
constexpr double kStatsInterval = 10.0;

std::string objectName(OBJHANDLE object) {
    char name[256] = {};
    oapiGetObjectName(object, name, sizeof(name));
    return name;
}

olc::Vec3 toVec3(const VECTOR3& v) {
    olc::Vec3 result;
    result.x = v.x;
    result.y = v.y;
    result.z = v.z;
    return result;
}

// Уровень двигателя 0…1 → байт 0…255.
uint8_t levelToByte(double level) {
    if (level <= 0.0) {
        return 0;
    }
    if (level >= 1.0) {
        return 255;
    }
    return static_cast<uint8_t>(std::lround(level * 255.0));
}

}  // namespace

HostSide::HostSide(const Config& config)
    : config_(config), table_(config.maxPlayers), link_(table_) {}

bool HostSide::start() {
    buildBodies();
    buildShips();

    std::vector<olc::BodyEntry> entries;
    for (size_t i = 0; i < bodies_.size(); i++) {
        olc::BodyEntry entry;
        entry.netId = static_cast<uint16_t>(i);
        entry.name = objectName(bodies_[i]);
        entries.push_back(entry);
    }
    link_.setBodies(entries);

    olc::HostConfig hostConfig;
    hostConfig.port = static_cast<uint16_t>(config_.port);
    // Номер сессии - случайный: пакеты от прошлой сессии хоста клиент отбросит.
    std::random_device random;
    hostConfig.session = random();
    hostConfig.orbiterVersion = static_cast<uint32_t>(oapiGetOrbiterVersion());
    // Имя звезды (тело с номером 0) отличает одну планетную систему от другой.
    hostConfig.system = bodies_.empty() ? std::string() : objectName(bodies_[0]);
    hostConfig.stateRateHz = static_cast<uint8_t>(config_.stateRateHz);

    const bool started = link_.start(hostConfig);
    flushLog();
    if (!started) {
        return false;
    }
    oapiWriteLogV("OLC host: %u vessels, %u bodies, system %s, state rate %d Hz",
                  static_cast<unsigned>(ships_.size()), static_cast<unsigned>(bodies_.size()),
                  hostConfig.system.c_str(), config_.stateRateHz);

    if (!config_.traceVessel.empty()) {
        traceHandle_ = oapiGetVesselByName(&config_.traceVessel[0]);
        if (traceHandle_ && trace_.open(config_.traceFile)) {
            oapiWriteLogV("OLC host: tracing '%s' to %s", config_.traceVessel.c_str(),
                          config_.traceFile.c_str());
        } else {
            oapiWriteLogV("OLC host: cannot trace '%s'", config_.traceVessel.c_str());
            traceHandle_ = nullptr;
        }
    }
    return true;
}

void HostSide::stop() {
    link_.stop("host session ended");
    flushLog();
    trace_.close();
}

void HostSide::buildBodies() {
    bodies_.clear();
    const DWORD count = oapiGetGbodyCount();
    for (DWORD i = 0; i < count; i++) {
        bodies_.push_back(oapiGetGbodyByIndex(static_cast<int>(i)));
    }
}

void HostSide::buildShips() {
    ships_.clear();
    const DWORD count = oapiGetVesselCount();
    for (DWORD i = 0; i < count; i++) {
        OBJHANDLE handle = oapiGetVesselByIndex(static_cast<int>(i));
        VESSEL* vessel = oapiGetVesselInterface(handle);
        const char* className = vessel->GetClassName();
        uint16_t netId = 0;
        // Хост - владелец всех кораблей: на этапе 1 клиенты только смотрят.
        if (!table_.addVessel(vessel->GetName(), className ? className : "", netId)) {
            oapiWriteLogV("OLC host: vessel '%s' skipped (duplicate or too long name)",
                          vessel->GetName());
            continue;
        }
        table_.setOwner(netId, olc::kHostOwner);
        Ship ship;
        ship.netId = netId;
        ship.handle = handle;
        ships_.push_back(ship);
    }
}

bool HostSide::bodyId(OBJHANDLE body, uint16_t& id) const {
    for (size_t i = 0; i < bodies_.size(); i++) {
        if (bodies_[i] == body) {
            id = static_cast<uint16_t>(i);
            return true;
        }
    }
    return false;
}

void HostSide::tick(double now, bool paused) {
    link_.poll(now);

    const double rate = paused ? kPausedStateRate : static_cast<double>(config_.stateRateHz);
    if (now >= nextState_) {
        sendWorld(paused);
        // Отсчёт от now, а не от nextState_: после долгого кадра не шлём пачку пакетов подряд.
        nextState_ = now + 1.0 / rate;
    }
    if (now >= nextFuel_) {
        sendFuel();
        nextFuel_ = now + 1.0;
    }
    if (now >= nextLanded_) {
        repeatLanded();
        nextLanded_ = now + kLandedRepeat;
    }
    if (now >= nextStats_) {
        if (nextStats_ > 0.0) {
            logStats();
        }
        nextStats_ = now + kStatsInterval;
    }
    flushLog();
}

void HostSide::sendWorld(bool paused) {
    olc::WorldState world;
    world.simTime = oapiGetSimTime();
    world.mjd = oapiGetSimMJD();
    world.warp = oapiGetTimeAcceleration();
    world.paused = paused ? 1 : 0;

    for (Ship& ship : ships_) {
        if (!ship.handle) {
            continue;
        }
        VESSEL* vessel = oapiGetVesselInterface(ship.handle);
        VESSELSTATUS2 status;
        std::memset(&status, 0, sizeof(status));
        status.version = 2;
        vessel->GetStatusEx(&status);

        olc::VesselState state;
        state.netId = ship.netId;
        if (!bodyId(status.rbody, state.refBody)) {
            continue;  // тело не из списка - такого быть не должно
        }
        const bool landed = status.status == 1;
        if (landed && !ship.landed) {
            // Корабль только что встал: клиент ставит его один раз (ORBITER-INTERNALS.md,
            // раздел 6).
            olc::Landed message;
            message.netId = ship.netId;
            message.longitude = status.surf_lng;
            message.latitude = status.surf_lat;
            message.heading = status.surf_hdg;
            link_.sendLanded(message);
        }
        ship.landed = landed;

        if (landed) {
            state.status = static_cast<uint8_t>(olc::VesselStatus::Landed);
        } else {
            state.status = static_cast<uint8_t>(olc::VesselStatus::Flying);
            state.rpos = toVec3(status.rpos);
            state.rvel = toVec3(status.rvel);
            state.arot = toVec3(status.arot);
            state.vrot = toVec3(status.vrot);
        }

        DWORD thrusters = vessel->GetThrusterCount();
        if (thrusters > olc::kMaxThrusters) {
            thrusters = static_cast<DWORD>(olc::kMaxThrusters);
        }
        for (DWORD i = 0; i < thrusters; i++) {
            const double level = vessel->GetThrusterLevel(vessel->GetThrusterHandleByIndex(i));
            state.thrusters.push_back(levelToByte(level));
        }
        world.vessels.push_back(state);

        if (ship.handle == traceHandle_ && !landed) {
            trace_.write(world.mjd, objectName(status.rbody).c_str(), status.rpos.x,
                         status.rpos.y, status.rpos.z, false);
        }
    }
    link_.sendWorldState(world);
    worldStatesSent_++;
}

void HostSide::sendFuel() {
    olc::Fuel fuel;
    for (const Ship& ship : ships_) {
        if (!ship.handle) {
            continue;
        }
        VESSEL* vessel = oapiGetVesselInterface(ship.handle);
        olc::VesselFuel entry;
        entry.netId = ship.netId;
        entry.emptyMass = vessel->GetEmptyMass();
        DWORD tanks = vessel->GetPropellantCount();
        if (tanks > olc::kMaxTanks) {
            tanks = static_cast<DWORD>(olc::kMaxTanks);
        }
        for (DWORD i = 0; i < tanks; i++) {
            PROPELLANT_HANDLE tank = vessel->GetPropellantHandleByIndex(i);
            const double maxMass = vessel->GetPropellantMaxMass(tank);
            const double level = maxMass > 0.0 ? vessel->GetPropellantMass(tank) / maxMass : 0.0;
            entry.tankLevels.push_back(static_cast<float>(level));
        }
        fuel.vessels.push_back(entry);
    }
    link_.sendFuel(fuel);
}

void HostSide::repeatLanded() {
    for (const Ship& ship : ships_) {
        if (!ship.handle || !ship.landed) {
            continue;
        }
        VESSEL* vessel = oapiGetVesselInterface(ship.handle);
        VESSELSTATUS2 status;
        std::memset(&status, 0, sizeof(status));
        status.version = 2;
        vessel->GetStatusEx(&status);
        if (status.status != 1) {
            continue;  // уже взлетел - узнаем при следующем снятии состояния
        }
        olc::Landed message;
        message.netId = ship.netId;
        message.longitude = status.surf_lng;
        message.latitude = status.surf_lat;
        message.heading = status.surf_hdg;
        link_.sendLanded(message);
    }
}

void HostSide::onDeleteVessel(OBJHANDLE vessel) {
    for (Ship& ship : ships_) {
        if (ship.handle == vessel) {
            ship.handle = nullptr;
            oapiWriteLogV("OLC host: vessel %u deleted, no longer synced",
                          static_cast<unsigned>(ship.netId));
        }
    }
    if (traceHandle_ == vessel) {
        traceHandle_ = nullptr;
    }
}

void HostSide::onNewVessel(OBJHANDLE vessel) {
    // VESSEL_ADDED - этап 2. Пока только отмечаем, что такой корабль клиенты не увидят.
    oapiWriteLogV("OLC host: vessel '%s' created during the session is not synced yet",
                  objectName(vessel).c_str());
}

void HostSide::logStats() {
    const olc::HostStats& stats = link_.stats();
    oapiWriteLogV("OLC host: %d client(s), %d ready, world states %llu, UDP packets %llu, "
                  "bad UDP %llu",
                  link_.clientCount(), link_.readyClientCount(),
                  static_cast<unsigned long long>(worldStatesSent_),
                  static_cast<unsigned long long>(stats.udpPacketsSent),
                  static_cast<unsigned long long>(stats.udpBadPackets));
}

void HostSide::flushLog() {
    for (const std::string& line : link_.takeLog()) {
        oapiWriteLogV("%s", line.c_str());
    }
}

}  // namespace olcplugin
