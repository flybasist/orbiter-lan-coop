#include "host_side.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>

#include "protocol.h"
#include "vessel_control.h"
#include "wall_clock.h"

namespace olcplugin {

namespace {

// WORLD_STATE rate while paused: the state does not change, only the pause flag is needed.
constexpr double kPausedStateRate = 5.0;

// How often to repeat LANDED for landed vessels, seconds.
constexpr double kLandedRepeat = 2.0;

// How often to write statistics to the log, seconds.
constexpr double kStatsInterval = 10.0;

// A latency probe is dropped after this many seconds without thrust (no fuel, engine off).
constexpr double kProbeGiveUp = 2.0;

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

// Whether any thruster of the vessel would show thrust in WORLD_STATE.
bool hasThrust(VESSEL* vessel) {
    const DWORD thrusters = vessel->GetThrusterCount();
    for (DWORD i = 0; i < thrusters; i++) {
        if (levelToByte(vessel->GetThrusterLevel(vessel->GetThrusterHandleByIndex(i))) > 0) {
            return true;
        }
    }
    return false;
}

// What a pilot's keys did to the vessel, for checks on the rigs (task 3.11). Written when the set of
// held keys changes, right after the INPUT is applied: on a press the levels show what the key set,
// on a release the rotation shows what the RCS did.
void logControls(VESSEL* vessel, uint32_t keys) {
    double rcs = 0.0;
    for (int group = THGROUP_ATT_PITCHUP; group <= THGROUP_ATT_BACK; group++) {
        rcs = (std::max)(rcs, vessel->GetThrusterGroupLevel(static_cast<THGROUP_TYPE>(group)));
    }
    VECTOR3 rotation;
    vessel->GetAngularVel(rotation);
    oapiWriteLogV("OLC host: '%s' keys 0x%06X: RCS mode %d level %.2f, main %.2f, retro %.2f, "
                  "hover %.2f, trim %.2f, wheel brakes %.2f %.2f, rotation %.2f %.2f %.2f deg/s",
                  vessel->GetName(), static_cast<unsigned>(keys), vessel->GetAttitudeMode(), rcs,
                  vessel->GetThrusterGroupLevel(THGROUP_MAIN),
                  vessel->GetThrusterGroupLevel(THGROUP_RETRO),
                  vessel->GetThrusterGroupLevel(THGROUP_HOVER),
                  vessel->GetControlSurfaceLevel(AIRCTRL_ELEVATORTRIM),
                  vessel->GetWheelbrakeLevel(1), vessel->GetWheelbrakeLevel(2),
                  rotation.x * DEG, rotation.y * DEG, rotation.z * DEG);
}

}  // namespace

HostSide::HostSide(const Config& config, bool headless)
    : config_(config), headless_(headless), table_(config.maxPlayers), link_(table_) {}

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
        // All vessels start free; the host-player takes its focus vessel after the list is built.
        if (!table_.addVessel(vessel->GetName(), className ? className : "", netId)) {
            oapiWriteLogV("OLC host: vessel '%s' skipped (duplicate or too long name)",
                          vessel->GetName());
            continue;
        }
        Ship ship;
        ship.netId = netId;
        ship.handle = handle;
        ships_.push_back(ship);
    }
    // A host with graphics is a pilot too: it flies the vessel in its focus (decision 35).
    // A server without graphics flies nothing, all vessels are for the pilots.
    if (!headless_) {
        takeHostVessel(oapiGetFocusObject());
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
    // Frame length by the wall clock: oapiGetSysTime does not see frames longer than 0.1 s.
    const double frameAt = wallClock();
    if (lastFrameAt_ >= 0.0) {
        const double frame = frameAt - lastFrameAt_;
        longestFrame_ = (std::max)(longestFrame_, frame);
        for (Ship& ship : ships_) {
            if (ship.probeAt >= 0.0) {
                ship.probeLongestFrame = (std::max)(ship.probeLongestFrame, frame);
            }
        }
    }
    lastFrameAt_ = frameAt;
    link_.poll(now);
    if (!paused) {
        applyInputs(now);
        applyKeys();
        detectDocking();
    }

    const double rate = paused ? kPausedStateRate : static_cast<double>(config_.stateRateHz);
    if (now >= nextState_) {
        sendWorld(paused);
        // From the planned time: a host with graphics at about 60 frames per second keeps 60 states
        // per second; after a long frame - from now, without a burst (nextSendTime).
        nextState_ = olc::nextSendTime(nextState_, now, 1.0 / rate);
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
        if (ship.probeAt >= 0.0) {
            const double probeTime = wallClock() - ship.probeAt;
            if (hasThrust(vessel)) {
                oapiWriteLogV("OLC host: input to state %.0f ms (full main engine key of '%s' "
                              "applied to the first state with thrust sent), longest frame %.0f ms",
                              probeTime * 1000.0, vessel->GetName(),
                              ship.probeLongestFrame * 1000.0);
                ship.probeAt = -1.0;
            } else if (probeTime > kProbeGiveUp) {
                ship.probeAt = -1.0;
            }
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

void HostSide::applyInputs(double now) {
    const double simdt = oapiGetSimStep();
    for (Ship& ship : ships_) {
        olc::Input input;
        double receivedAt = 0.0;
        // latestInput checks that the vessel belongs to the client that sent the input.
        if (!link_.latestInput(ship.netId, input, receivedAt)) {
            continue;
        }
        // No INPUT for a while (the connection is lost): the controls are released, no key stays
        // held. Throttle and hover keep their levels, as after releasing the keys in Orbiter.
        if (now - receivedAt > olc::kInputTimeout) {
            continue;
        }
        VESSEL* vessel = oapiGetVesselInterface(ship.handle);
        // A new press of the full main engine key on a quiet vessel starts a latency probe, as on
        // the client (see sendWorld).
        const uint32_t fullMain = olc::kInputFullMain;
        if ((input.keys & fullMain) != 0 && (ship.keys & fullMain) == 0 && !hasThrust(vessel)) {
            ship.probeAt = wallClock();
            ship.probeLongestFrame = 0.0;
        }
        applyInput(vessel, input, simdt);
        if (input.keys != ship.keys) {
            logControls(vessel, input.keys);
            ship.keys = input.keys;
        }
        inputFrames_++;
        if (input.keys != 0) {
            inputKeyFrames_++;
        }
    }
}

void HostSide::detectDocking() {
    std::vector<olc::Dock> current;
    for (const Ship& a : ships_) {
        VESSEL* vesselA = oapiGetVesselInterface(a.handle);
        for (UINT portA = 0; portA < vesselA->DockCount(); portA++) {
            OBJHANDLE mate = vesselA->GetDockStatus(vesselA->GetDockHandle(portA));
            if (!mate) {
                continue;
            }
            for (const Ship& b : ships_) {
                if (b.handle != mate || b.netId < a.netId) {
                    continue;  // each link is found once, from the side with the smaller net_id
                }
                // The SDK tells which vessel is docked to a port, but not to which of its ports.
                // With two links between one pair, ports are matched in order, each port of b once:
                // the pair may come out crossed, but there are exactly two links, not four.
                VESSEL* vesselB = oapiGetVesselInterface(b.handle);
                for (UINT portB = 0; portB < vesselB->DockCount(); portB++) {
                    const bool portTaken = std::any_of(
                        current.begin(), current.end(), [&](const olc::Dock& found) {
                            return found.b == b.netId && found.portB == portB;
                        });
                    if (!portTaken &&
                        vesselB->GetDockStatus(vesselB->GetDockHandle(portB)) == a.handle) {
                        olc::Dock link;
                        link.a = a.netId;
                        link.portA = static_cast<uint8_t>(portA);
                        link.b = b.netId;
                        link.portB = static_cast<uint8_t>(portB);
                        link.docked = 1;
                        current.push_back(link);
                        break;  // this port of a has its mate
                    }
                }
            }
        }
    }

    auto sameLink = [](const olc::Dock& x, const olc::Dock& y) {
        return x.a == y.a && x.portA == y.portA && x.b == y.b && x.portB == y.portB;
    };
    auto contains = [&sameLink](const std::vector<olc::Dock>& list, const olc::Dock& link) {
        return std::any_of(list.begin(), list.end(),
                           [&](const olc::Dock& other) { return sameLink(other, link); });
    };
    for (const olc::Dock& link : docked_) {
        if (!contains(current, link)) {
            olc::Dock undocked = link;
            undocked.docked = 0;
            link_.sendDock(undocked);
            oapiWriteLogV("OLC host: vessels %d and %d undocked", link.a, link.b);
        }
    }
    for (const olc::Dock& link : current) {
        if (!contains(docked_, link)) {
            link_.sendDock(link);
            oapiWriteLogV("OLC host: vessels %d (port %d) and %d (port %d) docked", link.a,
                          link.portA, link.b, link.portB);
        }
    }
    docked_ = current;
}

void HostSide::applyKeys() {
    // HostLink has already checked that the pilot owns the vessel. A key press that came during
    // pause waits for the next step: the vessel's key handlers expect a running simulation.
    for (const olc::KeyPress& press : link_.takeKeys()) {
        for (const Ship& ship : ships_) {
            if (ship.netId == press.netId) {
                applyKey(oapiGetVesselInterface(ship.handle), press);
                keysApplied_++;
                break;
            }
        }
    }
}

uint16_t HostSide::hostVessel() const {
    for (const Ship& ship : ships_) {
        const olc::VesselRecord* record = table_.findById(ship.netId);
        if (record && record->owner == olc::kHostOwner) {
            return ship.netId;
        }
    }
    return kNoVessel;
}

void HostSide::takeHostVessel(OBJHANDLE vessel) {
    if (headless_ || hostVessel() != kNoVessel) {
        return;  // the host keeps its vessel until it releases it (Ctrl+Alt+U)
    }
    for (const Ship& ship : ships_) {
        if (ship.handle != vessel) {
            continue;
        }
        const olc::VesselRecord* record = table_.findById(ship.netId);
        // The same rule as for clients: only a free vessel that is not reserved for a pilot.
        if (record && record->owner == olc::kNoOwner && record->holdPlayer.empty()) {
            table_.setOwner(ship.netId, olc::kHostOwner);
            link_.broadcastOwnership();
            oapiWriteLogV("OLC host: the host flies '%s'", record->name.c_str());
        } else if (record) {
            oapiWriteLogV("OLC host: '%s' is flown by a pilot, the host only watches it",
                          record->name.c_str());
        }
        return;
    }
}

void HostSide::onFocusChanged(OBJHANDLE vessel) {
    takeHostVessel(vessel);
}

void HostSide::releaseHostVessel() {
    const uint16_t netId = hostVessel();
    if (netId == kNoVessel) {
        return;
    }
    table_.setOwner(netId, olc::kNoOwner);
    link_.broadcastOwnership();
    oapiWriteLogV("OLC host: the host released '%s'", table_.findById(netId)->name.c_str());
}

void HostSide::logStats() {
    const olc::HostStats& stats = link_.stats();
    oapiWriteLogV("OLC host: %d client(s), %d ready, world states %llu, UDP packets %llu, "
                  "bad UDP %llu, input accepted %llu rejected %llu, applied %llu (with keys %llu), "
                  "key presses accepted %llu rejected %llu applied %llu, longest frame %.0f ms",
                  link_.clientCount(), link_.readyClientCount(),
                  static_cast<unsigned long long>(worldStatesSent_),
                  static_cast<unsigned long long>(stats.udpPacketsSent),
                  static_cast<unsigned long long>(stats.udpBadPackets),
                  static_cast<unsigned long long>(stats.inputAccepted),
                  static_cast<unsigned long long>(stats.inputRejected),
                  static_cast<unsigned long long>(inputFrames_),
                  static_cast<unsigned long long>(inputKeyFrames_),
                  static_cast<unsigned long long>(stats.keysAccepted),
                  static_cast<unsigned long long>(stats.keysRejected),
                  static_cast<unsigned long long>(keysApplied_), longestFrame_ * 1000.0);
    longestFrame_ = 0.0;
}

void HostSide::flushLog() {
    for (const std::string& line : link_.takeLog()) {
        oapiWriteLogV("%s", line.c_str());
    }
}

}  // namespace olcplugin
