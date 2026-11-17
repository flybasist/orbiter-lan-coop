#include "host_link.h"

#include <utility>

#include "protocol.h"
#include "serialize.h"

namespace olc {

namespace {

// How many times we try to get the same UDP port number the system gave for TCP
// (only with port = 0: TCP and UDP ports are taken independently, the number may be busy).
constexpr int kPortAttempts = 20;

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

HostLink::HostLink(SessionTable& table) : table_(table) {}

HostLink::~HostLink() {
    stop("host stopped");
}

bool HostLink::start(const HostConfig& config) {
    if (running_) {
        return false;
    }
    config_ = config;

    // With port = 0 the system chooses the TCP port, and UDP is opened on the same number.
    int attempts = config.port == 0 ? kPortAttempts : 1;
    bool opened = false;
    for (int i = 0; i < attempts && !opened; i++) {
        if (!listener_.open(config.port)) {
            break;
        }
        opened = udp_.open(listener_.localPort());
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
            table_.removePlayer(client.clientId);
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

    // New connections.
    while (true) {
        TcpConnection connection;
        if (!listener_.accept(connection)) {
            break;
        }
        Client client;
        client.tcp = std::move(connection);
        client.connectedAt = now;
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
}

bool HostLink::handleFrames(Client& client) {
    uint8_t type = 0;
    std::vector<uint8_t> body;
    TcpFrameReader::Result result;
    while ((result = client.frames.next(type, body)) == TcpFrameReader::Result::Ready) {
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
            case TcpType::Bye: {
                std::string reason;
                if (!readBye(reader, reason)) {
                    reason = "no reason given";
                }
                client.leaveReason = reason;
                return false;
            }
            default:
                // Other messages (KEY and others) are not needed by the host at stage 1.
                break;
        }
    }

    if (result == TcpFrameReader::Result::Broken) {
        client.leaveReason = "broken TCP stream";
        return false;
    }
    return true;
}

bool HostLink::handleHello(Client& client, Reader& reader) {
    Hello hello;
    if (!readHello(reader, hello)) {
        client.leaveReason = "broken HELLO";
        return false;
    }

    if (hello.protocol != kProtocolVersion) {
        return reject(client, RejectReason::ProtocolVersion,
                      "protocol version mismatch: host " + std::to_string(kProtocolVersion) +
                          ", client " + std::to_string(hello.protocol));
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

    Welcome welcome;
    welcome.clientId = clientId;
    welcome.session = config_.session;
    welcome.stateRateHz = config_.stateRateHz;
    welcome.system = config_.system;
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
    log("OLC host: player '" + hello.player + "' joined as client " + std::to_string(clientId) +
        " (" + scenario + ")");
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
    if (client.welcomed) {
        table_.removePlayer(client.clientId);
        log("OLC host: client " + std::to_string(client.clientId) + " left (" + reason + ")");
    } else {
        log("OLC host: connection from " + endpointToString(client.tcp.remote()) +
            " closed (" + reason + ")");
    }
    // Send what is left in the queue (e.g. REJECT) and close.
    client.tcp.flush();
    client.tcp.close();
    clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(index));
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
            learnUdpAddress(ping.clientId, from);
            // PONG goes to whoever sent the PING, with the same body.
            Writer body;
            writePing(body, ping);
            if (udp_.sendTo(from, makeUdpPacket(UdpType::Pong, config_.session, seq_++,
                                                body.bytes()))) {
                stats_.udpPacketsSent++;
            }
        }
        // The host ignores INPUT and the rest at stage 1.
    }
}

void HostLink::learnUdpAddress(uint8_t clientId, const Endpoint& from) {
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
        client.udpFromPing = true;
        return;
    }
}

// ---------- Sending ----------

void HostLink::sendUdpToReady(uint8_t type, const std::vector<uint8_t>& body) {
    std::vector<uint8_t> packet =
        makeUdpPacket(static_cast<UdpType>(type), config_.session, seq_++, body);
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

void HostLink::sendLanded(const Landed& landed) {
    Writer body;
    writeLanded(body, landed);
    for (Client& client : clients_) {
        if (client.welcomed) {
            sendMessage(client, TcpType::Landed, body);
        }
    }
}

}  // namespace olc
