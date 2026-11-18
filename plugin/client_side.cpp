#include "client_side.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

#include "protocol.h"
#include "scenario_launch.h"
#include "wall_clock.h"

namespace olcplugin {

// std::max in parentheses: windows.h (via Orbitersdk.h) defines a max macro.

namespace {

// Do not jump in time more often than once per this many seconds: a jump is heavy
// (ORBITER-INTERNALS.md, section 4), and right after it the error has not settled yet.
constexpr double kTimeJumpPause = 2.0;

// How often to check whether the player changed time acceleration, seconds.
constexpr double kWarpCheckInterval = 1.0;

// Do not repeat a pause request more often: oapiSetPause takes effect only in the next frame.
constexpr double kPauseRequestPause = 0.5;

constexpr double kStatsInterval = 10.0;

// A latency measurement is dropped after this many seconds without thrust.
constexpr double kLatencyGiveUp = 2.0;

// How long a DOCK waits for a vessel that is not created yet, seconds.
constexpr double kDockRetrySeconds = 5.0;

constexpr double kSecondsPerDay = 86400.0;

// How long the notice about classes that are not installed stays on screen, seconds.
constexpr double kNoticeSeconds = 20.0;

VECTOR3 toVector3(const olc::Vec3& v) {
    return _V(v.x, v.y, v.z);
}

bool hasThrust(const olc::VesselState& state) {
    return std::any_of(state.thrusters.begin(), state.thrusters.end(),
                       [](uint8_t level) { return level > 0; });
}

// Milliseconds from start to end for the latency log; -1 if the step was not reached.
double stepMs(double start, double end) {
    return end >= 0.0 ? (end - start) * 1000.0 : -1.0;
}

std::string objectName(OBJHANDLE object) {
    char name[256] = {};
    oapiGetObjectName(object, name, sizeof(name));
    return name;
}

}  // namespace

ClientSide::ClientSide(const Config& config, uint32_t scenarioId)
    : config_(config), scenarioId_(scenarioId) {}

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
    clientConfig.pluginVersion = kPluginVersion;
    clientConfig.scenarioId = scenarioId_;
    // As on the host: the name of the star - the body with number 0.
    clientConfig.system = oapiGetGbodyCount() > 0 ? objectName(oapiGetGbodyByIndex(0)) : "";
    clientConfig.displayDelay = config_.displayDelay;
    clientConfig.classExists = classInstalled;

    // Orbiter's key layout: keymap.cfg in the Orbiter folder (Orbiter writes it at start).
    std::ifstream keymapFile("keymap.cfg");
    if (keymapFile) {
        std::stringstream text;
        text << keymapFile.rdbuf();
        oapiWriteLogV("OLC client: %d key bindings read from keymap.cfg", keymap_.parse(text.str()));
    } else {
        oapiWriteLogV("OLC client: keymap.cfg not found, Orbiter's default keys are used");
    }

    const bool started = link_.start(clientConfig, oapiGetSysTime());
    flushLog();
    return started;
}

void ClientSide::stop() {
    // The world is already deleted: the notice went with it, it is only forgotten here.
    notice_ = nullptr;
    link_.stop("client session ended");
    flushLog();
    trace_.close();
}

void ClientSide::tick(double now, bool paused) {
    const double frameAt = wallClock();
    if (keyDownAt_ >= 0.0 && lastFrameAt_ >= 0.0) {
        longestFrame_ = (std::max)(longestFrame_, frameAt - lastFrameAt_);
    }
    lastFrameAt_ = frameAt;
    link_.poll(now);
    // Deletions requested in the previous frame are carried out at its end.
    deletesThisFrame_ = 0;

    // ClientLink logs connecting and disconnecting itself.
    if (link_.state() == olc::ClientLink::State::Ready && link_.hasWorld()) {
        const double displayTime = link_.displayTime(now);
        followTime(now, displayTime);
        if (!paused) {
            applyVesselChanges();
            applyDocks(now);
            applyFuel();
            collectLanded();
            applyStates(displayTime);
            // After applyStates: host vessels are created first, so a client vessel the host
            // does not have is not kept just because it was the last one.
            if (!extrasRemoved_) {
                removeExtraVessels();
                extrasRemoved_ = true;
            }
            // The focus at session start counts as a focus change: the pilot takes this vessel.
            if (!focusChecked_) {
                onFocusChanged(oapiGetFocusObject());
            }
            followOwnership();
            sendInput(now);
            updateNotice(now);
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
    // Pause follows the host.
    const bool hostPaused = link_.hostPaused();
    if (oapiGetPause() != hostPaused && now - lastPauseRequest_ > kPauseRequestPause) {
        oapiSetPause(hostPaused);
        lastPauseRequest_ = now;
        oapiWriteLogV("OLC client: following host pause %s", hostPaused ? "on" : "off");
    }

    // Time acceleration follows the host. Repeat the request only if the host changed it or
    // the player changed it (checked once per kWarpCheckInterval).
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

    // Sim time: the whole client world lags behind the host by the display delay (PROTOCOL.md,
    // section 8), so the target is the MJD of display time.
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
    // Fuel before thruster levels: the core zeroes a thruster with an empty tank
    // (ORBITER-INTERNALS.md, section 17).
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
    // The landing place is remembered and applied in applyStates when display time reaches
    // the landing: LANDED arrives at once, but the client shows the world with a delay. If the
    // vessel is put down earlier, the very next frame imposes flight from the buffer again.
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
    // arot.x > 4: no "extended" attitude, the core puts the vessel on its legs itself
    // (Vesselstatus.cpp, SetState2, the status 1 branch).
    status.arot.x = 10.0;
    vessel->DefSetStateEx(&status);
    oapiWriteLogV("OLC client: vessel '%s' landed", vessel->GetName());
}

void ClientSide::applyDocks(double now) {
    for (const olc::Dock& dock : link_.takeDocks()) {
        pendingDocks_.push_back({dock, now + config_.displayDelay});
    }
    while (!pendingDocks_.empty() && pendingDocks_.front().applyAt <= now) {
        const olc::Dock dock = pendingDocks_.front().dock;
        OBJHANDLE handleA = vesselHandle(dock.a);
        OBJHANDLE handleB = vesselHandle(dock.b);
        if (!handleA || !handleB) {
            // A vessel is not created yet: try again for a few seconds, then give up.
            if (now - pendingDocks_.front().applyAt > kDockRetrySeconds) {
                oapiWriteLogV("OLC client: DOCK for vessels %d and %d dropped: no such vessel", dock.a,
                              dock.b);
                pendingDocks_.erase(pendingDocks_.begin());
                continue;
            }
            break;
        }
        VESSEL* vesselA = oapiGetVesselInterface(handleA);
        auto sameLink = [&dock](const olc::Dock& other) {
            return other.a == dock.a && other.portA == dock.portA && other.b == dock.b &&
                   other.portB == dock.portB;
        };
        dockedLinks_.erase(std::remove_if(dockedLinks_.begin(), dockedLinks_.end(), sameLink),
                           dockedLinks_.end());
        if (dock.docked) {
            // Mode 0: the vessels are not moved, they are already where the host docked them.
            const int result = vesselA->Dock(handleB, dock.portA, dock.portB, 0);
            if (result == 0) {
                dockedLinks_.push_back(dock);
            }
            oapiWriteLogV("OLC client: docking vessels %d and %d: %s (code %d)", dock.a, dock.b,
                          result == 0 ? "done" : "failed", result);
        } else {
            vesselA->Undock(dock.portA);
            oapiWriteLogV("OLC client: undocked vessels %d and %d", dock.a, dock.b);
        }
        pendingDocks_.erase(pendingDocks_.begin());
    }
}

std::map<uint16_t, uint16_t> ClientSide::dockLeaders() const {
    // Each vessel starts as its own leader; every link joins two bundles under the smaller leader.
    std::map<uint16_t, uint16_t> leader;
    for (const olc::Dock& link : dockedLinks_) {
        leader.emplace(link.a, link.a);
        leader.emplace(link.b, link.b);
    }
    for (const olc::Dock& link : dockedLinks_) {
        const uint16_t from = (std::max)(leader[link.a], leader[link.b]);
        const uint16_t to = (std::min)(leader[link.a], leader[link.b]);
        for (auto& entry : leader) {
            if (entry.second == from) {
                entry.second = to;
            }
        }
    }
    return leader;
}

void ClientSide::applyStates(double displayTime) {
    // Trace: the vessel is looked up once, when the table has arrived.
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

    const std::map<uint16_t, uint16_t> leaders = dockLeaders();
    for (const olc::VesselEntry& entry : link_.table().vesselEntries()) {
        auto bundle = leaders.find(entry.netId);
        const bool inBundle = bundle != leaders.end();
        olc::VesselState state;
        bool starved = false;
        if (!link_.sampleVessel(entry.netId, displayTime, state, starved)) {
            continue;  // no packets for this vessel yet
        }
        OBJHANDLE body = bodyHandle(state.refBody);
        if (!body) {
            continue;
        }
        OBJHANDLE handle = vesselHandle(entry.netId);
        if (!handle) {
            handle = createVessel(entry.netId, state, body);
            if (!handle) {
                continue;
            }
        }
        VESSEL* vessel = oapiGetVesselInterface(handle);

        if (inBundle && bundle->second != entry.netId) {
            // Not the leader of its bundle: the leader moves the whole bundle (ORBITER-INTERNALS.md,
            // section 9), a state of this vessel would only fight with it.
        } else if (state.status == static_cast<uint8_t>(olc::VesselStatus::Flying)) {
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
            if (inBundle) {
                // Position and velocity move the whole bundle, the orientation of the bundle is set
                // separately (and would be overwritten at the next step otherwise).
                vessel->SetGlobalOrientation(status.arot);
                vessel->SetAngularVel(status.vrot);
            }

            if (traceResolved_ && trace_.isOpen() && entry.netId == traceNetId_) {
                trace_.write(link_.hostMjdAt(displayTime), objectName(body).c_str(),
                             state.rpos.x, state.rpos.y, state.rpos.z, starved);
            }
        } else if (!(vessel->GetFlightStatus() & 1)) {
            // Landed on the host but not yet on the client: put it down once
            // (ORBITER-INTERNALS.md, section 6). After that the core holds the landed vessel itself.
            auto place = landedAt_.find(entry.netId);
            if (place != landedAt_.end()) {
                putOnSurface(vessel, body, place->second);
            }
        }

        uint16_t own = 0;
        if (ownVessel(own) && own == entry.netId) {
            const bool thrusting = hasThrust(state);
            const double now = wallClock();
            olc::VesselState newest;
            if (keyDownAt_ >= 0.0 && stateAt_ < 0.0 && link_.latestVessel(own, newest) &&
                hasThrust(newest)) {
                stateAt_ = now;  // seen in this frame; it may have arrived up to a frame earlier
            }
            if (keyDownAt_ >= 0.0 && thrusting) {
                oapiWriteLogV("OLC client: control latency %.0f ms (key press to thrust on the "
                              "displayed vessel): INPUT sent at %.0f ms, state with thrust "
                              "received at %.0f ms, longest frame %.0f ms",
                              (now - keyDownAt_) * 1000.0, stepMs(keyDownAt_, inputSentAt_),
                              stepMs(keyDownAt_, stateAt_), longestFrame_ * 1000.0);
                keyDownAt_ = -1.0;
            } else if (keyDownAt_ >= 0.0 && now - keyDownAt_ > kLatencyGiveUp) {
                keyDownAt_ = -1.0;  // the key had no effect (vessel not in focus, no fuel)
            }
            ownThrusting_ = thrusting;
        }

        // Thruster levels: for the exhaust, and also the client physics between packets
        // moves the vessel the same way as the host.
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
    const olc::VesselRecord* record = link_.table().findById(netId);
    if (!record) {
        return nullptr;
    }
    std::string name = record->name;
    OBJHANDLE handle = oapiGetVesselByName(&name[0]);
    if (handle) {
        vessels_[netId] = handle;
    }
    return handle;
}

OBJHANDLE ClientSide::createVessel(uint16_t netId, const olc::VesselState& state, OBJHANDLE body) {
    const olc::VesselRecord* record = link_.table().findById(netId);
    if (!record || cannotCreate_.count(netId) > 0) {
        return nullptr;
    }
    if (!classInstalled(record->className)) {
        // ClientLink has already told the host (CLASSES_MISSING).
        cannotCreate_.insert(netId);
        noticeMissingClass(record->className);
        oapiWriteLogV("OLC client: cannot create vessel '%s': class '%s' is not installed",
                      record->name.c_str(), record->className.c_str());
        return nullptr;
    }

    VESSELSTATUS2 status;
    std::memset(&status, 0, sizeof(status));
    status.version = 2;
    status.rbody = body;
    if (state.status == static_cast<uint8_t>(olc::VesselStatus::Flying)) {
        status.status = 0;
        status.rpos = toVector3(state.rpos);
        status.rvel = toVector3(state.rvel);
        status.arot = toVector3(state.arot);
        status.vrot = toVector3(state.vrot);
    } else {
        // A landed vessel needs its place; the host repeats LANDED, so wait for it.
        auto place = landedAt_.find(netId);
        if (place == landedAt_.end()) {
            return nullptr;
        }
        status.status = 1;
        status.surf_lng = place->second.longitude;
        status.surf_lat = place->second.latitude;
        status.surf_hdg = place->second.heading;
        status.arot.x = 10.0;  // see putOnSurface
    }

    OBJHANDLE handle = oapiCreateVesselEx(record->name.c_str(), record->className.c_str(), &status);
    if (handle) {
        vessels_[netId] = handle;
        oapiWriteLogV("OLC client: created vessel '%s' (%s)", record->name.c_str(),
                      record->className.c_str());
    }
    return handle;
}

bool ClientSide::deleteVessel(OBJHANDLE handle) {
    const std::string name = objectName(handle);
    if (static_cast<int>(oapiGetVesselCount()) - deletesThisFrame_ <= 1) {
        oapiWriteLogV("OLC client: vessel '%s' kept: it is the last one, Orbiter would end "
                      "the session", name.c_str());
        return false;
    }
    if (!oapiDeleteVessel(handle)) {
        oapiWriteLogV("OLC client: cannot delete vessel '%s'", name.c_str());
        return false;
    }
    deletesThisFrame_++;
    oapiWriteLogV("OLC client: deleted vessel '%s'", name.c_str());
    return true;
}

void ClientSide::noticeMissingClass(const std::string& className) {
    if (std::find(missingClasses_.begin(), missingClasses_.end(), className) !=
        missingClasses_.end()) {
        return;
    }
    missingClasses_.push_back(className);
    std::string text = "OLC: the host has vessels of classes not installed on this computer, "
                       "they are not shown: ";
    for (size_t i = 0; i < missingClasses_.size(); i++) {
        text += (i == 0 ? "" : ", ") + missingClasses_[i];
    }
    showNotice(text);
}

void ClientSide::showNotice(const std::string& text) {
    noticeText_ = text;
    noticeShownAt_ = -1.0;  // shown at the next tick
}

void ClientSide::updateNotice(double now) {
    if (noticeText_.empty()) {
        return;
    }
    if (noticeShownAt_ < 0.0) {
        if (!notice_) {
            notice_ = oapiCreateAnnotation(true, 1.0, _V(1.0, 0.6, 0.2));
            // Below the HUD text at the top of the window.
            oapiAnnotationSetPos(notice_, 0.1, 0.35, 0.9, 0.6);
        }
        oapiAnnotationSetText(notice_, &noticeText_[0]);
        noticeShownAt_ = now;
    } else if (now - noticeShownAt_ > kNoticeSeconds) {
        if (notice_) {
            oapiDelAnnotation(notice_);
            notice_ = nullptr;
        }
        noticeText_.clear();
    }
}

bool ClientSide::ownVessel(uint16_t& netId) const {
    for (const olc::VesselEntry& entry : link_.table().vesselEntries()) {
        if (link_.table().isControlledBy(entry.netId, link_.clientId())) {
            netId = entry.netId;
            return true;
        }
    }
    return false;
}

void ClientSide::onFocusChanged(OBJHANDLE vessel) {
    // Before the vessel list has arrived, the start focus is checked later from tick.
    if (link_.state() != olc::ClientLink::State::Ready || !vessel) {
        return;
    }
    focusChecked_ = true;
    const olc::VesselRecord* record = link_.table().findByName(objectName(vessel));
    if (!record) {
        return;  // not a host vessel
    }
    uint16_t own = 0;
    if (ownVessel(own)) {
        if (own != record->netId) {
            showNotice("OLC: view only - your vessel is '" +
                       link_.table().findById(own)->name + "'. Ctrl+Alt+U releases it.");
        }
        return;
    }
    if (record->owner == olc::kNoOwner) {
        link_.requestTake(record->netId);  // the answer comes as OWNERSHIP or OWNER_DENIED
    } else {
        showNotice("OLC: view only - '" + record->name + "' is flown by another pilot.");
    }
}

void ClientSide::onKeyboard(char kstate[256]) {
    if (link_.state() != olc::ClientLink::State::Ready) {
        return;
    }
    const uint32_t before = heldKeys_;
    heldKeys_ = keymap_.inputKeys(kstate);
    // A new press of the full main engine key starts a latency measurement, if the vessel is quiet.
    if ((heldKeys_ & olc::kInputFullMain) != 0 && (before & olc::kInputFullMain) == 0 &&
        !ownThrusting_) {
        keyDownAt_ = wallClock();
        inputSentAt_ = -1.0;
        stateAt_ = -1.0;
        longestFrame_ = 0.0;
    }
    if (heldKeys_ != 0) {
        keyFrames_++;
    }
    keymap_.clearControlKeys(kstate);
}

void ClientSide::onKeyPressed(DWORD key, const char kstate[256]) {
    uint16_t own = 0;
    if (link_.state() != olc::ClientLink::State::Ready || !ownVessel(own) ||
        vesselHandle(own) != oapiGetFocusObject() || key > 0xFF) {
        return;
    }
    switch (key) {
        case OAPI_KEY_LSHIFT:
        case OAPI_KEY_RSHIFT:
        case OAPI_KEY_LCONTROL:
        case OAPI_KEY_RCONTROL:
        case OAPI_KEY_LALT:
        case OAPI_KEY_RALT:
            return;
        default:
            break;
    }
    uint8_t mods = 0;
    if (KEYMOD_SHIFT(kstate)) mods |= 0x01;
    if (KEYMOD_CONTROL(kstate)) mods |= 0x02;
    if (KEYMOD_ALT(kstate)) mods |= 0x04;
    link_.sendKey(own, static_cast<uint8_t>(key), mods);
    keyPresses_++;
}

void ClientSide::sendInput(double now) {
    uint16_t own = 0;
    if (!ownVessel(own)) {
        return;
    }
    olc::Input input;
    input.netId = own;
    OBJHANDLE handle = vesselHandle(own);
    // Keys and joystick fly the vessel in focus only; while the pilot watches another vessel, the
    // own one gets an empty INPUT (no key held).
    if (handle && handle == oapiGetFocusObject()) {
        input.keys = heldKeys_;
        VESSEL* vessel = oapiGetVesselInterface(handle);
        auto axis = [vessel](THGROUP_TYPE positive, THGROUP_TYPE negative) {
            return static_cast<float>(
                vessel->GetManualControlLevel(positive, MANCTRL_ANYMODE, MANCTRL_JOYSTICK) -
                vessel->GetManualControlLevel(negative, MANCTRL_ANYMODE, MANCTRL_JOYSTICK));
        };
        input.rot[0] = axis(THGROUP_ATT_PITCHUP, THGROUP_ATT_PITCHDOWN);
        input.rot[1] = axis(THGROUP_ATT_YAWLEFT, THGROUP_ATT_YAWRIGHT);
        input.rot[2] = axis(THGROUP_ATT_BANKLEFT, THGROUP_ATT_BANKRIGHT);
        input.lin[0] = axis(THGROUP_ATT_UP, THGROUP_ATT_DOWN);
        input.lin[1] = axis(THGROUP_ATT_LEFT, THGROUP_ATT_RIGHT);
        input.lin[2] = axis(THGROUP_ATT_FORWARD, THGROUP_ATT_BACK);
    }
    // At most 60 times per second (ClientLink).
    if (link_.sendInput(input, now) && keyDownAt_ >= 0.0 && inputSentAt_ < 0.0 &&
        (input.keys & olc::kInputFullMain) != 0) {
        inputSentAt_ = wallClock();
    }
}

void ClientSide::releaseOwnVessel() {
    uint16_t own = 0;
    if (link_.state() == olc::ClientLink::State::Ready && ownVessel(own)) {
        link_.requestRelease(own);
    }
}

void ClientSide::followOwnership() {
    uint16_t own = 0;
    const bool hasOwn = ownVessel(own);
    if (hasOwn && (!hadOwnVessel_ || own != lastOwnVessel_)) {
        const std::string name = link_.table().findById(own)->name;
        showNotice("OLC: you fly '" + name + "'. Ctrl+Alt+U releases it.");
        oapiWriteLogV("OLC client: flying '%s'", name.c_str());
    } else if (!hasOwn && hadOwnVessel_) {
        showNotice("OLC: your vessel is released. Focus a free vessel (F3) to take it.");
        oapiWriteLogV("OLC client: vessel released");
    }
    hadOwnVessel_ = hasOwn;
    lastOwnVessel_ = own;

    for (const olc::OwnerDenied& denied : link_.takeOwnerDenials()) {
        const olc::VesselRecord* record = link_.table().findById(denied.netId);
        const std::string name = record ? record->name : "vessel";
        std::string reason;
        switch (static_cast<olc::OwnerResult>(denied.reason)) {
        case olc::OwnerResult::Busy:
            reason = "it is flown or reserved by another pilot";
            break;
        case olc::OwnerResult::AlreadyHasVessel:
            reason = "you already fly another vessel";
            break;
        case olc::OwnerResult::NoSuchVessel:
            reason = "the host has no such vessel";
            break;
        default:
            reason = "the host refused";
            break;
        }
        showNotice("OLC: cannot take '" + name + "': " + reason + ". View only.");
        oapiWriteLogV("OLC client: cannot take '%s': %s", name.c_str(), reason.c_str());
    }
}

void ClientSide::removeExtraVessels() {
    // Collect first: the list of vessels must not change while it is being walked.
    std::vector<OBJHANDLE> extra;
    const DWORD count = oapiGetVesselCount();
    for (DWORD i = 0; i < count; i++) {
        OBJHANDLE handle = oapiGetVesselByIndex(static_cast<int>(i));
        if (!link_.table().findByName(objectName(handle))) {
            extra.push_back(handle);
        }
    }
    for (OBJHANDLE handle : extra) {
        deleteVessel(handle);
    }
}

void ClientSide::applyVesselChanges() {
    // New vessels need nothing here: applyStates creates them once their state is known.
    for (const olc::VesselChange& change : link_.takeVesselChanges()) {
        if (change.added) {
            continue;
        }
        cannotCreate_.erase(change.vessel.netId);
        landedAt_.erase(change.vessel.netId);
        std::string name = change.vessel.name;
        OBJHANDLE handle = oapiGetVesselByName(&name[0]);
        vessels_.erase(change.vessel.netId);
        if (handle) {
            deleteVessel(handle);
        }
    }
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
    for (auto entry = vessels_.begin(); entry != vessels_.end();) {
        if (entry->second == vessel) {
            entry = vessels_.erase(entry);
        } else {
            ++entry;
        }
    }
}

void ClientSide::logStats() {
    const olc::ClientStats& stats = link_.stats();
    const double average = timeErrorCount_ > 0 ? timeErrorSum_ / static_cast<double>(timeErrorCount_) : 0.0;
    oapiWriteLogV("OLC client: rtt %.1f ms, world states %llu, dropped %llu, starved %llu, "
                  "sim time error avg %.3f s max %.3f s, time jumps %d, input sent %llu, "
                  "frames with control keys %llu, key presses sent %llu",
                  link_.rtt() * 1000.0,
                  static_cast<unsigned long long>(stats.worldStatePackets),
                  static_cast<unsigned long long>(stats.droppedPackets),
                  static_cast<unsigned long long>(stats.starvedSamples), average,
                  timeErrorMax_, timeJumps_, static_cast<unsigned long long>(stats.inputPackets),
                  static_cast<unsigned long long>(keyFrames_),
                  static_cast<unsigned long long>(keyPresses_));
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
