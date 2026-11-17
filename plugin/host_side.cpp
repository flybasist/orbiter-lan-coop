#include "host_side.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>

#include "protocol.h"

namespace olcplugin {

namespace {

// WORLD_STATE rate while paused: the state does not change, only the pause flag is needed.
constexpr double kPausedStateRate = 5.0;

// How often to repeat LANDED for landed vessels, seconds.
constexpr double kLandedRepeat = 2.0;

// How often to write statistics to the log, seconds.
constexpr double kStatsInterval = 10.0;

// Where the host saves its world for clients that ask for it: oapiSaveScenario takes the name
// without the Scenarios folder and the .scn extension.
const char kSentScenario[] = "OLC\\Sent to clients";
const char kSentScenarioPath[] = "Scenarios\\OLC\\Sent to clients.scn";

// Saves the current world and reads the file back for SCENARIO (HostConfig::makeScenario).
bool saveWorld(olc::Scenario& scenario) {
    CreateDirectoryA("Scenarios\\OLC", nullptr);  // an existing folder is not an error here
    if (!oapiSaveScenario(kSentScenario, "World of the OLC host, sent to a connecting client.")) {
        oapiWriteLogV("OLC host: cannot save the scenario for a client");
        return false;
    }
    std::ifstream file(kSentScenarioPath, std::ios::binary);
    if (!file) {
        oapiWriteLogV("OLC host: cannot read %s", kSentScenarioPath);
        return false;
    }
    scenario.name = "OLC host world";
    scenario.data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

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

// Thruster level 0…1 → byte 0…255.
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
    // The session number is random: the client drops packets from a previous host session.
    std::random_device random;
    hostConfig.session = random();
    hostConfig.orbiterVersion = static_cast<uint32_t>(oapiGetOrbiterVersion());
    // The star's name (the body with number 0) tells one planetary system from another.
    hostConfig.system = bodies_.empty() ? std::string() : objectName(bodies_[0]);
    hostConfig.stateRateHz = static_cast<uint8_t>(config_.stateRateHz);
    hostConfig.makeScenario = saveWorld;

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
        // The host owns all vessels: at stage 1 clients only watch.
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

void HostSide::addNewShips() {
    for (OBJHANDLE handle : newVessels_) {
        VESSEL* vessel = oapiGetVesselInterface(handle);
        const char* className = vessel->GetClassName();
        uint16_t netId = 0;
        if (!table_.addVessel(vessel->GetName(), className ? className : "", netId)) {
            oapiWriteLogV("OLC host: vessel '%s' skipped (duplicate or too long name)",
                          vessel->GetName());
            continue;
        }
        table_.setOwner(netId, olc::kHostOwner);
        Ship ship;
        ship.netId = netId;
        ship.handle = handle;
        ship.announce = true;
        ships_.push_back(ship);
    }
    newVessels_.clear();
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
        // Count from now, not from nextState_: after a long frame do not send a burst of packets.
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

    addNewShips();
    for (Ship& ship : ships_) {
        VESSEL* vessel = oapiGetVesselInterface(ship.handle);
        VESSELSTATUS2 status;
        std::memset(&status, 0, sizeof(status));
        status.version = 2;
        vessel->GetStatusEx(&status);

        olc::VesselState state;
        state.netId = ship.netId;
        if (!bodyId(status.rbody, state.refBody)) {
            continue;  // body not in the list - this must not happen
        }
        const bool landed = status.status == 1;
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

        // A new vessel goes to clients before anything else about it (LANDED below).
        if (ship.announce) {
            olc::VesselAdded added;
            added.name = vessel->GetName();
            const char* className = vessel->GetClassName();
            added.className = className ? className : "";
            added.state = state;
            if (!link_.sendVesselAdded(added)) {
                oapiWriteLogV("OLC host: cannot send new vessel '%s' to clients",
                              vessel->GetName());
            }
            ship.announce = false;
        }

        if (landed && !ship.landed) {
            // The vessel just came to rest: the client puts it down once (ORBITER-INTERNALS.md,
            // section 6).
            olc::Landed message;
            message.netId = ship.netId;
            message.longitude = status.surf_lng;
            message.latitude = status.surf_lat;
            message.heading = status.surf_hdg;
            link_.sendLanded(message);
        }
        ship.landed = landed;
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
        if (!ship.landed) {
            continue;
        }
        VESSEL* vessel = oapiGetVesselInterface(ship.handle);
        VESSELSTATUS2 status;
        std::memset(&status, 0, sizeof(status));
        status.version = 2;
        vessel->GetStatusEx(&status);
        if (status.status != 1) {
            continue;  // already took off - we learn it at the next capture
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
    // A vessel deleted before it got a number: clients never heard of it.
    newVessels_.erase(std::remove(newVessels_.begin(), newVessels_.end(), vessel),
                      newVessels_.end());
    for (size_t i = 0; i < ships_.size(); i++) {
        if (ships_[i].handle == vessel) {
            const uint16_t netId = ships_[i].netId;
            table_.removeVessel(netId);
            // VESSEL_REMOVED goes out only if VESSEL_ADDED did (or the vessel was in VESSELS).
            if (!ships_[i].announce) {
                link_.sendVesselRemoved(netId);
            }
            ships_.erase(ships_.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    }
    if (traceHandle_ == vessel) {
        traceHandle_ = nullptr;
    }
}

void HostSide::onNewVessel(OBJHANDLE vessel) {
    // The vessel's setup is not finished yet (ORBITER-INTERNALS.md, "Plugin life"), so it gets
    // its number and goes to clients at the next state capture.
    newVessels_.push_back(vessel);
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
