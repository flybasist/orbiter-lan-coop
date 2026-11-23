#include "host_link.h"

#include <algorithm>
#include <random>
#include <utility>

#include "protocol.h"
#include "serialize.h"

namespace olc {

namespace {

// With port = 0 (tests) the host needs one number that is free for both TCP and UDP. Numbers are
// tried at random from a wide range: the system hands out TCP ports one after another, and Windows
// reserves whole blocks of ports (often hundreds in a row) for one protocol, so neighbouring numbers
// tend to fail together.
constexpr int kPortAttempts = 20;
constexpr uint16_t kRandomPortFirst = 20000;
constexpr uint16_t kRandomPortLast = 60000;

}  // namespace

HostLink::HostLink(SessionTable& table) : table_(table) {}

HostLink::~HostLink() {
    stop("host stopped");
}

bool HostLink::start(const HostConfig& config) {
    if (running_) {
        return false;
    }
    config_ = config;

    // A given port is tried once; port = 0 - random numbers until one is free for TCP and UDP.
    std::mt19937 random(std::random_device{}());
    std::uniform_int_distribution<int> anyPort(kRandomPortFirst, kRandomPortLast);
    const int attempts = config.port == 0 ? kPortAttempts : 1;
    bool opened = false;
    for (int i = 0; i < attempts && !opened; i++) {
        const uint16_t port = config.port != 0 ? config.port : static_cast<uint16_t>(anyPort(random));
        opened = listener_.open(port) && udp_.open(port);
    }
    if (!opened) {
        listener_.close();
        udp_.close();
        log("OLC host: cannot open port " + std::to_string(config.port) +
            " (busy or not allowed)");
        return false;
    }

    running_ = true;
    log("OLC host: listening on port " + std::to_string(listener_.localPort()));
    return true;
}

void HostLink::stop(const std::string& reason) {
    if (!running_) {
        return;
    }
    Writer body;
    bool byeOk = writeBye(body, reason);
    for (Client& client : clients_) {
        if (byeOk) {
            sendMessage(client, TcpType::Bye, body);
        }
        client.tcp.flush();
        client.tcp.close();
        if (client.welcomed) {
            table_.removePlayer(client.clientId, "", now_);  // the session ends: nothing to reserve
        }
    }
    clients_.clear();
    listener_.close();
    udp_.close();
    running_ = false;
    log("OLC host: stopped (" + reason + ")");
}

uint16_t HostLink::port() const {
    return running_ ? listener_.localPort() : 0;
}

void HostLink::setBodies(const std::vector<BodyEntry>& bodies) {
    bodies_ = bodies;
}

int HostLink::clientCount() const {
    int count = 0;
    for (const Client& client : clients_) {
        if (client.welcomed) {
            count++;
        }
    }
    return count;
}

int HostLink::readyClientCount() const {
    int count = 0;
    for (const Client& client : clients_) {
        if (client.udpReady) {
            count++;
        }
    }
    return count;
}

std::vector<std::string> HostLink::takeLog() {
    std::vector<std::string> lines;
    lines.swap(log_);
    return lines;
}

void HostLink::log(const std::string& line) {
    log_.push_back(line);
}

// ---------- Receiving ----------

void HostLink::poll(double now) {
    if (!running_) {
        return;
    }
    now_ = now;

    // New connections.
    while (true) {
        TcpConnection connection;
        if (!listener_.accept(connection)) {
            break;
        }
        Client client;
        client.tcp = std::move(connection);
        client.connectedAt = now;
        client.lastHeard = now;
        clients_.push_back(std::move(client));
    }

    // Client messages. A disconnected client is removed from the list.
    for (size_t i = 0; i < clients_.size();) {
        Client& client = clients_[i];
        bool keep = client.tcp.receive(client.frames);
        // Messages are parsed even if the connection is already closed: a BYE may be left in it.
        if (!handleFrames(client)) {
            keep = false;  // the reason is already in leaveReason
        } else if (!keep) {
            client.leaveReason = "connection lost";
        }
        if (keep && !client.welcomed && !client.scenarioSent &&
            now - client.connectedAt > kHelloTimeout) {
            keep = false;
            client.leaveReason = "no HELLO received in time";
        }
        if (keep && client.scenarioSent && now - client.connectedAt > kScenarioSendTimeout) {
            keep = false;
            client.leaveReason = "scenario was not delivered in time";
        }
        if (keep && !client.tcp.flush()) {
            keep = false;
            client.leaveReason = "connection lost";
        }
        // The client that asked for a scenario is done once the whole file has gone out.
        if (keep && client.scenarioSent && client.tcp.pendingBytes() == 0) {
            keep = false;
            client.leaveReason = "scenario sent";
        }
        if (keep) {
            i++;
        } else {
            dropClient(i, client.leaveReason);
        }
    }

    handleUdp();

    if (now >= nextPlayers_) {
        broadcastPlayers();
        nextPlayers_ = now + kPlayersInterval;
    }

    // Silent clients. Checked after all sockets are read: after a long host frame the client's
    // packets are already waiting in them, so only a really silent client is dropped.
    for (size_t i = 0; i < clients_.size();) {
        if (clients_[i].welcomed && now - clients_[i].lastHeard > kSilenceTimeout) {
            dropClient(i, "nothing heard for " + std::to_string(static_cast<int>(kSilenceTimeout)) + " s");
        } else {
            i++;
        }
    }
}

bool HostLink::handleFrames(Client& client) {
    uint8_t type = 0;
    std::vector<uint8_t> body;
    TcpFrameReader::Result result;
    while ((result = client.frames.next(type, body)) == TcpFrameReader::Result::Ready) {
        client.lastHeard = now_;
        Reader reader(body.data(), body.size());

        // A client that got its scenario has nothing more to say: the rest is ignored.
        if (client.scenarioSent && type != static_cast<uint8_t>(TcpType::Hello)) {
            continue;
        }
        // Before the handshake a client may send only HELLO.
        if (!client.welcomed && type != static_cast<uint8_t>(TcpType::Hello)) {
            client.leaveReason = "message received before HELLO";
            return false;
        }

        switch (static_cast<TcpType>(type)) {
            case TcpType::Hello:
                if (client.welcomed || client.scenarioSent) {
                    client.leaveReason = "second HELLO";
                    return false;
                }
                if (!handleHello(client, reader)) {
                    return false;
                }
                break;
            case TcpType::ClassesMissing: {
                std::vector<std::string> classes;
                if (!readClassesMissing(reader, classes)) {
                    client.leaveReason = "broken CLASSES_MISSING";
                    return false;
                }
                if (!classes.empty()) {
                    std::string line = "OLC host: client " + std::to_string(client.clientId) +
                                       " is missing classes: ";
                    for (size_t i = 0; i < classes.size(); i++) {
                        line += (i == 0 ? "" : ", ") + classes[i];
                    }
                    log(line);
                }
                break;
            }
            case TcpType::UdpReady: {
                uint16_t udpPort = 0;
                if (!readUdpReady(reader, udpPort)) {
                    client.leaveReason = "broken UDP_READY";
                    return false;
                }
                // An address already learned from PING is more accurate: the port in UDP_READY is the
                // port on the client machine, and behind NAT the host sees another one.
                if (!client.udpFromPing) {
                    client.udp.ip = client.tcp.remote().ip;
                    client.udp.port = udpPort;
                }
                client.udpReady = true;
                log("OLC host: client " + std::to_string(client.clientId) +
                    " is ready for UDP at " + endpointToString(client.udp));
                break;
            }
            case TcpType::Key:
                if (!handleKey(client, reader)) {
                    return false;
                }
                break;
            case TcpType::TimeRequest:
                if (!handleTimeRequest(client, reader)) {
                    return false;
                }
                break;
            case TcpType::SaveRequest:
                if (!handleSaveRequest(client, reader)) {
                    return false;
                }
                break;
            case TcpType::NavState:
                if (!handleNavState(client, reader)) {
                    return false;
                }
                break;
            case TcpType::OwnerRequest:
                if (!handleOwnerRequest(client, reader)) {
                    return false;
                }
                break;
            case TcpType::Bye: {
                std::string reason;
                if (!readBye(reader, reason)) {
                    reason = "no reason given";
                }
                client.leaveReason = reason;
                return false;
            }
            default:
                // Other messages are not handled by the host.
                break;
        }
    }

    if (result == TcpFrameReader::Result::Broken) {
        client.leaveReason = "broken TCP stream";
        return false;
    }
    return true;
}

bool HostLink::handleKey(Client& client, Reader& reader) {
    KeyPress press;
    if (!readKey(reader, press)) {
        client.leaveReason = "broken KEY";
        return false;
    }
    if (!table_.isControlledBy(press.netId, client.clientId)) {
        stats_.keysRejected++;
        return true;
    }
    stats_.keysAccepted++;
    keys_.push_back(press);
    return true;
}

std::vector<KeyPress> HostLink::takeKeys() {
    std::vector<KeyPress> result;
    result.swap(keys_);
    return result;
}

bool HostLink::handleTimeRequest(Client& client, Reader& reader) {
    TimeRequest request;
    if (!readTimeRequest(reader, request)) {
        client.leaveReason = "broken TIME_REQUEST";
        return false;
    }
    if (!client.director) {
        stats_.timeRequestsRejected++;
        log("OLC host: client " + std::to_string(client.clientId) + " ('" + client.player +
            "') asked for pause or warp but is not the flight director");
        return true;
    }
    stats_.timeRequestsAccepted++;
    timeRequests_.push_back(request);
    return true;
}

std::vector<TimeRequest> HostLink::takeTimeRequests() {
    std::vector<TimeRequest> result;
    result.swap(timeRequests_);
    return result;
}

bool HostLink::handleSaveRequest(Client& client, Reader& reader) {
    if (reader.remaining() != 0) {
        client.leaveReason = "broken SAVE_REQUEST";
        return false;
    }
    if (!client.director) {
        stats_.saveRequestsRejected++;
        log("OLC host: client " + std::to_string(client.clientId) + " ('" + client.player +
            "') asked for a save but is not the flight director");
        return true;
    }
    stats_.saveRequestsAccepted++;
    saveRequests_++;
    return true;
}

int HostLink::takeSaveRequests() {
    const int result = saveRequests_;
    saveRequests_ = 0;
    return result;
}

bool HostLink::handleNavState(Client& client, Reader& reader) {
    NavState state;
    if (!readNavState(reader, state)) {
        client.leaveReason = "broken NAV_STATE";
        return false;
    }
    // Frequencies are set by the pilot on their own copy of the vessel: only the owner is the source.
    if (!table_.isControlledBy(state.netId, client.clientId)) {
        stats_.navStatesRejected++;
        return true;
    }
    stats_.navStatesAccepted++;
    navStates_[state.netId] = state;
    navUpdates_.push_back(state);
    Writer body;
    writeNavState(body, state);  // cannot fail: readNavState has accepted the same channel count
    for (Client& other : clients_) {
        if (other.welcomed && &other != &client) {
            sendMessage(other, TcpType::NavState, body);
        }
    }
    return true;
}

std::vector<NavState> HostLink::takeNavStates() {
    std::vector<NavState> result;
    result.swap(navUpdates_);
    return result;
}

bool HostLink::handleOwnerRequest(Client& client, Reader& reader) {
    OwnerRequest request;
    if (!readOwnerRequest(reader, request) ||
        (request.action != kOwnerTake && request.action != kOwnerRelease)) {
        client.leaveReason = "broken OWNER_REQUEST";
        return false;
    }

    OwnerResult result =
        request.action == kOwnerTake
            ? (client.observer
                   ? OwnerResult::Observer
                   : table_.takeVessel(request.netId, client.clientId, client.player, now_))
            : table_.releaseVessel(request.netId, client.clientId);
    const char* verb = request.action == kOwnerTake ? "take" : "release";
    std::string who = "client " + std::to_string(client.clientId) + " ('" + client.player + "')";

    if (result == OwnerResult::Ok) {
        const VesselRecord* vessel = table_.findById(request.netId);
        log("OLC host: " + who + " " + verb + "s vessel '" + (vessel ? vessel->name : "?") + "'");
        broadcastOwnership();
        return true;
    }

    log("OLC host: " + who + " cannot " + verb + " vessel " + std::to_string(request.netId) +
        " (reason " + std::to_string(static_cast<int>(result)) + ")");
    OwnerDenied denied;
    denied.netId = request.netId;
    denied.reason = static_cast<uint8_t>(result);
    Writer body;
    writeOwnerDenied(body, denied);
    sendMessage(client, TcpType::OwnerDenied, body);
    return true;
}

void HostLink::broadcastOwnership() {
    Writer body;
    if (!writeOwnership(body, table_.ownerEntries())) {
        return;  // more than 65535 vessels: not reachable, VESSELS would not have been built either
    }
    for (Client& client : clients_) {
        if (client.welcomed) {
            sendMessage(client, TcpType::Ownership, body);
        }
    }
    // The players' vessels changed with the owners.
    broadcastPlayers();
}

void HostLink::broadcastPlayers() {
    std::vector<PlayerEntry> players;
    if (!config_.hostPlayer.empty()) {
        PlayerEntry self;
        self.clientId = kHostOwner;
        self.player = config_.hostPlayer;
        self.netId = table_.vesselOwnedBy(kHostOwner);
        self.rttMs = 0;  // the host is the reference point
        players.push_back(self);
    }
    for (const Client& client : clients_) {
        if (!client.welcomed) {
            continue;
        }
        PlayerEntry entry;
        entry.clientId = client.clientId;
        entry.player = client.player;
        entry.flags = static_cast<uint8_t>((client.director ? kPlayerDirector : 0) |
                                           (client.observer ? kPlayerObserver : 0));
        entry.netId = table_.vesselOwnedBy(client.clientId);
        entry.rttMs = client.rttMs;
        players.push_back(entry);
    }
    Writer body;
    if (!writePlayers(body, players)) {
        return;  // a name longer than 255 bytes: HELLO would not have been built either
    }
    for (Client& client : clients_) {
        if (client.welcomed) {
            sendMessage(client, TcpType::Players, body);
        }
    }
}

bool HostLink::handleHello(Client& client, Reader& reader) {
    // The protocol version is checked before the rest of HELLO: another version may have
    // other fields, and its client must get REJECT 1, not a dropped connection.
    Reader versionReader = reader;
    uint16_t protocol = 0;
    if (versionReader.readU16(protocol) && protocol != kProtocolVersion) {
        return reject(client, RejectReason::ProtocolVersion,
                      "protocol version mismatch: host " + std::to_string(kProtocolVersion) +
                          ", client " + std::to_string(protocol));
    }

    Hello hello;
    if (!readHello(reader, hello)) {
        client.leaveReason = "broken HELLO";
        return false;
    }
    if (hello.orbiterVersion != config_.orbiterVersion) {
        return reject(client, RejectReason::OrbiterVersion,
                      "Orbiter version mismatch: host " +
                          std::to_string(config_.orbiterVersion) + ", client " +
                          std::to_string(hello.orbiterVersion));
    }
    if (hello.scenarioId == kNoScenario) {
        return sendScenario(client, hello);
    }
    uint8_t clientId = 0;
    if (!table_.addPlayer(clientId)) {
        return reject(client, RejectReason::NoFreeSlots, "no free player slots");
    }

    client.welcomed = true;
    client.clientId = clientId;
    client.player = hello.player;
    client.director = !config_.director.empty() && hello.player == config_.director;
    client.observer = client.director || (hello.flags & kHelloObserver) != 0;
    // A pilot who dropped out and came back under the same name gets their vessel back. An observer
    // (or the director) never takes a vessel, even under the name of a pilot who left.
    bool restored = !client.observer && table_.restoreHeldVessels(clientId, hello.player, now_) > 0;

    Welcome welcome;
    welcome.clientId = clientId;
    welcome.session = config_.session;
    welcome.stateRateHz = config_.stateRateHz;
    welcome.system = config_.system;
    welcome.flags = static_cast<uint8_t>((client.director ? kWelcomeDirector : 0) |
                                         (client.observer ? kWelcomeObserver : 0));
    Writer body;
    if (!writeWelcome(body, welcome)) {
        client.leaveReason = "cannot build WELCOME";
        return false;
    }
    sendMessage(client, TcpType::Welcome, body);
    if (!sendWorldLists(client)) {
        client.leaveReason = "cannot build session lists";
        return false;
    }

    std::string scenario = hello.scenarioId == kOwnScenario
                               ? "own scenario"
                               : "scenario #" + std::to_string(hello.scenarioId);
    // The plugin version shows in the log who came with a different package.
    log("OLC host: player '" + hello.player + "' joined as client " + std::to_string(clientId) +
        " (" + scenario + ", plugin " + hello.pluginVersion + ")");
    if (restored) {
        log("OLC host: player '" + hello.player + "' got their vessel back");
        broadcastOwnership();  // includes PLAYERS
    } else {
        broadcastPlayers();
    }
    return true;
}

bool HostLink::sendScenario(Client& client, const Hello& hello) {
    // Without a free place the client would load the scenario for nothing and be refused afterwards.
    if (table_.playerCount() >= table_.maxPlayers()) {
        return reject(client, RejectReason::NoFreeSlots, "no free player slots");
    }
    Scenario scenario;
    if (!config_.makeScenario || !config_.makeScenario(scenario)) {
        return reject(client, RejectReason::ScenarioUnavailable, "host could not save its scenario");
    }
    if (scenario.data.size() > kMaxScenarioSize) {
        return reject(client, RejectReason::ScenarioUnavailable,
                      "host scenario is too large: " + std::to_string(scenario.data.size()) +
                          " bytes, the limit is " + std::to_string(kMaxScenarioSize));
    }
    // Numbers start at 1 and skip the two special values.
    lastScenarioId_++;
    if (lastScenarioId_ == kNoScenario || lastScenarioId_ == kOwnScenario) {
        lastScenarioId_ = 1;
    }
    scenario.scenarioId = lastScenarioId_;

    Writer body;
    if (!writeScenario(body, scenario)) {
        return reject(client, RejectReason::ScenarioUnavailable, "host scenario name is too long");
    }
    sendMessage(client, TcpType::Scenario, body);
    client.scenarioSent = true;
    log("OLC host: sent scenario #" + std::to_string(scenario.scenarioId) + " (" +
        std::to_string(scenario.data.size()) + " bytes) to player '" + hello.player + "'");
    return true;
}

bool HostLink::sendWorldLists(Client& client) {
    Writer bodies;
    if (!writeBodies(bodies, bodies_)) {
        return false;
    }
    sendMessage(client, TcpType::Bodies, bodies);

    Writer vessels;
    if (!writeVessels(vessels, table_.vesselEntries())) {
        return false;
    }
    sendMessage(client, TcpType::Vessels, vessels);

    Writer owners;
    if (!writeOwnership(owners, table_.ownerEntries())) {
        return false;
    }
    sendMessage(client, TcpType::Ownership, owners);

    for (const Dock& dock : docks_) {
        Writer body;
        writeDock(body, dock);
        sendMessage(client, TcpType::Dock, body);
    }

    // The latest animations and frequencies: the new client starts with the same cabin state.
    for (const auto& vessel : anims_) {
        AnimState state;
        state.netId = vessel.first;
        for (const auto& anim : vessel.second) {
            state.anims.push_back({anim.first, anim.second});
        }
        Writer body;
        if (!writeAnimState(body, state)) {
            return false;
        }
        sendMessage(client, TcpType::AnimState, body);
    }
    for (const auto& vessel : navStates_) {
        Writer body;
        if (!writeNavState(body, vessel.second)) {
            return false;
        }
        sendMessage(client, TcpType::NavState, body);
    }
    return true;
}

bool HostLink::sendMessage(Client& client, TcpType type, const Writer& body) {
    return client.tcp.send(makeTcpFrame(static_cast<uint8_t>(type), body.bytes()));
}

bool HostLink::reject(Client& client, RejectReason reason, const std::string& text) {
    Reject message;
    message.reason = static_cast<uint8_t>(reason);
    message.text = text;
    Writer body;
    if (writeReject(body, message)) {
        sendMessage(client, TcpType::Reject, body);
    }
    client.leaveReason = "rejected: " + text;
    return false;
}

void HostLink::dropClient(size_t index, const std::string& reason) {
    Client& client = clients_[index];
    bool announceOwners = false;
    if (client.welcomed) {
        table_.removePlayer(client.clientId, client.player, now_);
        announceOwners = true;
        log("OLC host: client " + std::to_string(client.clientId) + " left (" + reason + ")");
    } else {
        log("OLC host: connection from " + endpointToString(client.tcp.remote()) +
            " closed (" + reason + ")");
    }
    // Send what is left in the queue (e.g. REJECT) and close.
    client.tcp.flush();
    client.tcp.close();
    clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(index));
    // The others learn that the leaver's vessels have no owner now.
    if (announceOwners) {
        broadcastOwnership();  // includes PLAYERS
    }
}

void HostLink::handleUdp() {
    std::vector<uint8_t> packet;
    Endpoint from;
    while (udp_.receiveFrom(packet, from)) {
        Reader reader(packet.data(), packet.size());
        UdpHeader header;
        if (readUdpHeader(reader, header) != UdpHeaderResult::Ok ||
            header.session != config_.session) {
            stats_.udpBadPackets++;
            continue;
        }
        if (header.type == static_cast<uint8_t>(UdpType::Ping)) {
            Ping ping;
            if (!readPing(reader, ping)) {
                stats_.udpBadPackets++;
                continue;
            }
            learnUdpAddress(ping.clientId, from, ping.rttMs);
            // PONG goes to whoever sent the PING, with the same body.
            Writer body;
            writePing(body, ping);
            if (udp_.sendTo(from, makeUdpPacket(static_cast<uint8_t>(UdpType::Pong),
                                                config_.session, seq_++, body.bytes()))) {
                stats_.udpPacketsSent++;
            }
        }
        if (header.type == static_cast<uint8_t>(UdpType::Input)) {
            handleInput(reader, header.seq, from);
        }
        // The rest is ignored.
    }
}

void HostLink::handleInput(Reader& reader, uint32_t seq, const Endpoint& from) {
    // The sender is the welcomed client whose UDP address this is (from UDP_READY or PING).
    Client* sender = nullptr;
    for (Client& client : clients_) {
        if (client.welcomed && client.udp == from) {
            sender = &client;
            break;
        }
    }
    if (sender == nullptr) {
        stats_.udpBadPackets++;
        return;
    }
    sender->lastHeard = now_;
    Input input;
    if (!readInput(reader, input)) {
        stats_.udpBadPackets++;
        return;
    }
    // Old and repeated packets are dropped; the first one is always accepted.
    if (sender->haveInputSeq && !isSequenceNewer(seq, sender->lastInputSeq)) {
        return;
    }
    sender->haveInputSeq = true;
    sender->lastInputSeq = seq;
    if (!table_.isControlledBy(input.netId, sender->clientId)) {
        stats_.inputRejected++;
        return;
    }
    stats_.inputAccepted++;
    LastInput& last = sender->inputs[input.netId];
    last.input = input;
    last.receivedAt = now_;
}

bool HostLink::latestInput(uint16_t netId, Input& input, double& receivedAt) const {
    for (const Client& client : clients_) {
        auto found = client.inputs.find(netId);
        // The owner may have changed since the packet came: check at query time too.
        if (!client.welcomed || found == client.inputs.end() ||
            !table_.isControlledBy(netId, client.clientId)) {
            continue;
        }
        input = found->second.input;
        receivedAt = found->second.receivedAt;
        return true;
    }
    return false;
}

void HostLink::learnUdpAddress(uint8_t clientId, const Endpoint& from, uint16_t rttMs) {
    for (Client& client : clients_) {
        // A client number is easy to fake, so the address is accepted only from the same IP
        // as this client's TCP connection.
        if (!client.welcomed || client.clientId != clientId ||
            client.tcp.remote().ip != from.ip) {
            continue;
        }
        if (client.udpReady && !(client.udp == from)) {
            log("OLC host: client " + std::to_string(clientId) + " UDP address is " +
                endpointToString(from) + ", not " + endpointToString(client.udp) +
                " (NAT?)");
        }
        client.udp = from;
        client.rttMs = rttMs;
        client.udpFromPing = true;
        client.lastHeard = now_;
        return;
    }
}

// ---------- Sending ----------

void HostLink::sendUdpToReady(uint8_t type, const std::vector<uint8_t>& body) {
    std::vector<uint8_t> packet =
        makeUdpPacket(type, config_.session, seq_++, body);
    for (const Client& client : clients_) {
        if (client.udpReady && udp_.sendTo(client.udp, packet)) {
            stats_.udpPacketsSent++;
        }
    }
}

void HostLink::sendWorldState(const WorldState& state) {
    if (!running_ || readyClientCount() == 0) {
        return;
    }

    // The common part of the packets: everything except the vessel list. Its size is what
    // is written for an empty list; the rest of the packet is for vessels.
    WorldState packet = state;
    packet.vessels.clear();
    Writer emptyPacket;
    if (!writeWorldState(emptyPacket, packet)) {
        return;
    }
    size_t budget = kMaxUdpPacketSize - kUdpHeaderSize - emptyPacket.bytes().size();

    size_t used = 0;  // how many vessel bytes are already in packet
    bool sentAny = false;
    // Sends the accumulated packet and starts the next one.
    auto flushPacket = [&]() {
        Writer body;
        if (writeWorldState(body, packet)) {
            sendUdpToReady(static_cast<uint8_t>(UdpType::WorldState), body.bytes());
            sentAny = true;
        }
        packet.vessels.clear();
        used = 0;
    };

    for (const VesselState& vessel : state.vessels) {
        Writer probe;
        if (!writeVesselState(probe, vessel)) {
            continue;  // more than kMaxThrusters thrusters: the plugin must not pass that
        }
        size_t size = probe.bytes().size();
        if (!packet.vessels.empty() &&
            (used + size > budget || packet.vessels.size() >= kMaxVesselsPerPacket)) {
            flushPacket();
        }
        packet.vessels.push_back(vessel);
        used += size;
    }
    // The last packet. Even with no vessels at all we send one: the client needs
    // the time and the pause flag.
    if (!packet.vessels.empty() || !sentAny) {
        flushPacket();
    }
}

void HostLink::sendFuel(const Fuel& fuel) {
    if (!running_ || readyClientCount() == 0 || fuel.vessels.empty()) {
        return;
    }

    Fuel packet;
    Writer emptyPacket;
    if (!writeFuel(emptyPacket, packet)) {
        return;
    }
    size_t budget = kMaxUdpPacketSize - kUdpHeaderSize - emptyPacket.bytes().size();

    size_t used = 0;
    auto flushPacket = [&]() {
        Writer body;
        if (writeFuel(body, packet)) {
            sendUdpToReady(static_cast<uint8_t>(UdpType::Fuel), body.bytes());
        }
        packet.vessels.clear();
        used = 0;
    };

    for (const VesselFuel& vessel : fuel.vessels) {
        // A vessel's size in the packet: the difference between a packet with it and an empty packet.
        Fuel single;
        single.vessels.push_back(vessel);
        Writer probe;
        if (!writeFuel(probe, single)) {
            continue;  // more than kMaxTanks tanks: the plugin must not pass that
        }
        size_t size = probe.bytes().size() - emptyPacket.bytes().size();
        if (!packet.vessels.empty() &&
            (used + size > budget || packet.vessels.size() >= kMaxVesselsPerPacket)) {
            flushPacket();
        }
        packet.vessels.push_back(vessel);
        used += size;
    }
    if (!packet.vessels.empty()) {
        flushPacket();
    }
}

void HostLink::sendDock(const Dock& dock) {
    auto sameLink = [&dock](const Dock& other) { return sameDockLink(other, dock); };
    docks_.erase(std::remove_if(docks_.begin(), docks_.end(), sameLink), docks_.end());
    if (dock.docked) {
        docks_.push_back(dock);
    }
    Writer body;
    writeDock(body, dock);
    for (Client& client : clients_) {
        if (client.welcomed) {
            sendMessage(client, TcpType::Dock, body);
        }
    }
}

bool HostLink::sendAnimState(const AnimState& state) {
    Writer body;
    if (!writeAnimState(body, state)) {
        return false;
    }
    for (const AnimValue& anim : state.anims) {
        anims_[state.netId][anim.index] = anim.value;
    }
    for (Client& client : clients_) {
        if (client.welcomed) {
            sendMessage(client, TcpType::AnimState, body);
        }
    }
    return true;
}

bool HostLink::sendNavState(const NavState& state) {
    Writer body;
    if (!writeNavState(body, state)) {
        return false;
    }
    navStates_[state.netId] = state;
    for (Client& client : clients_) {
        if (client.welcomed) {
            sendMessage(client, TcpType::NavState, body);
        }
    }
    return true;
}

void HostLink::sendLanded(const Landed& landed) {
    Writer body;
    writeLanded(body, landed);
    for (Client& client : clients_) {
        if (client.welcomed) {
            sendMessage(client, TcpType::Landed, body);
        }
    }
}

bool HostLink::sendVesselAdded(const VesselAdded& added) {
    Writer body;
    if (!writeVesselAdded(body, added)) {
        return false;
    }
    for (Client& client : clients_) {
        if (client.welcomed) {
            sendMessage(client, TcpType::VesselAdded, body);
        }
    }
    log("OLC host: vessel '" + added.name + "' (" + added.className + ") added as " +
        std::to_string(added.state.netId));
    // The plugin may have set the owner of the new vessel: clients should know it.
    broadcastOwnership();
    return true;
}

void HostLink::sendVesselRemoved(uint16_t netId) {
    anims_.erase(netId);
    navStates_.erase(netId);
    Writer body;
    writeVesselRemoved(body, netId);
    for (Client& client : clients_) {
        if (client.welcomed) {
            sendMessage(client, TcpType::VesselRemoved, body);
        }
    }
    log("OLC host: vessel " + std::to_string(netId) + " removed");
}

}  // namespace olc
