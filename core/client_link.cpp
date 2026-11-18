#include "client_link.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "protocol.h"
#include "serialize.h"

namespace olc {

namespace {

// Whether strings are equal ignoring case (Orbiter system names are ASCII).
bool equalsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// Builds a UDP packet: header and a ready body.
std::vector<uint8_t> makeUdpPacket(UdpType type, uint32_t session, uint32_t seq,
                                   const std::vector<uint8_t>& body) {
    UdpHeader header;
    header.type = static_cast<uint8_t>(type);
    header.session = session;
    header.seq = seq;
    Writer writer;
    writeUdpHeader(writer, header);
    writer.writeBytes(body);
    return writer.bytes();
}

}  // namespace

ClientLink::~ClientLink() {
    stop("client stopped");
}

bool ClientLink::start(const ClientConfig& config, double now) {
    if (state_ != State::Idle) {
        return false;
    }
    config_ = config;
    state_ = State::Connecting;
    closeReason_.clear();
    startedAt_ = now;
    clock_ = ClientClock(config.displayDelay);
    log("OLC client: connecting to " + endpointToString(config.host));

    if (!tcp_.startConnect(config.host)) {
        close("cannot start connection");
        return false;
    }

    // HELLO goes out at once: while connecting, the frame waits in the queue.
    Hello hello;
    hello.protocol = kProtocolVersion;
    hello.orbiterVersion = config.orbiterVersion;
    hello.scenarioId = config.scenarioId;
    hello.pluginVersion = config.pluginVersion;
    hello.player = config.player;
    hello.flags = config.observer ? kHelloObserver : 0;
    Writer body;
    if (!writeHello(body, hello)) {
        close("player name or plugin version is too long");
        return false;
    }
    sendMessage(TcpType::Hello, body);
    return true;
}

void ClientLink::stop(const std::string& reason) {
    if (state_ != State::Connecting && state_ != State::Ready) {
        return;
    }
    Writer body;
    if (writeBye(body, reason)) {
        sendMessage(TcpType::Bye, body);
    }
    tcp_.flush();
    close(reason);
}

void ClientLink::close(const std::string& reason) {
    if (state_ == State::Closed) {
        return;
    }
    state_ = State::Closed;
    closeReason_ = reason;
    tcp_.close();
    udp_.close();
    log("OLC client: disconnected (" + reason + ")");
}

void ClientLink::log(const std::string& line) {
    log_.push_back(line);
}

std::vector<std::string> ClientLink::takeLog() {
    std::vector<std::string> lines;
    lines.swap(log_);
    return lines;
}

void ClientLink::sendMessage(TcpType type, const Writer& body) {
    // Send errors are not examined: poll sees a broken connection when receiving.
    tcp_.send(makeTcpFrame(static_cast<uint8_t>(type), body.bytes()));
}

// ---------- Receiving ----------

void ClientLink::poll(double now) {
    if (state_ != State::Connecting && state_ != State::Ready) {
        return;
    }

    bool connected = tcp_.receive(frames_);
    // Messages are parsed even if the connection is already closed: before closing the host
    // may have sent REJECT or BYE.
    if (!handleFrames(now)) {
        return;
    }
    if (!connected) {
        close(welcomed_ ? "connection lost" : "connection refused");
        return;
    }
    double timeout = config_.scenarioId == kNoScenario ? kScenarioTimeout : kConnectTimeout;
    if (!welcomed_ && now - startedAt_ > timeout) {
        close("connection timeout");
        return;
    }
    if (!tcp_.flush()) {
        close(welcomed_ ? "connection lost" : "connection refused");
        return;
    }

    if (state_ == State::Ready) {
        handleUdp(now);
        // Checked after the sockets are read, as on the host (kSilenceTimeout).
        if (now - lastHeard_ > kSilenceTimeout) {
            close("nothing heard from the host for " +
                  std::to_string(static_cast<int>(kSilenceTimeout)) + " s");
            return;
        }
        if (state_ == State::Ready && now >= nextPing_) {
            sendPing(now);
        }
    }
}

bool ClientLink::handleFrames(double now) {
    uint8_t type = 0;
    std::vector<uint8_t> body;
    TcpFrameReader::Result result = TcpFrameReader::Result::NeedMore;
    while (state_ != State::Closed &&
           (result = frames_.next(type, body)) == TcpFrameReader::Result::Ready) {
        lastHeard_ = now;
        Reader reader(body.data(), body.size());

        // Before WELCOME the host may send only WELCOME or REJECT, and SCENARIO if we asked for it.
        bool askedForScenario = config_.scenarioId == kNoScenario;
        bool isHandshake = type == static_cast<uint8_t>(TcpType::Welcome) ||
                           type == static_cast<uint8_t>(TcpType::Reject) ||
                           (askedForScenario && type == static_cast<uint8_t>(TcpType::Scenario));
        if (!welcomed_ && !isHandshake) {
            close("unexpected message before WELCOME");
            break;
        }

        switch (static_cast<TcpType>(type)) {
            case TcpType::Welcome:
                handleWelcome(reader);
                break;
            case TcpType::Reject:
                handleReject(reader);
                break;
            case TcpType::Scenario:
                handleScenario(reader);
                break;
            case TcpType::Bodies:
                if (!readBodies(reader, bodies_)) {
                    close("broken BODIES message");
                }
                break;
            case TcpType::Vessels:
                handleVessels(reader, now);
                break;
            case TcpType::VesselAdded:
                handleVesselAdded(reader);
                break;
            case TcpType::VesselRemoved:
                handleVesselRemoved(reader);
                break;
            case TcpType::Ownership: {
                std::vector<OwnerEntry> owners;
                if (!readOwnership(reader, owners)) {
                    close("broken OWNERSHIP message");
                    break;
                }
                for (const OwnerEntry& entry : owners) {
                    table_.setOwner(entry.netId, entry.owner);
                }
                break;
            }
            case TcpType::OwnerDenied: {
                OwnerDenied denied;
                if (!readOwnerDenied(reader, denied)) {
                    close("broken OWNER_DENIED message");
                    break;
                }
                ownerDenials_.push_back(denied);
                break;
            }
            case TcpType::Players:
                if (!readPlayers(reader, players_)) {
                    close("broken PLAYERS message");
                }
                break;
            case TcpType::Landed: {
                Landed landed;
                if (!readLanded(reader, landed)) {
                    close("broken LANDED message");
                    break;
                }
                landed_.push_back(landed);
                break;
            }
            case TcpType::Dock: {
                Dock dock;
                if (!readDock(reader, dock)) {
                    close("broken DOCK message");
                    break;
                }
                docks_.push_back(dock);
                break;
            }
            case TcpType::Bye: {
                std::string reason;
                if (!readBye(reader, reason)) {
                    reason = "no reason given";
                }
                close("host left: " + reason);
                break;
            }
            default:
                // Other types are not sent to a client: ignored.
                break;
        }
    }

    if (state_ != State::Closed && result == TcpFrameReader::Result::Broken) {
        close("broken TCP stream");
    }
    return state_ != State::Closed;
}

void ClientLink::handleScenario(Reader& reader) {
    // A SCENARIO the client did not ask for is a protocol error.
    if (config_.scenarioId != kNoScenario || welcomed_) {
        close("unexpected SCENARIO message");
        return;
    }
    if (!readScenario(reader, scenario_)) {
        close("broken SCENARIO message");
        return;
    }
    hasScenario_ = true;
    log("OLC client: received scenario #" + std::to_string(scenario_.scenarioId) + " '" +
        scenario_.name + "' (" + std::to_string(scenario_.data.size()) + " bytes)");
    close("scenario received");
}

void ClientLink::handleWelcome(Reader& reader) {
    Welcome welcome;
    if (welcomed_ || !readWelcome(reader, welcome)) {
        close("broken or repeated WELCOME message");
        return;
    }
    welcomed_ = true;
    session_ = welcome.session;
    clientId_ = welcome.clientId;
    director_ = (welcome.flags & kWelcomeDirector) != 0;
    observer_ = (welcome.flags & kWelcomeObserver) != 0;

    if (!equalsIgnoreCase(welcome.system, config_.system)) {
        Writer body;
        if (writeBye(body, "different planetary system")) {
            sendMessage(TcpType::Bye, body);
        }
        tcp_.flush();
        close("host plays in system '" + welcome.system + "', this client in '" +
              config_.system + "'");
    }
}

void ClientLink::handleReject(Reader& reader) {
    Reject reject;
    if (!readReject(reader, reject)) {
        close("rejected by host");
        return;
    }
    close("rejected by host: " + reject.text);
}

void ClientLink::handleVessels(Reader& reader, double now) {
    std::vector<VesselEntry> vessels;
    if (state_ != State::Connecting || !readVessels(reader, vessels)) {
        close("broken or repeated VESSELS message");
        return;
    }
    for (const VesselEntry& entry : vessels) {
        table_.addVesselWithId(entry.netId, entry.name, entry.className);
    }

    // Which classes the client lacks: each one is named once.
    std::vector<std::string> missing;
    for (const VesselEntry& entry : vessels) {
        if (isNewMissingClass(entry.className)) {
            missing.push_back(entry.className);
        }
    }
    Writer classesBody;
    if (!writeClassesMissing(classesBody, missing)) {
        close("cannot build CLASSES_MISSING");
        return;
    }
    sendMessage(TcpType::ClassesMissing, classesBody);

    if (!udp_.open(0)) {
        close("cannot open UDP socket");
        return;
    }
    Writer readyBody;
    writeUdpReady(readyBody, udp_.localPort());
    sendMessage(TcpType::UdpReady, readyBody);

    state_ = State::Ready;
    nextPing_ = now;
    log("OLC client: joined session as client " + std::to_string(clientId_) + ", " +
        std::to_string(vessels.size()) + " vessels, " + std::to_string(bodies_.size()) +
        " bodies");
}

bool ClientLink::isNewMissingClass(const std::string& className) {
    if (!config_.classExists || config_.classExists(className)) {
        return false;
    }
    if (std::find(missingClasses_.begin(), missingClasses_.end(), className) !=
        missingClasses_.end()) {
        return false;
    }
    missingClasses_.push_back(className);
    return true;
}

void ClientLink::handleVesselAdded(Reader& reader) {
    VesselAdded added;
    if (!readVesselAdded(reader, added)) {
        close("broken VESSEL_ADDED message");
        return;
    }
    if (table_.addVesselWithId(added.state.netId, added.name, added.className)) {
        // The state in VESSEL_ADDED refers to the latest known host moment.
        buffers_[added.state.netId].add(lastPacketSimTime_, added.state);
        // A class the client lacks: the host learns about it the same way as at join.
        if (isNewMissingClass(added.className)) {
            Writer body;
            if (writeClassesMissing(body, {added.className})) {
                sendMessage(TcpType::ClassesMissing, body);
            }
        }
        VesselChange change;
        change.added = true;
        change.vessel.netId = added.state.netId;
        change.vessel.name = added.name;
        change.vessel.className = added.className;
        vesselChanges_.push_back(change);
    }
}

void ClientLink::handleVesselRemoved(Reader& reader) {
    uint16_t netId = 0;
    if (!readVesselRemoved(reader, netId)) {
        close("broken VESSEL_REMOVED message");
        return;
    }
    // The name is needed by the plugin to find its vessel, so it is taken before removal.
    VesselChange change;
    change.vessel.netId = netId;
    for (const VesselEntry& entry : table_.vesselEntries()) {
        if (entry.netId == netId) {
            change.vessel = entry;
        }
    }
    if (table_.removeVessel(netId)) {
        vesselChanges_.push_back(change);
    }
    buffers_.erase(netId);
    fuel_.erase(netId);
}

void ClientLink::handleUdp(double now) {
    std::vector<uint8_t> packet;
    Endpoint from;
    while (udp_.receiveFrom(packet, from)) {
        // Accept only the host's packets.
        if (!(from == config_.host)) {
            stats_.droppedPackets++;
            continue;
        }
        Reader reader(packet.data(), packet.size());
        UdpHeader header;
        if (readUdpHeader(reader, header) != UdpHeaderResult::Ok || header.session != session_) {
            stats_.droppedPackets++;
            continue;
        }
        lastHeard_ = now;  // even an old packet shows the host is alive
        // WORLD_STATE, FUEL and PONG share one sequence counter: old and repeated packets are dropped.
        if (haveSeq_ && !isSequenceNewer(header.seq, lastSeq_)) {
            stats_.droppedPackets++;
            continue;
        }
        haveSeq_ = true;
        lastSeq_ = header.seq;

        if (header.type == static_cast<uint8_t>(UdpType::WorldState)) {
            WorldState world;
            if (!readWorldState(reader, world)) {
                stats_.droppedPackets++;
                continue;
            }
            stats_.worldStatePackets++;
            // A large state arrives in several packets with the same sim_time: the clock
            // is adjusted once per state, otherwise the correction would be applied several times.
            // Packets during pause also share a sim_time, but pause and warp changes are not skipped.
            const bool sameState = clock_.started() && world.simTime == lastPacketSimTime_ &&
                                   world.warp == hostWarp_ &&
                                   (world.paused != 0) == hostPaused_;
            hostWarp_ = world.warp;
            hostPaused_ = world.paused != 0;
            lastPacketSimTime_ = world.simTime;
            lastPacketMjd_ = world.mjd;
            if (!sameState) {
                clock_.onPacket(world.simTime, world.warp, hostPaused_, now);
            }
            for (const VesselState& vessel : world.vessels) {
                // A late packet may still carry a removed vessel (PROTOCOL.md, section 1).
                if (table_.findById(vessel.netId) == nullptr) {
                    stats_.unknownVessels++;
                    continue;
                }
                buffers_[vessel.netId].add(world.simTime, vessel);
            }
        } else if (header.type == static_cast<uint8_t>(UdpType::Fuel)) {
            Fuel fuel;
            if (!readFuel(reader, fuel)) {
                stats_.droppedPackets++;
                continue;
            }
            for (const VesselFuel& vessel : fuel.vessels) {
                if (table_.findById(vessel.netId) == nullptr) {
                    stats_.unknownVessels++;
                    continue;
                }
                fuel_[vessel.netId] = vessel;
            }
        } else if (header.type == static_cast<uint8_t>(UdpType::Pong)) {
            Ping pong;
            if (!readPing(reader, pong)) {
                stats_.droppedPackets++;
                continue;
            }
            rtt_ = now - pong.time;
        }
    }
}

void ClientLink::sendPing(double now) {
    // PING carries the client number: from it the host learns our real UDP address (behind NAT
    // the port may differ from the one in UDP_READY).
    Ping ping;
    ping.time = now;
    ping.clientId = clientId_;
    // The latest measured round trip time, for the host's player list.
    ping.rttMs = rtt_ < 0.0 ? kRttUnknown
                            : static_cast<uint16_t>(std::min(rtt_ * 1000.0 + 0.5, 65534.0));
    Writer body;
    writePing(body, ping);
    udp_.sendTo(config_.host,
                makeUdpPacket(UdpType::Ping, session_, pingSeq_++, body.bytes()));
    nextPing_ = now + kPingInterval;
}

bool ClientLink::sendInput(const Input& input, double now) {
    if (state_ != State::Ready || now < nextInput_) {
        return false;
    }
    Writer body;
    writeInput(body, input);
    udp_.sendTo(config_.host,
                makeUdpPacket(UdpType::Input, session_, inputSeq_++, body.bytes()));
    nextInput_ = nextSendTime(nextInput_, now, 1.0 / kMaxInputRate);
    stats_.inputPackets++;
    return true;
}

void ClientLink::sendKey(uint16_t netId, uint8_t key, uint8_t mods) {
    if (state_ != State::Ready) {
        return;
    }
    KeyPress press;
    press.netId = netId;
    press.key = key;
    press.mods = mods;
    Writer body;
    writeKey(body, press);
    sendMessage(TcpType::Key, body);
}

// ---------- What we hand to the plugin ----------

double ClientLink::hostMjdAt(double simTime) const {
    return lastPacketMjd_ + (simTime - lastPacketSimTime_) / 86400.0;
}

bool ClientLink::sampleVessel(uint16_t netId, double simTime, VesselState& result,
                              bool& starved) {
    auto found = buffers_.find(netId);
    if (found == buffers_.end() || found->second.size() == 0) {
        return false;
    }
    if (!found->second.sample(simTime, result, starved)) {
        return false;
    }
    if (starved) {
        stats_.starvedSamples++;
    }
    return true;
}

bool ClientLink::latestVessel(uint16_t netId, VesselState& result) const {
    auto found = buffers_.find(netId);
    if (found == buffers_.end()) {
        return false;
    }
    return found->second.latest(result);
}

std::vector<VesselFuel> ClientLink::takeFuel() {
    std::vector<VesselFuel> result;
    for (const auto& entry : fuel_) {
        result.push_back(entry.second);
    }
    fuel_.clear();
    return result;
}

void ClientLink::sendOwnerRequest(uint16_t netId, uint8_t action) {
    if (state_ != State::Ready) {
        return;
    }
    OwnerRequest request;
    request.netId = netId;
    request.action = action;
    Writer body;
    writeOwnerRequest(body, request);
    sendMessage(TcpType::OwnerRequest, body);
}

void ClientLink::requestTake(uint16_t netId) {
    sendOwnerRequest(netId, kOwnerTake);
}

void ClientLink::requestRelease(uint16_t netId) {
    sendOwnerRequest(netId, kOwnerRelease);
}

void ClientLink::sendTimeRequest(bool paused, double warp) {
    if (state_ != State::Ready) {
        return;
    }
    TimeRequest request;
    request.paused = paused ? 1 : 0;
    request.warp = warp;
    Writer body;
    writeTimeRequest(body, request);
    sendMessage(TcpType::TimeRequest, body);
}

std::vector<OwnerDenied> ClientLink::takeOwnerDenials() {
    std::vector<OwnerDenied> result;
    result.swap(ownerDenials_);
    return result;
}

std::vector<Dock> ClientLink::takeDocks() {
    std::vector<Dock> result;
    result.swap(docks_);
    return result;
}

std::vector<Landed> ClientLink::takeLanded() {
    std::vector<Landed> result;
    result.swap(landed_);
    return result;
}

std::vector<VesselChange> ClientLink::takeVesselChanges() {
    std::vector<VesselChange> result;
    result.swap(vesselChanges_);
    return result;
}

}  // namespace olc
